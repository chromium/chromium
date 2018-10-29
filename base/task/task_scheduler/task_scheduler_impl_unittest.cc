// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/task_scheduler_impl.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/cfi_buildflags.h"
#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_scheduler/environment_config.h"
#include "base/task/task_scheduler/scheduler_worker_observer.h"
#include "base/task/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task/task_scheduler/test_task_factory.h"
#include "base/task/task_scheduler/test_utils.h"
#include "base/task/task_traits.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <unistd.h>

#include "base/debug/leak_annotations.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
#include "base/win/com_init_util.h"
#endif  // defined(OS_WIN)

namespace base {
namespace internal {

namespace {

enum class PoolConfiguration {
  kDefault,
  kMergeBlockingNonBlocking,
};

enum class SchedulerState {
  // TaskScheduler::Start() was not called yet, no thread was created.
  kBeforeSchedulerStart,
  // TaskScheduler::Start() has been called.
  kAfterSchedulerStart,
};

struct TaskSchedulerImplTestParams {
  TaskSchedulerImplTestParams(const TaskTraits& traits,
                              test::ExecutionMode execution_mode,
                              PoolConfiguration pool_config)
      : traits(traits),
        execution_mode(execution_mode),
        pool_config(pool_config) {}

  TaskTraits traits;
  test::ExecutionMode execution_mode;
  PoolConfiguration pool_config;
};

#if DCHECK_IS_ON()
// Returns whether I/O calls are allowed on the current thread.
bool GetIOAllowed() {
  const bool previous_value = ThreadRestrictions::SetIOAllowed(true);
  ThreadRestrictions::SetIOAllowed(previous_value);
  return previous_value;
}
#endif

// Verify that the current thread priority and I/O restrictions are appropriate
// to run a Task with |traits|.
// Note: ExecutionMode is verified inside TestTaskFactory.
void VerifyTaskEnvironment(const TaskTraits& traits, SchedulerState state) {
  EXPECT_EQ(CanUseBackgroundPriorityForSchedulerWorker() &&
                    traits.priority() == TaskPriority::BEST_EFFORT
                ? ThreadPriority::BACKGROUND
                : ThreadPriority::NORMAL,
            PlatformThread::GetCurrentThreadPriority());

#if DCHECK_IS_ON()
  // The #if above is required because GetIOAllowed() always returns true when
  // !DCHECK_IS_ON(), even when |traits| don't allow file I/O.
  EXPECT_EQ(traits.may_block(), GetIOAllowed());
#endif

  // Verify that the thread the task is running on is named as expected.
  const std::string current_thread_name(PlatformThread::GetName());
  EXPECT_NE(std::string::npos, current_thread_name.find("TaskScheduler"));

  if (current_thread_name.find("SingleThread") != std::string::npos) {
    // For now, single-threaded best-effort tasks run on their own threads.
    // TODO(fdoray): Run single-threaded best-effort tasks on foreground workers
    // on platforms that don't support background thread priority.
    EXPECT_NE(
        std::string::npos,
        current_thread_name.find(traits.priority() == TaskPriority::BEST_EFFORT
                                     ? "Background"
                                     : "Foreground"));
  } else {
    EXPECT_NE(std::string::npos,
              current_thread_name.find(
                  CanUseBackgroundPriorityForSchedulerWorker() &&
                          traits.priority() == TaskPriority::BEST_EFFORT
                      ? "Background"
                      : "Foreground"));
  }
  // TaskScheduler only handles |kMergeBlockingNonBlockingPools| once started 
  // (early task runners are not merged for this experiment).
  // TODO(etiennep): Simplify this after the experiment.
  // Merging pools does not affect SingleThread workers.
  if (base::FeatureList::IsEnabled(kMergeBlockingNonBlockingPools) &&
      state == SchedulerState::kAfterSchedulerStart &&
      current_thread_name.find("SingleThread") == std::string::npos) {
    EXPECT_EQ(std::string::npos, current_thread_name.find("Blocking"));
  } else {
    EXPECT_EQ(traits.may_block(),
              current_thread_name.find("Blocking") != std::string::npos);
  }
}

void VerifyTaskEnvironmentAndSignalEvent(const TaskTraits& traits,
                                         SchedulerState state,
                                         WaitableEvent* event) {
  DCHECK(event);
  VerifyTaskEnvironment(traits, state);
  event->Signal();
}

void VerifyTimeAndTaskEnvironmentAndSignalEvent(const TaskTraits& traits,
                                                SchedulerState state,
                                                TimeTicks expected_time,
                                                WaitableEvent* event) {
  DCHECK(event);
  EXPECT_LE(expected_time, TimeTicks::Now());
  VerifyTaskEnvironment(traits, state);
  event->Signal();
}

scoped_refptr<TaskRunner> CreateTaskRunnerWithTraitsAndExecutionMode(
    TaskScheduler* scheduler,
    const TaskTraits& traits,
    test::ExecutionMode execution_mode,
    SingleThreadTaskRunnerThreadMode default_single_thread_task_runner_mode =
        SingleThreadTaskRunnerThreadMode::SHARED) {
  switch (execution_mode) {
    case test::ExecutionMode::PARALLEL:
      return scheduler->CreateTaskRunnerWithTraits(traits);
    case test::ExecutionMode::SEQUENCED:
      return scheduler->CreateSequencedTaskRunnerWithTraits(traits);
    case test::ExecutionMode::SINGLE_THREADED: {
      return scheduler->CreateSingleThreadTaskRunnerWithTraits(
          traits, default_single_thread_task_runner_mode);
    }
  }
  ADD_FAILURE() << "Unknown ExecutionMode";
  return nullptr;
}

class ThreadPostingTasks : public SimpleThread {
 public:
  // Creates a thread that posts Tasks to |scheduler| with |traits| and
  // |execution_mode|.
  ThreadPostingTasks(TaskSchedulerImpl* scheduler,
                     const TaskTraits& traits,
                     test::ExecutionMode execution_mode)
      : SimpleThread("ThreadPostingTasks"),
        traits_(traits),
        factory_(CreateTaskRunnerWithTraitsAndExecutionMode(scheduler,
                                                            traits,
                                                            execution_mode),
                 execution_mode) {}

