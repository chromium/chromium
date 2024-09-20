// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feedback/system_logs/log_sources/performance_log_source.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"

class QuitRunLoopOnPowerStateChangeObserver
    : public performance_manager::user_tuning::BatterySaverModeManager::
          Observer {
 public:
  explicit QuitRunLoopOnPowerStateChangeObserver(
      base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}

  ~QuitRunLoopOnPowerStateChangeObserver() override = default;

  void OnExternalPowerConnectedChanged(bool) override { quit_closure_.Run(); }

 private:
  base::RepeatingClosure quit_closure_;
};

namespace {

constexpr char kMemorySaverModeActiveKey[] = "high_efficiency_mode_active";

#if !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kBatterySaverModeStateKey[] = "battery_saver_state";
constexpr char kBatterySaverModeActiveKey[] = "battery_saver_mode_active";
constexpr char kBatterySaverModeDisabledForSessionKey[] =
    "battery_saver_disabled_for_session";
constexpr char kHasBatteryKey[] = "device_has_battery";
constexpr char kUsingBatteryPowerKey[] = "device_using_battery_power";
constexpr char kBatteryPercentage[] = "device_battery_percentage";
#endif

class PerformanceLogSourceTest : public BrowserWithTestWindowTest {
 public:
  PerformanceLogSourceTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    local_state_ = testing_local_state_.Get();
  }

  PerformanceLogSourceTest(const PerformanceLogSourceTest&) = delete;
  PerformanceLogSourceTest& operator=(const PerformanceLogSourceTest&) = delete;

  ~PerformanceLogSourceTest() override = default;

  void SetUp() override { environment_.SetUp(local_state_); }

  void TearDown() override {
    base::PowerMonitor::GetInstance()->ShutdownForTesting();
    environment_.TearDown();
  }

  std::unique_ptr<system_logs::SystemLogsResponse> GetPerformanceLogs() {
    base::RunLoop run_loop;
    system_logs::PerformanceLogSource source;
    std::unique_ptr<system_logs::SystemLogsResponse> response;
    source.Fetch(base::BindLambdaForTesting(
        [&](std::unique_ptr<system_logs::SystemLogsResponse> r) {
          response = std::move(r);
          run_loop.Quit();
        }));
    run_loop.Run();
    return response;
  }

  void SetBatterySaverModeEnabled(bool enabled) {
    auto mode = enabled ? performance_manager::user_tuning::prefs::
                              BatterySaverModeState::kEnabled
                        : performance_manager::user_tuning::prefs::
                              BatterySaverModeState::kDisabled;
    local_state_->SetInteger(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        static_cast<int>(mode));
  }

  void SetMemorySaverModeEnabled(bool enabled) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetMemorySaverModeEnabled(enabled);
  }

  void SetOnBatteryPower(bool on_battery_power) {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
        ->AddObserver(observer.get());
    environment_.power_monitor_source()->SetBatteryPowerStatus(
        on_battery_power
            ? base::PowerStateObserver::BatteryPowerStatus::kBatteryPower
            : base::PowerStateObserver::BatteryPowerStatus::kExternalPower);
    run_loop.Run();
    performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
        ->RemoveObserver(observer.get());
  }

  ScopedTestingLocalState testing_local_state_;
  performance_manager::user_tuning::TestUserPerformanceTuningManagerEnvironment
      environment_;
  raw_ptr<TestingPrefServiceSimple> local_state_ = nullptr;
};

TEST_F(PerformanceLogSourceTest, CheckMemorySaverModeLogs) {
  SetMemorySaverModeEnabled(true);
  auto response = GetPerformanceLogs();
  EXPECT_EQ("true", response->at(kMemorySaverModeActiveKey));

  SetMemorySaverModeEnabled(false);
  response = GetPerformanceLogs();
  EXPECT_EQ("false", response->at(kMemorySaverModeActiveKey));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Battery and battery saver logs are not used on ChromeOS.
TEST_F(PerformanceLogSourceTest, CheckBatterySaverModeLogs) {
  SetBatterySaverModeEnabled(true);
  auto response = GetPerformanceLogs();
  EXPECT_EQ("enabled", response->at(kBatterySaverModeStateKey));
  EXPECT_EQ("true", response->at(kBatterySaverModeActiveKey));
  EXPECT_EQ("false", response->at(kBatterySaverModeDisabledForSessionKey));

  SetBatterySaverModeEnabled(false);
  response = GetPerformanceLogs();
  EXPECT_EQ("disabled", response->at(kBatterySaverModeStateKey));
  EXPECT_EQ("false", response->at(kBatterySaverModeActiveKey));
  EXPECT_EQ("false", response->at(kBatterySaverModeDisabledForSessionKey));
}

TEST_F(PerformanceLogSourceTest, CheckBatteryDetailLogs) {
  base::test::TestBatteryLevelProvider* battery_level_provider =
      environment_.battery_level_provider();
  base::test::TestSamplingEventSource* sampling_source =
      environment_.sampling_source();

  battery_level_provider->SetBatteryState(
      base::test::TestBatteryLevelProvider::CreateBatteryState(1, false, 25));
  sampling_source->SimulateEvent();
  SetOnBatteryPower(true);

  auto response = GetPerformanceLogs();
  EXPECT_EQ("true", response->at(kHasBatteryKey));
  EXPECT_EQ("true", response->at(kUsingBatteryPowerKey));
  EXPECT_EQ("25", response->at(kBatteryPercentage));

  battery_level_provider->SetBatteryState(
      base::test::TestBatteryLevelProvider::CreateBatteryState(0, true, 100));
  sampling_source->SimulateEvent();
  SetOnBatteryPower(false);

  response = GetPerformanceLogs();
  EXPECT_EQ("false", response->at(kHasBatteryKey));
  EXPECT_EQ("false", response->at(kUsingBatteryPowerKey));
  EXPECT_EQ("100", response->at(kBatteryPercentage));
}
#endif

}  // namespace
