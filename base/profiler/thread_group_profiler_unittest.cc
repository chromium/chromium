// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/thread_group_profiler.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/profiler/periodic_sampling_scheduler.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/thread_group_profiler_client.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

namespace {
constexpr int kSamplesPerProfile = 20;
constexpr TimeDelta kSamplingInterval = Milliseconds(100);
constexpr TimeDelta kTimeToNextCollection = Hours(1);
}  // namespace

class MockPeriodicSamplingScheduler : public PeriodicSamplingScheduler {
 public:
  explicit MockPeriodicSamplingScheduler(TimeDelta time_to_next_collection)
      : PeriodicSamplingScheduler(Seconds(30), 0.02, TimeTicks::Now()),
        time_to_next_collection_(time_to_next_collection) {}
  TimeDelta GetTimeToNextCollection() override {
    return time_to_next_collection_;
  }

 protected:
  TimeDelta time_to_next_collection_;
};

class MockProfileBuilder : public ProfileBuilder {
 public:
  explicit MockProfileBuilder(OnceClosure completed_callback)
      : completed_callback_(std::move(completed_callback)) {}
  void OnProfileCompleted(TimeDelta profile_duration,
                          TimeDelta sampling_period) override {
    std::move(completed_callback_).Run();
  }
  ModuleCache* GetModuleCache() override { return &module_cache_; }
  MOCK_METHOD(void,
              OnSampleCompleted,
              (std::vector<Frame> frames, TimeTicks sample_timestamp),
              (override));

 protected:
  ModuleCache module_cache_;
  // Callback made when sampling a profile completes.
  OnceClosure completed_callback_;
};

class MockThreadGroupProfilerClient : public ThreadGroupProfilerClient {
 public:
  MockThreadGroupProfilerClient() = default;
  StackSamplingProfiler::SamplingParams GetSamplingParams() override {
    return {.samples_per_profile = kSamplesPerProfile,
            .sampling_interval = kSamplingInterval};
  }
  std::unique_ptr<ProfileBuilder> CreateProfileBuilder(
      OnceClosure callback) override {
    return std::make_unique<MockProfileBuilder>(std::move(callback));
  }
  bool IsProfilerEnabledForCurrentProcess() override { return true; }
  bool IsSingleProcess(const CommandLine& command_line) override {
    return false;
  }
  StackSamplingProfiler::UnwindersFactory GetUnwindersFactory() override {
    return CreateCoreUnwindersFactoryForTesting(nullptr);
  }
  std::unique_ptr<PeriodicSamplingScheduler> CreatePeriodicSamplingScheduler()
      override {
    return std::make_unique<MockPeriodicSamplingScheduler>(
        kTimeToNextCollection);
  }
};

class MockProfiler : public ThreadGroupProfiler::Profiler {
 public:
  MockProfiler(std::map<PlatformThreadId, MockProfiler*>& sampling_profilers,
               int& sampling_profilers_created,
               PlatformThreadId target_thread_id,
               const StackSamplingProfiler::SamplingParams& params,
               std::unique_ptr<ProfileBuilder> profile_builder)
      : sampling_profilers_(sampling_profilers),
        sampling_profilers_created_(sampling_profilers_created),
        target_thread_id_(target_thread_id),
        sampling_params_(params),
        profile_builder_(std::move(profile_builder)) {
    EXPECT_EQ(sampling_profilers_->count(target_thread_id_), 0u);
    (*sampling_profilers_)[target_thread_id_] = this;
    ++*sampling_profilers_created_;
    EXPECT_CALL(*this, Start());
  }

  // ThreadGroupProfiler::Profiler:
  MOCK_METHOD(void, Start, (), (override));

  const StackSamplingProfiler::SamplingParams& sampling_params() const {
    return sampling_params_;
  }

  void CompleteProfiling() {
    profile_builder_->OnProfileCompleted(TimeDelta(), TimeDelta());
  }

 protected:
  ~MockProfiler() override { sampling_profilers_->erase(target_thread_id_); }

 private:
  friend class RefCountedThreadSafe<MockProfiler>;

