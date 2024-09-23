// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_state_sampler.h"

#include <queue>
#include <utility>

#include "base/power_monitor/power_monitor_buildflags.h"
#include "base/power_monitor/sampling_event_source.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class TestBatteryLevelProvider : public BatteryLevelProvider {
 public:
  TestBatteryLevelProvider() = default;
  ~TestBatteryLevelProvider() override = default;

  void GetBatteryState(OnceCallback<void(const std::optional<BatteryState>&)>
                           callback) override {
    DCHECK(!battery_states_.empty());

    auto next_battery_state = std::move(battery_states_.front());
    battery_states_.pop();

    std::move(callback).Run(next_battery_state);
  }

  void PushBatteryState(const std::optional<BatteryState>& battery_state) {
    battery_states_.push(battery_state);
  }

 protected:
  std::queue<std::optional<BatteryState>> battery_states_;
};

class TestBatteryLevelProviderAsync : public TestBatteryLevelProvider {
 public:
  TestBatteryLevelProviderAsync() = default;
  ~TestBatteryLevelProviderAsync() override = default;

  void GetBatteryState(OnceCallback<void(const std::optional<BatteryState>&)>
                           callback) override {
    DCHECK(!battery_states_.empty());

    auto next_battery_state = std::move(battery_states_.front());
    battery_states_.pop();

    SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        BindLambdaForTesting([callback = std::move(callback),
                              battery_state = next_battery_state]() mutable {
          std::move(callback).Run(battery_state);
        }));
  }
};

// A test observer that exposes the last battery state received.
class TestObserver : public BatteryStateSampler::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnBatteryStateSampled(
      const std::optional<BatteryLevelProvider::BatteryState>& battery_state)
      override {
    battery_state_ = battery_state;
  }

  std::optional<BatteryLevelProvider::BatteryState> battery_state() const {
    return battery_state_;
  }

 private:
  std::optional<BatteryLevelProvider::BatteryState> battery_state_;
};

// Those test battery states just need to be different. The actual values don't
// matter.
const std::optional<BatteryLevelProvider::BatteryState> kTestBatteryState1 =
    BatteryLevelProvider::BatteryState{
        .battery_count = 1,
        .is_external_power_connected = false,
        .current_capacity = 10000,
        .full_charged_capacity = 10000,
        .charge_unit = BatteryLevelProvider::BatteryLevelUnit::kMWh};
const std::optional<BatteryLevelProvider::BatteryState> kTestBatteryState2 =
    BatteryLevelProvider::BatteryState{
        .battery_count = 1,
        .is_external_power_connected = false,
        .current_capacity = 5000,
        .full_charged_capacity = 10000,
        .charge_unit = BatteryLevelProvider::BatteryLevelUnit::kMWh};
const std::optional<BatteryLevelProvider::BatteryState> kTestBatteryState3 =
    BatteryLevelProvider::BatteryState{
        .battery_count = 1,
        .is_external_power_connected = true,
        .current_capacity = 2000,
        .full_charged_capacity = 10000,
        .charge_unit = BatteryLevelProvider::BatteryLevelUnit::kMWh};

bool operator==(const BatteryLevelProvider::BatteryState& lhs,
                const BatteryLevelProvider::BatteryState& rhs) {
  return std::tie(lhs.battery_count, lhs.is_external_power_connected,
                  lhs.current_capacity, lhs.full_charged_capacity,
                  lhs.charge_unit) ==
         std::tie(rhs.battery_count, rhs.is_external_power_connected,
                  rhs.current_capacity, rhs.full_charged_capacity,
                  rhs.charge_unit);
}

bool operator!=(const BatteryLevelProvider::BatteryState& lhs,
                const BatteryLevelProvider::BatteryState& rhs) {
  return !(lhs == rhs);
}

TEST(BatteryStateSamplerTest, GlobalInstance) {
#if BUILDFLAG(HAS_BATTERY_LEVEL_PROVIDER_IMPL) || BUILDFLAG(IS_CHROMEOS_ASH)
  // Get() DCHECKs on platforms with a battery level provider if it's called
  // without being initialized. ChromeOS behaves the same because it has a
  // `BatteryLevelProvider`, but it doesn't live in base so it doesn't exist in
  // this test.
  EXPECT_DCHECK_DEATH(BatteryStateSampler::Get());
#else
  // Get() returns null if the sampler doesn't exist on platforms without a
  // battery level provider.
  EXPECT_FALSE(BatteryStateSampler::Get());
#endif

  auto battery_level_provider = std::make_unique<TestBatteryLevelProvider>();
  battery_level_provider->PushBatteryState(kTestBatteryState1);

  // Create the sampler.
  auto battery_state_sampler = std::make_unique<BatteryStateSampler>(
      std::make_unique<test::TestSamplingEventSource>(),
      std::move(battery_level_provider));

  // Now the getter works.
  EXPECT_TRUE(BatteryStateSampler::Get());

  // Can't create a second sampler.
  EXPECT_DCHECK_DEATH({
    BatteryStateSampler another_battery_state_sampler(
        std::make_unique<test::TestSamplingEventSource>(),
        std::make_unique<TestBatteryLevelProvider>());
  });

  battery_state_sampler.reset();

  // The sampler no longer exists.
#if BUILDFLAG(HAS_BATTERY_LEVEL_PROVIDER_IMPL) || BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_DCHECK_DEATH(BatteryStateSampler::Get());
#else
  EXPECT_FALSE(BatteryStateSampler::Get());
#endif
}

