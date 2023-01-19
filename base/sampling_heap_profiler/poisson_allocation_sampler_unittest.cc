// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"

#include <stdlib.h>

#include <array>
#include <atomic>
#include <bitset>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

class DummySamplesObserver : public PoissonAllocationSampler::SamplesObserver {
 public:
  void SampleAdded(void* address,
                   size_t size,
                   size_t total,
                   base::allocator::dispatcher::AllocationSubsystem type,
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

}  // namespace base
