// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/sequence_manager.h"

#include <stddef.h>
#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_default.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/post_task.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/test/mock_time_domain.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_queue.h"
#include "base/task/sequence_manager/test/test_task_time_observer.h"
#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task/task_scheduler/task_scheduler_impl.h"
#include "base/task/task_traits.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace base {
namespace sequence_manager {

// To reduce noise related to the OS timer, we use a mock time domain to
// fast forward the timers.
class PerfTestTimeDomain : public MockTimeDomain {
 public:
  PerfTestTimeDomain() : MockTimeDomain(TimeTicks::Now()) {}
  ~PerfTestTimeDomain() override = default;

  Optional<TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override {
    Optional<TimeTicks> run_time = NextScheduledRunTime();
    if (!run_time)
      return nullopt;
    SetNowTicks(*run_time);
    // Makes SequenceManager to continue immediately.
    return TimeDelta();
  }

  void SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time) override {
    // De-dupe DoWorks.
    if (NumberOfScheduledWakeUps() == 1u)
      RequestDoWork();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfTestTimeDomain);
};

enum class PerfTestType : int {
  kUseSequenceManagerWithMessageLoop = 0,
  kUseSequenceManagerWithMessagePump = 1,
  kUseMessageLoop = 2,
  kUseSingleThreadInWorkerPool = 3,
};

class SequenceManagerPerfTest : public testing::TestWithParam<PerfTestType> {
 public:
  SequenceManagerPerfTest()
      : num_queues_(0),
        max_tasks_in_flight_(0),
        num_tasks_in_flight_(0),
        num_tasks_to_post_(0),
        num_tasks_to_run_(0),
        done_cond_(&done_lock_) {}

  void SetUp() override {
    if (ThreadTicks::IsSupported())
      ThreadTicks::WaitUntilInitialized();

    delayed_task_closure_ = BindRepeating(
        &SequenceManagerPerfTest::TestDelayedTask, Unretained(this));

    immediate_task_closure_ = BindRepeating(
        &SequenceManagerPerfTest::TestImmediateTask, Unretained(this));

    switch (GetParam()) {
      case PerfTestType::kUseSequenceManagerWithMessageLoop:
        CreateSequenceManagerWithMessageLoop();
        break;
      case PerfTestType::kUseSequenceManagerWithMessagePump:
        CreateSequenceManagerWithMessagePump();
        break;
      case PerfTestType::kUseMessageLoop:
        CreateMessageLoop();
        break;
      case PerfTestType::kUseSingleThreadInWorkerPool:
        CreateTaskScheduler();
        break;
    }

    if (manager_) {
      time_domain_ = std::make_unique<PerfTestTimeDomain>();
      manager_->RegisterTimeDomain(time_domain_.get());
    }
  }

  void CreateSequenceManagerWithMessageLoop() {
    message_loop_ = std::make_unique<MessageLoop>();
    manager_ = SequenceManagerForTest::Create(message_loop_.get(),
                                              message_loop_->task_runner(),
                                              DefaultTickClock::GetInstance());
  }

  void CreateSequenceManagerWithMessagePump() {
    manager_ = SequenceManagerForTest::Create(
        std::make_unique<internal::ThreadControllerWithMessagePumpImpl>(
            std::make_unique<MessagePumpDefault>(),
            DefaultTickClock::GetInstance()));
    // ThreadControllerWithMessagePumpImpl doesn't provide a default task
    // runner.
    scoped_refptr<TaskQueue> default_task_queue =
        manager_->CreateTaskQueue<TestTaskQueue>(TaskQueue::Spec("default"));
    manager_->SetDefaultTaskRunner(default_task_queue->task_runner());
  }

  void CreateMessageLoop() { message_loop_ = std::make_unique<MessageLoop>(); }

  void CreateTaskScheduler() {
    TaskScheduler::SetInstance(
        std::make_unique<::base::internal::TaskSchedulerImpl>("Test"));
    TaskScheduler::GetInstance()->StartWithDefaultParams();
  }

  void TearDown() override {
    task_runners_.clear();
    if (manager_) {
      manager_->UnregisterTimeDomain(time_domain_.get());
      manager_.reset();
    }
    if (GetParam() == PerfTestType::kUseSingleThreadInWorkerPool) {
      TaskScheduler::GetInstance()->JoinForTesting();
      TaskScheduler::SetInstance(nullptr);
    }
  }