  raw_ref<std::map<PlatformThreadId, MockProfiler*>> sampling_profilers_;
  raw_ref<int> sampling_profilers_created_;
  PlatformThreadId target_thread_id_;
  const StackSamplingProfiler::SamplingParams sampling_params_;
  std::unique_ptr<ProfileBuilder> profile_builder_;
};

ThreadGroupProfiler::ProfilerFactory GetMockProfilerFactory(
    std::map<PlatformThreadId, MockProfiler*>& sampling_profilers,
    int& sampling_profilers_created) {
  return BindRepeating(BindLambdaForTesting(
      [&sampling_profilers, &sampling_profilers_created](
          int64_t thread_group_type, SamplingProfilerThreadToken thread_token,
          const StackSamplingProfiler::SamplingParams& params,
          std::unique_ptr<ProfileBuilder> profile_builder,
          StackSamplingProfiler::UnwindersFactory unwinder_factory)
          -> scoped_refptr<ThreadGroupProfiler::Profiler> {
        return MakeRefCounted<MockProfiler>(
            sampling_profilers, sampling_profilers_created, thread_token.id,
            params, std::move(profile_builder));
      }));
}

class ThreadGroupProfilerTest : public testing::Test,
                                public ThreadGroupProfiler::Delegate {
 public:
  void SetUp() override {
    ThreadGroupProfiler::SetClient(
        std::make_unique<MockThreadGroupProfilerClient>());
    profiler_ = std::make_unique<ThreadGroupProfiler>(
        /*thread_group_type=*/0, this,
        std::make_unique<MockPeriodicSamplingScheduler>(kTimeToNextCollection),
        GetMockProfilerFactory(sampling_profilers_,
                               sampling_profilers_created_));
    profiler_->Start(task_environment_->GetMainThreadTaskRunner());
  }

  void TearDown() override {
    task_environment_.reset();
    active_collection_.reset();
    profiler_.reset();
    ThreadGroupProfiler::SetClient(nullptr);
  }

  // ThreadGroupProfiler::Delegate:
  void OnStartProfilingSession(
      ThreadGroupProfiler::ActiveCollection active_collection) override {
    active_collection_.emplace(std::move(active_collection));
    std::vector<scoped_refptr<ThreadGroupProfiler::Profiler>>
        profilers_to_start;
    for (FakeWorkerThread* worker : active_workers_) {
      if (auto profiler = active_collection_->MaybeAddWorkerThread(
              worker->fake_pointer(),
              SamplingProfilerThreadToken{worker->GetThreadId()})) {
        profilers_to_start.push_back(std::move(profiler));
      }
    }
    for (auto& profiler : profilers_to_start) {
      profiler->Start();
    }
  }

  void OnEndProfilingSession() override { active_collection_.reset(); }

 protected:
  class FakeWorkerThread {
   public:
    FakeWorkerThread(ThreadGroupProfilerTest* test,
                     PlatformThreadId thread_id,
                     internal::WorkerThread* fake_pointer)
        : test_(test), thread_id_(thread_id), fake_pointer_(fake_pointer) {}

    ~FakeWorkerThread() {
      if (test_) {
        test_->UnregisterWorker(this);
      }
    }

    PlatformThreadId GetThreadId() const { return thread_id_; }
    internal::WorkerThread* fake_pointer() const { return fake_pointer_; }

    void SetActive() { test_->SetWorkerActive(this); }
    void SetIdle() { test_->SetWorkerIdle(this); }
    void Exit() {
      test_->UnregisterWorker(this);
      test_ = nullptr;
    }

   private:
    raw_ptr<ThreadGroupProfilerTest> test_;
    PlatformThreadId const thread_id_;
    raw_ptr<internal::WorkerThread> const fake_pointer_;
  };

  void StopProfilingSession() { active_collection_.reset(); }

  std::unique_ptr<FakeWorkerThread> CreateFakeWorkerThread() {
    PlatformThreadId id = PlatformThreadId::ForTest(next_worker_thread_id_++);
    auto* fake_pointer =
        reinterpret_cast<internal::WorkerThread*>(static_cast<uint64_t>(id));
    return std::make_unique<FakeWorkerThread>(this, id, fake_pointer);
  }

  void UnregisterWorker(FakeWorkerThread* worker) {
    active_workers_.erase(worker);
    if (active_collection_) {
      active_collection_->RemoveWorkerThread(worker->fake_pointer());
    }
  }

  void SetWorkerActive(FakeWorkerThread* worker) {
    active_workers_.insert(worker);
    if (active_collection_) {
      if (auto profiler = active_collection_->MaybeAddWorkerThread(
              worker->fake_pointer(),
              SamplingProfilerThreadToken{worker->GetThreadId()})) {
        profiler->Start();
      }
    }
  }

  void SetWorkerIdle(FakeWorkerThread* worker) {
    active_workers_.erase(worker);
  }

  void InitiateNextCollection() {
    task_environment_->FastForwardBy(time_to_next_collection_);
    task_environment_->RunUntilIdle();
    time_to_next_collection_ = kTimeToNextCollection;
  }

  void AdvanceBySamples(int samples) {
    const TimeDelta samples_duration = kSamplingInterval * samples;
    task_environment_->FastForwardBy(samples_duration);
    task_environment_->RunUntilIdle();
    time_to_next_collection_ -= samples_duration;
  }

  void AdvanceToEndOfCollection() {
    const TimeDelta duration_already_advanced =
        kTimeToNextCollection - time_to_next_collection_;
    const TimeDelta collection_duration =
        kSamplingInterval * kSamplesPerProfile;
    const TimeDelta duration_to_end_of_collection =
        collection_duration - duration_already_advanced;
    task_environment_->FastForwardBy(duration_to_end_of_collection);
    task_environment_->RunUntilIdle();
    time_to_next_collection_ -= duration_to_end_of_collection;
  }

  void CompleteProfiling(MockProfiler* profiler) {
    profiler->CompleteProfiling();
    task_environment_->RunUntilIdle();
  }

  // Optional to support destruction prior to profiler_.
  std::optional<test::TaskEnvironment> task_environment_{
      std::in_place, test::TaskEnvironment::TimeSource::MOCK_TIME,
      test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC};
  std::set<FakeWorkerThread*> active_workers_;
  std::optional<ThreadGroupProfiler::ActiveCollection> active_collection_;
  std::map<PlatformThreadId, MockProfiler*> sampling_profilers_;
  int sampling_profilers_created_ = 0;
  std::unique_ptr<ThreadGroupProfiler> profiler_;
  ModuleCache module_cache_;
  int next_worker_thread_id_ = 1;
  TimeDelta time_to_next_collection_ = kTimeToNextCollection;
};