  void WaitForAllTasksToRun() { factory_.WaitForAllTasksToRun(); }

 private:
  void Run() override {
    EXPECT_FALSE(factory_.task_runner()->RunsTasksInCurrentSequence());

    const size_t kNumTasksPerThread = 150;
    for (size_t i = 0; i < kNumTasksPerThread; ++i) {
      factory_.PostTask(
          test::TestTaskFactory::PostNestedTask::NO,
          Bind(&VerifyTaskEnvironment, traits_,
               SchedulerState::kAfterSchedulerStart));
    }
  }

  const TaskTraits traits_;
  test::TestTaskFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPostingTasks);
};

// Returns a vector with a TaskSchedulerImplTestParams for each valid
// combination of {ExecutionMode, TaskPriority, MayBlock()}.
std::vector<TaskSchedulerImplTestParams> GetTaskSchedulerImplTestParams() {
  std::vector<TaskSchedulerImplTestParams> params;

  const test::ExecutionMode execution_modes[] = {
      test::ExecutionMode::PARALLEL, test::ExecutionMode::SEQUENCED,
      test::ExecutionMode::SINGLE_THREADED};

  for (test::ExecutionMode execution_mode : execution_modes) {
    for (size_t priority_index = static_cast<size_t>(TaskPriority::LOWEST);
         priority_index <= static_cast<size_t>(TaskPriority::HIGHEST);
         ++priority_index) {
      const TaskPriority priority = static_cast<TaskPriority>(priority_index);
      params.push_back(TaskSchedulerImplTestParams(
          {priority}, execution_mode, PoolConfiguration::kDefault));
      params.push_back(TaskSchedulerImplTestParams(
          {MayBlock()}, execution_mode, PoolConfiguration::kDefault));

      params.push_back(TaskSchedulerImplTestParams(
          {priority}, execution_mode,
          PoolConfiguration::kMergeBlockingNonBlocking));
      params.push_back(TaskSchedulerImplTestParams(
          {MayBlock()}, execution_mode,
          PoolConfiguration::kMergeBlockingNonBlocking));
    }
  }

  return params;
}

class TaskSchedulerImplTest
    : public testing::TestWithParam<TaskSchedulerImplTestParams> {
 protected:
  TaskSchedulerImplTest() : scheduler_("Test"), field_trial_list_(nullptr) {
    if (GetParam().pool_config == PoolConfiguration::kMergeBlockingNonBlocking)
      feature_list.InitWithFeatures({kMergeBlockingNonBlockingPools}, {});
  }

  void EnableAllTasksUserBlocking() {
    constexpr char kFieldTrialName[] = "BrowserScheduler";
    constexpr char kFieldTrialTestGroup[] = "DummyGroup";
    std::map<std::string, std::string> variation_params;
    variation_params["AllTasksUserBlocking"] = "true";
    base::AssociateFieldTrialParams(kFieldTrialName, kFieldTrialTestGroup,
                                    variation_params);
    base::FieldTrialList::CreateFieldTrial(kFieldTrialName,
                                           kFieldTrialTestGroup);
  }

  void set_scheduler_worker_observer(
      SchedulerWorkerObserver* scheduler_worker_observer) {
    scheduler_worker_observer_ = scheduler_worker_observer;
  }

  void StartTaskScheduler() {
    constexpr TimeDelta kSuggestedReclaimTime = TimeDelta::FromSeconds(30);
    constexpr int kMaxNumBackgroundThreads = 1;
    constexpr int kMaxNumBackgroundBlockingThreads = 3;
    constexpr int kMaxNumForegroundThreads = 4;
    constexpr int kMaxNumForegroundBlockingThreads = 12;

    scheduler_.Start(
        {{kMaxNumBackgroundThreads, kSuggestedReclaimTime},
         {kMaxNumBackgroundBlockingThreads, kSuggestedReclaimTime},
         {kMaxNumForegroundThreads, kSuggestedReclaimTime},
         {kMaxNumForegroundBlockingThreads, kSuggestedReclaimTime}},
        scheduler_worker_observer_);
  }

  void TearDown() override {
    if (did_tear_down_)
      return;

    scheduler_.FlushForTesting();
    scheduler_.JoinForTesting();
    did_tear_down_ = true;
  }

  TaskSchedulerImpl scheduler_;

 private:
  base::FieldTrialList field_trial_list_;
  base::test::ScopedFeatureList feature_list;
  SchedulerWorkerObserver* scheduler_worker_observer_ = nullptr;
  bool did_tear_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerImplTest);
};

}  // namespace