  scoped_refptr<TaskRunner> CreateTaskRunner() {
    switch (GetParam()) {
      case PerfTestType::kUseSequenceManagerWithMessageLoop:
      case PerfTestType::kUseSequenceManagerWithMessagePump: {
        scoped_refptr<TestTaskQueue> task_queue =
            manager_->CreateTaskQueue<TestTaskQueue>(
                TaskQueue::Spec("test").SetTimeDomain(time_domain_.get()));
        owning_task_queues_.push_back(task_queue);
        return task_queue->task_runner();
      }

      case PerfTestType::kUseMessageLoop:
        return message_loop_->task_runner();

      case PerfTestType::kUseSingleThreadInWorkerPool:
        return CreateSingleThreadTaskRunnerWithTraits(
            {TaskPriority::USER_BLOCKING});
    };
  }

  void Initialize(size_t num_queues) {
    owning_task_queues_.clear();
    num_queues_ = num_queues;
    for (size_t i = 0; i < num_queues; i++) {
      task_runners_.push_back(CreateTaskRunner());
    }
  }

  void WaitUntilDone() {
    switch (GetParam()) {
      case PerfTestType::kUseSequenceManagerWithMessageLoop:
      case PerfTestType::kUseSequenceManagerWithMessagePump:
      case PerfTestType::kUseMessageLoop:
        run_loop_.reset(new RunLoop());
        run_loop_->Run();
        break;
      case PerfTestType::kUseSingleThreadInWorkerPool: {
        AutoLock auto_lock(done_lock_);
        done_cond_.Wait();
        break;
      }
    }
  }

  void SignalDone() {
    switch (GetParam()) {
      case PerfTestType::kUseSequenceManagerWithMessageLoop:
      case PerfTestType::kUseSequenceManagerWithMessagePump:
      case PerfTestType::kUseMessageLoop:
        run_loop_->Quit();
        break;
      case PerfTestType::kUseSingleThreadInWorkerPool: {
        AutoLock auto_lock(done_lock_);
        done_cond_.Signal();
        break;
      }
    }
  }

  void TestDelayedTask() {
    if (--num_tasks_to_run_ == 0) {
      SignalDone();
      return;
    }

    num_tasks_in_flight_--;
    // NOTE there are only up to max_tasks_in_flight_ pending delayed tasks at
    // any one time.  Thanks to the lower_num_tasks_to_post going to zero if
    // there are a lot of tasks in flight, the total number of task in flight at
    // any one time is very variable.
    unsigned int lower_num_tasks_to_post =
        num_tasks_in_flight_ < (max_tasks_in_flight_ / 2) ? 1 : 0;
    unsigned int max_tasks_to_post =
        num_tasks_to_post_ % 2 ? lower_num_tasks_to_post : 10;
    for (unsigned int i = 0;
         i < max_tasks_to_post && num_tasks_in_flight_ < max_tasks_in_flight_ &&
         num_tasks_to_post_ > 0;
         i++) {
      // Choose a queue weighted towards queue 0.
      unsigned int queue = num_tasks_to_post_ % (num_queues_ + 1);
      if (queue == num_queues_) {
        queue = 0;
      }
      // Simulate a mix of short and longer delays.
      unsigned int delay =
          num_tasks_to_post_ % 2 ? 1 : (10 + num_tasks_to_post_ % 10);
      task_runners_[queue]->PostDelayedTask(FROM_HERE, delayed_task_closure_,
                                            TimeDelta::FromMilliseconds(delay));
      num_tasks_in_flight_++;
      num_tasks_to_post_--;
    }
  }

  void TestImmediateTask() {
    if (--num_tasks_to_run_ == 0) {
      SignalDone();
      return;
    }

    num_tasks_in_flight_--;
    // NOTE there are only up to max_tasks_in_flight_ pending delayed tasks at
    // any one time.  Thanks to the lower_num_tasks_to_post going to zero if
    // there are a lot of tasks in flight, the total number of task in flight at
    // any one time is very variable.
    unsigned int lower_num_tasks_to_post =
        num_tasks_in_flight_ < (max_tasks_in_flight_ / 2) ? 1 : 0;
    unsigned int max_tasks_to_post =
        num_tasks_to_post_ % 2 ? lower_num_tasks_to_post : 10;
    for (unsigned int i = 0;
         i < max_tasks_to_post && num_tasks_in_flight_ < max_tasks_in_flight_ &&
         num_tasks_to_post_ > 0;
         i++) {
      // Choose a queue weighted towards queue 0.
      unsigned int queue = num_tasks_to_post_ % (num_queues_ + 1);
      if (queue == num_queues_) {
        queue = 0;
      }
      task_runners_[queue]->PostTask(FROM_HERE, immediate_task_closure_);
      num_tasks_in_flight_++;
      num_tasks_to_post_--;
    }
  }

