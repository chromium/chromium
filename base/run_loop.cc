// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"

#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {

namespace {

constinit thread_local RunLoop::Delegate* delegate = nullptr;
constinit thread_local const RunLoop::RunLoopTimeout* run_loop_timeout =
    nullptr;

// Runs |closure| immediately if this is called on |task_runner|, otherwise
// forwards |closure| to it.
void ProxyToTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner,
                       OnceClosure closure) {
  if (task_runner->RunsTasksInCurrentSequence()) {
    std::move(closure).Run();
    return;
  }
  task_runner->PostTask(FROM_HERE, std::move(closure));
}

void OnRunLoopTimeout(RunLoop* run_loop,
                      const Location& location,
                      OnceCallback<void(const Location&)> on_timeout) {
  run_loop->Quit();
  std::move(on_timeout).Run(location);
}

}  // namespace

RunLoop::Delegate::Delegate() {
  // The Delegate can be created on another thread. It is only bound in
  // RegisterDelegateForCurrentThread().
  DETACH_FROM_THREAD(bound_thread_checker_);
}

RunLoop::Delegate::~Delegate() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  DCHECK(active_run_loops_.empty());
  // A RunLoop::Delegate may be destroyed before it is bound, if so it may still
  // be on its creation thread (e.g. a Thread that fails to start) and
  // shouldn't disrupt that thread's state.
  if (bound_) {
    DCHECK_EQ(this, delegate);
    delegate = nullptr;
  }
}

bool RunLoop::Delegate::ShouldQuitWhenIdle() {
  const auto* top_loop = active_run_loops_.top().get();
  if (top_loop->quit_when_idle_) {
    TRACE_EVENT_WITH_FLOW0("toplevel.flow", "RunLoop_ExitedOnIdle",
                           TRACE_ID_LOCAL(top_loop), TRACE_EVENT_FLAG_FLOW_IN);
    return true;
  }
  return false;
}

// static
void RunLoop::RegisterDelegateForCurrentThread(Delegate* new_delegate) {
  // Bind |delegate| to this thread.
  DCHECK(!new_delegate->bound_);
  DCHECK_CALLED_ON_VALID_THREAD(new_delegate->bound_thread_checker_);

  // There can only be one RunLoop::Delegate per thread.
  DCHECK(!delegate)
      << "Error: Multiple RunLoop::Delegates registered on the same thread.\n\n"
         "Hint: You perhaps instantiated a second "
         "MessageLoop/TaskEnvironment on a thread that already had one?";
  delegate = new_delegate;
  delegate->bound_ = true;
}

RunLoop::RunLoop(Type type)
    : delegate_(delegate),
      type_(type),
      origin_task_runner_(SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(delegate_) << "A RunLoop::Delegate must be bound to this thread prior "
                       "to using RunLoop.";
  DCHECK(origin_task_runner_);
}

RunLoop::~RunLoop() {
  // ~RunLoop() must happen-after the RunLoop is done running but it doesn't
  // have to be on |sequence_checker_| (it usually is but sometimes it can be a
  // member of a RefCountedThreadSafe object and be destroyed on another thread
  // after being quit).
  DCHECK(!running_);
}

void RunLoop::Run(const Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // "test" tracing category is used here because in regular scenarios RunLoop
  // trace events are not useful (each process normally has one RunLoop covering
  // its entire lifetime) and might be confusing (they make idle processes look
  // non-idle). In tests, however, creating a RunLoop is a frequent and an
  // explicit action making this trace event very useful.
  TRACE_EVENT("test", "RunLoop::Run", "location", location);

  if (!BeforeRun())
    return;

  // If there is a RunLoopTimeout active then set the timeout.
  // TODO(crbug.com/40602467): Use real-time for Run() timeouts so that they
  // can be applied even in tests which mock TimeTicks::Now().
  CancelableOnceClosure cancelable_timeout;
  const RunLoopTimeout* run_timeout = GetTimeoutForCurrentThread();
  if (run_timeout) {
    cancelable_timeout.Reset(BindOnce(&OnRunLoopTimeout, Unretained(this),
                                      location, run_timeout->on_timeout));
    origin_task_runner_->PostDelayedTask(
        FROM_HERE, cancelable_timeout.callback(), run_timeout->timeout);
  }

  DCHECK_EQ(this, delegate_->active_run_loops_.top());
  const bool application_tasks_allowed =
      delegate_->active_run_loops_.size() == 1U ||
      type_ == Type::kNestableTasksAllowed;
  delegate_->Run(application_tasks_allowed, TimeDelta::Max());

  AfterRun();
}

