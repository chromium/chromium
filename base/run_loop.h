// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RUN_LOOP_H_
#define BASE_RUN_LOOP_H_

#include <stack>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/containers/stack.h"
#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

namespace test {
class ScopedRunLoopTimeout;
class ScopedDisableRunLoopTimeout;
}  // namespace test

#if defined(OS_ANDROID)
class MessagePumpForUI;
#endif

#if defined(OS_IOS)
class MessagePumpUIApplication;
#endif

class SingleThreadTaskRunner;

// Helper class to run the RunLoop::Delegate associated with the current thread.
// A RunLoop::Delegate must have been bound to this thread (ref.
// RunLoop::RegisterDelegateForCurrentThread()) prior to using any of RunLoop's
// member and static methods unless explicitly indicated otherwise (e.g.
// IsRunning/IsNestedOnCurrentThread()). RunLoop::Run can only be called once
// per RunLoop lifetime. Create a RunLoop on the stack and call Run/Quit to run
// a nested RunLoop but please avoid nested loops in production code!
// 帮助类，运行与当前线程关联的 RunLoop::Delegate。
// RunLoop::Delegate 必须已绑定到此线程（参考。
// RunLoop::RegisterDelegateForCurrentThread()) 在使用 RunLoop 的任何成员和静态方法之前，
// 除非另有明确说明（例如
// IsRunning/IsNestedOnCurrentThread())。 RunLoop::Run 在每个 RunLoop 生命周期中只能调
// 用一次。 在堆栈上创建一个 RunLoop 并调用 Run/Quit 来运行一个嵌套的 RunLoop，但请避免在生
// 产代码中使用嵌套循环！
class BASE_EXPORT RunLoop {
 public:
  // The type of RunLoop: a kDefault RunLoop at the top-level (non-nested) will
  // process system and application tasks assigned to its Delegate. When nested
  // however a kDefault RunLoop will only process system tasks while a
  // kNestableTasksAllowed RunLoop will continue to process application tasks
  // even if nested.
  // RunLoop 的类型：顶层（非嵌套）的 kDefault RunLoop 将处理分配给其 Delegate 的系统
  // 和应用程序任务。 然而，当嵌套时，kDefault RunLoop 将只处理系统任务，而
  // kNestableTasksAllowed RunLoop 将继续处理应用程序任务，即使嵌套也是如此。
  //
  // This is relevant in the case of recursive RunLoops. Some unwanted run loops
  // may occur when using common controls or printer functions. By default,
  // recursive task processing is disabled.
  // 这与递归 RunLoops 的情况有关。 使用常用控件或打印机功能时，可能会出现一些不需要的运行
  // 循环。 默认情况下，递归任务处理被禁用。
  //
  // In general, nestable RunLoops are to be avoided. They are dangerous and
  // difficult to get right, so please use with extreme caution.
  //
  // A specific example where this makes a difference is:
  // - The thread is running a RunLoop.
  // - It receives a task #1 and executes it.
  // - The task #1 implicitly starts a RunLoop, like a MessageBox in the unit
  //   test. This can also be StartDoc or GetSaveFileName.
  // - The thread receives a task #2 before or while in this second RunLoop.
  // - With a kNestableTasksAllowed RunLoop, the task #2 will run right away.
  //   Otherwise, it will get executed right after task #1 completes in the main
  //   RunLoop.
  // 上面描述的就是一个正常thread隐式启动一个消息循环，开始结束任务，并执行。
  enum class Type {
    kDefault,
    kNestableTasksAllowed, // 嵌套，一般不用
  };

  explicit RunLoop(Type type = Type::kDefault);
  RunLoop(const RunLoop&) = delete;
  RunLoop& operator=(const RunLoop&) = delete;
  ~RunLoop();

  // Run the current RunLoop::Delegate. This blocks until Quit is called
  // (directly or by running the RunLoop::QuitClosure).
  // 运行当前的 RunLoop::Delegate。 这会一直阻塞，直到调用 Quit（直接或通过运行
  // RunLoop::QuitClosure）。
  void Run(const Location& location = Location::Current());

