// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread.h"

#include <stddef.h>

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/common/checked_lock.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/sequence.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/task/thread_pool/worker_thread_observer.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)
#include "partition_alloc/extended_api.h"
#include "partition_alloc/thread_cache.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED)

using testing::_;
using testing::Mock;
using testing::Ne;
using testing::StrictMock;

namespace base::internal {
namespace {

const size_t kNumSequencesPerTest = 150;

class WorkerThreadDefaultDelegate : public WorkerThread::Delegate {
 public:
  WorkerThreadDefaultDelegate() = default;
  WorkerThreadDefaultDelegate(const WorkerThreadDefaultDelegate&) = delete;
  WorkerThreadDefaultDelegate& operator=(const WorkerThreadDefaultDelegate&) =
      delete;

  // WorkerThread::Delegate:
  WorkerThread::ThreadLabel GetThreadLabel() const override {
    return WorkerThread::ThreadLabel::DEDICATED;
  }
  void OnMainEntry(WorkerThread* worker) override {}
  RegisteredTaskSource GetWork(WorkerThread* worker) override {
    return nullptr;
  }
  RegisteredTaskSource SwapProcessedTask(RegisteredTaskSource task_source,
                                         WorkerThread* worker) override {
    ADD_FAILURE() << "Unexpected call to SwapProcessedTask()";
    return nullptr;
  }
  TimeDelta GetSleepTimeout() override { return TimeDelta::Max(); }
};
// The test parameter is the number of Tasks per Sequence returned by GetWork().

class ThreadPoolWorkerTest : public testing::Test {
 public:
  ThreadPoolWorkerTest(const ThreadPoolWorkerTest&) = delete;
  ThreadPoolWorkerTest& operator=(const ThreadPoolWorkerTest&) = delete;

 protected:
  ThreadPoolWorkerTest() {
    Thread::Options service_thread_options;
    service_thread_options.message_pump_type = MessagePumpType::IO;
    service_thread_.StartWithOptions(std::move(service_thread_options));
  }

  Thread service_thread_ = Thread("ServiceThread");
};

// The test parameter is the number of Tasks per Sequence returned by GetWork().
class ThreadPoolWorkerTestParam : public testing::TestWithParam<int> {
 public:
  ThreadPoolWorkerTestParam(const ThreadPoolWorkerTestParam&) = delete;
  ThreadPoolWorkerTestParam& operator=(const ThreadPoolWorkerTestParam&) =
      delete;

 protected:
  ThreadPoolWorkerTestParam()
      : num_get_work_cv_(lock_.CreateConditionVariable()) {
    Thread::Options service_thread_options;
    service_thread_options.message_pump_type = MessagePumpType::IO;
    service_thread_.StartWithOptions(std::move(service_thread_options));
  }

  void SetUp() override {
    worker_ = MakeRefCounted<WorkerThread>(
        ThreadType::kDefault,
        std::make_unique<TestWorkerThreadDelegate>(this),
        task_tracker_.GetTrackedRef(), 0);
    ASSERT_TRUE(worker_);
    worker_->Start(service_thread_.task_runner());
    worker_set_.Signal();
    main_entry_called_.Wait();
  }

  void TearDown() override {
    // |worker_| needs to be released before ~TaskTracker() as it holds a
    // TrackedRef to it.
    worker_->JoinForTesting();
    worker_ = nullptr;
  }

  int TasksPerSequence() const { return GetParam(); }

  // Wait until GetWork() has been called |num_get_work| times.
  void WaitForNumGetWork(size_t num_get_work) {
    CheckedAutoLock auto_lock(lock_);
    while (num_get_work_ < num_get_work) {
      num_get_work_cv_.Wait();
    }
  }

  void SetMaxGetWork(size_t max_get_work) {
    CheckedAutoLock auto_lock(lock_);
    max_get_work_ = max_get_work;
  }

  void SetNumSequencesToCreate(size_t num_sequences_to_create) {
    CheckedAutoLock auto_lock(lock_);
    EXPECT_EQ(0U, num_sequences_to_create_);
    num_sequences_to_create_ = num_sequences_to_create;
  }

  size_t NumRunTasks() {
    CheckedAutoLock auto_lock(lock_);
    return num_run_tasks_;
  }

  std::vector<scoped_refptr<TaskSource>> CreatedTaskSources() {
    CheckedAutoLock auto_lock(lock_);
    return created_sequences_;
  }