TEST(BatteryStateSamplerTest, InitialSample) {
  auto sampling_event_source =
      std::make_unique<test::TestSamplingEventSource>();

  auto battery_level_provider = std::make_unique<TestBatteryLevelProvider>();
  // Push the initial battery state that will be queried by the sampler.
  battery_level_provider->PushBatteryState(kTestBatteryState1);

  BatteryStateSampler battery_state_sampler(std::move(sampling_event_source),
                                            std::move(battery_level_provider));

  TestObserver observer;
  battery_state_sampler.AddObserver(&observer);

  EXPECT_EQ(observer.battery_state(), kTestBatteryState1);
}

TEST(BatteryStateSamplerTest, MultipleSamples) {
  auto battery_level_provider = std::make_unique<TestBatteryLevelProvider>();
  // Push the initial battery state and the first 3 samples after it.
  battery_level_provider->PushBatteryState(kTestBatteryState1);
  battery_level_provider->PushBatteryState(kTestBatteryState2);
  battery_level_provider->PushBatteryState(kTestBatteryState3);
  battery_level_provider->PushBatteryState(kTestBatteryState1);

  auto sampling_event_source =
      std::make_unique<test::TestSamplingEventSource>();
  auto* sampling_event_source_ptr = sampling_event_source.get();

  BatteryStateSampler battery_state_sampler(std::move(sampling_event_source),
                                            std::move(battery_level_provider));

  TestObserver observer;
  battery_state_sampler.AddObserver(&observer);
  EXPECT_EQ(observer.battery_state(), kTestBatteryState1);

  // Simulate 2 events to trigger 2 more samples.
  sampling_event_source_ptr->SimulateEvent();
  EXPECT_EQ(observer.battery_state(), kTestBatteryState2);
  sampling_event_source_ptr->SimulateEvent();
  EXPECT_EQ(observer.battery_state(), kTestBatteryState3);

  battery_state_sampler.RemoveObserver(&observer);

  // Simulate 1 event and ensure the observer does not receive the new state.
  sampling_event_source_ptr->SimulateEvent();
  EXPECT_NE(observer.battery_state(), kTestBatteryState1);
}

TEST(BatteryStateSamplerTest, MultipleObservers) {
  auto battery_level_provider = std::make_unique<TestBatteryLevelProvider>();
  // Push the initial battery state and the first sample after it.
  battery_level_provider->PushBatteryState(kTestBatteryState1);
  battery_level_provider->PushBatteryState(kTestBatteryState2);

  auto sampling_event_source =
      std::make_unique<test::TestSamplingEventSource>();
  auto* sampling_event_source_ptr = sampling_event_source.get();

  BatteryStateSampler battery_state_sampler(std::move(sampling_event_source),
                                            std::move(battery_level_provider));

  // Add 2 observers.
  TestObserver observer1;
  battery_state_sampler.AddObserver(&observer1);
  EXPECT_EQ(observer1.battery_state(), kTestBatteryState1);
  TestObserver observer2;
  battery_state_sampler.AddObserver(&observer2);
  EXPECT_EQ(observer2.battery_state(), kTestBatteryState1);

  // Simulate an event to get another sample.
  sampling_event_source_ptr->SimulateEvent();
  EXPECT_EQ(observer1.battery_state(), kTestBatteryState2);
  EXPECT_EQ(observer2.battery_state(), kTestBatteryState2);
}

// Tests that if sampling the battery state is asynchronous (like it is on
// Windows), the sampler will correctly notify new observers when the first
// sample arrives.
TEST(BatteryStateSamplerTest, InitialSample_Async) {
  test::SingleThreadTaskEnvironment task_environment;

  auto battery_level_provider =
      std::make_unique<TestBatteryLevelProviderAsync>();
  // Push the initial battery state.
  battery_level_provider->PushBatteryState(kTestBatteryState1);

  auto sampling_event_source =
      std::make_unique<test::TestSamplingEventSource>();

  // Creating the sampler starts the first async sample.
  BatteryStateSampler battery_state_sampler(std::move(sampling_event_source),
                                            std::move(battery_level_provider));

  TestObserver observer;
  battery_state_sampler.AddObserver(&observer);
  EXPECT_EQ(observer.battery_state(), std::nullopt);

  RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.battery_state(), kTestBatteryState1);
}

}  // namespace base