void RunLoop::RunUntilIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  quit_when_idle_ = true;
  Run();

  if (!AnyQuitCalled()) {
    quit_when_idle_ = false;
#if DCHECK_IS_ON()
    run_allowed_ = true;
#endif
  }
}

void RunLoop::Quit() {
  // Thread-safe.

  // This can only be hit if RunLoop::Quit() is called directly (QuitClosure()
  // proxies through ProxyToTaskRunner() as it can only deref its WeakPtr on
  // |origin_task_runner_|).
  if (!origin_task_runner_->RunsTasksInCurrentSequence()) {
    origin_task_runner_->PostTask(FROM_HERE,
                                  BindOnce(&RunLoop::Quit, Unretained(this)));
    return;
  }

  // While Quit() is an "OUT" call to reach one of the quit-states ("IN"),
  // OUT|IN is used to visually link multiple Quit*() together which can help
  // when debugging flaky tests.
  TRACE_EVENT_WITH_FLOW0("toplevel.flow", "RunLoop::Quit", TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN);

  quit_called_ = true;
  if (running_ && delegate_->active_run_loops_.top() == this) {
    // This is the inner-most RunLoop, so quit now.
    delegate_->Quit();
  }
}

void RunLoop::QuitWhenIdle() {
  // Thread-safe.

  // This can only be hit if RunLoop::QuitWhenIdle() is called directly
  // (QuitWhenIdleClosure() proxies through ProxyToTaskRunner() as it can only
  // deref its WeakPtr on |origin_task_runner_|).
  if (!origin_task_runner_->RunsTasksInCurrentSequence()) {
    origin_task_runner_->PostTask(
        FROM_HERE, BindOnce(&RunLoop::QuitWhenIdle, Unretained(this)));
    return;
  }

  // OUT|IN as in Quit() to link all Quit*() together should there be multiple.
  TRACE_EVENT_WITH_FLOW0("toplevel.flow", "RunLoop::QuitWhenIdle",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN);

  quit_when_idle_ = true;
  quit_when_idle_called_ = true;
}

RepeatingClosure RunLoop::QuitClosure() & {
  // Obtaining the QuitClosure() is not thread-safe; either obtain the
  // QuitClosure() from the owning thread before Run() or invoke Quit() directly
  // (which is thread-safe).
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return BindRepeating(
      &ProxyToTaskRunner, origin_task_runner_,
      BindRepeating(&RunLoop::Quit, weak_factory_.GetWeakPtr()));
}

RepeatingClosure RunLoop::QuitWhenIdleClosure() & {
  // Obtaining the QuitWhenIdleClosure() is not thread-safe; either obtain the
  // QuitWhenIdleClosure() from the owning thread before Run() or invoke
  // QuitWhenIdle() directly (which is thread-safe).
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return BindRepeating(
      &ProxyToTaskRunner, origin_task_runner_,
      BindRepeating(&RunLoop::QuitWhenIdle, weak_factory_.GetWeakPtr()));
}

bool RunLoop::AnyQuitCalled() {
  return quit_called_ || quit_when_idle_called_;
}

// static
bool RunLoop::IsRunningOnCurrentThread() {
  return delegate && !delegate->active_run_loops_.empty();
}