// Verifies that a Task posted via PostDelayedTaskWithTraits with parameterized
// TaskTraits and no delay runs on a thread with the expected priority and I/O
// restrictions. The ExecutionMode parameter is ignored by this test.
TEST_P(TaskSchedulerImplTest, PostDelayedTaskWithTraitsNoDelay) {
  StartTaskScheduler();
  WaitableEvent task_ran;
  scheduler_.PostDelayedTaskWithTraits(
      FROM_HERE, GetParam().traits,
      BindOnce(&VerifyTaskEnvironmentAndSignalEvent, GetParam().traits,
               SchedulerState::kAfterSchedulerStart, Unretained(&task_ran)),
      TimeDelta());
  task_ran.Wait();
}

// Verifies that a Task posted via PostDelayedTaskWithTraits with parameterized
// TaskTraits and a non-zero delay runs on a thread with the expected priority
// and I/O restrictions after the delay expires. The ExecutionMode parameter is
// ignored by this test.
TEST_P(TaskSchedulerImplTest, PostDelayedTaskWithTraitsWithDelay) {
  StartTaskScheduler();
  WaitableEvent task_ran;
  scheduler_.PostDelayedTaskWithTraits(
      FROM_HERE, GetParam().traits,
      BindOnce(&VerifyTimeAndTaskEnvironmentAndSignalEvent, GetParam().traits,
               SchedulerState::kAfterSchedulerStart,
               TimeTicks::Now() + TestTimeouts::tiny_timeout(),
               Unretained(&task_ran)),
      TestTimeouts::tiny_timeout());
  task_ran.Wait();
}

// Verifies that Tasks posted via a TaskRunner with parameterized TaskTraits and
// ExecutionMode run on a thread with the expected priority and I/O restrictions
// and respect the characteristics of their ExecutionMode.
TEST_P(TaskSchedulerImplTest, PostTasksViaTaskRunner) {
  StartTaskScheduler();
  test::TestTaskFactory factory(
      CreateTaskRunnerWithTraitsAndExecutionMode(&scheduler_, GetParam().traits,
                                                 GetParam().execution_mode),
      GetParam().execution_mode);
  EXPECT_FALSE(factory.task_runner()->RunsTasksInCurrentSequence());

  const size_t kNumTasksPerTest = 150;
  for (size_t i = 0; i < kNumTasksPerTest; ++i) {
    factory.PostTask(
        test::TestTaskFactory::PostNestedTask::NO,
        Bind(&VerifyTaskEnvironment, GetParam().traits,
             SchedulerState::kAfterSchedulerStart));
  }

  factory.WaitForAllTasksToRun();
}

