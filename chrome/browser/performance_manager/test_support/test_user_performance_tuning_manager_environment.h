// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"

class PrefService;

namespace base {
class BatteryStateSampler;
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
  void TearDown();

  base::test::TestSamplingEventSource* sampling_source();
  base::test::TestBatteryLevelProvider* battery_level_provider();

  FakePowerMonitorSource* power_monitor_source();

 private:
  raw_ptr<FakePowerMonitorSource, DanglingUntriaged> power_monitor_source_;
  raw_ptr<base::test::TestSamplingEventSource> sampling_source_;
  raw_ptr<base::test::TestBatteryLevelProvider> battery_level_provider_;
  std::unique_ptr<base::BatteryStateSampler> battery_sampler_;

  bool throttling_enabled_ = false;
  std::unique_ptr<UserPerformanceTuningManager> manager_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_USER_PERFORMANCE_TUNING_MANAGER_ENVIRONMENT_H_