  std::vector<scoped_refptr<TaskSource>> DidProcessTaskSequences() {
    CheckedAutoLock auto_lock(lock_);
    return did_run_task_sources_;
  }

  scoped_refptr<WorkerThread> worker_;
  Thread service_thread_ = Thread("ServiceThread");

 private:
  class TestWorkerThreadDelegate
      : public WorkerThreadDefaultDelegate {
   public:
    explicit TestWorkerThreadDelegate(ThreadPoolWorkerTestParam* outer)
        : outer_(outer) {}
    TestWorkerThreadDelegate(
        const TestWorkerThreadDelegate&) = delete;
    TestWorkerThreadDelegate& operator=(
        const TestWorkerThreadDelegate&) = delete;

    ~TestWorkerThreadDelegate() override {
      EXPECT_FALSE(IsCallToDidProcessTaskExpected());
    }

    // WorkerThread::Delegate:
    void OnMainEntry(WorkerThread* worker) override {
      outer_->worker_set_.Wait();
      EXPECT_EQ(outer_->worker_.get(), worker);
      EXPECT_FALSE(IsCallToDidProcessTaskExpected());

      // Without synchronization, OnMainEntry() could be called twice without
      // generating an error.
      CheckedAutoLock auto_lock(outer_->lock_);
      EXPECT_FALSE(outer_->main_entry_called_.IsSignaled());
      outer_->main_entry_called_.Signal();
    }

    RegisteredTaskSource GetWork(WorkerThread* worker) override {
      EXPECT_FALSE(IsCallToDidProcessTaskExpected());
      EXPECT_EQ(outer_->worker_.get(), worker);

      {
        CheckedAutoLock auto_lock(outer_->lock_);

        // Increment the number of times that this method has been called.
        ++outer_->num_get_work_;
        outer_->num_get_work_cv_.Signal();

        // Verify that this method isn't called more times than expected.
        EXPECT_LE(outer_->num_get_work_, outer_->max_get_work_);

        // Check if a Sequence should be returned.
        if (outer_->num_sequences_to_create_ == 0) {
          return nullptr;
        }
        --outer_->num_sequences_to_create_;
      }

      // Create a Sequence with TasksPerSequence() Tasks.
      scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
          TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);
      Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
      for (int i = 0; i < outer_->TasksPerSequence(); ++i) {
        Task task(FROM_HERE,
                  BindOnce(&ThreadPoolWorkerTestParam::RunTaskCallback,
                           Unretained(outer_)),
                  TimeTicks::Now(), TimeDelta());
        sequence_transaction.WillPushImmediateTask();
        EXPECT_TRUE(outer_->task_tracker_.WillPostTask(
            &task, sequence->shutdown_behavior()));
        sequence_transaction.PushImmediateTask(std::move(task));
      }
      auto registered_task_source =
          outer_->task_tracker_.RegisterTaskSource(sequence);
      EXPECT_TRUE(registered_task_source);

      ExpectCallToDidProcessTask();

      {
        // Add the Sequence to the vector of created Sequences.
        CheckedAutoLock auto_lock(outer_->lock_);
        outer_->created_sequences_.push_back(sequence);
      }
      auto run_status = registered_task_source.WillRunTask();
      EXPECT_NE(run_status, TaskSource::RunStatus::kDisallowed);
      return registered_task_source;
    }

    RegisteredTaskSource SwapProcessedTask(
        RegisteredTaskSource registered_task_source,
        WorkerThread* worker) override {
      {
        CheckedAutoLock auto_lock(expect_did_run_task_lock_);
        EXPECT_TRUE(expect_did_run_task_);
        expect_did_run_task_ = false;
      }

      // If TasksPerSequence() is 1, |registered_task_source| should be nullptr.
      // Otherwise, |registered_task_source| should contain TasksPerSequence() -
      // 1 Tasks.
      if (outer_->TasksPerSequence() == 1) {
        EXPECT_FALSE(registered_task_source);
      } else {
        EXPECT_TRUE(registered_task_source);
        EXPECT_TRUE(registered_task_source.WillReEnqueue(TimeTicks::Now()));

        // Verify the number of Tasks in |registered_task_source|.
        for (int i = 0; i < outer_->TasksPerSequence() - 1; ++i) {
          registered_task_source.WillRunTask();
          IgnoreResult(registered_task_source.TakeTask());
          if (i < outer_->TasksPerSequence() - 2) {
            EXPECT_TRUE(registered_task_source.DidProcessTask());
            EXPECT_TRUE(registered_task_source.WillReEnqueue(TimeTicks::Now()));
          } else {
            EXPECT_FALSE(registered_task_source.DidProcessTask());
          }
        }
        scoped_refptr<TaskSource> task_source =
            registered_task_source.Unregister();
        {
          // Add |task_source| to |did_run_task_sources_|.
          CheckedAutoLock auto_lock(outer_->lock_);
          outer_->did_run_task_sources_.push_back(std::move(task_source));
          EXPECT_LE(outer_->did_run_task_sources_.size(),
                    outer_->created_sequences_.size());
        }
      }
      return GetWork(worker);
    }