// static
bool RunLoop::IsNestedOnCurrentThread() {
  return delegate && delegate->active_run_loops_.size() > 1;
}

// static
void RunLoop::AddNestingObserverOnCurrentThread(NestingObserver* observer) {
  DCHECK(delegate);
  delegate->nesting_observers_.AddObserver(observer);
}

// static
void RunLoop::RemoveNestingObserverOnCurrentThread(NestingObserver* observer) {
  DCHECK(delegate);
  delegate->nesting_observers_.RemoveObserver(observer);
}


#if DCHECK_IS_ON()
ScopedDisallowRunningRunLoop::ScopedDisallowRunningRunLoop()
    : current_delegate_(delegate),
      previous_run_allowance_(current_delegate_ &&
                              current_delegate_->allow_running_for_testing_) {
  if (current_delegate_)
    current_delegate_->allow_running_for_testing_ = false;
}

ScopedDisallowRunningRunLoop::~ScopedDisallowRunningRunLoop() {
  DCHECK_EQ(current_delegate_, delegate);
  if (current_delegate_)
    current_delegate_->allow_running_for_testing_ = previous_run_allowance_;
}
#else   // DCHECK_IS_ON()
// Defined out of line so that the compiler doesn't inline these and realize
// the scope has no effect and then throws an "unused variable" warning in
// non-dcheck builds.
ScopedDisallowRunningRunLoop::ScopedDisallowRunningRunLoop() = default;
ScopedDisallowRunningRunLoop::~ScopedDisallowRunningRunLoop() = default;
#endif  // DCHECK_IS_ON()

RunLoop::RunLoopTimeout::RunLoopTimeout() = default;

RunLoop::RunLoopTimeout::~RunLoopTimeout() = default;

// static
void RunLoop::SetTimeoutForCurrentThread(const RunLoopTimeout* timeout) {
  run_loop_timeout = timeout;
}

// static
const RunLoop::RunLoopTimeout* RunLoop::GetTimeoutForCurrentThread() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&run_loop_timeout, sizeof(RunLoopTimeout*));

  return run_loop_timeout;
}

bool RunLoop::BeforeRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(delegate_->allow_running_for_testing_)
      << "RunLoop::Run() isn't allowed in the scope of a "
         "ScopedDisallowRunningRunLoop. Hint: if mixing "
         "TestMockTimeTaskRunners on same thread, use TestMockTimeTaskRunner's "
         "API instead of RunLoop to drive individual task runners.";
  DCHECK(run_allowed_);
  run_allowed_ = false;
#endif  // DCHECK_IS_ON()

  // Allow Quit to be called before Run.
  if (quit_called_) {
    TRACE_EVENT_WITH_FLOW0("toplevel.flow", "RunLoop_ExitedEarly",
                           TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
    return false;
  }

  auto& active_run_loops = delegate_->active_run_loops_;
  active_run_loops.push(this);

  const bool is_nested = active_run_loops.size() > 1;

  if (is_nested) {
    for (auto& observer : delegate_->nesting_observers_)
      observer.OnBeginNestedRunLoop();
    if (type_ == Type::kNestableTasksAllowed)
      delegate_->EnsureWorkScheduled();
  }

  running_ = true;
  return true;
}

void RunLoop::AfterRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  running_ = false;

  TRACE_EVENT_WITH_FLOW0("toplevel.flow", "RunLoop_Exited",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);

  auto& active_run_loops = delegate_->active_run_loops_;
  DCHECK_EQ(active_run_loops.top(), this);
  active_run_loops.pop();

  // Exiting a nested RunLoop?
  if (!active_run_loops.empty()) {
    for (auto& observer : delegate_->nesting_observers_)
      observer.OnExitNestedRunLoop();

    // Execute deferred Quit, if any:
    if (active_run_loops.top()->quit_called_)
      delegate_->Quit();
  }
}

}  // namespace base
