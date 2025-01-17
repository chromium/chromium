// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/thread_group_profiler.h"

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/profiler/periodic_sampling_scheduler.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/thread_group_profiler_client.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

namespace internal {
class WorkerThread;
}

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
  MOCK_METHOD2(OnSampleCompleted,
               void(std::vector<Frame> frames, TimeTicks sample_timestamp));

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
    EXPECT_EQ(sampling_profilers_->count(target_thread_id_), 0);
    (*sampling_profilers_)[target_thread_id_] = this;
    ++*sampling_profilers_created_;
    EXPECT_CALL(*this, Start());
  }

  ~MockProfiler() override { sampling_profilers_->erase(target_thread_id_); }

  // ThreadGroupProfiler::Profiler:
  MOCK_METHOD(void, Start, (), (override));

  const StackSamplingProfiler::SamplingParams& sampling_params() const {
    return sampling_params_;
  }

  void CompleteProfiling() {
    profile_builder_->OnProfileCompleted(TimeDelta(), TimeDelta());
  }

 private:
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
          SamplingProfilerThreadToken thread_token,
          const StackSamplingProfiler::SamplingParams& params,
          std::unique_ptr<ProfileBuilder> profile_builder,
          StackSamplingProfiler::UnwindersFactory unwinder_factory)
          -> std::unique_ptr<ThreadGroupProfiler::Profiler> {
        return std::make_unique<MockProfiler>(
            sampling_profilers, sampling_profilers_created, thread_token.id,
            params, std::move(profile_builder));
      }));
}

class ThreadGroupProfilerTest : public testing::Test {
 public:
  void SetUp() override {
    ThreadGroupProfiler::SetClient(
        std::make_unique<MockThreadGroupProfilerClient>());
    profiler_ = std::make_unique<ThreadGroupProfiler>(
        ThreadPool::CreateSequencedTaskRunner({}), "Test",
        std::make_unique<MockPeriodicSamplingScheduler>(kTimeToNextCollection),
        GetMockProfilerFactory(sampling_profilers_,
                               sampling_profilers_created_));
  }

  void TearDown() override {
    task_environment_.reset();
    if (!shutdown_started_) {
      profiler_->Shutdown();
    }
    ThreadGroupProfiler::SetClient(nullptr);
  }

 protected:
  class FakeWorkerThread : public Thread {
   public:
    FakeWorkerThread(internal::WorkerThread* fake_pointer,
                     test::TaskEnvironment& task_environment)
        : Thread("FakeWorkerThread"),
          fake_pointer_(fake_pointer),
          task_environment_(task_environment) {
      Start();
    }

    ~FakeWorkerThread() override { Stop(); }

    void RunOnThread(FunctionRef<void(internal::WorkerThread*)> callable) {
      // First, synchronously execute the callable on the Thread's task runner.
      WaitableEvent callable_completed{
          WaitableEvent::ResetPolicy::MANUAL,
          WaitableEvent::InitialState::NOT_SIGNALED};

      RunOnThreadAsync(callable, callable_completed);

      callable_completed.Wait();

      // Then, ensure any thread pool tasks that the profiler posted run to
      // completion. The thread pool is configured to run tasks eagerly via
      // ThreadPoolExecutionMode::ASYNC, so tasks may already be executing. This
      // call ensures they finish.
      task_environment_->RunUntilIdle();
    }

    void RunOnThreadAsync(FunctionRef<void(internal::WorkerThread*)> callable,
                          WaitableEvent& callable_completed) {
      task_runner()->PostTask(
          FROM_HERE, BindLambdaForTesting([callable, &callable_completed,
                                           fake_pointer = fake_pointer_] {
            callable(fake_pointer);
            callable_completed.Signal();
          }));
    }

   private:
    raw_ptr<internal::WorkerThread> const fake_pointer_;
    raw_ref<test::TaskEnvironment> task_environment_;
  };

  std::unique_ptr<FakeWorkerThread> CreateFakeWorkerThread() {
    return std::make_unique<FakeWorkerThread>(
        reinterpret_cast<internal::WorkerThread*>(next_worker_thread_id_++),
        *task_environment_);
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
  std::map<PlatformThreadId, MockProfiler*> sampling_profilers_;
  int sampling_profilers_created_ = 0;
  std::unique_ptr<ThreadGroupProfiler> profiler_;
  ModuleCache module_cache_;
  int next_worker_thread_id_ = 1;
  TimeDelta time_to_next_collection_ = kTimeToNextCollection;
  bool shutdown_started_ = false;
};

TEST_F(ThreadGroupProfilerTest, Construction) {
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionInactive_WorkerInactiveLifecycle) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadExiting(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionInactive_WorkerActiveLifecycle) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadIdle(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadExiting(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_NoWorkers) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_WorkerExited) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadExiting(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_InactiveWorker) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_NewlyInactiveWorker) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadIdle(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionBecomesActive_ActiveWorker) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_EQ(sampling_profilers_.size(), 1);
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

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadIdle(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_EQ(sampling_profilers_.size(), 1);
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
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();
  std::unique_ptr<FakeWorkerThread> worker2 = CreateFakeWorkerThread();

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  worker2->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  worker2->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_EQ(sampling_profilers_.size(), 2);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  ASSERT_TRUE(sampling_profilers_.find(worker2->GetThreadId()) !=
              sampling_profilers_.end());
  MockProfiler* const sampling_profiler =
      sampling_profilers_[worker->GetThreadId()];
  EXPECT_EQ(sampling_profiler->sampling_params().samples_per_profile,
            kSamplesPerProfile);
  EXPECT_EQ(sampling_profiler->sampling_params().sampling_interval,
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

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });

  EXPECT_EQ(sampling_profilers_.size(), 1);
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

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadExiting(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionActive_WorkerActiveStartsProfiling) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });

  EXPECT_EQ(sampling_profilers_.size(), 1);
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

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_EQ(sampling_profilers_.size(), 1);

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadIdle(worker_thread);
  });
  EXPECT_EQ(sampling_profilers_.size(), 1);

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });

  EXPECT_EQ(sampling_profilers_.size(), 1);
  EXPECT_EQ(sampling_profilers_created_, 1);
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_WorkerActiveToIdleContinuesProfiling) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_EQ(sampling_profilers_.size(), 1);

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadIdle(worker_thread);
  });

  EXPECT_EQ(sampling_profilers_.size(), 1);
  EXPECT_EQ(sampling_profilers_created_, 1);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_WorkerActiveToExitStopsProfiling) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_EQ(sampling_profilers_.size(), 1);

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadExiting(worker_thread);
  });
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

  worker_to_exit->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker_to_exit->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_EQ(sampling_profilers_.size(), 1);

  worker_to_exit->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadExiting(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  // Make a new worker thread active. It should start profiling as part of the
  // collection.
  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_EQ(sampling_profilers_.size(), 1);
}