  void ResetAndCallTestDelayedTask(unsigned int num_tasks_to_run) {
    num_tasks_in_flight_ = 1;
    num_tasks_to_post_ = num_tasks_to_run;
    num_tasks_to_run_ = num_tasks_to_run;
    TestDelayedTask();
  }

  void ResetAndCallTestImmediateTask(unsigned int num_tasks_to_run) {
    num_tasks_in_flight_ = 1;
    num_tasks_to_post_ = num_tasks_to_run;
    num_tasks_to_run_ = num_tasks_to_run;
    TestImmediateTask();
  }

  void Benchmark(const std::string& trace, const RepeatingClosure& test_task) {
    TimeTicks start = TimeTicks::Now();
    TimeTicks now;
    unsigned long long num_iterations = 0;
    do {
      test_task.Run();
      WaitUntilDone();
      now = TimeTicks::Now();
      num_iterations++;
    } while (now - start < TimeDelta::FromSeconds(5));

    std::string trace_suffix;
    switch (GetParam()) {
      case PerfTestType::kUseSequenceManagerWithMessageLoop:
        trace_suffix = " SequenceManager with message loop";
        break;
      case PerfTestType::kUseSequenceManagerWithMessagePump:
        trace_suffix = " SequenceManager with message pump";
        break;
      case PerfTestType::kUseMessageLoop:
        trace_suffix = " message loop";
        break;
      case PerfTestType::kUseSingleThreadInWorkerPool:
        trace_suffix = " single thread in WorkerPool";
        break;
    }

    perf_test::PrintResult(
        "task", "", trace + trace_suffix,
        (now - start).InMicroseconds() / static_cast<double>(num_iterations),
        "us/run", true);
  }

  size_t num_queues_;
  unsigned int max_tasks_in_flight_;
  unsigned int num_tasks_in_flight_;
  unsigned int num_tasks_to_post_;
  unsigned int num_tasks_to_run_;
  std::unique_ptr<MessageLoop> message_loop_;
  std::unique_ptr<SequenceManager> manager_;
  std::unique_ptr<TimeDomain> time_domain_;
  std::vector<scoped_refptr<TaskRunner>> task_runners_;

  Lock done_lock_;
  ConditionVariable done_cond_;
  std::unique_ptr<RunLoop> run_loop_;

  // May own |task_runners_|.
  std::vector<scoped_refptr<TestTaskQueue>> owning_task_queues_;

  RepeatingClosure delayed_task_closure_;
  RepeatingClosure immediate_task_closure_;
};

INSTANTIATE_TEST_CASE_P(
    ,
    SequenceManagerPerfTest,
    testing::Values(PerfTestType::kUseSequenceManagerWithMessageLoop,
                    PerfTestType::kUseSequenceManagerWithMessagePump,
                    PerfTestType::kUseMessageLoop,
                    PerfTestType::kUseSingleThreadInWorkerPool));

TEST_P(SequenceManagerPerfTest, RunTenThousandDelayedTasks_OneQueue) {
  if (!ThreadTicks::IsSupported())
    return;

  switch (GetParam()) {
    // Virtual time is not supported for MessageLoop or WorkerPool.
    case PerfTestType::kUseMessageLoop:
    case PerfTestType::kUseSingleThreadInWorkerPool:
      LOG(INFO) << "Unsupported";
      return;

    default:
      break;
  }

  Initialize(1u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with one queue",
            BindRepeating(&SequenceManagerPerfTest::ResetAndCallTestDelayedTask,
                          Unretained(this), 10000));
}