   private:
    // Expect a call to DidProcessTask() before the next call to any other
    // method of this delegate.
    void ExpectCallToDidProcessTask() {
      CheckedAutoLock auto_lock(expect_did_run_task_lock_);
      expect_did_run_task_ = true;
    }

    bool IsCallToDidProcessTaskExpected() const {
      CheckedAutoLock auto_lock(expect_did_run_task_lock_);
      return expect_did_run_task_;
    }

    raw_ptr<ThreadPoolWorkerTestParam> outer_;

    // Synchronizes access to |expect_did_run_task_|.
    mutable CheckedLock expect_did_run_task_lock_;

    // Whether the next method called on this delegate should be
    // DidProcessTask().
    bool expect_did_run_task_ = false;
  };

  void RunTaskCallback() {
    CheckedAutoLock auto_lock(lock_);
    ++num_run_tasks_;
    EXPECT_LE(num_run_tasks_, created_sequences_.size());
  }

  TaskTracker task_tracker_;

  // Synchronizes access to all members below.
  mutable CheckedLock lock_;

  // Signaled once OnMainEntry() has been called.
  TestWaitableEvent main_entry_called_;

  // Number of Sequences that should be created by GetWork(). When this
  // is 0, GetWork() returns nullptr.
  size_t num_sequences_to_create_ = 0;

  // Number of times that GetWork() has been called.
  size_t num_get_work_ = 0;

  // Maximum number of times that GetWork() can be called.
  size_t max_get_work_ = 0;

  // Condition variable signaled when |num_get_work_| is incremented.
  ConditionVariable num_get_work_cv_;

  // Sequences created by GetWork().
  std::vector<scoped_refptr<TaskSource>> created_sequences_;

  // Sequences passed to DidProcessTask().
  std::vector<scoped_refptr<TaskSource>> did_run_task_sources_;

  // Number of times that RunTaskCallback() has been called.
  size_t num_run_tasks_ = 0;

  // Signaled after |worker_| is set.
  TestWaitableEvent worker_set_;
};

}  // namespace

// Verify that when GetWork() continuously returns Sequences, all Tasks in these
// Sequences run successfully. The test wakes up the WorkerThread once.
TEST_P(ThreadPoolWorkerTestParam, ContinuousWork) {
  // Set GetWork() to return |kNumSequencesPerTest| Sequences before starting to
  // return nullptr.
  SetNumSequencesToCreate(kNumSequencesPerTest);

  // Expect |kNumSequencesPerTest| calls to GetWork() in which it returns a
  // Sequence and one call in which its returns nullptr.
  const size_t kExpectedNumGetWork = kNumSequencesPerTest + 1;
  SetMaxGetWork(kExpectedNumGetWork);

  // Wake up |worker_| and wait until GetWork() has been invoked the
  // expected amount of times.
  worker_->WakeUp();
  WaitForNumGetWork(kExpectedNumGetWork);

  // All tasks should have run.
  EXPECT_EQ(kNumSequencesPerTest, NumRunTasks());

  // If Sequences returned by GetWork() contain more than one Task, they aren't
  // empty after the worker pops Tasks from them and thus should be returned to
  // DidProcessTask().
  if (TasksPerSequence() > 1) {
    EXPECT_EQ(CreatedTaskSources(), DidProcessTaskSequences());
  } else {
    EXPECT_TRUE(DidProcessTaskSequences().empty());
  }
}