// Verifies that a task posted via PostDelayedTaskWithTraits without a delay
// doesn't run before Start() is called.
TEST_P(TaskSchedulerImplTest, PostDelayedTaskWithTraitsNoDelayBeforeStart) {
  WaitableEvent task_running;
  scheduler_.PostDelayedTaskWithTraits(
      FROM_HERE, GetParam().traits,
      BindOnce(&VerifyTaskEnvironmentAndSignalEvent, GetParam().traits,
               SchedulerState::kBeforeSchedulerStart, Unretained(&task_running)),
      TimeDelta());

  // Wait a little bit to make sure that the task doesn't run before Start().
  // Note: This test won't catch a case where the task runs just after the check
  // and before Start(). However, we expect the test to be flaky if the tested
  // code allows that to happen.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(task_running.IsSignaled());

  StartTaskScheduler();
  task_running.Wait();
}

// Verifies that a task posted via PostDelayedTaskWithTraits with a delay
// doesn't run before Start() is called.
TEST_P(TaskSchedulerImplTest, PostDelayedTaskWithTraitsWithDelayBeforeStart) {
  WaitableEvent task_running;
  scheduler_.PostDelayedTaskWithTraits(
      FROM_HERE, GetParam().traits,
      BindOnce(&VerifyTimeAndTaskEnvironmentAndSignalEvent, GetParam().traits,
               SchedulerState::kAfterSchedulerStart,
               TimeTicks::Now() + TestTimeouts::tiny_timeout(),
               Unretained(&task_running)),
      TestTimeouts::tiny_timeout());

  // Wait a little bit to make sure that the task doesn't run before Start().
  // Note: This test won't catch a case where the task runs just after the check
  // and before Start(). However, we expect the test to be flaky if the tested
  // code allows that to happen.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(task_running.IsSignaled());

  StartTaskScheduler();
  task_running.Wait();
}

// Verifies that a task posted via a TaskRunner doesn't run before Start() is
// called.
TEST_P(TaskSchedulerImplTest, PostTaskViaTaskRunnerBeforeStart) {
  WaitableEvent task_running;
  CreateTaskRunnerWithTraitsAndExecutionMode(&scheduler_, GetParam().traits,
                                             GetParam().execution_mode)
      ->PostTask(FROM_HERE, BindOnce(&VerifyTaskEnvironmentAndSignalEvent,
                                     GetParam().traits,
                                     SchedulerState::kBeforeSchedulerStart,
                                     Unretained(&task_running)));

  // Wait a little bit to make sure that the task doesn't run before Start().
  // Note: This test won't catch a case where the task runs just after the check
  // and before Start(). However, we expect the test to be flaky if the tested
  // code allows that to happen.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(task_running.IsSignaled());

  StartTaskScheduler();

  // This should not hang if the task runs after Start().
  task_running.Wait();
}

// Verify that all tasks posted to a TaskRunner after Start() run in a
// USER_BLOCKING environment when the AllTasksUserBlocking variation param of
// the BrowserScheduler experiment is true.
TEST_P(TaskSchedulerImplTest, AllTasksAreUserBlockingTaskRunner) {
  EnableAllTasksUserBlocking();
  StartTaskScheduler();

  WaitableEvent task_running;
  CreateTaskRunnerWithTraitsAndExecutionMode(&scheduler_, GetParam().traits,
                                             GetParam().execution_mode)
      ->PostTask(FROM_HERE,
                 BindOnce(&VerifyTaskEnvironmentAndSignalEvent,
                          TaskTraits::Override(GetParam().traits,
                                               {TaskPriority::USER_BLOCKING}),
                          SchedulerState::kAfterSchedulerStart,
                          Unretained(&task_running)));
  task_running.Wait();
}

// Verify that all tasks posted via PostDelayedTaskWithTraits() after Start()
// run in a USER_BLOCKING environment when the AllTasksUserBlocking variation
// param of the BrowserScheduler experiment is true.
TEST_P(TaskSchedulerImplTest, AllTasksAreUserBlocking) {
  EnableAllTasksUserBlocking();
  StartTaskScheduler();

  WaitableEvent task_running;
  // Ignore |params.execution_mode| in this test.
  scheduler_.PostDelayedTaskWithTraits(
      FROM_HERE, GetParam().traits,
      BindOnce(&VerifyTaskEnvironmentAndSignalEvent,
               TaskTraits::Override(GetParam().traits,
                                    {TaskPriority::USER_BLOCKING}),
               SchedulerState::kAfterSchedulerStart, Unretained(&task_running)),
      TimeDelta());
  task_running.Wait();
}