TEST_F(ThreadGroupProfilerTest, Construction) {
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionInactive_WorkerInactiveLifecycle) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->Exit();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionInactive_WorkerActiveLifecycle) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetIdle();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->Exit();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_NoWorkers) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_WorkerExited) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  worker->SetActive();
  worker->Exit();
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_InactiveWorker) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_NewlyInactiveWorker) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  worker->SetActive();
  worker->SetIdle();
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_ActiveWorker) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_EQ(sampling_profilers_.size(), 1u);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  MockProfiler* const sampling_profiler =
      sampling_profilers_[worker->GetThreadId()];
  EXPECT_EQ(sampling_profiler->sampling_params().samples_per_profile,
            kSamplesPerProfile);
  EXPECT_EQ(sampling_profiler->sampling_params().sampling_interval,
            kSamplingInterval);
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_ReactivatedWorker) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  worker->SetActive();
  worker->SetIdle();
  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_EQ(sampling_profilers_.size(), 1u);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  MockProfiler* const sampling_profiler =
      sampling_profilers_[worker->GetThreadId()];
  EXPECT_EQ(sampling_profiler->sampling_params().samples_per_profile,
            kSamplesPerProfile);
  EXPECT_EQ(sampling_profiler->sampling_params().sampling_interval,
            kSamplingInterval);
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_MultipleWorkers) {
  std::unique_ptr<FakeWorkerThread> worker1 = CreateFakeWorkerThread();
  std::unique_ptr<FakeWorkerThread> worker2 = CreateFakeWorkerThread();

  worker1->SetActive();
  worker2->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_EQ(sampling_profilers_.size(), 2u);
  ASSERT_TRUE(sampling_profilers_.find(worker1->GetThreadId()) !=
              sampling_profilers_.end());
  ASSERT_TRUE(sampling_profilers_.find(worker2->GetThreadId()) !=
              sampling_profilers_.end());
  MockProfiler* const sampling_profiler1 =
      sampling_profilers_[worker1->GetThreadId()];
  EXPECT_EQ(sampling_profiler1->sampling_params().samples_per_profile,
            kSamplesPerProfile);
  EXPECT_EQ(sampling_profiler1->sampling_params().sampling_interval,
            kSamplingInterval);
  MockProfiler* const sampling_profiler2 =
      sampling_profilers_[worker2->GetThreadId()];
  EXPECT_EQ(sampling_profiler2->sampling_params().samples_per_profile,
            kSamplesPerProfile);
  EXPECT_EQ(sampling_profiler2->sampling_params().sampling_interval,
            kSamplingInterval);
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_WorkerBecomesActive) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  MockProfiler* const sampling_profiler =
      sampling_profilers_[worker->GetThreadId()];
  EXPECT_EQ(sampling_profiler->sampling_params().samples_per_profile,
            kSamplesPerProfile);
  EXPECT_EQ(sampling_profiler->sampling_params().sampling_interval,
            kSamplingInterval);
}