// Verify that when GetWork() alternates between returning a Sequence and
// returning nullptr, all Tasks in the returned Sequences run successfully. The
// test wakes up the WorkerThread once for each Sequence.
TEST_P(ThreadPoolWorkerTestParam, IntermittentWork) {
  for (size_t i = 0; i < kNumSequencesPerTest; ++i) {
    // Set GetWork() to return 1 Sequence before starting to return
    // nullptr.
    SetNumSequencesToCreate(1);

    // Expect |i + 1| calls to GetWork() in which it returns a Sequence and
    // |i + 1| calls in which it returns nullptr.
    const size_t expected_num_get_work = 2 * (i + 1);
    SetMaxGetWork(expected_num_get_work);

    // Wake up |worker_| and wait until GetWork() has been invoked
    // the expected amount of times.
    worker_->WakeUp();
    WaitForNumGetWork(expected_num_get_work);

    // The Task should have run
    EXPECT_EQ(i + 1, NumRunTasks());

    // If Sequences returned by GetWork() contain more than one Task, they
    // aren't empty after the worker pops Tasks from them and thus should be
    // returned to DidProcessTask().
    if (TasksPerSequence() > 1) {
      EXPECT_EQ(CreatedTaskSources(), DidProcessTaskSequences());
    } else {
      EXPECT_TRUE(DidProcessTaskSequences().empty());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(OneTaskPerSequence,
                         ThreadPoolWorkerTestParam,
                         ::testing::Values(1));
INSTANTIATE_TEST_SUITE_P(TwoTasksPerSequence,
                         ThreadPoolWorkerTestParam,
                         ::testing::Values(2));

namespace {

class ControllableCleanupDelegate : public WorkerThreadDefaultDelegate {
 public:
  class Controls : public RefCountedThreadSafe<Controls> {
   public:
    Controls() = default;
    Controls(const Controls&) = delete;
    Controls& operator=(const Controls&) = delete;

    void HaveWorkBlock() { work_running_.Reset(); }

    void UnblockWork() { work_running_.Signal(); }

    void WaitForWorkToRun() { work_processed_.Wait(); }

    void WaitForCleanupRequest() { cleanup_requested_.Wait(); }

    void WaitForDelegateDestroy() { destroyed_.Wait(); }

    void WaitForMainExit() { exited_.Wait(); }

    void set_expect_get_work(bool expect_get_work) {
      expect_get_work_ = expect_get_work;
    }

    void ResetState() {
      work_running_.Signal();
      work_processed_.Reset();
      cleanup_requested_.Reset();
      exited_.Reset();
      work_requested_ = false;
    }

    void set_can_cleanup(bool can_cleanup) { can_cleanup_ = can_cleanup; }

   private:
    friend class ControllableCleanupDelegate;
    friend class RefCountedThreadSafe<Controls>;
    ~Controls() = default;

    TestWaitableEvent work_running_{WaitableEvent::ResetPolicy::MANUAL,
                                    WaitableEvent::InitialState::SIGNALED};
    TestWaitableEvent work_processed_;
    TestWaitableEvent cleanup_requested_;
    TestWaitableEvent destroyed_;
    TestWaitableEvent exited_;

    bool expect_get_work_ = true;
    bool can_cleanup_ = false;
    bool work_requested_ = false;
  };

  explicit ControllableCleanupDelegate(TaskTracker* task_tracker)
      : task_tracker_(task_tracker), controls_(new Controls()) {}

  ControllableCleanupDelegate(const ControllableCleanupDelegate&) = delete;
  ControllableCleanupDelegate& operator=(const ControllableCleanupDelegate&) =
      delete;
  ~ControllableCleanupDelegate() override { controls_->destroyed_.Signal(); }

  RegisteredTaskSource GetWork(WorkerThread* worker) override {
    EXPECT_TRUE(controls_->expect_get_work_);

    // Sends one item of work to signal |work_processed_|. On subsequent calls,
    // sends nullptr to indicate there's no more work to be done.
    if (controls_->work_requested_) {
      if (CanCleanup(worker)) {
        OnCleanup();
        worker->Cleanup();
        controls_->set_expect_get_work(false);
      }
      return nullptr;
    }

    controls_->work_requested_ = true;
    scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
        TaskTraits(WithBaseSyncPrimitives(),
                   TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
        nullptr, TaskSourceExecutionMode::kParallel);
    Task task(FROM_HERE,
              BindOnce(
                  [](TestWaitableEvent* work_processed,
                     TestWaitableEvent* work_running) {
                    work_processed->Signal();
                    work_running->Wait();
                  },
                  Unretained(&controls_->work_processed_),
                  Unretained(&controls_->work_running_)),
              TimeTicks::Now(), TimeDelta());
    auto transaction = sequence->BeginTransaction();
    transaction.WillPushImmediateTask();
    EXPECT_TRUE(
        task_tracker_->WillPostTask(&task, sequence->shutdown_behavior()));
    transaction.PushImmediateTask(std::move(task));
    auto registered_task_source =
        task_tracker_->RegisterTaskSource(std::move(sequence));
    EXPECT_TRUE(registered_task_source);
    registered_task_source.WillRunTask();
    return registered_task_source;
  }

  RegisteredTaskSource SwapProcessedTask(RegisteredTaskSource task_source,
                                         WorkerThread* worker) override {
    return GetWork(worker);
  }

  void OnMainExit(WorkerThread* worker) override {
    controls_->exited_.Signal();
  }

  bool CanCleanup(WorkerThread* worker) {
    // Saving |can_cleanup_| now so that callers waiting on |cleanup_requested_|
    // have the thread go to sleep and then allow timing out.
    bool can_cleanup = controls_->can_cleanup_;
    controls_->cleanup_requested_.Signal();
    return can_cleanup;
  }

  void OnCleanup() {
    EXPECT_TRUE(controls_->can_cleanup_);
    EXPECT_TRUE(controls_->cleanup_requested_.IsSignaled());
  }

  // ControllableCleanupDelegate:
  scoped_refptr<Controls> controls() { return controls_; }

 private:
  scoped_refptr<Sequence> work_sequence_;
  const raw_ptr<TaskTracker> task_tracker_;
  scoped_refptr<Controls> controls_;
};

class MockedControllableCleanupDelegate : public ControllableCleanupDelegate {
 public:
  explicit MockedControllableCleanupDelegate(TaskTracker* task_tracker)
      : ControllableCleanupDelegate(task_tracker) {}
  MockedControllableCleanupDelegate(const MockedControllableCleanupDelegate&) =
      delete;
  MockedControllableCleanupDelegate& operator=(
      const MockedControllableCleanupDelegate&) = delete;
  ~MockedControllableCleanupDelegate() override = default;

  // WorkerThread::Delegate:
  MOCK_METHOD1(OnMainEntry, void(WorkerThread* worker));
};

}  // namespace

// Verify that calling WorkerThread::Cleanup() from GetWork() causes
// the WorkerThread's thread to exit.
TEST_F(ThreadPoolWorkerTest, WorkerCleanupFromGetWork) {
  TaskTracker task_tracker;
  // Will be owned by WorkerThread.
  MockedControllableCleanupDelegate* delegate =
      new StrictMock<MockedControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();
  controls->set_can_cleanup(true);
  EXPECT_CALL(*delegate, OnMainEntry(_));
  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, WrapUnique(delegate), task_tracker.GetTrackedRef(),
      0);
  worker->Start(service_thread_.task_runner());
  worker->WakeUp();
  controls->WaitForWorkToRun();
  Mock::VerifyAndClear(delegate);
  controls->WaitForMainExit();
  // Join the worker to avoid leaks.
  worker->JoinForTesting();
}

TEST_F(ThreadPoolWorkerTest, WorkerCleanupDuringWork) {
  TaskTracker task_tracker;
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the delegate
  // may destroy on a different thread. Mocks aren't designed with that in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();

  controls->HaveWorkBlock();

  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, std::move(delegate), task_tracker.GetTrackedRef(),
      0);
  worker->Start(service_thread_.task_runner());
  worker->WakeUp();

  controls->WaitForWorkToRun();
  worker->Cleanup();
  worker = nullptr;
  controls->UnblockWork();
  controls->WaitForDelegateDestroy();
}

TEST_F(ThreadPoolWorkerTest, WorkerCleanupDuringWait) {
  TaskTracker task_tracker;
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the delegate
  // may destroy on a different thread. Mocks aren't designed with that in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();

  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, std::move(delegate), task_tracker.GetTrackedRef(),
      0);
  worker->Start(service_thread_.task_runner());
  worker->WakeUp();

  controls->WaitForCleanupRequest();
  worker->Cleanup();
  worker = nullptr;
  controls->WaitForDelegateDestroy();
}

