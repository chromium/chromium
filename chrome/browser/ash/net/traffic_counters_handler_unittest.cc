// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/net/traffic_counters_handler.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/services/network_config/cros_network_config.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace traffic_counters {

namespace {

class TrafficCountersHandlerTest : public ::testing::Test {
 public:
  TrafficCountersHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    LoginState::Initialize();
    helper_ = std::make_unique<NetworkHandlerTestHelper>();
    helper_->AddDefaultProfiles();
    helper_->ResetDevicesAndServices();
    helper_->RegisterPrefs(user_prefs_.registry(), local_state_.registry());

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    helper_->InitializePrefs(&user_prefs_, &local_state_);

    NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::Value::List(),
        /*global_network_config=*/base::Value::Dict());

    cros_network_config_ =
        std::make_unique<network_config::CrosNetworkConfig>();
    network_config::OverrideInProcessInstanceForTesting(
        cros_network_config_.get());
    task_environment_.RunUntilIdle();

    helper_->service_test()->SetTimeGetterForTest(base::BindRepeating(
        [](base::test::TaskEnvironment* env) {
          return env->GetMockClock()->Now();
        },
        &task_environment_));

    traffic_counters_handler_ = std::make_unique<TrafficCountersHandler>();
    traffic_counters_handler_->SetTimeGetterForTest(base::BindRepeating(
        [](base::test::TaskEnvironment* env) {
          return env->GetMockClock()->Now();
        },
        &task_environment_));

    SetUpWiFi();
  }

  ~TrafficCountersHandlerTest() override {
    traffic_counters_handler_.reset();
    cros_network_config_.reset();
    helper_.reset();
    LoginState::Shutdown();
  }

  base::Time GetTime() { return task_environment_.GetMockClock()->Now(); }

 protected:
  base::Time SetLastResetTimeAndRun(const std::string& time_str) {
    base::Time reset_time = GetTimeFromString(time_str);
    double last_reset_time_ms =
        reset_time.ToDeltaSinceWindowsEpoch().InMilliseconds();
    SetServiceProperty(wifi_path_, shill::kTrafficCounterResetTimeProperty,
                       base::Value(last_reset_time_ms));
    task_environment_.RunUntilIdle();
    RunTrafficCountersHandler();
    return reset_time;
  }

  base::Time GetLastResetTime() {
    double traffic_counter_reset_time_ms =
        helper_
            ->GetServiceDoubleProperty(wifi_path_,
                                       shill::kTrafficCounterResetTimeProperty)
            .value();
    return base::Time::FromDeltaSinceWindowsEpoch(
        base::Milliseconds(traffic_counter_reset_time_ms));
  }

  void SetAutoResetAndDay(bool auto_reset, int user_specified_day) {
    NetworkHandler::Get()
        ->network_metadata_store()
        ->SetEnableTrafficCountersAutoReset(wifi_guid_, auto_reset);
    NetworkHandler::Get()
        ->network_metadata_store()
        ->SetDayOfTrafficCountersAutoReset(wifi_guid_, user_specified_day);
    task_environment_.RunUntilIdle();
  }

  base::Time AdvanceClockTo(const std::string& time_str) {
    base::Time time = GetTimeFromString(time_str);
    task_environment_.AdvanceClock(
        time.ToDeltaSinceWindowsEpoch() -
        task_environment_.GetMockClock()->Now().ToDeltaSinceWindowsEpoch());
    task_environment_.RunUntilIdle();
    return task_environment_.GetMockClock()->Now();
  }

  void RunTrafficCountersHandler() {
    traffic_counters_handler_->RunForTesting();
    task_environment_.RunUntilIdle();
  }

  void FastForwardBy(const base::TimeDelta& days) {
    task_environment_.FastForwardBy(days);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TrafficCountersHandler* traffic_counters_handler() {
    return traffic_counters_handler_.get();
  }

  const std::string& wifi_guid() const { return wifi_guid_; }

 private:
  void SetUpWiFi() {
    ASSERT_TRUE(wifi_path_.empty());
    // By default, NetworkStateTestHelper already adds a WiFi device, so, we
    // do not need to add one here. All that remains to be done is configuring
    // the WiFi service.
    wifi_guid_ = "wifi_guid";
    wifi_path_ = helper_->ConfigureService(
        R"({"GUID": "wifi_guid", "Type": "wifi", "State": "idle",
            "SSID": "wifi", "Strength": 100, "AutoConnect": true,
            "WiFi.HiddenSSID": false,
            "TrafficCounterResetTime": 0})");
    SetServiceProperty(wifi_path_, shill::kStateProperty,
                       base::Value(shill::kStateOnline));
    helper_->profile_test()->AddService(
        NetworkProfileHandler::GetSharedProfilePath(), wifi_path_);
    NetworkHandler::Get()
        ->network_metadata_store()
        ->SetEnableTrafficCountersAutoReset(wifi_guid_, false);
    task_environment_.RunUntilIdle();
  }

  base::Time GetTimeFromString(const std::string& time_str) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString(time_str.c_str(), &time));
    return time;
  }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    helper_->SetServiceProperty(service_path, key, value);
  }

  // Member order declaration done in a way so that members outlive those that
  // are dependent on them.
  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  std::unique_ptr<NetworkHandlerTestHelper> helper_;
  std::unique_ptr<network_config::CrosNetworkConfig> cros_network_config_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  std::string wifi_path_;
  std::string wifi_guid_;
  std::unique_ptr<TrafficCountersHandler> traffic_counters_handler_;
};

}  // namespace

