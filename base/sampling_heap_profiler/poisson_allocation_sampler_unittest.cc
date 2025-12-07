// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"

#include <stdlib.h>

#include <array>
#include <atomic>
#include <bitset>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/barrier_closure.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace base {

namespace {

using allocator::dispatcher::AllocationNotificationData;
using allocator::dispatcher::AllocationSubsystem;
using allocator::dispatcher::FreeNotificationData;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

class DummySamplesObserver : public PoissonAllocationSampler::SamplesObserver {
 public:
  void SampleAdded(void* address,
                   size_t size,
                   size_t total,
                   AllocationSubsystem type,
                   const char* context) final {}
  void SampleRemoved(void* address) final {}
};

class AddRemoveObserversTester {
 public:
  AddRemoveObserversTester() = default;
  ~AddRemoveObserversTester() = default;

  // Posts tasks to add and remove observers to `task_runner_`. Invokes
  // `barrier_closure` if a task fails or after `num_repetitions`.
  void Start(base::RepeatingClosure* barrier_closure, int num_repetitions);

  // Gives the caller ownership of the observers so that they can be safely
  // deleted in single-threaded context once the PoissonAllocationSampler is not
  // running.
  std::vector<std::unique_ptr<DummySamplesObserver>> DetachObservers() {
    std::vector<std::unique_ptr<DummySamplesObserver>> observers;
    observers.push_back(std::move(observer1_));
    observers.push_back(std::move(observer2_));
    return observers;
  }

 private:
  // Posts tasks to add and remove observers to `task_runner_`. If they fail or
  // if this is the last repetition, invokes `barrier_closure`,  otherwise
  // schedules another repetition.
  void TestAddRemoveObservers(base::RepeatingClosure* barrier_closure,
                              int remaining_repetitions);

  // These observers must live until all threads are destroyed, because the
  // profiler might call into them racily after they're removed. (See the
  // comment on PoissonAllocationSampler::RemoveSamplesObserver().) They can
  // safely be destroyed on the main thread after the test is back in a
  // single-threaded context.
  std::unique_ptr<DummySamplesObserver> observer1_ =
      std::make_unique<DummySamplesObserver>();
  std::unique_ptr<DummySamplesObserver> observer2_ =
      std::make_unique<DummySamplesObserver>();

  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
};

class MuteHookedSamplesTester {
 public:
  MuteHookedSamplesTester() = default;
  ~MuteHookedSamplesTester() = default;

  // Posts tasks to mute and unmute samples to `task_runner_`. Invokes
  // `barrier_closure` if a task fails or after `num_repetitions`.
  void Start(base::RepeatingClosure* barrier_closure, int num_repetitions);

 private:
  // Posts tasks to mute and unmute samples to `task_runner_`. If they fail or
  // if this is the last repetition, invokes `barrier_closure`,  otherwise
  // schedules another repetition.
  void TestMuteUnmuteSamples(base::RepeatingClosure* barrier_closure,
                             int remaining_repetitions);

  base::OnceClosure unmute_samples_closure_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
};

}  // namespace

// PoissonAllocationSamplerStateTest has access to read the state of
// PoissonAllocationSampler.
class PoissonAllocationSamplerStateTest : public ::testing::Test {
 public:
  static void AddSamplesObservers(DummySamplesObserver* observer1,
                                  DummySamplesObserver* observer2);

  static void RemoveSamplesObservers(DummySamplesObserver* observer1,
                                     DummySamplesObserver* observer2);

  // Creates a ScopedMuteHookedSamplesForTesting object and returns a closure
  // that will destroy it.
  static base::OnceClosure MuteHookedSamples();

  static void UnmuteHookedSamples(base::OnceClosure unmute_closure);

 protected:
  using ProfilingStateFlag = PoissonAllocationSampler::ProfilingStateFlag;
  using ProfilingStateFlagMask =
      PoissonAllocationSampler::ProfilingStateFlagMask;

