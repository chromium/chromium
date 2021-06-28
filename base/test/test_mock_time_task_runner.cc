// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_mock_time_task_runner.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// A SingleThreadTaskRunner which forwards everything to its |target_|. This
// serves two purposes:
// 1) If a ThreadTaskRunnerHandle owned by TestMockTimeTaskRunner were to be
//    set to point to that TestMockTimeTaskRunner, a reference cycle would
//    result.  As |target_| here is a non-refcounting raw pointer, the cycle is
//    broken.
// 2) Since SingleThreadTaskRunner is ref-counted, it's quite easy for it to
//    accidentally get captured between tests in a singleton somewhere.
//    Indirecting via NonOwningProxyTaskRunner permits TestMockTimeTaskRunner
//    to be cleaned up (removing the RunLoop::Delegate in the kBoundToThread
//    mode), and to also cleanly flag any actual attempts to use the leaked
//    task runner.
class TestMockTimeTaskRunner::NonOwningProxyTaskRunner
    : public SingleThreadTaskRunner {
 public:
  explicit NonOwningProxyTaskRunner(SingleThreadTaskRunner* target)
      : target_(target) {
    DCHECK(target_);
  }

  // Detaches this NonOwningProxyTaskRunner instance from its |target_|. It is
  // invalid to post tasks after this point but RunsTasksInCurrentSequence()
  // will still pass on the original thread for convenience with legacy code.
  void Detach() {
    AutoLock scoped_lock(lock_);
    target_ = nullptr;
  }

  // SingleThreadTaskRunner:
  bool RunsTasksInCurrentSequence() const override {
    AutoLock scoped_lock(lock_);
    if (target_)
      return target_->RunsTasksInCurrentSequence();
    return thread_checker_.CalledOnValidThread();
  }

  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       TimeDelta delay) override {
    AutoLock scoped_lock(lock_);
    if (target_)
      return target_->PostDelayedTask(from_here, std::move(task), delay);

    // The associated TestMockTimeTaskRunner is dead, so fail this PostTask.
    return false;
  }

  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  TimeDelta delay) override {
    AutoLock scoped_lock(lock_);
    if (target_) {
      return target_->PostNonNestableDelayedTask(from_here, std::move(task),
                                                 delay);
    }

    // The associated TestMockTimeTaskRunner is dead, so fail this PostTask.
    return false;
  }

 private:
  friend class RefCountedThreadSafe<NonOwningProxyTaskRunner>;
  ~NonOwningProxyTaskRunner() override = default;

  mutable Lock lock_;
  SingleThreadTaskRunner* target_;  // guarded by lock_

  // Used to implement RunsTasksInCurrentSequence, without relying on |target_|.
  ThreadCheckerImpl thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NonOwningProxyTaskRunner);
};

// TestMockTimeTaskRunner::TestOrderedPendingTask -----------------------------

// Subclass of TestPendingTask which has a strictly monotonically increasing ID
// for every task, so that tasks posted with the same 'time to run' can be run
// in the order of being posted.
struct TestMockTimeTaskRunner::TestOrderedPendingTask
    : public base::TestPendingTask {
  TestOrderedPendingTask();
  TestOrderedPendingTask(const Location& location,
                         OnceClosure task,
                         TimeTicks post_time,
                         TimeDelta delay,
                         size_t ordinal,
                         TestNestability nestability);
  TestOrderedPendingTask(TestOrderedPendingTask&&);
  ~TestOrderedPendingTask();

  TestOrderedPendingTask& operator=(TestOrderedPendingTask&&);

  size_t ordinal;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOrderedPendingTask);
};

TestMockTimeTaskRunner::TestOrderedPendingTask::TestOrderedPendingTask()
    : ordinal(0) {
}

TestMockTimeTaskRunner::TestOrderedPendingTask::TestOrderedPendingTask(
    TestOrderedPendingTask&&) = default;

TestMockTimeTaskRunner::TestOrderedPendingTask::TestOrderedPendingTask(
    const Location& location,
    OnceClosure task,
    TimeTicks post_time,
    TimeDelta delay,
    size_t ordinal,
    TestNestability nestability)
    : base::TestPendingTask(location,
                            std::move(task),
                            post_time,
                            delay,
                            nestability),
      ordinal(ordinal) {}