TEST_F(ThreadGroupProfilerTest, CollectionActive_WorkerInactiveLifecycle) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->Exit();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionActive_WorkerActiveStartsProfiling) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  MockProfiler* const sampling_profiler =
      sampling_profilers_[worker->GetThreadId()];
  EXPECT_EQ(sampling_profiler->sampling_params().samples_per_profile,
            kSamplesPerProfile);
  EXPECT_EQ(sampling_profiler->sampling_params().sampling_interval,
            kSamplingInterval);
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_WorkerReactivatedContinuesExistingProfiling) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  worker->SetIdle();
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);
  EXPECT_EQ(sampling_profilers_created_, 1);
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_WorkerActiveToIdleContinuesProfiling) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  // Transitioning to idle does not interrupt the existing profiling.
  worker->SetIdle();
  EXPECT_EQ(sampling_profilers_.size(), 1u);
  EXPECT_EQ(sampling_profilers_created_, 1);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_WorkerActiveToExitStopsProfiling) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  worker->Exit();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_CollectionContinuesOnWorkerExit) {
  std::unique_ptr<FakeWorkerThread> worker_to_exit = CreateFakeWorkerThread();
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  // Start a collection and make the worker thread active to start profiling,
  // then have the worker exit.
  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker_to_exit->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  worker_to_exit->Exit();
  EXPECT_TRUE(sampling_profilers_.empty());

  // Make a new worker thread active. It should start profiling as part of the
  // collection.
  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_WorkerProfilesOnlyUntilEndOfCollection) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  AdvanceBySamples(5);
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  MockProfiler* const sampling_profiler =
      sampling_profilers_[worker->GetThreadId()];
  EXPECT_EQ(sampling_profiler->sampling_params().samples_per_profile,
            kSamplesPerProfile - 5);
  EXPECT_EQ(sampling_profiler->sampling_params().sampling_interval,
            kSamplingInterval);
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_WorkerDoesNotProfileNearEndOfCollection) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  AdvanceBySamples(kSamplesPerProfile - 2);
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());
}

// Ensure no profiling occurs if a worker thread is activated after the
// collection ends.
TEST_F(ThreadGroupProfilerTest, CollectionEnded_NoWorkers) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  StopProfilingSession();
  EXPECT_TRUE(sampling_profilers_.empty());

  // Making the worker active after session ends should not result in any new
  // profiling.
  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());
}