// Verifies that FlushAsyncForTesting() calls back correctly for all trait and
// execution mode pairs.
TEST_P(TaskSchedulerImplTest, FlushAsyncForTestingSimple) {
  StartTaskScheduler();

  WaitableEvent unblock_task;
  CreateTaskRunnerWithTraitsAndExecutionMode(
      &scheduler_,
      TaskTraits::Override(GetParam().traits, {WithBaseSyncPrimitives()}),
      GetParam().execution_mode, SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(FROM_HERE,
                 BindOnce(&WaitableEvent::Wait, Unretained(&unblock_task)));

  WaitableEvent flush_event;
  scheduler_.FlushAsyncForTesting(
      BindOnce(&WaitableEvent::Signal, Unretained(&flush_event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(flush_event.IsSignaled());

  unblock_task.Signal();

  flush_event.Wait();
}

INSTANTIATE_TEST_CASE_P(OneTaskSchedulerImplTestParams,
                        TaskSchedulerImplTest,
                        ::testing::ValuesIn(GetTaskSchedulerImplTestParams()));

// Spawns threads that simultaneously post Tasks to TaskRunners with various
// TaskTraits and ExecutionModes. Verifies that each Task runs on a thread with
// the expected priority and I/O restrictions and respects the characteristics
// of its ExecutionMode.
TEST_P(TaskSchedulerImplTest, MultipleTaskSchedulerImplTestParams) {
  StartTaskScheduler();
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (const auto& test_params : GetTaskSchedulerImplTestParams()) {
    threads_posting_tasks.push_back(std::make_unique<ThreadPostingTasks>(
        &scheduler_, test_params.traits,
        test_params.execution_mode));
    threads_posting_tasks.back()->Start();
  }

  for (const auto& thread : threads_posting_tasks) {
    thread->WaitForAllTasksToRun();
    thread->Join();
  }
}

TEST_P(TaskSchedulerImplTest,
       GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated) {
  StartTaskScheduler();

  // GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated() does not support
  // TaskPriority::BEST_EFFORT.
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_DCHECK_DEATH({
    scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
        {TaskPriority::BEST_EFFORT});
  });
  EXPECT_DCHECK_DEATH({
    scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
        {MayBlock(), TaskPriority::BEST_EFFORT});
  });

  if (GetParam().pool_config == PoolConfiguration::kMergeBlockingNonBlocking) {
    EXPECT_EQ(4, scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                     {TaskPriority::USER_VISIBLE}));
    EXPECT_EQ(4, scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                     {MayBlock(), TaskPriority::USER_VISIBLE}));
    EXPECT_EQ(4, scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                     {TaskPriority::USER_BLOCKING}));
    EXPECT_EQ(4, scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                     {MayBlock(), TaskPriority::USER_BLOCKING}));
  } else {
    EXPECT_EQ(4, scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                     {TaskPriority::USER_VISIBLE}));
    EXPECT_EQ(12,
              scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                  {MayBlock(), TaskPriority::USER_VISIBLE}));
    EXPECT_EQ(4, scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                     {TaskPriority::USER_BLOCKING}));
    EXPECT_EQ(12,
              scheduler_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                  {MayBlock(), TaskPriority::USER_BLOCKING}));
  }
}

// Verify that the RunsTasksInCurrentSequence() method of a SequencedTaskRunner
// returns false when called from a task that isn't part of the sequence.
TEST_P(TaskSchedulerImplTest, SequencedRunsTasksInCurrentSequence) {
  StartTaskScheduler();
  auto single_thread_task_runner =
      scheduler_.CreateSingleThreadTaskRunnerWithTraits(
          TaskTraits(), SingleThreadTaskRunnerThreadMode::SHARED);
  auto sequenced_task_runner =
      scheduler_.CreateSequencedTaskRunnerWithTraits(TaskTraits());

  WaitableEvent task_ran;
  single_thread_task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<TaskRunner> sequenced_task_runner,
             WaitableEvent* task_ran) {
            EXPECT_FALSE(sequenced_task_runner->RunsTasksInCurrentSequence());
            task_ran->Signal();
          },
          sequenced_task_runner, Unretained(&task_ran)));
  task_ran.Wait();
}