TEST_F(ThreadPoolWorkerTest, WorkerCleanupDuringShutdown) {
  TaskTracker task_tracker;
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the delegate
  // may destroy on a different thread. Mocks aren't designed with that in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();

  controls->HaveWorkBlock();

  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, std::move(delegate), task_tracker.GetTrackedRef(),
      0);
  worker->Start(service_thread_.task_runner());
  worker->WakeUp();

  controls->WaitForWorkToRun();
  test::ShutdownTaskTracker(&task_tracker);
  worker->Cleanup();
  worker = nullptr;
  controls->UnblockWork();
  controls->WaitForDelegateDestroy();
}

// Verify that Start() is a no-op after Cleanup().
TEST_F(ThreadPoolWorkerTest, CleanupBeforeStart) {
  TaskTracker task_tracker;
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the delegate
  // may destroy on a different thread. Mocks aren't designed with that in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();
  controls->set_expect_get_work(false);

  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, std::move(delegate), task_tracker.GetTrackedRef(),
      0);

  worker->Cleanup();
  worker->Start(service_thread_.task_runner());

  EXPECT_FALSE(worker->ThreadAliveForTesting());
}

namespace {

class CallJoinFromDifferentThread : public SimpleThread {
 public:
  explicit CallJoinFromDifferentThread(
      WorkerThread* worker_to_join)
      : SimpleThread("WorkerThreadJoinThread"),
        worker_to_join_(worker_to_join) {}

