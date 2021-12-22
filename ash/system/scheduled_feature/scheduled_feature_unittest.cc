// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "ash/system/time/time_of_day.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/pattern.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@featuredschedule";
constexpr char kUser2Email[] = "user2@featuredschedule";

class TestScheduledFeature : public ScheduledFeature {
 public:
  TestScheduledFeature(const std::string prefs_path_enabled,
                       const std::string prefs_path_schedule_type,
                       const std::string prefs_path_custom_start_time,
                       const std::string prefs_path_custom_end_time)
      : ScheduledFeature(prefs_path_enabled,
                         prefs_path_schedule_type,
                         prefs_path_custom_start_time,
                         prefs_path_custom_end_time) {}
  TestScheduledFeature(const TestScheduledFeature& other) = delete;
  TestScheduledFeature& operator=(const TestScheduledFeature& rhs) = delete;
  ~TestScheduledFeature() override {}

  const char* GetFeatureName() const override { return "TestFeature"; }
};

class ScheduledFeatureTest : public NoSessionAshTestBase {
 public:
  ScheduledFeatureTest() = default;
  ScheduledFeatureTest(const ScheduledFeatureTest& other) = delete;
  ScheduledFeatureTest& operator=(const ScheduledFeatureTest& rhs) = delete;
  ~ScheduledFeatureTest() override = default;

  PrefService* user1_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  PrefService* user2_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser2Email));
  }

  ScheduledFeature* feature() { return feature_.get(); }

  // AshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    CreateTestUserSessions();

    // Simulate user 1 login.
    SimulateNewUserFirstLogin(kUser1Email);

    geolocation_controller_ = std::make_unique<GeolocationController>(
        /*url_context_getter=*/nullptr);

    // Use user prefs of NightLight, which is an example of ScheduledFeature.
    feature_ = std::make_unique<TestScheduledFeature>(
        prefs::kNightLightEnabled, prefs::kNightLightScheduleType,
        prefs::kNightLightCustomStartTime, prefs::kNightLightCustomEndTime);

    feature_->SetClockForTesting(&test_clock_);
    feature_->OnActiveUserPrefServiceChanged(
        Shell::Get()->session_controller()->GetActivePrefService());
  }

  void TearDown() override {
    geolocation_controller_.reset();
    feature_.reset();
    NoSessionAshTestBase::TearDown();
  }

  void CreateTestUserSessions() {
    auto* session_controller_client = GetSessionControllerClient();
    session_controller_client->Reset();
    session_controller_client->AddUserSession(kUser1Email);
    session_controller_client->AddUserSession(kUser2Email);
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  bool GetEnabled() { return feature_->GetEnabled(); }
  ScheduledFeature::ScheduleType GetScheduleType() {
    return feature_->GetScheduleType();
  }

  void SetFeatureEnabled(bool enabled) { feature_->SetEnabled(enabled); }
  void SetScheduleType(ScheduledFeature::ScheduleType type) {
    feature_->SetScheduleType(type);
  }

 protected:
  std::unique_ptr<TestScheduledFeature> feature_;
  std::unique_ptr<GeolocationController> geolocation_controller_;
  base::SimpleTestClock test_clock_;
};

// Tests that switching users retrieves the feature settings for the active
// user's prefs.
TEST_F(ScheduledFeatureTest, UserSwitchAndSettingsPersistence) {
  // Start with user1 logged in and update to sunset-to-sunrise schedule type.
  const std::string kScheduleTypePrefString = prefs::kNightLightScheduleType;
  ScheduledFeature::ScheduleType user1_schedule_type =
      ScheduledFeature::ScheduleType::kSunsetToSunrise;
  feature()->SetScheduleType(user1_schedule_type);
  EXPECT_EQ(user1_schedule_type, GetScheduleType());
  EXPECT_EQ(user1_schedule_type,
            user1_pref_service()->GetInteger(kScheduleTypePrefString));

  // Switch to user 2, and set to custom schedule type.
  SwitchActiveUser(kUser2Email);

  ScheduledFeature::ScheduleType user2_schedule_type =
      ScheduledFeature::ScheduleType::kCustom;
  user2_pref_service()->SetInteger(kScheduleTypePrefString,
                                   user2_schedule_type);
  EXPECT_EQ(user2_schedule_type, GetScheduleType());
  EXPECT_EQ(user1_schedule_type,
            user1_pref_service()->GetInteger(kScheduleTypePrefString));

  // Switch back to user 1, to find feature schedule type is restored to
  // sunset-to-sunrise.
  SwitchActiveUser(kUser1Email);
  EXPECT_EQ(user1_schedule_type, GetScheduleType());
}