TestMockTimeTaskRunner::TestOrderedPendingTask::~TestOrderedPendingTask() =
    default;

TestMockTimeTaskRunner::TestOrderedPendingTask&
TestMockTimeTaskRunner::TestOrderedPendingTask::operator=(
    TestOrderedPendingTask&&) = default;

// TestMockTimeTaskRunner -----------------------------------------------------

// TODO(gab): This should also set the SequenceToken for the current thread.
// Ref. TestMockTimeTaskRunner::RunsTasksInCurrentSequence().
TestMockTimeTaskRunner::ScopedContext::ScopedContext(
    scoped_refptr<TestMockTimeTaskRunner> scope)
    : thread_task_runner_handle_override_(scope) {
  scope->RunUntilIdle();
}

TestMockTimeTaskRunner::ScopedContext::~ScopedContext() = default;

bool TestMockTimeTaskRunner::TemporalOrder::operator()(
    const TestOrderedPendingTask& first_task,
    const TestOrderedPendingTask& second_task) const {
  if (first_task.GetTimeToRun() == second_task.GetTimeToRun())
    return first_task.ordinal > second_task.ordinal;
  return first_task.GetTimeToRun() > second_task.GetTimeToRun();
}

TestMockTimeTaskRunner::TestMockTimeTaskRunner(Type type)
    : TestMockTimeTaskRunner(Time::UnixEpoch(), TimeTicks(), type) {}

TestMockTimeTaskRunner::TestMockTimeTaskRunner(Time start_time,
                                               TimeTicks start_ticks,
                                               Type type)
    : now_(start_time),
      now_ticks_(start_ticks),
      tasks_lock_cv_(&tasks_lock_),
      proxy_task_runner_(MakeRefCounted<NonOwningProxyTaskRunner>(this)),
      mock_clock_(this) {
  if (type == Type::kBoundToThread) {
    RunLoop::RegisterDelegateForCurrentThread(this);
    thread_task_runner_handle_ =
        std::make_unique<ThreadTaskRunnerHandle>(proxy_task_runner_);
  }
}

TestMockTimeTaskRunner::~TestMockTimeTaskRunner() {
  proxy_task_runner_->Detach();
}

void TestMockTimeTaskRunner::FastForwardBy(TimeDelta delta) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(delta, TimeDelta());

  const TimeTicks original_now_ticks = NowTicks();
  ProcessTasksNoLaterThan(delta);
  ForwardClocksUntilTickTime(original_now_ticks + delta);
}

void TestMockTimeTaskRunner::AdvanceMockTickClock(TimeDelta delta) {
  ForwardClocksUntilTickTime(NowTicks() + delta);
}

void TestMockTimeTaskRunner::AdvanceWallClock(TimeDelta delta) {
  now_ += delta;
  OnAfterTimePassed();
}

void TestMockTimeTaskRunner::RunUntilIdle() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProcessTasksNoLaterThan(TimeDelta());
}

void TestMockTimeTaskRunner::ProcessNextNTasks(int n) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProcessTasksNoLaterThan(TimeDelta::Max(), n);
}

void TestMockTimeTaskRunner::FastForwardUntilNoTasksRemain() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProcessTasksNoLaterThan(TimeDelta::Max());
}

void TestMockTimeTaskRunner::ClearPendingTasks() {
  AutoLock scoped_lock(tasks_lock_);
  // This is repeated in case task destruction triggers further tasks.
  while (!tasks_.empty()) {
    TaskPriorityQueue cleanup_tasks;
    tasks_.swap(cleanup_tasks);

    // Destroy task objects with |tasks_lock_| released. Task deletion can cause
    // calls to NonOwningProxyTaskRunner::RunsTasksInCurrentSequence()
    // (e.g. for DCHECKs), which causes |NonOwningProxyTaskRunner::lock_| to be
    // grabbed.
    //
    // On the other hand, calls from NonOwningProxyTaskRunner::PostTask ->
    // TestMockTimeTaskRunner::PostTask acquire locks as
    // |NonOwningProxyTaskRunner::lock_| followed by |tasks_lock_|, so it's
    // desirable to avoid the reverse order, for deadlock freedom.
    AutoUnlock scoped_unlock(tasks_lock_);
    while (!cleanup_tasks.empty())
      cleanup_tasks.pop();
  }
}