TEST_F(ThreadGroupProfilerTest,
       CollectionActive_WorkerProfilesOnlyUntilEndOfCollection) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  AdvanceBySamples(5);
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });

  EXPECT_EQ(sampling_profilers_.size(), 1);
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

  AdvanceBySamples(kSamplesPerProfile - 5);
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest, CollectionEnded_NoWorkers) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  AdvanceToEndOfCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  // Making the worker active should not result in any new profiling.
  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest,
       CollectionEnded_ActiveWorker_FinishesBeforeCollection) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  // Start a collection and make the worker thread active to start profiling.
  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_EQ(sampling_profilers_.size(), 1);

  // Advance to just before the end of the collection period, and have the
  // profiler complete.
  AdvanceBySamples(kSamplesPerProfile - 1);

  EXPECT_EQ(sampling_profilers_.size(), 1);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  CompleteProfiling(sampling_profilers_[worker->GetThreadId()]);
  EXPECT_TRUE(sampling_profilers_.empty());

  // Complete the collection.
  AdvanceToEndOfCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  // Make the thread idle then active again. This should not result in any new
  // profiling.
  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadIdle(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());
}

TEST_F(ThreadGroupProfilerTest,
       CollectionEnded_ActiveWorker_FinishesAfterCollection) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  // Start a collection and make the worker thread active to start profiling.
  InitiateNextCollection();
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_EQ(sampling_profilers_.size(), 1);

  // Advance to the end of the collection. The worker profiling should still be
  // taking place since it hasn't completed yet.
  AdvanceToEndOfCollection();
  EXPECT_EQ(sampling_profilers_.size(), 1);

  // Complete the worker profiling.
  EXPECT_EQ(sampling_profilers_.size(), 1);
  ASSERT_TRUE(sampling_profilers_.find(worker->GetThreadId()) !=
              sampling_profilers_.end());
  CompleteProfiling(sampling_profilers_[worker->GetThreadId()]);
  EXPECT_TRUE(sampling_profilers_.empty());

  // Make the thread idle then active again. This should not result in any new
  // profiling.
  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadIdle(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());

  worker->RunOnThread([this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  });
  EXPECT_TRUE(sampling_profilers_.empty());
}

// Ensure that worker thread calls post task runner shutdown have no effect.
TEST_F(ThreadGroupProfilerTest, PostTaskRunnerShutdown) {
  std::unique_ptr<FakeWorkerThread> worker = CreateFakeWorkerThread();

  // Shut down the task runner by destroying the TaskEnvironment.
  task_environment_.reset();

  WaitableEvent on_thread_call_completed{
      WaitableEvent::ResetPolicy::AUTOMATIC,
      WaitableEvent::InitialState::NOT_SIGNALED};

  auto worker_started = [this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadStarted(worker_thread);
  };
  worker->RunOnThreadAsync(worker_started, on_thread_call_completed);

  on_thread_call_completed.Wait();
  EXPECT_TRUE(sampling_profilers_.empty());

  auto worker_active = [this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadActive(worker_thread);
  };
  worker->RunOnThreadAsync(worker_active, on_thread_call_completed);

  on_thread_call_completed.Wait();
  EXPECT_TRUE(sampling_profilers_.empty());

  auto worker_idle = [this](internal::WorkerThread* worker_thread) {
    profiler_->OnWorkerThreadIdle(worker_thread);
  };
  worker->RunOnThreadAsync(worker_idle, on_thread_call_completed);

  on_thread_call_completed.Wait();
  EXPECT_TRUE(sampling_profilers_.empty());

  // When the task runner has been shut down, OnWorkerThreadExiting depends on
  // ThreadGroupProfiler::Shutdown() being invoked to know that the thread's
  // profiling has ceased. Choreograph shutdown to mimic those steps.
  auto worker_exit =
      [profiler = profiler_.get()](internal::WorkerThread* worker_thread) {
        profiler->OnWorkerThreadExiting(worker_thread);
      };
  worker->RunOnThreadAsync(worker_exit, on_thread_call_completed);

  profiler_->Shutdown();
  shutdown_started_ = true;
  on_thread_call_completed.Wait();
  EXPECT_TRUE(sampling_profilers_.empty());
}

}  // namespace base