  static AssertionResult HasAllStateFlags(ProfilingStateFlagMask flags) {
    const ProfilingStateFlagMask state =
        PoissonAllocationSampler::profiling_state_.load(
            std::memory_order_relaxed);
    AssertionResult result =
        (state & flags) == flags ? AssertionSuccess() : AssertionFailure();
    return result << "Expected all of " << std::bitset<4>(flags) << ", got "
                  << std::bitset<4>(state);
  }

  static AssertionResult HasAnyStateFlags(ProfilingStateFlagMask flags) {
    const ProfilingStateFlagMask state =
        PoissonAllocationSampler::profiling_state_.load(
            std::memory_order_relaxed);
    AssertionResult result =
        (state & flags) != 0 ? AssertionSuccess() : AssertionFailure();
    return result << "Expected any of " << std::bitset<4>(flags) << ", got "
                  << std::bitset<4>(state);
  }

  // This must come before `task_environment_` do that it's deleted afterward,
  // since DummySamplesObserver objects must be deleted in single-threaded
  // context.
  std::vector<std::unique_ptr<DummySamplesObserver>> observers_to_delete_;

  base::test::TaskEnvironment task_environment_;
};

void AddRemoveObserversTester::Start(base::RepeatingClosure* barrier_closure,
                                     int num_repetitions) {
  // Unretained is safe because `this` isn't deleted until the
  // `barrier_closure` is invoked often enough to quit the main RunLoop.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AddRemoveObserversTester::TestAddRemoveObservers,
                     base::Unretained(this), barrier_closure, num_repetitions));
}

void AddRemoveObserversTester::TestAddRemoveObservers(
    base::RepeatingClosure* barrier_closure,
    int remaining_repetitions) {
  if (!remaining_repetitions || ::testing::Test::HasFailure()) {
    barrier_closure->Run();
    return;
  }
  auto add_observers =
      base::BindOnce(&PoissonAllocationSamplerStateTest::AddSamplesObservers,
                     observer1_.get(), observer2_.get());
  auto remove_observers = base::BindPostTask(
      task_runner_,
      base::BindOnce(&PoissonAllocationSamplerStateTest::RemoveSamplesObservers,
                     observer1_.get(), observer2_.get()));
  // Unretained is safe because `this` isn't deleted until the
  // `barrier_closure` is invoked often enough to quit the main RunLoop.
  auto next_repetition = base::BindPostTask(
      task_runner_,
      base::BindOnce(&AddRemoveObserversTester::TestAddRemoveObservers,
                     base::Unretained(this), barrier_closure,
                     remaining_repetitions - 1));
  task_runner_->PostTask(FROM_HERE, std::move(add_observers)
                                        .Then(std::move(remove_observers))
                                        .Then(std::move(next_repetition)));
}

void MuteHookedSamplesTester::Start(base::RepeatingClosure* barrier_closure,
                                    int num_repetitions) {
  // Unretained is safe because `this` isn't deleted until the
  // `barrier_closure` is invoked often enough to quit the main RunLoop.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MuteHookedSamplesTester::TestMuteUnmuteSamples,
                     base::Unretained(this), barrier_closure, num_repetitions));
}

void MuteHookedSamplesTester::TestMuteUnmuteSamples(
    base::RepeatingClosure* barrier_closure,
    int remaining_repetitions) {
  if (!remaining_repetitions || ::testing::Test::HasFailure()) {
    barrier_closure->Run();
    return;
  }
  auto mute_samples =
      base::BindOnce(&PoissonAllocationSamplerStateTest::MuteHookedSamples);
  auto unmute_samples = base::BindPostTask(
      task_runner_,
      base::BindOnce(&PoissonAllocationSamplerStateTest::UnmuteHookedSamples));
  // Unretained is safe because `this` isn't deleted until the
  // `barrier_closure` is invoked often enough to quit the main RunLoop.
  auto next_repetition = base::BindPostTask(
      task_runner_,
      base::BindOnce(&MuteHookedSamplesTester::TestMuteUnmuteSamples,
                     base::Unretained(this), barrier_closure,
                     remaining_repetitions - 1));
  task_runner_->PostTask(FROM_HERE, std::move(mute_samples)
                                        .Then(std::move(unmute_samples))
                                        .Then(std::move(next_repetition)));
}