Time TestMockTimeTaskRunner::Now() const {
  AutoLock scoped_lock(tasks_lock_);
  return now_;
}

TimeTicks TestMockTimeTaskRunner::NowTicks() const {
  AutoLock scoped_lock(tasks_lock_);
  return now_ticks_;
}

Clock* TestMockTimeTaskRunner::GetMockClock() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return &mock_clock_;
}

const TickClock* TestMockTimeTaskRunner::GetMockTickClock() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return &mock_clock_;
}

base::circular_deque<TestPendingTask>
TestMockTimeTaskRunner::TakePendingTasks() {
  AutoLock scoped_lock(tasks_lock_);
  base::circular_deque<TestPendingTask> tasks;
  while (!tasks_.empty()) {
    // It's safe to remove const and consume |task| here, since |task| is not
    // used for ordering the item.
    if (!tasks_.top().task.IsCancelled()) {
      tasks.push_back(
          std::move(const_cast<TestOrderedPendingTask&>(tasks_.top())));
    }
    tasks_.pop();
  }
  return tasks;
}

bool TestMockTimeTaskRunner::HasPendingTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  AutoLock scoped_lock(tasks_lock_);
  while (!tasks_.empty() && tasks_.top().task.IsCancelled())
    tasks_.pop();
  return !tasks_.empty();
}

size_t TestMockTimeTaskRunner::GetPendingTaskCount() {
  DCHECK(thread_checker_.CalledOnValidThread());
  AutoLock scoped_lock(tasks_lock_);
  TaskPriorityQueue preserved_tasks;
  while (!tasks_.empty()) {
    if (!tasks_.top().task.IsCancelled()) {
      preserved_tasks.push(
          std::move(const_cast<TestOrderedPendingTask&>(tasks_.top())));
    }
    tasks_.pop();
  }
  tasks_.swap(preserved_tasks);
  return tasks_.size();
}

TimeDelta TestMockTimeTaskRunner::NextPendingTaskDelay() {
  DCHECK(thread_checker_.CalledOnValidThread());
  AutoLock scoped_lock(tasks_lock_);
  while (!tasks_.empty() && tasks_.top().task.IsCancelled())
    tasks_.pop();
  return tasks_.empty() ? TimeDelta::Max()
                        : tasks_.top().GetTimeToRun() - now_ticks_;
}

// TODO(gab): Combine |thread_checker_| with a SequenceToken to differentiate
// between tasks running in the scope of this TestMockTimeTaskRunner and other
// task runners sharing this thread. http://crbug.com/631186
bool TestMockTimeTaskRunner::RunsTasksInCurrentSequence() const {
  return thread_checker_.CalledOnValidThread();
}

bool TestMockTimeTaskRunner::PostDelayedTask(const Location& from_here,
                                             OnceClosure task,
                                             TimeDelta delay) {
  AutoLock scoped_lock(tasks_lock_);
  tasks_.push(TestOrderedPendingTask(from_here, std::move(task), now_ticks_,
                                     delay, next_task_ordinal_++,
                                     TestPendingTask::NESTABLE));
  tasks_lock_cv_.Signal();
  return true;
}

bool TestMockTimeTaskRunner::PostNonNestableDelayedTask(
    const Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  return PostDelayedTask(from_here, std::move(task), delay);
}

void TestMockTimeTaskRunner::OnBeforeSelectingTask() {
  // Empty default implementation.
}

void TestMockTimeTaskRunner::OnAfterTimePassed() {
  // Empty default implementation.
}

void TestMockTimeTaskRunner::OnAfterTaskRun() {
  // Empty default implementation.
}