TEST_F(TrafficCountersHandlerTest, GetLastResetTime) {
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), base::Time());

  base::Time reset_time =
      SetLastResetTimeAndRun("Fri, 15 December 2023 10:00:00 UTC");
  EXPECT_EQ(GetLastResetTime(), reset_time);
}

TEST_F(TrafficCountersHandlerTest, SetMetadata) {
  RunTrafficCountersHandler();
  const base::Value* enabled =
      NetworkHandler::Get()
          ->network_metadata_store()
          ->GetEnableTrafficCountersAutoReset(wifi_guid());
  ASSERT_TRUE(enabled && enabled->is_bool());
  EXPECT_FALSE(enabled->GetBool());

  NetworkHandler::Get()
      ->network_metadata_store()
      ->SetEnableTrafficCountersAutoReset(wifi_guid(), true);
  RunUntilIdle();
  enabled = NetworkHandler::Get()
                ->network_metadata_store()
                ->GetEnableTrafficCountersAutoReset(wifi_guid());
  ASSERT_TRUE(enabled && enabled->is_bool());
  EXPECT_TRUE(enabled->GetBool());

  NetworkHandler::Get()
      ->network_metadata_store()
      ->SetDayOfTrafficCountersAutoReset(wifi_guid(), 13);
  RunUntilIdle();
  const base::Value* reset_day_value =
      NetworkHandler::Get()
          ->network_metadata_store()
          ->GetDayOfTrafficCountersAutoReset(wifi_guid());
  ASSERT_TRUE(reset_day_value && reset_day_value->is_int());
  EXPECT_EQ(reset_day_value->GetInt(), 13);
}

TEST_F(TrafficCountersHandlerTest, AutoReset) {
  base::Time reset_time =
      SetLastResetTimeAndRun("Fri, 15 December 2023 10:00:00 UTC");
  EXPECT_EQ(GetLastResetTime(), reset_time);

  SetAutoResetAndDay(true, 1);

  // Advance the clock to Jan 1st, 2024. The counters should get reset.
  base::Time simulated_time1 =
      AdvanceClockTo("Mon, 1 January 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time1);

  // Fast forwarding the date by 15 days to Jan 16th should not affect the
  // last reset time.
  FastForwardBy(base::Days(15));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time1);

  // Setting AutoReset to false should prevent auto reset.
  SetAutoResetAndDay(false, 1);
  base::Time simulated_time2 =
      AdvanceClockTo("Fri, 15 March 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time1);

  // Setting AutoReset back to true should auto reset.
  SetAutoResetAndDay(true, 1);
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time2);

  // Fast forwarding the date by 40 days should auto reset.
  FastForwardBy(base::Days(40));
  RunTrafficCountersHandler();
  EXPECT_GT(GetLastResetTime(), simulated_time2);
}