  CallJoinFromDifferentThread(const CallJoinFromDifferentThread&) = delete;
  CallJoinFromDifferentThread& operator=(const CallJoinFromDifferentThread&) =
      delete;
  ~CallJoinFromDifferentThread() override = default;

  void Run() override {
    run_started_event_.Signal();
    worker_to_join_.ExtractAsDangling()->JoinForTesting();
  }

  void WaitForRunToStart() { run_started_event_.Wait(); }

 private:
  raw_ptr<WorkerThread> worker_to_join_;
  TestWaitableEvent run_started_event_;
};

}  // namespace

TEST_F(ThreadPoolWorkerTest, WorkerCleanupDuringJoin) {
  TaskTracker task_tracker;
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the
  // delegate may destroy on a different thread. Mocks aren't designed with that
  // in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();

  controls->HaveWorkBlock();

  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, std::move(delegate), task_tracker.GetTrackedRef(),
      0);
  worker->Start(service_thread_.task_runner());
  worker->WakeUp();

  controls->WaitForWorkToRun();
  CallJoinFromDifferentThread join_from_different_thread(worker.get());
  join_from_different_thread.Start();
  join_from_different_thread.WaitForRunToStart();
  // Sleep here to give the other thread a chance to call JoinForTesting().
  // Receiving a signal that Run() was called doesn't mean JoinForTesting() was
  // necessarily called, and we can't signal after JoinForTesting() as
  // JoinForTesting() blocks until we call UnblockWork().
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  worker->Cleanup();
  worker = nullptr;
  controls->UnblockWork();
  controls->WaitForDelegateDestroy();
  join_from_different_thread.Join();
}

namespace {

class ExpectThreadTypeDelegate : public WorkerThreadDefaultDelegate {
 public:
  ExpectThreadTypeDelegate()
      : thread_type_verified_in_get_work_event_(
            WaitableEvent::ResetPolicy::AUTOMATIC) {}
  ExpectThreadTypeDelegate(const ExpectThreadTypeDelegate&) = delete;
  ExpectThreadTypeDelegate& operator=(const ExpectThreadTypeDelegate&) = delete;

  void SetExpectedThreadType(ThreadType expected_thread_type) {
    expected_thread_type_ = expected_thread_type;
  }

  void WaitForThreadTypeVerifiedInGetWork() {
    thread_type_verified_in_get_work_event_.Wait();
  }

  // WorkerThread::Delegate:
  void OnMainEntry(WorkerThread* worker) override { VerifyThreadType(); }
  RegisteredTaskSource GetWork(WorkerThread* worker) override {
    VerifyThreadType();
    thread_type_verified_in_get_work_event_.Signal();
    return nullptr;
  }

 private:
  void VerifyThreadType() {
    CheckedAutoLock auto_lock(expected_thread_type_lock_);
    EXPECT_EQ(expected_thread_type_, PlatformThread::GetCurrentThreadType());
  }

  // Signaled after GetWork() has verified the thread type of the worker thread.
  TestWaitableEvent thread_type_verified_in_get_work_event_;

  // Synchronizes access to |expected_thread_type_|.
  CheckedLock expected_thread_type_lock_;

  // Expected thread type for the next call to OnMainEntry() or GetWork().
  ThreadType expected_thread_type_ = ThreadType::kDefault;
};

}  // namespace