// static
void PoissonAllocationSamplerStateTest::AddSamplesObservers(
    DummySamplesObserver* observer1,
    DummySamplesObserver* observer2) {
  // The first observer should start the profiler running if it isn't already.
  // The second should not change the state.
  PoissonAllocationSampler::Get()->AddSamplesObserver(observer1);
  EXPECT_TRUE(HasAllStateFlags(ProfilingStateFlag::kIsRunning |
                               ProfilingStateFlag::kWasStarted));
  PoissonAllocationSampler::Get()->AddSamplesObserver(observer2);
  EXPECT_TRUE(HasAllStateFlags(ProfilingStateFlag::kIsRunning |
                               ProfilingStateFlag::kWasStarted));
}

// static
void PoissonAllocationSamplerStateTest::RemoveSamplesObservers(
    DummySamplesObserver* observer1,
    DummySamplesObserver* observer2) {
  // Removing the first observer should leave the profiler running. Removing the
  // second might leave it running, or might stop it. It should never remove the
  // kWasStarted flag.
  EXPECT_TRUE(HasAllStateFlags(ProfilingStateFlag::kIsRunning |
                               ProfilingStateFlag::kWasStarted));
  PoissonAllocationSampler::Get()->RemoveSamplesObserver(observer1);
  EXPECT_TRUE(HasAllStateFlags(ProfilingStateFlag::kIsRunning |
                               ProfilingStateFlag::kWasStarted));
  PoissonAllocationSampler::Get()->RemoveSamplesObserver(observer2);
  EXPECT_TRUE(HasAllStateFlags(ProfilingStateFlag::kWasStarted));
}

// static
base::OnceClosure PoissonAllocationSamplerStateTest::MuteHookedSamples() {
  using ScopedMuteHookedSamplesForTesting =
      PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting;
  EXPECT_FALSE(
      HasAllStateFlags(ProfilingStateFlag::kHookedSamplesMutedForTesting));
  auto scoped_mute_hooked_samples =
      std::make_unique<ScopedMuteHookedSamplesForTesting>();
  EXPECT_TRUE(
      HasAllStateFlags(ProfilingStateFlag::kHookedSamplesMutedForTesting));
  auto unmute_closure = base::BindOnce(
      [](std::unique_ptr<ScopedMuteHookedSamplesForTesting> p) { p.reset(); },
      std::move(scoped_mute_hooked_samples));
  return unmute_closure;
}

// static
void PoissonAllocationSamplerStateTest::UnmuteHookedSamples(
    base::OnceClosure unmute_closure) {
  EXPECT_TRUE(
      HasAllStateFlags(ProfilingStateFlag::kHookedSamplesMutedForTesting));
  std::move(unmute_closure).Run();
  EXPECT_FALSE(
      HasAllStateFlags(ProfilingStateFlag::kHookedSamplesMutedForTesting));
}

TEST(PoissonAllocationSamplerTest, MuteHooksWithoutInit) {
  // Make sure it's safe to create a ScopedMuteHookedSamplesForTesting from
  // tests that might not call PoissonAllocationSampler::Get() to initialize the
  // rest of the PoissonAllocationSampler.
  EXPECT_FALSE(PoissonAllocationSampler::AreHookedSamplesMuted());
  void* volatile p = nullptr;
  {
    PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting mute_hooks;
    EXPECT_TRUE(PoissonAllocationSampler::AreHookedSamplesMuted());
    p = malloc(10000);
  }
  EXPECT_FALSE(PoissonAllocationSampler::AreHookedSamplesMuted());
  free(p);
}