// Tests transitioning from kNone to kCustom and back to kNone schedule
// types.
TEST_F(ScheduledFeatureTest, ScheduleNoneToCustomTransition) {
  // Now is 6:00 PM.
  test_clock_.SetNow(TimeOfDay(18 * 60).ToTimeToday());
  SetFeatureEnabled(false);
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kNone);
  // Start time is at 3:00 PM and end time is at 8:00 PM.
  feature()->SetCustomStartTime(TimeOfDay(15 * 60));
  feature()->SetCustomEndTime(TimeOfDay(20 * 60));

  //      15:00         18:00         20:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //      start          now           end
  //
  // Even though "Now" is inside the feature interval, nothing should
  // change, since the schedule type is "none".
  EXPECT_FALSE(GetEnabled());

  // Now change the schedule type to custom, the feature should turn on
  // immediately, and the timer should be running with a delay of exactly 2
  // hours scheduling the end.
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_TRUE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(2), feature()->timer()->GetCurrentDelay());

  // If the user changes the schedule type to "none", the feature status
  // should not change, but the timer should not be running.
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kNone);
  EXPECT_TRUE(GetEnabled());
  EXPECT_FALSE(feature()->timer()->IsRunning());
}

// Tests what happens when the time now reaches the end of the feature
// interval when the feature mode is on.
TEST_F(ScheduledFeatureTest, TestCustomScheduleReachingEndTime) {
  test_clock_.SetNow(TimeOfDay(18 * 60).ToTimeToday());
  feature()->SetCustomStartTime(TimeOfDay(15 * 60));
  feature()->SetCustomEndTime(TimeOfDay(20 * 60));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_TRUE(GetEnabled());

  // Simulate reaching the end time by triggering the timer's user task. Make
  // sure that the feature ended.
  //
  //      15:00                      20:00
  // <----- + ------------------------ + ----->
  //        |                          |
  //      start                    end & now
  //
  // Now is 8:00 PM.
  test_clock_.SetNow(TimeOfDay(20 * 60).ToTimeToday());
  feature()->timer()->FireNow();
  EXPECT_FALSE(GetEnabled());
  // The timer should still be running, but now scheduling the start at 3:00 PM
  // tomorrow which is 19 hours from "now" (8:00 PM).
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(19), feature()->timer()->GetCurrentDelay());
}

// Tests that user toggles from the system menu or system settings override any
// status set by an automatic schedule.
TEST_F(ScheduledFeatureTest, ExplicitUserTogglesWhileScheduleIsActive) {
  // Start with the below custom schedule, where the feature is off.
  //
  //      15:00               20:00          23:00
  // <----- + ----------------- + ------------ + ---->
  //        |                   |              |
  //      start                end            now
  //
  test_clock_.SetNow(TimeOfDay(23 * 60).ToTimeToday());
  feature()->SetCustomStartTime(TimeOfDay(15 * 60));
  feature()->SetCustomEndTime(TimeOfDay(20 * 60));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_FALSE(GetEnabled());

  // What happens if the user manually turns the feature on while the schedule
  // type says it should be off?
  // User toggles either from the system menu or the System Settings toggle
  // button must override any automatic schedule.
  SetFeatureEnabled(true);
  EXPECT_TRUE(GetEnabled());
  // The timer should still be running, but the feature should automatically
  // turn off at 8:00 PM tomorrow, which is 21 hours from now (11:00 PM).
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(21), feature()->timer()->GetCurrentDelay());

  // Manually turning it back off should also be respected, and this time the
  // start is scheduled at 3:00 PM tomorrow after 19 hours from "now" (8:00
  // PM).
  SetFeatureEnabled(false);
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(16), feature()->timer()->GetCurrentDelay());
}