void TestMockTimeTaskRunner::ProcessTasksNoLaterThan(TimeDelta max_delta,
                                                     int limit) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(max_delta, TimeDelta());

  // Multiple test task runners can share the same thread for determinism in
  // unit tests. Make sure this TestMockTimeTaskRunner's tasks run in its scope.
  absl::optional<ThreadTaskRunnerHandleOverrideForTesting> ttrh_override;
  if (!ThreadTaskRunnerHandle::IsSet() ||
      ThreadTaskRunnerHandle::Get() != proxy_task_runner_.get()) {
    ttrh_override.emplace(proxy_task_runner_.get());
  }

  const TimeTicks original_now_ticks = NowTicks();
  for (int i = 0; !quit_run_loop_ && (limit < 0 || i < limit); i++) {
    OnBeforeSelectingTask();
    TestPendingTask task_info;
    if (!DequeueNextTask(original_now_ticks, max_delta, &task_info))
      break;
    if (task_info.task.IsCancelled())
      continue;
    // If tasks were posted with a negative delay, task_info.GetTimeToRun() will
    // be less than |now_ticks_|. ForwardClocksUntilTickTime() takes care of not
    // moving the clock backwards in this case.
    ForwardClocksUntilTickTime(task_info.GetTimeToRun());
    std::move(task_info.task).Run();
    OnAfterTaskRun();
  }
}

void TestMockTimeTaskRunner::ForwardClocksUntilTickTime(TimeTicks later_ticks) {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    AutoLock scoped_lock(tasks_lock_);
    if (later_ticks <= now_ticks_)
      return;

    now_ += later_ticks - now_ticks_;
    now_ticks_ = later_ticks;
  }
  OnAfterTimePassed();
}

bool TestMockTimeTaskRunner::DequeueNextTask(const TimeTicks& reference,
                                             const TimeDelta& max_delta,
                                             TestPendingTask* next_task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AutoLock scoped_lock(tasks_lock_);
  if (!tasks_.empty() &&
      (tasks_.top().GetTimeToRun() - reference) <= max_delta) {
    // It's safe to remove const and consume |task| here, since |task| is not
    // used for ordering the item.
    *next_task = std::move(const_cast<TestOrderedPendingTask&>(tasks_.top()));
    tasks_.pop();
    return true;
  }
  return false;
}

void TestMockTimeTaskRunner::Run(bool application_tasks_allowed,
                                 TimeDelta timeout) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Since TestMockTimeTaskRunner doesn't process system messages: there's no
  // hope for anything but an application task to call Quit(). If this RunLoop
  // can't process application tasks (i.e. disallowed by default in nested
  // RunLoops) it's guaranteed to hang...
  DCHECK(application_tasks_allowed)
      << "This is a nested RunLoop instance and needs to be of "
         "Type::kNestableTasksAllowed.";

  // This computation relies on saturated arithmetic.
  TimeTicks run_until = now_ticks_ + timeout;
  while (!quit_run_loop_ && now_ticks_ < run_until) {
    RunUntilIdle();
    if (quit_run_loop_ || ShouldQuitWhenIdle())
      break;

    // Peek into |tasks_| to perform one of two things:
    //   A) If there are no remaining tasks, wait until one is posted and
    //      restart from the top.
    //   B) If there is a remaining delayed task. Fast-forward to reach the next
    //      round of tasks.
    TimeDelta auto_fast_forward_by;
    {
      AutoLock scoped_lock(tasks_lock_);
      if (tasks_.empty()) {
        while (tasks_.empty())
          tasks_lock_cv_.Wait();
        continue;
      }
      auto_fast_forward_by =
          std::min(run_until, tasks_.top().GetTimeToRun()) - now_ticks_;
    }
    FastForwardBy(auto_fast_forward_by);
  }
  quit_run_loop_ = false;
}

void TestMockTimeTaskRunner::Quit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  quit_run_loop_ = true;
}

void TestMockTimeTaskRunner::EnsureWorkScheduled() {
  // Nothing to do: TestMockTimeTaskRunner::Run() will always process tasks and
  // doesn't need an extra kick on nested runs.
}

TimeTicks TestMockTimeTaskRunner::MockClock::NowTicks() const {
  return task_runner_->NowTicks();
}

Time TestMockTimeTaskRunner::MockClock::Now() const {
  return task_runner_->Now();
}

}  // namespace base
