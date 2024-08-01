// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_

#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"

class PrefService;

namespace base {
class BatteryStateSampler;
class SamplingEventSource;
class BatteryLevelProvider;

namespace test {
class TestSamplingEventSource;
class TestBatteryLevelProvider;
}  // namespace test
}  // namespace base

namespace performance_manager::user_tuning {

class TestUserPerformanceTuningManagerEnvironment {
 public:
  TestUserPerformanceTuningManagerEnvironment();
  ~TestUserPerformanceTuningManagerEnvironment();

  void SetUp(PrefService* local_state);

  // SetUp, but provide your own SamplingEventSource and BatteryLevelProvider.
  // If sampling_event_source is not nullptr, then sampling_source() may not be
  // called.
  // If battery_level_provider is not nullptr, then battery_level_provider() may
  // not be called.
  void SetUp(
      PrefService* local_state,
      std::unique_ptr<base::SamplingEventSource> sampling_event_source,
      std::unique_ptr<base::BatteryLevelProvider> battery_level_provider);

  void TearDown();

  static void SetBatterySaverMode(PrefService* local_state, bool enabled);

  base::test::TestSamplingEventSource* sampling_source();
  base::test::TestBatteryLevelProvider* battery_level_provider();
  base::BatteryStateSampler* battery_state_sampler();

  FakePowerMonitorSource* power_monitor_source();

 private:
  raw_ptr<FakePowerMonitorSource, DanglingUntriaged> power_monitor_source_;
  raw_ptr<base::test::TestSamplingEventSource> sampling_source_;
  raw_ptr<base::test::TestBatteryLevelProvider> battery_level_provider_;
  std::unique_ptr<base::BatteryStateSampler> battery_sampler_;

  // Some tests combine this helper with other helpers that also initialize
  // FakePowerManagerClient. E.g. BrowserWithTestWindowTest tests. True if we
  // called chromeos::PowerManagerClient::InitializeFake, because we are then
  // responsible for cleanup.
  bool tear_down_power_manager_client_ = false;

  bool throttling_enabled_ = false;
  bool child_process_tuning_enabled_ = false;
  std::unique_ptr<UserPerformanceTuningManager>
      user_performance_tuning_manager_;
  std::unique_ptr<BatterySaverModeManager> battery_saver_mode_manager_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_