// Verify that the RunsTasksInCurrentSequence() method of a
// SingleThreadTaskRunner returns false when called from a task that isn't part
// of the sequence.
TEST_P(TaskSchedulerImplTest, SingleThreadRunsTasksInCurrentSequence) {
  StartTaskScheduler();
  auto sequenced_task_runner =
      scheduler_.CreateSequencedTaskRunnerWithTraits(TaskTraits());
  auto single_thread_task_runner =
      scheduler_.CreateSingleThreadTaskRunnerWithTraits(
          TaskTraits(), SingleThreadTaskRunnerThreadMode::SHARED);

  WaitableEvent task_ran;
  sequenced_task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<TaskRunner> single_thread_task_runner,
             WaitableEvent* task_ran) {
            EXPECT_FALSE(
                single_thread_task_runner->RunsTasksInCurrentSequence());
            task_ran->Signal();
          },
          single_thread_task_runner, Unretained(&task_ran)));
  task_ran.Wait();
}

#if defined(OS_WIN)
TEST_P(TaskSchedulerImplTest, COMSTATaskRunnersRunWithCOMSTA) {
  StartTaskScheduler();
  auto com_sta_task_runner = scheduler_.CreateCOMSTATaskRunnerWithTraits(
      TaskTraits(), SingleThreadTaskRunnerThreadMode::SHARED);

  WaitableEvent task_ran;
  com_sta_task_runner->PostTask(
      FROM_HERE, Bind(
                     [](WaitableEvent* task_ran) {
                       win::AssertComApartmentType(win::ComApartmentType::STA);
                       task_ran->Signal();
                     },
                     Unretained(&task_ran)));
  task_ran.Wait();
}
#endif  // defined(OS_WIN)

TEST_P(TaskSchedulerImplTest, DelayedTasksNotRunAfterShutdown) {
  StartTaskScheduler();
  // As with delayed tasks in general, this is racy. If the task does happen to
  // run after Shutdown within the timeout, it will fail this test.
  //
  // The timeout should be set sufficiently long enough to ensure that the
  // delayed task did not run. 2x is generally good enough.
  //
  // A non-racy way to do this would be to post two sequenced tasks:
  // 1) Regular Post Task: A WaitableEvent.Wait
  // 2) Delayed Task: ADD_FAILURE()
  // and signalling the WaitableEvent after Shutdown() on a different thread
  // since Shutdown() will block. However, the cost of managing this extra
  // thread was deemed to be too great for the unlikely race.
  scheduler_.PostDelayedTaskWithTraits(FROM_HERE, TaskTraits(),
                                       BindOnce([]() { ADD_FAILURE(); }),
                                       TestTimeouts::tiny_timeout());
  scheduler_.Shutdown();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 2);
}

#if defined(OS_POSIX)

TEST_P(TaskSchedulerImplTest, FileDescriptorWatcherNoOpsAfterShutdown) {
  StartTaskScheduler();

  int pipes[2];
  ASSERT_EQ(0, pipe(pipes));

  scoped_refptr<TaskRunner> blocking_task_runner =
      scheduler_.CreateSequencedTaskRunnerWithTraits(
          {TaskShutdownBehavior::BLOCK_SHUTDOWN});
  blocking_task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](int read_fd) {
            std::unique_ptr<FileDescriptorWatcher::Controller> controller =
                FileDescriptorWatcher::WatchReadable(
                    read_fd, BindRepeating([]() { NOTREACHED(); }));

            // This test is for components that intentionally leak their
            // watchers at shutdown. We can't clean |controller| up because its
            // destructor will assert that it's being called from the correct
            // sequence. After the task scheduler is shutdown, it is not
            // possible to run tasks on this sequence.
            //
            // Note: Do not inline the controller.release() call into the
            //       ANNOTATE_LEAKING_OBJECT_PTR as the annotation is removed
            //       by the preprocessor in non-LEAK_SANITIZER builds,
            //       effectively breaking this test.
            ANNOTATE_LEAKING_OBJECT_PTR(controller.get());
            controller.release();
          },
          pipes[0]));

  scheduler_.Shutdown();

  constexpr char kByte = '!';
  ASSERT_TRUE(WriteFileDescriptor(pipes[1], &kByte, sizeof(kByte)));

  // Give a chance for the file watcher to fire before closing the handles.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  EXPECT_EQ(0, IGNORE_EINTR(close(pipes[0])));
  EXPECT_EQ(0, IGNORE_EINTR(close(pipes[1])));
}
#endif  // defined(OS_POSIX)