TEST(PoissonAllocationSamplerTest, NestedScopedMuteThreadSamples) {
  PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting mute_hooks;

  EXPECT_FALSE(PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
  intptr_t accumulated_bytes_snapshot =
      PoissonAllocationSampler::GetAccumulatedBytesForTesting();

  {
    PoissonAllocationSampler::ScopedMuteThreadSamples nesting_level1;
    EXPECT_TRUE(PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
    // When first muted, `accumulated_bytes` should be swizzled to avoid bias.
    intptr_t accumulated_bytes =
        PoissonAllocationSampler::GetAccumulatedBytesForTesting();
    EXPECT_NE(accumulated_bytes, accumulated_bytes_snapshot);

    {
      PoissonAllocationSampler::ScopedMuteThreadSamples nesting_level2;
      EXPECT_TRUE(PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
      // `accumulated_bytes` should only be modified when the state changes.
      EXPECT_EQ(PoissonAllocationSampler::GetAccumulatedBytesForTesting(),
                accumulated_bytes);
    }

    EXPECT_TRUE(PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
    EXPECT_EQ(PoissonAllocationSampler::GetAccumulatedBytesForTesting(),
              accumulated_bytes);
  }

  // `accumulated_bytes` should be restored when no longer muted.
  EXPECT_FALSE(PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
  EXPECT_EQ(PoissonAllocationSampler::GetAccumulatedBytesForTesting(),
            accumulated_bytes_snapshot);
}

TEST_F(PoissonAllocationSamplerStateTest, UpdateProfilingState) {
  constexpr int kNumObserverThreads = 100;
  constexpr int kNumRepetitions = 100;

  base::RunLoop run_loop;

  // Quit the run loop once all AddRemoveObserversTesters and the
  // MuteUnmuteSamplesTester signal that they're done.
  auto barrier_closure =
      base::BarrierClosure(kNumObserverThreads + 1, run_loop.QuitClosure());

  // No observers or ScopedMuteHookedSamplesForTesting objects exist.
  // The kWasStarted flag may or may not be set depending on whether other tests
  // have changed the singleton state.
  ASSERT_FALSE(
      HasAnyStateFlags(ProfilingStateFlag::kIsRunning |
                       ProfilingStateFlag::kHookedSamplesMutedForTesting));

  std::array<std::unique_ptr<AddRemoveObserversTester>, kNumObserverThreads>
      observer_testers;
  for (int i = 0; i < kNumObserverThreads; ++i) {
    // Start a thread to add and remove observers, toggling the kIsRunning and
    // kHookedSamplesMutedForTesting state flags.
    observer_testers[i] = std::make_unique<AddRemoveObserversTester>();
    observer_testers[i]->Start(&barrier_closure, kNumRepetitions);
  }

  // There can only be one ScopedMuteHookedSamplesForTesting object at a time so
  // test them on only one thread.
  MuteHookedSamplesTester mute_samples_tester;
  mute_samples_tester.Start(&barrier_closure, kNumRepetitions);

  run_loop.Run();

  // No observers or ScopedMuteHookedSamplesForTesting objects exist again.
  EXPECT_FALSE(
      HasAnyStateFlags(ProfilingStateFlag::kIsRunning |
                       ProfilingStateFlag::kHookedSamplesMutedForTesting));

  // Move the observers into `observers_to_delete_` to be destroyed during
  // teardown, in single-threaded context.
  for (int i = 0; i < kNumObserverThreads; ++i) {
    base::Extend(observers_to_delete_, observer_testers[i]->DetachObservers());
  }
}

class PoissonAllocationSamplerLoadFactorTest
    : public ::testing::TestWithParam<float> {};

INSTANTIATE_TEST_SUITE_P(All,
                         PoissonAllocationSamplerLoadFactorTest,
                         ::testing::Values(0, 0.5, 1.5));

// TODO(crbug.com/383374205): This test flakily crashes on iOS without leaving
// any logs.
#if BUILDFLAG(IS_IOS)
#define MAYBE_BalanceSampledAddressesSet DISABLED_BalanceSampledAddressesSet
#else
#define MAYBE_BalanceSampledAddressesSet BalanceSampledAddressesSet
#endif

TEST_P(PoissonAllocationSamplerLoadFactorTest,
       MAYBE_BalanceSampledAddressesSet) {
  PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting mute_hooks;
  PoissonAllocationSampler::ScopedSuppressRandomnessForTesting
      suppress_randomness;

  auto* sampler = PoissonAllocationSampler::Get();

  // Validate the starting state of the hash set.
  size_t starting_buckets;
  float last_load_factor;
  {
    base::AutoLock lock(sampler->mutex_);
    starting_buckets = sampler->sampled_addresses_set().buckets_count();
    EXPECT_EQ(sampler->sampled_addresses_set().size(), 0);
    last_load_factor = sampler->sampled_addresses_set().load_factor();
    EXPECT_EQ(last_load_factor, 0.0);
  }

  size_t target_load_factor = GetParam();
  if (target_load_factor == 0) {
    // Use the default load factor.
    target_load_factor = 1.0;
  } else {
    sampler->SetTargetHashSetLoadFactor(target_load_factor);
  }

  // Adding an observer starts the profiler.
  DummySamplesObserver observer;
  sampler->AddSamplesObserver(&observer);

  absl::Cleanup reset_singleton_state = [&] {
    sampler->RemoveSamplesObserver(&observer);
    sampler->SetTargetHashSetLoadFactor(std::nullopt);
  };

  // Helper to dump the hash set state to the test output.
  auto dump_hash_set_state = [sampler] {
    base::AutoLock lock(sampler->mutex_);
    return ::testing::Message()
           << "hash set size " << sampler->sampled_addresses_set().size()
           << ", bucket count "
           << sampler->sampled_addresses_set().buckets_count()
           << ", load factor "
           << sampler->sampled_addresses_set().load_factor();
  };

  // Allocations that keep the load factor below target should not rebalance.
  const size_t target_allocations = starting_buckets * target_load_factor;
  SCOPED_TRACE(::testing::Message()
               << "target_allocations " << target_allocations);
  for (uintptr_t i = 1; i < target_allocations; ++i) {
    SCOPED_TRACE(::testing::Message() << "allocation " << i);
    sampler->OnAllocation(AllocationNotificationData(
        reinterpret_cast<void*>(i), sampler->SamplingInterval(), "dummy",
        AllocationSubsystem::kManualForTesting));

    SCOPED_TRACE(dump_hash_set_state());
    base::AutoLock lock(sampler->mutex_);
    EXPECT_EQ(sampler->sampled_addresses_set().size(), i);
    EXPECT_EQ(sampler->sampled_addresses_set().buckets_count(),
              starting_buckets);

    float load_factor = sampler->sampled_addresses_set().load_factor();
    EXPECT_GT(load_factor, last_load_factor);
    EXPECT_LT(load_factor, target_load_factor);
    last_load_factor = load_factor;
  }

  // Next allocation should rebalance: number of buckets goes up, load factor
  // goes down.
  sampler->OnAllocation(AllocationNotificationData(
      reinterpret_cast<void*>(target_allocations), sampler->SamplingInterval(),
      "dummy", AllocationSubsystem::kManualForTesting));

  size_t rebalanced_buckets;
  {
    SCOPED_TRACE(dump_hash_set_state());
    base::AutoLock lock(sampler->mutex_);
    EXPECT_EQ(sampler->sampled_addresses_set().size(), target_allocations);
    rebalanced_buckets = sampler->sampled_addresses_set().buckets_count();
    EXPECT_GT(rebalanced_buckets, starting_buckets);

    float load_factor = sampler->sampled_addresses_set().load_factor();
    EXPECT_LT(load_factor, last_load_factor);
    last_load_factor = load_factor;
  }

  // Deallocating should not rebalance.
  for (uintptr_t i = target_allocations; i > 0; --i) {
    SCOPED_TRACE(::testing::Message() << "deallocation " << i);
    sampler->OnFree(FreeNotificationData(
        reinterpret_cast<void*>(i), AllocationSubsystem::kManualForTesting));

    SCOPED_TRACE(dump_hash_set_state());
    base::AutoLock lock(sampler->mutex_);
    EXPECT_EQ(sampler->sampled_addresses_set().size(), i - 1);
    EXPECT_EQ(sampler->sampled_addresses_set().buckets_count(),
              rebalanced_buckets);

    float load_factor = sampler->sampled_addresses_set().load_factor();
    EXPECT_LT(load_factor, last_load_factor);
    last_load_factor = load_factor;
  }
}

}  // namespace base