  // Run the current RunLoop::Delegate until it doesn't find any tasks or
  // messages in its queue (it goes idle).
  // 运行当前的 RunLoop::Delegate 直到它在其队列中找不到任何任务或消息（它进入空闲状态）
  // WARNING #1: This may run long (flakily timeout) and even never return! Do
  //             not use this when repeating tasks such as animated web pages
  //             are present.
  //             这可能会运行很长时间（片状超时），甚至永远不会返回！ 当存在重复任务
  //             （例如动画网页）时，请勿使用此选项。
  // WARNING #2: This may return too early! For example, if used to run until an
  //             incoming event has occurred but that event depends on a task in
  //             a different queue -- e.g. another TaskRunner or a system event.
  //             这可能会为时过早！例如，如果用于运行直到发生传入事件但该事件取决于不同队列
  //             中的任务 - 例如 另一个 TaskRunner 或系统事件。
  // Per the warnings above, this tends to lead to flaky tests; prefer
  // QuitClosure()+Run() when at all possible.
  // 根据上面的警告，这往往会导致不稳定的测试；尽可能选择 QuitClosure() + Run()。
  void RunUntilIdle();

  bool running() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return running_;
  }

  // Quit() transitions this RunLoop to a state where no more tasks will be
  // allowed to run at the run-loop-level of this RunLoop. If invoked from the
  // owning thread, the effect is immediate; otherwise it is thread-safe but
  // asynchronous. When the transition takes effect, the underlying message loop
  // quits this run-loop-level if it is topmost (otherwise the desire to quit
  // this level is saved until run-levels nested above it are quit).
  // Quit() 将此 RunLoop 转换为不允许更多任务在此 RunLoop 的运行循环级别运行的状态。
  // 如果从拥有的线程调用，效果是立即的；否则它是线程安全但异步的。当转换生效时，如果底
  // 层消息循环位于最顶层，则退出此运行循环级别（否则会保存退出此级别的愿望，直到嵌套在其
  // 上方的运行级别退出）。
  //
  // QuitWhenIdle() results in this RunLoop returning true from
  // ShouldQuitWhenIdle() at this run-level (the delegate decides when "idle" is
  // reached). This is also thread-safe.
  // QuitWhenIdle() 导致此 RunLoop 在此运行级别从 ShouldQuitWhenIdle() 返回 true（
  // Delegate 决定何时达到“空闲”）。 这也是线程安全的。
  //
  // There can be other nested RunLoops servicing the same task queue. As
  // mentioned above, quitting one RunLoop has no bearing on the others. Hence,
  // you may never assume that a call to Quit() will terminate the underlying
  // message loop. If a nested RunLoop continues running, the target may NEVER
  // terminate.
  void Quit();
  void QuitWhenIdle();

  // Returns a RepeatingClosure that safely calls Quit() or QuitWhenIdle() (has
  // no effect if the RunLoop instance is gone).
  // 返回一个可以安全调用 Quit() 或 QuitWhenIdle() 的 RepeatingClosure（如果 RunLoop
  // 实例消失则无效）。
  //
  // The closures must be obtained from the thread owning the RunLoop but may
  // then be invoked from any thread.
  // 闭包必须从拥有 RunLoop 的线程获得，但随后可以从任何线程调用。
  //
  // Returned closures may be safely:
  //   * Passed to other threads.
  //   * Run() from other threads, though this will quit the RunLoop
  //     asynchronously.
  //   * Run() after the RunLoop has stopped or been destroyed, in which case
  //     they are a no-op).
  //   * Run() before RunLoop::Run(), in which case RunLoop::Run() returns
  //     immediately."
  //
  // Example:
  //   RunLoop run_loop;
  //   DoFooAsyncAndNotify(run_loop.QuitClosure());
  //   run_loop.Run();
  //
  // Note that Quit() itself is thread-safe and may be invoked directly if you
  // have access to the RunLoop reference from another thread (e.g. from a
  // capturing lambda or test observer).
  // 请注意，Quit() 本身是线程安全的，如果您可以从另一个线程（例如，从捕获 lambda 或测试
  // 观察者）访问 RunLoop 引用，则可以直接调用它。
  RepeatingClosure QuitClosure();
  RepeatingClosure QuitWhenIdleClosure();

  // Returns true if Quit() or QuitWhenIdle() was called.
  bool AnyQuitCalled();

  // Returns true if there is an active RunLoop on this thread.
  // 如果此线程上有一个活动的 RunLoop，则返回 true。
  // Safe to call before RegisterDelegateForCurrentThread().
  // 在 RegisterDelegateForCurrentThread() 之前调用是安全的。
  static bool IsRunningOnCurrentThread();

  // Returns true if there is an active RunLoop on this thread and it's nested
  // within another active RunLoop.
  // Safe to call before RegisterDelegateForCurrentThread().
  static bool IsNestedOnCurrentThread();

  // A NestingObserver is notified when a nested RunLoop begins and ends.
  class BASE_EXPORT NestingObserver { // 嵌套场景，略
   public:
    // Notified before a nested loop starts running work on the current thread.
    virtual void OnBeginNestedRunLoop() = 0;
    // Notified after a nested loop is done running work on the current thread.
    virtual void OnExitNestedRunLoop() {}

   protected:
    virtual ~NestingObserver() = default;
  };

  static void AddNestingObserverOnCurrentThread(NestingObserver* observer);
  static void RemoveNestingObserverOnCurrentThread(NestingObserver* observer);

  // A RunLoop::Delegate is a generic interface that allows RunLoop to be
  // separate from the underlying implementation of the message loop for this
  // thread. It holds private state used by RunLoops on its associated thread.
  // One and only one RunLoop::Delegate must be registered on a given thread
  // via RunLoop::RegisterDelegateForCurrentThread() before RunLoop instances
  // and RunLoop static methods can be used on it.
  // RunLoop::Delegate 是一个通用接口，它允许 RunLoop 与该线程的消息循环的底层实现分开。
  // 它持有 RunLoops 在其关联线程上使用的私有状态。 一个且只有一个 RunLoop::Delegate
  // 必须通过 RunLoop::RegisterDelegateForCurrentThread() 在给定线程上注册，然后才
  // 能在其上使用 RunLoop 实例和 RunLoop 静态方法。
  class BASE_EXPORT Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate();

    // Used by RunLoop to inform its Delegate to Run/Quit. Implementations are
    // expected to keep on running synchronously from the Run() call until the
    // eventual matching Quit() call or a delay of |timeout| expires. Upon
    // receiving a Quit() call or timing out it should return from the Run()
    // call as soon as possible without executing remaining tasks/messages.
    // Run() calls can nest in which case each Quit() call should result in the
    // topmost active Run() call returning. The only other trigger for Run()
    // to return is the |should_quit_when_idle_callback_| which the Delegate
    // should probe before sleeping when it becomes idle.
    // |application_tasks_allowed| is true if this is the first Run() call on
    // the stack or it was made from a nested RunLoop of
    // Type::kNestableTasksAllowed (otherwise this Run() level should only
    // process system tasks).
    // RunLoop 使用它来通知其 Delegate 运行/退出。实现应该从 Run() 调用开始同步运行，直
    // 到最终匹配 Quit() 调用或延迟 |timeout| 过期。在收到 Quit() 调用或超时后，它应该尽
    // 快从 Run() 调用返回，而不执行剩余的任务/消息。Run() 调用可以嵌套，在这种情况下，每
    // 个 Quit() 调用都应该导致最上面的活动 Run() 调用返回。 Run() 返回的唯一其他触发
    // 器是 |should_quit_when_idle_callback_| 当它变得空闲时，Delegate 应该在睡眠之
    // 前探测它。 |application_tasks_allowed| 如果这是堆栈上的第一个 Run() 调用，或
    // 者它是由 Type::kNestableTasksAllowed 的嵌套 RunLoop 进行的，则为 true（否则
    // 此 Run() 级别应该只处理系统任务）。
    virtual void Run(bool application_tasks_allowed, TimeDelta timeout) = 0;
    virtual void Quit() = 0;

    // Invoked right before a RunLoop enters a nested Run() call on this
    // Delegate iff this RunLoop is of type kNestableTasksAllowed. The Delegate
    // should ensure that the upcoming Run() call will result in processing
    // application tasks queued ahead of it without further probing. e.g.
    // message pumps on some platforms, like Mac, need an explicit request to
    // process application tasks when nested, otherwise they'll only wait for
    // system messages.
    // 尽在嵌套场景使用，这里略
    virtual void EnsureWorkScheduled() = 0;

   protected:
    // Returns the result of this Delegate's |should_quit_when_idle_callback_|.
    // "protected" so it can be invoked only by the Delegate itself. The
    // Delegate is expected to quit Run() if this returns true.
    // 返回此 Delegate 的 |should_quit_when_idle_callback_| 的结果。 “受保护”，因此
    // 只能由 Delegate 本身调用。 如果返回 true，则 Delegate 应退出 Run()。
    bool ShouldQuitWhenIdle();

   private:
    // While the state is owned by the Delegate subclass, only RunLoop can use it.
    // 虽然状态归 Delegate 子类所有，但只有 RunLoop 可以使用它。
    friend class RunLoop;

    // A vector-based stack is more memory efficient than the default
    // deque-based stack as the active RunLoop stack isn't expected to ever
    // have more than a few entries.
    // 基于向量的堆栈比默认的基于双端队列的栈更节省内存，因为预计活动的 RunLoop 栈不会有多个条目
    using RunLoopStack = stack<RunLoop*, std::vector<RunLoop*>>;

    RunLoopStack active_run_loops_;
    ObserverList<RunLoop::NestingObserver>::Unchecked nesting_observers_;