// Ensure that a worker whose profiling finishes before the collection session
// ends is cleaned up properly, and doesn't profile again when reactivated after
// session end.
TEST_F(ThreadGroupProfilerTest,
       CollectionEnded_ActiveWorker_FinishesBeforeCollection) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  // Start a collection and make the worker thread active to start profiling.
  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  // Advance to just before the end of the collection period, and have the
  // profiler complete.
  AdvanceBySamples(kSamplesPerProfile - 1);
  EXPECT_EQ(sampling_profilers_.size(), 1u);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  CompleteProfiling(sampling_profilers_[worker->GetThreadId()]);

  // Complete the collection.
  StopProfilingSession();
  EXPECT_TRUE(sampling_profilers_.empty());

  // Make the thread idle then active again. This should not result in any new
  // profiling.
  worker->SetIdle();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());
}

// Ensure that active worker profilers are cleanly destroyed when the
// collection session ends.
TEST_F(ThreadGroupProfilerTest,
       CollectionEnded_ActiveWorker_DestroyedOnSessionEnd) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  // Start a collection and make the worker thread active to start profiling.
  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  // Advance partway through the collection while worker is still profiling.
  AdvanceBySamples(5);
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  // Stop the profiling session. The active profiler should be destroyed.
  StopProfilingSession();
  EXPECT_TRUE(sampling_profilers_.empty());

  // Reactivating the worker after session end does not start profiling.
  worker->SetIdle();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());
}

// Ensure that worker thread calls after task runner shutdown have no effect or
// crash.
TEST_F(ThreadGroupProfilerTest, PostTaskRunnerShutdown) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  // Shut down the task runner by destroying the TaskEnvironment.
  task_environment_.reset();

  worker->SetActive();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->SetIdle();
  EXPECT_TRUE(sampling_profilers_.empty());

  StopProfilingSession();
  worker->Exit();
  EXPECT_TRUE(sampling_profilers_.empty());
}

// Ensures that a worker thread holding a profiler reference can safely start
// profiling even if the active collection session ends before Start() is
// called.
TEST_F(ThreadGroupProfilerTest, ActiveCollectionEndedBeforeProfilerStart) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  InitiateNextCollection();

  // Obtain a ref to the profiler from ActiveCollection as in
  // WorkerDelegate::GetWork().
  scoped_refptr<ThreadGroupProfiler::Profiler> profiler_to_start =
      active_collection_->MaybeAddWorkerThread(
          worker->fake_pointer(),
          SamplingProfilerThreadToken{worker->GetThreadId()});
  ASSERT_TRUE(profiler_to_start);
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  // Stop the profiling session concurrently before Start() is called.
  // ActiveCollection drops its ref, but profiler_to_start keeps the profiler
  // alive.
  StopProfilingSession();
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  // Calling Start() on the retained ref must succeed safely.
  profiler_to_start->Start();

  // When the retained ref is released, the profiler is safely destroyed.
  profiler_to_start.reset();
  EXPECT_TRUE(sampling_profilers_.empty());
}

// Ensures that a profiler reference remains valid and can be started even if
// the associated worker thread is removed from the active collection
// concurrently.
TEST_F(ThreadGroupProfilerTest, WorkerRemovedBeforeProfilerStart) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  InitiateNextCollection();

  scoped_refptr<ThreadGroupProfiler::Profiler> profiler_to_start =
      active_collection_->MaybeAddWorkerThread(
          worker->fake_pointer(),
          SamplingProfilerThreadToken{worker->GetThreadId()});
  ASSERT_TRUE(profiler_to_start);
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  // Worker cleans up / is removed from active collection.
  // ActiveCollection drops its ref, but profiler_to_start keeps the profiler
  // alive.
  active_collection_->RemoveWorkerThread(worker->fake_pointer());
  EXPECT_EQ(sampling_profilers_.size(), 1u);

  // Start() on the staged ref succeeds without UAF.
  profiler_to_start->Start();

  // When the staged ref is released, the profiler is safely destroyed.
  profiler_to_start.reset();
  EXPECT_TRUE(sampling_profilers_.empty());
}

}  // namespace base