TEST_F(ThreadPoolWorkerTest, BumpThreadTypeOfAliveThreadDuringShutdown) {
  if (!CanUseBackgroundThreadTypeForWorkerThread()) {
    return;
  }

  TaskTracker task_tracker;

  // Block shutdown to ensure that the worker doesn't exit when StartShutdown()
  // is called.
  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(TaskTraits{TaskShutdownBehavior::BLOCK_SHUTDOWN},
                               nullptr, TaskSourceExecutionMode::kParallel);
  auto registered_task_source =
      task_tracker.RegisterTaskSource(std::move(sequence));

  std::unique_ptr<ExpectThreadTypeDelegate> delegate(
      new ExpectThreadTypeDelegate);
  ExpectThreadTypeDelegate* delegate_raw = delegate.get();
  delegate_raw->SetExpectedThreadType(ThreadType::kBackground);
  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kBackground, std::move(delegate),
      task_tracker.GetTrackedRef(), 0);
  worker->Start(service_thread_.task_runner());

  // Verify that the initial thread type is kBackground (or kNormal if thread
  // type can't be increased).
  worker->WakeUp();
  delegate_raw->WaitForThreadTypeVerifiedInGetWork();

  // Verify that the thread type is bumped to kNormal during shutdown.
  delegate_raw->SetExpectedThreadType(ThreadType::kDefault);
  task_tracker.StartShutdown();
  worker->WakeUp();
  delegate_raw->WaitForThreadTypeVerifiedInGetWork();

  worker->JoinForTesting();
}

namespace {

class VerifyCallsToObserverDelegate : public WorkerThreadDefaultDelegate {
 public:
  explicit VerifyCallsToObserverDelegate(
      test::MockWorkerThreadObserver* observer)
      : observer_(observer) {}
  VerifyCallsToObserverDelegate(const VerifyCallsToObserverDelegate&) = delete;
  VerifyCallsToObserverDelegate& operator=(
      const VerifyCallsToObserverDelegate&) = delete;

  // WorkerThread::Delegate:
  void OnMainEntry(WorkerThread* worker) override {
    Mock::VerifyAndClear(observer_);
  }

  void OnMainExit(WorkerThread* worker) override {
    observer_->AllowCallsOnMainExit(1);
  }

 private:
  const raw_ptr<test::MockWorkerThreadObserver> observer_;
};

}  // namespace

// Verify that the WorkerThreadObserver is notified when the worker enters
// and exits its main function.
TEST_F(ThreadPoolWorkerTest, WorkerThreadObserver) {
  StrictMock<test::MockWorkerThreadObserver> observer;
  TaskTracker task_tracker;
  auto delegate = std::make_unique<VerifyCallsToObserverDelegate>(&observer);
  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, std::move(delegate), task_tracker.GetTrackedRef(),
      0);
  EXPECT_CALL(observer, OnWorkerThreadMainEntry());
  worker->Start(service_thread_.task_runner(), &observer);
  worker->Cleanup();
  // Join the worker to avoid leaks.
  worker->JoinForTesting();
  Mock::VerifyAndClear(&observer);
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)
namespace {
NOINLINE void FreeForTest(void* data) {
  free(data);
}

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;

}  // namespace

class WorkerThreadThreadCacheDelegate : public WorkerThreadDefaultDelegate {
 public:
  void PrepareForTesting() {
    TimeTicks now = TimeTicks::Now();
    WorkerThreadDefaultDelegate::set_first_sleep_time_for_testing(now);
    first_sleep_time_for_testing_ = now;
  }

  TimeDelta GetSleepDurationBeforePurge(TimeTicks) override {
    // Expect `GetSleepDurationBeforePurge()` to return
    // `kFirstSleepDurationBeforePurge` when invoked within
    // `kFirstSleepDurationBeforePurge` of the first sleep (with
    // `kPurgeThreadCacheIdleDelay` tolerance due to alignment).
    EXPECT_THAT(
        WorkerThreadDefaultDelegate::GetSleepDurationBeforePurge(
            /* now=*/first_sleep_time_for_testing_),
        AllOf(Ge(WorkerThread::Delegate::kFirstSleepDurationBeforePurge),
              Le(WorkerThread::Delegate::kFirstSleepDurationBeforePurge +
                 WorkerThread::Delegate::kPurgeThreadCacheIdleDelay)));

    // Expect `GetSleepDurationBeforePurge()` to return
    // `WorkerThread::Delegate::kPurgeThreadCacheIdleDelay` when invoked later
    // than `kFirstSleepDurationBeforePurge` after the first sleep (with
    // `kPurgeThreadCacheIdleDelay` tolerance due to alignment).
    constexpr base::TimeDelta kEpsilon = base::Microseconds(1);
    EXPECT_THAT(
        WorkerThreadDefaultDelegate::GetSleepDurationBeforePurge(
            /* now=*/first_sleep_time_for_testing_ +
            WorkerThread::Delegate::kFirstSleepDurationBeforePurge + kEpsilon),
        AllOf(Ge(WorkerThread::Delegate::kPurgeThreadCacheIdleDelay),
              Le(WorkerThread::Delegate::kPurgeThreadCacheIdleDelay +
                 WorkerThread::Delegate::kPurgeThreadCacheIdleDelay)));

    // The output of this function is used to drive real sleep in tests.
    // Return a constant so that the tests can validate the wakeups without
    // timing out.
    return GetSleepTimeout();
  }