#if DCHECK_IS_ON()
    bool allow_running_for_testing_ = true;
#endif

    // True once this Delegate is bound to a thread via
    // RegisterDelegateForCurrentThread().
    // 一旦此 Delegate 通过 RegisterDelegateForCurrentThread() 绑定到线程，则为true。
    bool bound_ = false;

    // Thread-affine per its use of TLS.
    // 根据其使用 TLS 的线程仿射。
    THREAD_CHECKER(bound_thread_checker_);
  };

  // Registers |delegate| on the current thread. Must be called once and only
  // once per thread before using RunLoop methods on it. |delegate| is from then
  // on forever bound to that thread (including its destruction).
  static void RegisterDelegateForCurrentThread(Delegate* delegate);

  // Quits the active RunLoop (when idle) -- there must be one. These were
  // introduced as prefered temporary replacements to the long deprecated
  // MessageLoop::Quit(WhenIdle)(Closure) methods. Callers should properly plumb
  // a reference to the appropriate RunLoop instance (or its QuitClosure)
  // instead of using these in order to link Run()/Quit() to a single RunLoop
  // instance and increase readability.
  // 退出active的 RunLoop（空闲时）——必须有一个。这些被引入作为首选临时替代长期弃用的
  // MessageLoop::Quit(WhenIdle)(Closure) 方法。调用者应该正确地检测对适当 RunLoop
  // 实例（或其 QuitClosure）的引用，而不是使用这些引用来将 Run()/Quit() 链接到单个
  // RunLoop 实例并增加可读性。
  static void QuitCurrentDeprecated();
  static void QuitCurrentWhenIdleDeprecated();
  static RepeatingClosure QuitCurrentWhenIdleClosureDeprecated();

  // Run() will DCHECK if called while there's a ScopedDisallowRunning
  // in scope on its thread. This is useful to add safety to some test
  // constructs which allow multiple task runners to share the main thread in
  // unit tests. While the main thread can be shared by multiple runners to
  // deterministically fake multi threading, there can still only be a single
  // RunLoop::Delegate per thread and RunLoop::Run() should only be invoked from
  // it (or it would result in incorrectly driving TaskRunner A while in
  // TaskRunner B's context).
  // 如果在其线程的范围内存在 ScopedDisallowRunning 时调用 Run()，它将 DCHECK。 这
  // 对于增加一些测试结构的安全性很有用，这些结构允许多个任务运行器在单元测试中共享主线程。
  // 虽然主线程可以由多个运行器共享以确定性地伪造多线程，但每个线程仍然只能有一个
  // RunLoop::Delegate 并且 RunLoop::Run() 只能从中调用（否则会导致错误驱动
  // TaskRunner A 在 TaskRunner B 的上下文中）。
  class BASE_EXPORT ScopedDisallowRunning {
   public:
    ScopedDisallowRunning();
    ScopedDisallowRunning(const ScopedDisallowRunning&) = delete;
    ScopedDisallowRunning& operator=(const ScopedDisallowRunning&) = delete;
    ~ScopedDisallowRunning();

   private:
#if DCHECK_IS_ON()
    Delegate* current_delegate_;
    const bool previous_run_allowance_;
#endif  // DCHECK_IS_ON()
  };

  // Support for //base/test/scoped_run_loop_timeout.h.
  // This must be public for access by the implementation code in run_loop.cc.
  // 这必须是 public 的，以便 run_loop.cc 中的实现代码访问。
  struct BASE_EXPORT RunLoopTimeout {
    RunLoopTimeout();
    ~RunLoopTimeout();
    TimeDelta timeout;
    RepeatingCallback<void(const Location&)> on_timeout;
  };

 private:
  FRIEND_TEST_ALL_PREFIXES(SingleThreadTaskExecutorTypedTest,
                           RunLoopQuitOrderAfter);