// Verify that tasks posted on the same sequence access the same values on
// SequenceLocalStorage, and tasks on different sequences see different values.
TEST_P(TaskSchedulerImplTest, SequenceLocalStorage) {
  StartTaskScheduler();

  SequenceLocalStorageSlot<int> slot;
  auto sequenced_task_runner1 =
      scheduler_.CreateSequencedTaskRunnerWithTraits(TaskTraits());
  auto sequenced_task_runner2 =
      scheduler_.CreateSequencedTaskRunnerWithTraits(TaskTraits());

  sequenced_task_runner1->PostTask(
      FROM_HERE,
      BindOnce([](SequenceLocalStorageSlot<int>* slot) { slot->Set(11); },
               &slot));

  sequenced_task_runner1->PostTask(FROM_HERE,
                                   BindOnce(
                                       [](SequenceLocalStorageSlot<int>* slot) {
                                         EXPECT_EQ(slot->Get(), 11);
                                       },
                                       &slot));

  sequenced_task_runner2->PostTask(FROM_HERE,
                                   BindOnce(
                                       [](SequenceLocalStorageSlot<int>* slot) {
                                         EXPECT_NE(slot->Get(), 11);
                                       },
                                       &slot));

  scheduler_.FlushForTesting();
}

TEST_P(TaskSchedulerImplTest, FlushAsyncNoTasks) {
  StartTaskScheduler();
  bool called_back = false;
  scheduler_.FlushAsyncForTesting(
      BindOnce([](bool* called_back) { *called_back = true; },
               Unretained(&called_back)));
  EXPECT_TRUE(called_back);
}

namespace {

// Verifies that |query| is found on the current stack. Ignores failures if this
// configuration doesn't have symbols.
void VerifyHasStringOnStack(const std::string& query) {
  const std::string stack = debug::StackTrace().ToString();
  SCOPED_TRACE(stack);
  const bool found_on_stack = stack.find(query) != std::string::npos;
  const bool stack_has_symbols =
      stack.find("SchedulerWorker") != std::string::npos;
  EXPECT_TRUE(found_on_stack || !stack_has_symbols) << query;
}

}  // namespace

#if defined(OS_POSIX)
// Many POSIX bots flakily crash on |debug::StackTrace().ToString()|,
// https://crbug.com/840429.
#define MAYBE_IdentifiableStacks DISABLED_IdentifiableStacks
#elif defined(OS_WIN) && \
    (defined(ADDRESS_SANITIZER) || BUILDFLAG(CFI_CAST_CHECK))
// Hangs on WinASan and WinCFI (grabbing StackTrace() too slow?),
// https://crbug.com/845010#c7.
#define MAYBE_IdentifiableStacks DISABLED_IdentifiableStacks
#else
#define MAYBE_IdentifiableStacks IdentifiableStacks
#endif

// Integration test that verifies that workers have a frame on their stacks
// which easily identifies the type of worker (useful to diagnose issues from
// logs without memory dumps).
TEST_P(TaskSchedulerImplTest, MAYBE_IdentifiableStacks) {
  StartTaskScheduler();

  scheduler_.CreateSequencedTaskRunnerWithTraits({})->PostTask(
      FROM_HERE, BindOnce(&VerifyHasStringOnStack, "RunPooledWorker"));
  scheduler_.CreateSequencedTaskRunnerWithTraits({TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringOnStack,
                                     "RunBackgroundPooledWorker"));

  scheduler_
      .CreateSingleThreadTaskRunnerWithTraits(
          {}, SingleThreadTaskRunnerThreadMode::SHARED)
      ->PostTask(FROM_HERE,
                 BindOnce(&VerifyHasStringOnStack, "RunSharedWorker"));
  scheduler_
      .CreateSingleThreadTaskRunnerWithTraits(
          {TaskPriority::BEST_EFFORT}, SingleThreadTaskRunnerThreadMode::SHARED)
      ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringOnStack,
                                     "RunBackgroundSharedWorker"));

  scheduler_
      .CreateSingleThreadTaskRunnerWithTraits(
          {}, SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(FROM_HERE,
                 BindOnce(&VerifyHasStringOnStack, "RunDedicatedWorker"));
  scheduler_
      .CreateSingleThreadTaskRunnerWithTraits(
          {TaskPriority::BEST_EFFORT},
          SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringOnStack,
                                     "RunBackgroundDedicatedWorker"));

