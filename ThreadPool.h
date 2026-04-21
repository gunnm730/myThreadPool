#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <type_traits>
std::mutex cout_mtx;
class ThreadPool
{
public:
    using Task = std::function<void()>;
    ThreadPool(int numThreads) : _isstop(false)
    {
        for (int i = 0; i < numThreads; ++i)
        {
            _threads.emplace_back([this]()
                                  {
                while(1){
                    std::unique_lock<std::mutex> lock(_mtx);
                    _condition.wait(lock, [this]()
                                    { return !_tasks.empty() || _isstop; });
                    if(_isstop&& _tasks.empty())
                        return;

                    Task curtask(std::move(_tasks.front()));
                    _tasks.pop();
                    lock.unlock();
                    curtask();
                } }

            );
        }
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _isstop = true;
        }
        _condition.notify_all();
        for (auto &t : _threads)
        {
            t.join();
        }
    }

    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type =
            std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable
            {
                return f(std::move(args)...);
            });
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // don't allow enqueueing after stopping the pool
            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");

            tasks.emplace([task]()
                          { (*task)(); });
        }
        _condition.notify_one();
        return res;
    }

private:
    std::vector<std::thread> _threads;
    std::queue<Task> _tasks;

    std::mutex _mtx;
    std::condition_variable _condition;

    bool _isstop;
};

#endif