#if defined(OS_ANDROID)
  // Android doesn't support the blocking RunLoop::Run, so it calls
  // BeforeRun and AfterRun directly.
  // Android 不支持阻塞式 RunLoop::Run，所以直接调用 BeforeRun 和 AfterRun。
  friend class MessagePumpForUI;
#endif

#if defined(OS_IOS)
  // iOS doesn't support the blocking RunLoop::Run, so it calls
  // BeforeRun directly.
  // iOS 不支持阻塞式 RunLoop::Run，所以直接调用 BeforeRun。
  friend class MessagePumpUIApplication;
#endif

  // Support for //base/test/scoped_run_loop_timeout.h.
  friend class test::ScopedRunLoopTimeout;
  friend class test::ScopedDisableRunLoopTimeout;

  static void SetTimeoutForCurrentThread(const RunLoopTimeout* timeout);
  static const RunLoopTimeout* GetTimeoutForCurrentThread();

  // Return false to abort the Run.
  bool BeforeRun();
  void AfterRun();

  // A cached reference of RunLoop::Delegate for the thread driven by this
  // RunLoop for quick access without using TLS (also allows access to state
  // from another sequence during Run(), ref. |sequence_checker_| below).
  Delegate* const delegate_;

  const Type type_;

#if DCHECK_IS_ON()
  bool run_allowed_ = true;