#if defined(OS_WIN)
  scheduler_
      .CreateCOMSTATaskRunnerWithTraits(
          {}, SingleThreadTaskRunnerThreadMode::SHARED)
      ->PostTask(FROM_HERE,
                 BindOnce(&VerifyHasStringOnStack, "RunSharedCOMWorker"));
  scheduler_
      .CreateCOMSTATaskRunnerWithTraits(
          {TaskPriority::BEST_EFFORT}, SingleThreadTaskRunnerThreadMode::SHARED)
      ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringOnStack,
                                     "RunBackgroundSharedCOMWorker"));

  scheduler_
      .CreateCOMSTATaskRunnerWithTraits(
          {}, SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(FROM_HERE,
                 BindOnce(&VerifyHasStringOnStack, "RunDedicatedCOMWorker"));
  scheduler_
      .CreateCOMSTATaskRunnerWithTraits(
          {TaskPriority::BEST_EFFORT},
          SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringOnStack,
                                     "RunBackgroundDedicatedCOMWorker"));
#endif  // defined(OS_WIN)

  scheduler_.FlushForTesting();
}

TEST_P(TaskSchedulerImplTest, SchedulerWorkerObserver) {
  testing::StrictMock<test::MockSchedulerWorkerObserver> observer;
  set_scheduler_worker_observer(&observer);

  // A worker should be created for each pool. After that, 8 threads should be
  // created for single-threaded work (16 on Windows).
  const int kExpectedNumPoolWorkers =
      CanUseBackgroundPriorityForSchedulerWorker() ? 4 : 2;
#if defined(OS_WIN)
  const int kExpectedNumSingleThreadedWorkers = 16;
#else
  const int kExpectedNumSingleThreadedWorkers = 8;
#endif
  const int kExpectedNumWorkers =
      kExpectedNumPoolWorkers + kExpectedNumSingleThreadedWorkers;

  EXPECT_CALL(observer, OnSchedulerWorkerMainEntry())
      .Times(kExpectedNumWorkers);

  StartTaskScheduler();

  std::vector<scoped_refptr<SingleThreadTaskRunner>> task_runners;

  task_runners.push_back(scheduler_.CreateSingleThreadTaskRunnerWithTraits(
      {TaskPriority::BEST_EFFORT}, SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(scheduler_.CreateSingleThreadTaskRunnerWithTraits(
      {TaskPriority::BEST_EFFORT, MayBlock()},
      SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(scheduler_.CreateSingleThreadTaskRunnerWithTraits(
      {TaskPriority::USER_BLOCKING}, SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(scheduler_.CreateSingleThreadTaskRunnerWithTraits(
      {TaskPriority::USER_BLOCKING, MayBlock()},
      SingleThreadTaskRunnerThreadMode::SHARED));

  task_runners.push_back(scheduler_.CreateSingleThreadTaskRunnerWithTraits(
      {TaskPriority::BEST_EFFORT},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(scheduler_.CreateSingleThreadTaskRunnerWithTraits(
      {TaskPriority::BEST_EFFORT, MayBlock()},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(scheduler_.CreateSingleThreadTaskRunnerWithTraits(
      {TaskPriority::USER_BLOCKING},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(scheduler_.CreateSingleThreadTaskRunnerWithTraits(
      {TaskPriority::USER_BLOCKING, MayBlock()},
      SingleThreadTaskRunnerThreadMode::DEDICATED));

#if defined(OS_WIN)
  task_runners.push_back(scheduler_.CreateCOMSTATaskRunnerWithTraits(
      {TaskPriority::BEST_EFFORT}, SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(scheduler_.CreateCOMSTATaskRunnerWithTraits(
      {TaskPriority::BEST_EFFORT, MayBlock()},
      SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(scheduler_.CreateCOMSTATaskRunnerWithTraits(
      {TaskPriority::USER_BLOCKING}, SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(scheduler_.CreateCOMSTATaskRunnerWithTraits(
      {TaskPriority::USER_BLOCKING, MayBlock()},
      SingleThreadTaskRunnerThreadMode::SHARED));

  task_runners.push_back(scheduler_.CreateCOMSTATaskRunnerWithTraits(
      {TaskPriority::BEST_EFFORT},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(scheduler_.CreateCOMSTATaskRunnerWithTraits(
      {TaskPriority::BEST_EFFORT, MayBlock()},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(scheduler_.CreateCOMSTATaskRunnerWithTraits(
      {TaskPriority::USER_BLOCKING},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(scheduler_.CreateCOMSTATaskRunnerWithTraits(
      {TaskPriority::USER_BLOCKING, MayBlock()},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
#endif

  for (auto& task_runner : task_runners)
    task_runner->PostTask(FROM_HERE, DoNothing());

  EXPECT_CALL(observer, OnSchedulerWorkerMainExit()).Times(kExpectedNumWorkers);

  // Allow single-threaded workers to be released.
  task_runners.clear();

  TearDown();
}

}  // namespace internal
}  // namespace base