TEST_P(SequenceManagerPerfTest, RunTenThousandDelayedTasks_FourQueues) {
  if (!ThreadTicks::IsSupported())
    return;

  switch (GetParam()) {
    // Virtual time is not supported for MessageLoop or WorkerPool.
    case PerfTestType::kUseMessageLoop:
    case PerfTestType::kUseSingleThreadInWorkerPool:
      LOG(INFO) << "Unsupported";
      return;

    default:
      break;
  }

  Initialize(4u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with four queues",
            BindRepeating(&SequenceManagerPerfTest::ResetAndCallTestDelayedTask,
                          Unretained(this), 10000));
}

TEST_P(SequenceManagerPerfTest, RunTenThousandDelayedTasks_EightQueues) {
  if (!ThreadTicks::IsSupported())
    return;

  switch (GetParam()) {
    // Virtual time is not supported for MessageLoop or WorkerPool.
    case PerfTestType::kUseMessageLoop:
    case PerfTestType::kUseSingleThreadInWorkerPool:
      LOG(INFO) << "Unsupported";
      return;

    default:
      break;
  }

  Initialize(8u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with eight queues",
            BindRepeating(&SequenceManagerPerfTest::ResetAndCallTestDelayedTask,
                          Unretained(this), 10000));
}

TEST_P(SequenceManagerPerfTest, RunTenThousandDelayedTasks_ThirtyTwoQueues) {
  if (!ThreadTicks::IsSupported())
    return;

  switch (GetParam()) {
    case PerfTestType::kUseMessageLoop:
    case PerfTestType::kUseSingleThreadInWorkerPool:
      LOG(INFO) << "Unsupported";
      return;

    default:
      break;
  }

  Initialize(32u);

  max_tasks_in_flight_ = 200;
  Benchmark("run 10000 delayed tasks with thirty two queues",
            BindRepeating(&SequenceManagerPerfTest::ResetAndCallTestDelayedTask,
                          Unretained(this), 10000));
}

TEST_P(SequenceManagerPerfTest, RunTenThousandImmediateTasks_OneQueue) {
  if (!ThreadTicks::IsSupported())
    return;
  Initialize(1u);

  max_tasks_in_flight_ = 200;
  Benchmark(
      "run 10000 immediate tasks with one queue",
      BindRepeating(&SequenceManagerPerfTest::ResetAndCallTestImmediateTask,
                    Unretained(this), 10000));
}

TEST_P(SequenceManagerPerfTest, RunTenThousandImmediateTasks_FourQueues) {
  if (!ThreadTicks::IsSupported())
    return;

  switch (GetParam()) {
    // We only support a single queue on the MessageLoop.
    case PerfTestType::kUseMessageLoop:
      LOG(INFO) << "Unsupported";
      return;

    default:
      break;
  }

  Initialize(4u);

  max_tasks_in_flight_ = 200;
  Benchmark(
      "run 10000 immediate tasks with four queues",
      BindRepeating(&SequenceManagerPerfTest::ResetAndCallTestImmediateTask,
                    Unretained(this), 10000));
}

TEST_P(SequenceManagerPerfTest, RunTenThousandImmediateTasks_EightQueues) {
  if (!ThreadTicks::IsSupported())
    return;

  switch (GetParam()) {
    // We only support a single queue on the MessageLoop.
    case PerfTestType::kUseMessageLoop:
      LOG(INFO) << "Unsupported";
      return;

    default:
      break;
  }
  Initialize(8u);

  max_tasks_in_flight_ = 200;
  Benchmark(
      "run 10000 immediate tasks with eight queues",
      BindRepeating(&SequenceManagerPerfTest::ResetAndCallTestImmediateTask,
                    Unretained(this), 10000));
}

TEST_P(SequenceManagerPerfTest, RunTenThousandImmediateTasks_ThirtyTwoQueues) {
  if (!ThreadTicks::IsSupported())
    return;

  switch (GetParam()) {
    // We only support a single queue on the MessageLoop.
    case PerfTestType::kUseMessageLoop:
      LOG(INFO) << "Unsupported";
      return;

    default:
      break;
  }
  Initialize(32u);

  max_tasks_in_flight_ = 200;
  Benchmark(
      "run 10000 immediate tasks with thirty two queues",
      BindRepeating(&SequenceManagerPerfTest::ResetAndCallTestImmediateTask,
                    Unretained(this), 10000));
}

// TODO(alexclarke): Add additional tests with different mixes of non-delayed vs
// delayed tasks.

}  // namespace sequence_manager
}  // namespace base