  void WaitForWork() override {
    // Fill several buckets before going to sleep.
    for (size_t size = 8;
         size < partition_alloc::ThreadCache::kDefaultSizeThreshold; size++) {
      void* data = malloc(size);
      // A simple malloc() / free() pair can be discarded by the compiler (and
      // is), making the test fail. It is sufficient to make |FreeForTest()| a
      // NOINLINE function for the call to not be eliminated, but it is
      // required.
      FreeForTest(data);
    }

    size_t cached_memory_before =
        partition_alloc::ThreadCache::Get()->CachedMemory();
    WorkerThreadDefaultDelegate::WaitForWork();
    size_t cached_memory_after =
        partition_alloc::ThreadCache::Get()->CachedMemory();

    if (!test_done_) {
      if (purge_expected_) {
        EXPECT_LT(cached_memory_after, cached_memory_before / 2);
      } else {
        EXPECT_GT(cached_memory_after, cached_memory_before / 2);
      }
      // Unblock the test.
      wakeup_done_.Signal();
      test_done_ = true;
    }
  }

  // Avoid using the default sleep timeout which is infinite and prevents the
  // tests from completing.
  TimeDelta GetSleepTimeout() override {
    return WorkerThread::Delegate::kPurgeThreadCacheIdleDelay +
           TestTimeouts::tiny_timeout();
  }

  void SetPurgeExpectation(bool purge_expected) {
    purge_expected_ = purge_expected;
  }

  TimeTicks first_sleep_time_for_testing_;
  bool test_done_ = false;
  bool purge_expected_ = false;
  base::WaitableEvent wakeup_done_;
};

TEST_F(ThreadPoolWorkerTest, WorkerThreadCacheNoPurgeOnSignal) {
  // Make sure the thread cache is enabled in the main partition.
  partition_alloc::internal::ThreadCacheProcessScopeForTesting scope(
      allocator_shim::internal::PartitionAllocMalloc::Allocator());

  TaskTracker task_tracker;
  auto delegate = std::make_unique<WorkerThreadThreadCacheDelegate>();
  auto* delegate_raw = delegate.get();
  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, std::move(delegate), task_tracker.GetTrackedRef(),
      0);
  delegate_raw->PrepareForTesting();

  // No purge is expected on waking up from a signal.
  delegate_raw->SetPurgeExpectation(false);

  // Wake up before the thread is started to make sure the first wakeup is
  // caused by a signal.
  worker->WakeUp();
  worker->Start(service_thread_.task_runner(), nullptr);

  // Wait until a wakeup has completed.
  delegate_raw->wakeup_done_.Wait();
  worker->JoinForTesting();
}

TEST_F(ThreadPoolWorkerTest, PurgeOnUninteruptedSleep) {
  // Make sure the thread cache is enabled in the main partition.
  partition_alloc::internal::ThreadCacheProcessScopeForTesting scope(
      allocator_shim::internal::PartitionAllocMalloc::Allocator());

  TaskTracker task_tracker;
  auto delegate = std::make_unique<WorkerThreadThreadCacheDelegate>();
  auto* delegate_raw = delegate.get();
  auto worker = MakeRefCounted<WorkerThread>(
      ThreadType::kDefault, std::move(delegate), task_tracker.GetTrackedRef(),
      0);

  delegate_raw->PrepareForTesting();

  // A purge will take place
  delegate_raw->SetPurgeExpectation(true);

  worker->Start(service_thread_.task_runner(), nullptr);

  // Wait until a wakeup has completed.
  delegate_raw->wakeup_done_.Wait();
  worker->JoinForTesting();
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED)

}  // namespace base::internal