#endif

  bool quit_called_ = false;
  bool running_ = false;

  // Used to record that QuitWhenIdle() was called on this RunLoop.
  bool quit_when_idle_called_ = false;
  // Whether the Delegate should quit Run() once it becomes idle (it's
  // responsible for probing this state via ShouldQuitWhenIdle()). This state is
  // stored here rather than pushed to Delegate to support nested RunLoops.
  bool quit_when_idle_ = false;

  // True if use of QuitCurrent*Deprecated() is allowed. Taking a Quit*Closure()
  // from a RunLoop implicitly sets this to false, so QuitCurrent*Deprecated()
  // cannot be used while that RunLoop is being Run().
  bool allow_quit_current_deprecated_ = true;

  // RunLoop is not thread-safe. Its state/methods, unless marked as such, may
  // not be accessed from any other sequence than the thread it was constructed
  // on. Exception: RunLoop can be safely accessed from one other sequence (or
  // single parallel task) during Run() -- e.g. to Quit() without having to
  // plumb ThreatTaskRunnerHandle::Get() throughout a test to repost QuitClosure
  // to origin thread.
  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<SingleThreadTaskRunner> origin_task_runner_;

  // WeakPtrFactory for QuitClosure safety.
  WeakPtrFactory<RunLoop> weak_factory_{this};
};

}  // namespace base

#endif  // BASE_RUN_LOOP_H_