// Tests that changing the custom start and end times, in such a way that
// shouldn't change the current status, only updates the timer but doesn't
// change the status.
TEST_F(ScheduledFeatureTest, ChangingStartTimesThatDontChangeTheStatus) {
  //       16:00        18:00         22:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now          start          end
  //
  test_clock_.SetNow(TimeOfDay(16 * 60).ToTimeToday());  // 4:00 PM.
  SetFeatureEnabled(false);
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kNone);
  feature()->SetCustomStartTime(TimeOfDay(18 * 60));  // 6:00 PM.
  feature()->SetCustomEndTime(TimeOfDay(22 * 60));    // 10:00 PM.

  // Since now is outside the feature interval, changing the schedule type
  // to kCustom, shouldn't affect the status. Validate the timer is running
  // with a 2-hour delay.
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(2), feature()->timer()->GetCurrentDelay());

  // Change the start time in such a way that doesn't change the status, but
  // despite that, confirm that schedule has been updated.
  feature()->SetCustomStartTime(TimeOfDay(19 * 60));  // 7:00 PM.
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(3), feature()->timer()->GetCurrentDelay());

  // Changing the end time in a similar fashion to the above and expect no
  // change.
  feature()->SetCustomEndTime(TimeOfDay(23 * 60));  // 11:00 PM.
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(3), feature()->timer()->GetCurrentDelay());
}

// Tests that the feature should turn on at sunset time and turn off at sunrise
// time.
TEST_F(ScheduledFeatureTest, SunsetSunrise) {
  EXPECT_FALSE(GetEnabled());

  // Set time now to 10:00 AM.
  base::Time current_time = TimeOfDay(10 * 60).ToTimeToday();
  test_clock_.SetNow(current_time);
  EXPECT_FALSE(feature()->timer()->IsRunning());
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(geolocation_controller_->GetSunsetTime() - current_time,
            feature()->timer()->GetCurrentDelay());

  // Firing a timer should to advance the time to sunset and automatically turn
  // on the feature.
  current_time = geolocation_controller_->GetSunsetTime();
  test_clock_.SetNow(current_time);
  feature()->timer()->FireNow();
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_TRUE(GetEnabled());
  EXPECT_EQ(geolocation_controller_->GetSunriseTime() + base::Hours(24) -
                current_time,
            feature()->timer()->GetCurrentDelay());

  // Firing a timer should advance the time to sunrise and automatically turn
  // off the feature.
  current_time = geolocation_controller_->GetSunriseTime();
  test_clock_.SetNow(current_time);
  feature()->timer()->FireNow();
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(geolocation_controller_->GetSunsetTime() - current_time,
            feature()->timer()->GetCurrentDelay());
}

// The following tests ensure that the feature schedule is correctly
// refreshed when the start and end times are inverted (i.e. the "start time" as
// a time of day today is in the future with respect to the "end time" also as a
// time of day today).
//
// Case 1: "Now" is less than both "end" and "start".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase1) {
  // Now is 4:00 AM.
  test_clock_.SetNow(TimeOfDay(4 * 60).ToTimeToday());
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 6:00 AM. "Now" is less than
  // both. The feature should be on.
  //       4:00          6:00         21:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now           end          start
  //
  feature()->SetCustomStartTime(TimeOfDay(21 * 60));
  feature()->SetCustomEndTime(TimeOfDay(6 * 60));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);

  EXPECT_TRUE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  // The feature should end in two hours.
  EXPECT_EQ(base::Hours(2), feature()->timer()->GetCurrentDelay());
}

// Case 2: "Now" is between "end" and "start".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase2) {
  // Now is 6:00 AM.
  test_clock_.SetNow(TimeOfDay(6 * 60).ToTimeToday());
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 4:00 AM. "Now" is between both.
  // The feature should be off.
  //       4:00          6:00         21:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       end           now          start
  //
  feature()->SetCustomStartTime(TimeOfDay(21 * 60));
  feature()->SetCustomEndTime(TimeOfDay(4 * 60));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);

  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  // The feature should start in 15 hours.
  EXPECT_EQ(base::Hours(15), feature()->timer()->GetCurrentDelay());
}

// Case 3: "Now" is greater than both "start" and "end".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase3) {
  // Now is 11:00 PM.
  test_clock_.SetNow(TimeOfDay(23 * 60).ToTimeToday());
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 4:00 AM. "Now" is greater than
  // both. NightLight should be on.
  //       4:00         21:00         23:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       end          start          now
  //
  feature()->SetCustomStartTime(TimeOfDay(21 * 60));
  feature()->SetCustomEndTime(TimeOfDay(4 * 60));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);

  EXPECT_TRUE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  // The feature should end in 5 hours.
  EXPECT_EQ(base::Hours(5), feature()->timer()->GetCurrentDelay());
}

}  // namespace

}  // namespace ash