TEST_F(TrafficCountersHandlerTest, AutoResetEndOfMonth) {
  SetLastResetTimeAndRun("Fri, 1 March 2024 07:01:00 UTC");
  SetAutoResetAndDay(true, 15);

  // Adjust the user specified day to the 15th and ensure that a reset occurs
  // on March 15th.
  base::Time simulated_time = AdvanceClockTo("Fri, 15 March 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Adjust the user specified day to the 14th and fast forward by 1 day to Mar
  // 16th. Ensure that no reset occurs on March 16th.
  SetAutoResetAndDay(true, 14);
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
  FastForwardBy(base::Days(1));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Adjust the user specified day to the 31st and Fast forward to Mar 31st.
  // Ensure that a reset occurs.
  SetAutoResetAndDay(true, 31);
  simulated_time = AdvanceClockTo("Sun, 31 March 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Advance the clock to Apr 30th and ensure auto reset occurs on April
  // 30th since April 31st does not exist.
  simulated_time = AdvanceClockTo("Tue, 30 Apr 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
  // Fast forward by 1 day to May 1st and ensure no auto reset occurs.
  FastForwardBy(base::Days(1));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Advance the clock to May 31st and ensure reset occurs.
  simulated_time = AdvanceClockTo("Fri, 31 May 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, AutoResetLeapYear) {
  SetLastResetTimeAndRun("Fri, 15 December 2023 10:00:00 UTC");
  SetAutoResetAndDay(true, 1);

  base::Time simulated_time =
      AdvanceClockTo("Mon, 1 January 2024 07:01:00 UTC");
  RunTrafficCountersHandler();

  // The first traffic counters reset is expected on Jan 1st.
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Fast forwarding the date by 15 days to Jan 16th should not affect the
  // last reset time.
  FastForwardBy(base::Days(15));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Fast forwarding the date to Feb 1st should update the last reset time.
  simulated_time = AdvanceClockTo("Thu, 1 February 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date by 28 days to Feb 29th, ensure February
  // 29th on leap years does not reset traffic counters when the user specified
  // day is the 1st.
  FastForwardBy(base::Days(28));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date to Mar 1st, ensure that two auto
  // traffic counter resets do not occur on the same day.
  // First, ensure traffic counters are reset on March 1st.
  simulated_time = AdvanceClockTo("Fri, 1 March 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
  // Second, run TrafficCountersHandler later on Mar 1st to ensure
  // that another reset doesn't occur.
  FastForwardBy(base::Hours(12));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, NoLeapYear) {
  SetLastResetTimeAndRun("Sat, 15 January 2023 10:00:00 UTC");
  SetAutoResetAndDay(true, 31);

  // Adjust the clock to set the "current time". Use Jan 31st, 2023
  // to test whether the functionality is correct on non-leap years.
  base::Time simulated_time =
      AdvanceClockTo("Tue, 31 January 2023 07:01:00 UTC");
  RunTrafficCountersHandler();

  // The first traffic counters reset is expected on Jan 31st.
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date to Feb 28th, ensure traffic
  // counters are reset on February 28th on non-leap years when the user
  // specified day is the 31st.
  simulated_time = AdvanceClockTo("Tue, 28 February 2023 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date by 1 day to Mar 1st, ensure traffic
  // counters are not reset again.
  FastForwardBy(base::Days(1));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date by 27 days to March 28th, ensure a
  // traffic counter reset has not occurred.
  FastForwardBy(base::Days(27));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding by 2 days to March 30th, ensure a traffic counter
  // reset has not occurred.
  FastForwardBy(base::Days(2));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date by 1 day to March 31st, ensure a traffic
  // counter reset occurs.
  simulated_time = AdvanceClockTo("Fri, 31 March 2023 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, ChangeUserSpecifiedDate) {
  SetAutoResetAndDay(true, 31);
  SetLastResetTimeAndRun("Sat, 15 January 2023 10:00:00 UTC");

  // Advancing the date to Jan 31st should reset the traffic counters.
  base::Time simulated_time =
      AdvanceClockTo("Tue, 31 January 2023 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Change user specified auto reset day.
  SetAutoResetAndDay(true, 5);
  RunTrafficCountersHandler();

  // After fast forwarding the date to February 5th, ensure traffic counters are
  // reset.
  simulated_time = AdvanceClockTo("Sun, 5 February 2023 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, AutoResetTimer) {
  // Start the traffic counters timer.
  traffic_counters_handler()->Start();

  base::Time reset_time =
      SetLastResetTimeAndRun("Fri, 15 December 2023 10:00:00 UTC");
  EXPECT_EQ(GetLastResetTime(), reset_time);

  SetAutoResetAndDay(true, 1);

  // Advance the clock to Jan 2nd, 2024. The timer should run and the counters
  // should get reset.
  base::Time simulated_time =
      AdvanceClockTo("Tue, 2 January 2024 07:01:00 UTC");
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

}  // namespace traffic_counters

}  // namespace ash
