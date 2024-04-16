// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/geolocation/geolocation_controller_test_util.h"
#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "ash/system/time/time_of_day.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/failing_local_time_converter.h"
#include "ash/test/time_of_day_test_util.h"
#include "ash/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_observation.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ash {

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Pair;
using RefreshReason = ScheduledFeature::RefreshReason;

constexpr char kUser1Email[] = "user1@featuredschedule";
constexpr char kUser2Email[] = "user2@featuredschedule";

constexpr char kTestEnabledPref[] = "ash.test.scheduled_feature.enabled";
constexpr char kTestScheduleTypePref[] =
    "ash.test.scheduled_feature.schedule_type";
constexpr char kTestCustomStartTimePref[] =
    "ash.test.scheduled_feature.custom_start_time";
constexpr char kTestCustomEndTimePref[] =
    "ash.test.scheduled_feature.custom_end_time";

// 6:00 PM
constexpr int kTestCustomStartTimeOffsetMinutes = 18 * 60;
// 6:00 AM
constexpr int kTestCustomEndTimeOffsetMinutes = 6 * 60;

// Maximum backoff time for refreshing the schedule when failures are
// encountered.
constexpr base::TimeDelta kMaxRefreshBackoff = base::Minutes(1);

enum AmPm { kAM, kPM };

// Returns the `ScheduleCheckpoint` that is expected to come next after
// `current_checkpoint` (sunrise, morning, late afternoon, sunset, sunrise,
// etc).
ScheduleCheckpoint GetNextExpectedCheckpoint(
    ScheduleCheckpoint current_checkpoint) {
  switch (current_checkpoint) {
    // Sunset to sunrise schedule type:
    case ScheduleCheckpoint::kSunset:
      return ScheduleCheckpoint::kSunrise;
    case ScheduleCheckpoint::kSunrise:
      return ScheduleCheckpoint::kMorning;
    case ScheduleCheckpoint::kMorning:
      return ScheduleCheckpoint::kLateAfternoon;
    case ScheduleCheckpoint::kLateAfternoon:
      return ScheduleCheckpoint::kSunset;

    // Custom schedule type:
    case ScheduleCheckpoint::kEnabled:
      return ScheduleCheckpoint::kEnabled;
    case ScheduleCheckpoint::kDisabled:
      return ScheduleCheckpoint::kDisabled;
  }
}

// Records all changes made to the feature state from the time that
// PrefChangeObserver is constructed (turned on at 2 PM, turned off at 5 PM,
// turned back on at 11 PM, etc). Allows tests to not only verify that the
// feature changed status at the appropriate times, but also that it did not
// change at an unintended time. For example, if the feature is expected to turn
// on at 2 PM and it's now 12 PM, we want to make sure that it did not turn off
// then back on again at some point in the middle (say 1 PM).
class PrefChangeObserver {
 public:
  PrefChangeObserver(PrefService* pref_service, const base::Clock* clock)
      : clock_(clock) {
    pref_registrar_.Init(pref_service);
    pref_registrar_.Add(
        kTestEnabledPref,
        base::BindRepeating(&PrefChangeObserver::OnEnabledPrefChanged,
                            base::Unretained(this)));
  }
  ~PrefChangeObserver() = default;

  // Elements appear in chronological order:
  // <Time of the status change, the status of the feature at the time>
  const std::vector<std::pair<TimeOfDay, bool>>& changes() const {
    return changes_;
  }

  void ClearHistory() { changes_.clear(); }

 private:
  void OnEnabledPrefChanged() {
    changes_.emplace_back(
        TimeOfDay::FromTime(clock_->Now()),
        pref_registrar_.prefs()->GetBoolean(kTestEnabledPref));
  }

  PrefChangeRegistrar pref_registrar_;
  const raw_ptr<const base::Clock> clock_;
  std::vector<std::pair<TimeOfDay, bool>> changes_;
};

class CheckpointObserver : public ScheduledFeature::CheckpointObserver {
 public:
  CheckpointObserver(ScheduledFeature* feature, const base::Clock* clock)
      : clock_(clock) {
    observation_.Observe(feature);
  }
  CheckpointObserver(const CheckpointObserver&) = delete;
  CheckpointObserver& operator=(const CheckpointObserver&) = delete;
  ~CheckpointObserver() override = default;

  // ScheduledFeature::CheckpointObserver:
  void OnCheckpointChanged(const ScheduledFeature* src,
                           ScheduleCheckpoint new_checkpoint) override {
    changes_.emplace_back(TimeOfDay::FromTime(clock_->Now()), new_checkpoint);
  }

  // Elements appear in chronological order:
  // <Time of the checkpoint change, the checkpoint received>
  const std::vector<std::pair<TimeOfDay, ScheduleCheckpoint>>& changes() const {
    return changes_;
  }

 private:
  base::ScopedObservation<ScheduledFeature,
                          ScheduledFeature::CheckpointObserver>
      observation_{this};
  const raw_ptr<const base::Clock> clock_;
  std::vector<std::pair<TimeOfDay, ScheduleCheckpoint>> changes_;
};

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

  // ScheduledFeature:
  const char* GetFeatureName() const override { return "TestFeature"; }
  const char* GetScheduleTypeHistogramName() const override {
    return schedule_type_histogram_name_.c_str();
  }
  MOCK_METHOD(void, RefreshFeatureState, (RefreshReason reason), (override));

  void set_schedule_type_histogram_name(
      std::string schedule_type_histogram_name) {
    schedule_type_histogram_name_ = std::move(schedule_type_histogram_name);
  }

 private:
  std::string schedule_type_histogram_name_;
};

class ScheduledFeatureTest : public NoSessionAshTestBase,
                             public ScheduledFeature::Clock {
 public:
  ScheduledFeatureTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
        task_runner_origin_ticks_(task_runner_->NowTicks()) {}
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

  TestScheduledFeature* feature() const { return feature_.get(); }
  GeolocationController* geolocation_controller() {
    return geolocation_controller_;
  }
  const base::OneShotTimer* timer_ptr() const { return timer_ptr_; }

  TestGeolocationUrlLoaderFactory* factory() const {
    CHECK(SimpleGeolocationProvider::GetInstance());
    return static_cast<TestGeolocationUrlLoaderFactory*>(
        SimpleGeolocationProvider::GetInstance()
            ->GetSharedURLLoaderFactoryForTesting());
  }

  // AshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    SetWallClockOrigin("23 Dec 2021 12:00:00");

    // Set the clock of geolocation controller to our test clock to control the
    // time now.
    geolocation_controller_ = ash::Shell::Get()->geolocation_controller();
    geolocation_controller()->SetClockForTesting(this);

    // Every feature that is auto scheduled by default needs to set test clock.
    // Otherwise the tests will fails `DCHECK_GE(start_time, now)` in
    // `ScheduledFeature::RefreshScheduleTimer()` when the new user session
    // is entered and `InitFromUserPrefs()` triggers `RefreshScheduleTimer()`.
    ash::Shell::Get()->dark_light_mode_controller()->SetClockForTesting(this);

    CreateTestUserSessions();

    // Simulate user 1 login.
    SimulateNewUserFirstLogin(kUser1Email);

    feature_ = std::make_unique<TestScheduledFeature>(
        kTestEnabledPref, kTestScheduleTypePref, kTestCustomStartTimePref,
        kTestCustomEndTimePref);
    ASSERT_FALSE(feature_->GetEnabled());

    feature_->SetClockForTesting(this);
    feature_->SetTaskRunnerForTesting(task_runner_);
    feature_->OnActiveUserPrefServiceChanged(
        Shell::Get()->session_controller()->GetActivePrefService());

    timer_ptr_ = geolocation_controller()->GetTimerForTesting();
  }

  void TearDown() override {
    feature_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // ScheduledFeature::Clock:
  base::Time Now() const override {
    // Some tests may want to control the UTC wall clock time around which the
    // test is centered. `wall_clock_origin_` allows for this, and the value
    // returned here still reflects the advancement of `task_runner_`'s mock
    // time.
    const base::TimeDelta mock_time_elapsed =
        NowTicks() - task_runner_origin_ticks_;
    return wall_clock_origin_ + wall_clock_artificial_advancement_ +
           mock_time_elapsed;
  }

  base::TimeTicks NowTicks() const override { return task_runner_->NowTicks(); }

  void CreateTestUserSessions() {
    GetSessionControllerClient()->Reset();
    AddUserSession(kUser1Email);
    AddUserSession(kUser2Email);
  }

  void AddUserSession(const std::string& user_email) {
    auto prefs = std::make_unique<TestingPrefServiceSimple>();
    prefs->registry()->RegisterBooleanPref(kTestEnabledPref, false);
    prefs->registry()->RegisterIntegerPref(
        kTestScheduleTypePref, static_cast<int>(ScheduleType::kNone));
    prefs->registry()->RegisterIntegerPref(kTestCustomStartTimePref,
                                           kTestCustomStartTimeOffsetMinutes);
    prefs->registry()->RegisterIntegerPref(kTestCustomEndTimePref,
                                           kTestCustomEndTimeOffsetMinutes);
    RegisterUserProfilePrefs(prefs->registry(), /*country=*/"",
                             /*for_test=*/true);
    auto* const session_controller_client = GetSessionControllerClient();
    session_controller_client->AddUserSession(user_email,
                                              user_manager::UserType::kRegular,
                                              /*provide_pref_service=*/false);
    session_controller_client->SetUserPrefService(
        AccountId::FromUserEmail(user_email), std::move(prefs));
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  bool GetEnabled() { return feature_->GetEnabled(); }
  ScheduleType GetScheduleType() { return feature_->GetScheduleType(); }

  void SetFeatureEnabled(bool enabled) { feature_->SetEnabled(enabled); }
  void SetScheduleType(ScheduleType type) { feature_->SetScheduleType(type); }

  // Convenience function for constructing a TimeOfDay object for exact hours
  // during the day. |hour| is between 1 and 12.
  TimeOfDay MakeTimeOfDay(int hour, AmPm am_pm) {
    DCHECK_GE(hour, 1);
    DCHECK_LE(hour, 12);

    if (am_pm == kAM) {
      hour %= 12;
    } else {
      if (hour != 12)
        hour += 12;
      hour %= 24;
    }

    return TimeOfDay(hour * 60).SetClock(this);
  }

  void FastForwardBy(base::TimeDelta amount) {
    task_runner_->FastForwardBy(amount);
  }

  // Fast forwards to the next point in time that the specified `time_of_day`
  // is hit. Examples:
  // 1) now = 2 PM time_of_day = 5 PM, advances 3 hours
  // 2) now = 7 PM time_of_day = 5 PM, advances 22 hours (the next day)
  void FastForwardTo(TimeOfDay time_of_day) {
    base::Time target_time = ToTimeToday(time_of_day.SetClock(this));
    const base::Time now = Now();
    if (target_time < now) {
      target_time += base::Days(1);
      ASSERT_GT(target_time, now);
    }
    FastForwardBy(target_time - now);
  }

  // To simulate events like the device suspending (wall clock advances, but
  // clock "ticks" don't).
  void AdvanceTimeBy(base::TimeDelta amount) {
    wall_clock_artificial_advancement_ += amount;
  }

  // Fires the timer of the scheduler to request geoposition and wait for all
  // observers to receive the latest geoposition from the server.
  void FireTimerToFetchGeoposition() {
    GeopositionResponsesWaiter waiter(geolocation_controller_);
    EXPECT_TRUE(timer_ptr()->IsRunning());
    // Fast forward the scheduler to reach the time when the controller
    // requests for geoposition from the server in
    // `GeolocationController::RequestGeoposition`.
    timer_ptr_->FireNow();
    // Waits for the observers to receive the geoposition from the server.
    waiter.Wait();
  }

  // Checks if the feature is observing geoposition changes.
  bool IsFeatureObservingGeoposition() {
    return geolocation_controller()->HasObserver(feature());
  }

  // Sets the wall clock time at which the test case starts. After calling this,
  // future calls to Now() will return `utc_time_str` plus the amount of mock
  // time that has elapsed thus far in the test. See
  // `base::Time::FromUTCString()` for format of `utc_time_str`.
  void SetWallClockOrigin(const char* const utc_time_str) {
    ASSERT_TRUE(base::Time::FromUTCString(utc_time_str, &wall_clock_origin_));
  }

  // Simulates scenarios where the code is receiving valid `base::Time` values
  // from the clock, but converting them to/from local time is failing.
  void SetLocalTimeConverter(const LocalTimeConverter* local_time_converter) {
    geolocation_controller()->SetLocalTimeConverterForTesting(
        local_time_converter);
    ash::Shell::Get()
        ->dark_light_mode_controller()
        ->SetLocalTimeConverterForTesting(local_time_converter);
    feature_->SetLocalTimeConverterForTesting(local_time_converter);
  }

 private:
  // It is infeasible to initialize AshTestBase with "mock" time and fast
  // forward the mock TaskEnvironment in these test cases. Why: The
  // AshTestBase harness also instantiates a large portion of the UI stack
  // (even though it's irrelevant to these tests), which causes
  // TaskEnvironment::FastForwardBy() to block for long periods of time. The
  // test cases ultimately take too long.
  //
  // To solve, create a dedicated `base::TestMockTimeTaskRunner` just for
  // `ScheduledFeature` and its dependencies. The `task_runner_` is still
  // run on the main test thread.
  const scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  // The time at which `task_runner_`'s internal mock tick clock starts.
  const base::TimeTicks task_runner_origin_ticks_;
  // Wall clock time at which the test case starts. Individual test cases may
  // adjust this with the `SetWallClockOrigin()` method. This is reflected in
  // the return value of `Now()`.
  base::Time wall_clock_origin_;
  // Used to support `AdvanceTimeBy()`, where the wall clock is advanced but
  // the tick clock is not (simulates a device suspending). This is reflected
  // in the return value of `Now()` and defaults to 0.
  base::TimeDelta wall_clock_artificial_advancement_;
  std::unique_ptr<TestScheduledFeature> feature_;
  raw_ptr<GeolocationController, DanglingUntriaged> geolocation_controller_;
  raw_ptr<base::OneShotTimer, DanglingUntriaged> timer_ptr_;
  Geoposition position_;
};

struct TestTimestamp {
  // Passed to `base::Time::FromUTCString()`.
  const char* utc_value = nullptr;
  // Human-readable label printed in test output.
  const char* label = nullptr;
};

struct TimeAndLocation {
  TestTimestamp timestamp;
  SimpleGeoposition geoposition;
};

// Iterates through all possible geopositions using the `kSunsetToSunrise`
// schedule type. Gives comprehensive test coverage for all seasons and for all
// parts of the globe.
class ScheduledFeatureGeopositionTest
    : public ScheduledFeatureTest,
      public testing::WithParamInterface<TimeAndLocation> {
 public:
  static constexpr TestTimestamp kAllTimestamps[] = {
      {"07 Jan 2023 20:30:00.000", "Winter"},
      {"07 Apr 2023 20:30:00.000", "Spring"},
      {"07 Jun 2023 20:30:00.000", "Summer"},
      {"07 Oct 2023 20:30:00.000", "Fall"},
  };

  // Generates coordinates in range ([-90, +90], [-180, +180]) with 15 degree
  // step size in latitude and 30 degree step size in longitude.
  static std::vector<TimeAndLocation> GenerateTestParams() {
    static constexpr double kMinLatitude = -90.f;
    static constexpr double kMaxLatitude = 90.f;
    static constexpr double kMinLongitude = -180.f;
    static constexpr double kMaxLongitude = 180.f;
    static constexpr double kLatitudeStepSize = 15.f;
    static constexpr double kLongitudeStepSize = 30.f;

    // Accounts for precision lost when incrementing latitudes/longitudes. Ex:
    // 150.f + 30.f may not necessarily equal 180.f exactly. If it evaluates to
    // something slightly greater than 180.f, the geoposition code will consider
    // this an invalid longitude and skip it. This ensures we are testing the
    // max values of the lat/long ranges.
    const auto increment_coordinate =
        [](const double max_value, const double step_size, double& coordinate) {
          constexpr double kCoordinateEpsilon = 0.001;
          coordinate += step_size;
          if (base::IsApproximatelyEqual(coordinate, max_value,
                                         kCoordinateEpsilon)) {
            coordinate = max_value;
          }
        };

    std::vector<TimeAndLocation> test_params;
    for (const TestTimestamp& timestamp : kAllTimestamps) {
      TimeAndLocation time_and_location;
      time_and_location.timestamp = timestamp;

      for (double latitude = kMinLatitude; latitude <= kMaxLatitude;
           increment_coordinate(kMaxLatitude, kLatitudeStepSize, latitude)) {
        for (double longitude = kMinLongitude; longitude <= kMaxLongitude;
             increment_coordinate(kMaxLongitude, kLongitudeStepSize,
                                  longitude)) {
          time_and_location.geoposition = {latitude, longitude};
          test_params.push_back(time_and_location);
        }
      }
    }
    return test_params;
  }

  void SetUp() override {
    ScheduledFeatureTest::SetUp();
    SetWallClockOrigin(GetParam().timestamp.utc_value);
    factory()->SetValidPosition(GetParam().geoposition.latitude,
                                GetParam().geoposition.longitude, Now());
    FireTimerToFetchGeoposition();
    feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);
  }
};

INSTANTIATE_TEST_SUITE_P(
    AllGeopositions,
    ScheduledFeatureGeopositionTest,
    testing::ValuesIn(ScheduledFeatureGeopositionTest::GenerateTestParams()),
    [](const testing::TestParamInfo<ScheduledFeatureGeopositionTest::ParamType>&
           info) {
      // gtest only permits alphanumeric characters in the generated name.
      const auto coordinate_to_string = [](double coordinate) {
        std::string coordinate_str =
            base::NumberToString(base::ClampRound(coordinate));
        base::ReplaceChars(coordinate_str, "-", "Negative", &coordinate_str);
        return coordinate_str;
      };
      return base::StringPrintf(
          "%sLat%sLong%s", info.param.timestamp.label,
          coordinate_to_string(info.param.geoposition.latitude).c_str(),
          coordinate_to_string(info.param.geoposition.longitude).c_str());
    });

// Tests that switching users retrieves the feature settings for the active
// user's prefs.
TEST_F(ScheduledFeatureTest, UserSwitchAndSettingsPersistence) {
  // Start with user1 logged in and update to sunset-to-sunrise schedule type.
  const std::string kScheduleTypePrefString = kTestScheduleTypePref;
  constexpr ScheduleType kUser1ScheduleType = ScheduleType::kSunsetToSunrise;
  constexpr bool kUser1EnabledState = false;
  constexpr ScheduleCheckpoint kUser1Checkpoint = ScheduleCheckpoint::kMorning;
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kSettingsChanged));
  feature()->SetScheduleType(kUser1ScheduleType);
  Mock::VerifyAndClearExpectations(feature());
  FastForwardTo(MakeTimeOfDay(10, AmPm::kAM));
  EXPECT_EQ(GetScheduleType(), kUser1ScheduleType);
  EXPECT_EQ(user1_pref_service()->GetInteger(kScheduleTypePrefString),
            static_cast<int>(kUser1ScheduleType));
  EXPECT_EQ(GetEnabled(), kUser1EnabledState);
  EXPECT_EQ(feature()->current_checkpoint(), kUser1Checkpoint);

  // Switch to user 2, and set to custom schedule type.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kSettingsChanged));
  SwitchActiveUser(kUser2Email);
  Mock::VerifyAndClearExpectations(feature());

  const ScheduleType user2_schedule_type = ScheduleType::kCustom;
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kSettingsChanged));
  user2_pref_service()->SetInteger(kScheduleTypePrefString,
                                   static_cast<int>(user2_schedule_type));
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_EQ(GetScheduleType(), user2_schedule_type);
  EXPECT_EQ(user2_pref_service()->GetInteger(kScheduleTypePrefString),
            static_cast<int>(user2_schedule_type));

  // Switch back to user 1, to find feature schedule type is restored to
  // sunset-to-sunrise with the correct enabled state and checkpoint.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kSettingsChanged));
  SwitchActiveUser(kUser1Email);
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_EQ(GetScheduleType(), kUser1ScheduleType);
  EXPECT_EQ(GetEnabled(), kUser1EnabledState);
  EXPECT_EQ(feature()->current_checkpoint(), kUser1Checkpoint);
}

// Tests that the scheduler type is initiallized from user prefs and observes
// geoposition when the scheduler is enabled.
TEST_F(ScheduledFeatureTest, InitScheduleTypeFromUserPrefs) {
  // Start with user1 logged in with the default disabled scheduler, `kNone`.
  const std::string kScheduleTypePrefString = kTestScheduleTypePref;
  const ScheduleType user1_schedule_type = ScheduleType::kNone;
  EXPECT_EQ(user1_schedule_type, GetScheduleType());
  // Check that the feature does not observe the geoposition when the schedule
  // type is `kNone`.
  EXPECT_FALSE(IsFeatureObservingGeoposition());

  // Update user2's schedule type pref to sunset-to-sunrise.
  const ScheduleType user2_schedule_type = ScheduleType::kSunsetToSunrise;
  user2_pref_service()->SetInteger(kScheduleTypePrefString,
                                   static_cast<int>(user2_schedule_type));
  // Switching to user2 should update the schedule type to sunset-to-sunrise.
  SwitchActiveUser(kUser2Email);
  EXPECT_EQ(user2_schedule_type, GetScheduleType());
  // Check that the feature starts observing geoposition when the schedule
  // type is changed to `kSunsetToSunrise`.
  EXPECT_TRUE(IsFeatureObservingGeoposition());

  // Set custom schedule to test that once we switch to the user1 with `kNone`
  // schedule type, the feature should remove itself from a geolocation
  // observer.
  feature()->SetScheduleType(ScheduleType::kCustom);
  EXPECT_TRUE(IsFeatureObservingGeoposition());
  // Make sure that switching back to user1 makes it remove itself from the
  // geoposition observer.
  SwitchActiveUser(kUser1Email);
  EXPECT_EQ(user1_schedule_type, GetScheduleType());
  EXPECT_FALSE(IsFeatureObservingGeoposition());
}

// Tests transitioning from kNone to kCustom and back to kNone schedule
// types.
TEST_F(ScheduledFeatureTest, ScheduleNoneToCustomTransition) {
  // Now is 6:00 PM.
  FastForwardTo(MakeTimeOfDay(6, AmPm::kPM));
  SetFeatureEnabled(false);
  feature()->SetScheduleType(ScheduleType::kNone);
  // Start time is at 3:00 PM and end time is at 8:00 PM.
  feature()->SetCustomStartTime(MakeTimeOfDay(3, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, AmPm::kPM));

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
  feature()->SetScheduleType(ScheduleType::kCustom);
  EXPECT_TRUE(GetEnabled());
  PrefChangeObserver change_log(user1_pref_service(), this);
  FastForwardTo(MakeTimeOfDay(8, AmPm::kPM));
  EXPECT_FALSE(GetEnabled());
  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(8, AmPm::kPM), false)));

  // First reset back to 6 p.m. the next day:
  FastForwardTo(MakeTimeOfDay(6, AmPm::kPM));
  ASSERT_TRUE(GetEnabled());

  // If the user changes the schedule type to "none", the feature status
  // should not change.
  feature()->SetScheduleType(ScheduleType::kNone);
  EXPECT_TRUE(GetEnabled());
  // Since schedule type is none, the feature should not switch off at the
  // scheduled end.
  FastForwardTo(MakeTimeOfDay(8, AmPm::kPM));
  EXPECT_TRUE(GetEnabled());
}

// Tests what happens when the time now reaches the end of the feature
// interval when the feature mode is on.
TEST_F(ScheduledFeatureTest, TestCustomScheduleReachingEndTime) {
  FastForwardTo(MakeTimeOfDay(6, AmPm::kPM));
  feature()->SetCustomStartTime(MakeTimeOfDay(3, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, AmPm::kPM));
  feature()->SetScheduleType(ScheduleType::kCustom);
  EXPECT_TRUE(GetEnabled());

  PrefChangeObserver change_log(user1_pref_service(), this);

  // Simulate reaching the end time by triggering the timer's user task. Make
  // sure that the feature ended.
  //
  //      15:00                      20:00
  // <----- + ------------------------ + ----->
  //        |                          |
  //      start                    end & now
  //
  // Now is 8:00 PM.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kScheduled));
  FastForwardTo(MakeTimeOfDay(8, AmPm::kPM));
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_FALSE(GetEnabled());
  // The feature should be scheduled to start again at 3:00 PM tomorrow.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kScheduled));
  FastForwardTo(MakeTimeOfDay(3, AmPm::kPM));
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(8, AmPm::kPM), false),
                          Pair(MakeTimeOfDay(3, AmPm::kPM), true)));
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
  FastForwardTo(MakeTimeOfDay(11, AmPm::kPM));
  feature()->SetCustomStartTime(MakeTimeOfDay(3, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, AmPm::kPM));
  feature()->SetScheduleType(ScheduleType::kCustom);
  EXPECT_FALSE(GetEnabled());

  // What happens if the user manually turns the feature on while the schedule
  // type says it should be off?
  // User toggles either from the system menu or the System Settings toggle
  // button must override any automatic schedule.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kExternal));
  SetFeatureEnabled(true);
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_TRUE(GetEnabled());

  PrefChangeObserver change_log(user1_pref_service(), this);
  // The feature should automatically turn off at 8:00 PM tomorrow. May refresh
  // at 3:00 PM with no change in status.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kScheduled))
      .Times(AtLeast(1));
  FastForwardTo(MakeTimeOfDay(8, AmPm::kPM));
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_FALSE(GetEnabled());

  // Manually reset the feature back to on.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kExternal));
  SetFeatureEnabled(true);
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_TRUE(GetEnabled());

  // Manually turning it back off should also be respected, and this time the
  // start is scheduled at 3:00 PM tomorrow after 19 hours from "now" (8:00
  // PM).
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kExternal));
  SetFeatureEnabled(false);
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_FALSE(GetEnabled());
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kScheduled));
  FastForwardTo(MakeTimeOfDay(3, AmPm::kPM));
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_TRUE(GetEnabled());
  EXPECT_THAT(
      change_log.changes(),
      ElementsAre(
          Pair(MakeTimeOfDay(8, AmPm::kPM), false),
          Pair(MakeTimeOfDay(8, AmPm::kPM), true),   // Manual toggle on
          Pair(MakeTimeOfDay(8, AmPm::kPM), false),  // Manual toggle off
          Pair(MakeTimeOfDay(3, AmPm::kPM), true)));
}

// Tests that changing the custom start and end times, in such a way that
// shouldn't change the current status, only updates the timer but doesn't
// change the status.
// TODO(crbug.com/40889492): Fix test failure and re-enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ChangingStartTimesThatDontChangeTheStatus \
  DISABLED_ChangingStartTimesThatDontChangeTheStatus
#else
#define MAYBE_ChangingStartTimesThatDontChangeTheStatus \
  ChangingStartTimesThatDontChangeTheStatus
#endif
TEST_F(ScheduledFeatureTest, MAYBE_ChangingStartTimesThatDontChangeTheStatus) {
  //       16:00        18:00         22:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now          start          end
  //
  FastForwardTo(MakeTimeOfDay(4, AmPm::kPM));  // 4:00 PM.
  SetFeatureEnabled(false);
  feature()->SetScheduleType(ScheduleType::kNone);
  feature()->SetCustomStartTime(MakeTimeOfDay(6, AmPm::kPM));  // 6:00 PM.
  feature()->SetCustomEndTime(MakeTimeOfDay(10, AmPm::kPM));   // 10:00 PM.

  // Since now is outside the feature interval, changing the schedule type
  // to kCustom, shouldn't affect the status. Validate the feature turns on then
  // off at the scheduled times.
  feature()->SetScheduleType(ScheduleType::kCustom);
  EXPECT_FALSE(GetEnabled());
  PrefChangeObserver change_log(user1_pref_service(), this);
  FastForwardBy(base::Days(1));
  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(6, AmPm::kPM), true),
                          Pair(MakeTimeOfDay(10, AmPm::kPM), false)));

  change_log.ClearHistory();
  // Change the start time in such a way that doesn't change the status, but
  // despite that, confirm that schedule has been updated.
  ASSERT_FALSE(GetEnabled());
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kSettingsChanged));
  feature()->SetCustomStartTime(MakeTimeOfDay(7, AmPm::kPM));  // 7:00 PM.
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_FALSE(GetEnabled());

  // Changing the end time in a similar fashion to the above and expect no
  // change.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kSettingsChanged));
  feature()->SetCustomEndTime(MakeTimeOfDay(11, AmPm::kPM));  // 11:00 PM.
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_FALSE(GetEnabled());
  FastForwardBy(base::Days(1));
  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(7, AmPm::kPM), true),
                          Pair(MakeTimeOfDay(11, AmPm::kPM), false)));
}

// Tests that the feature should turn on at sunset time and turn off at sunrise
// time.
TEST_F(ScheduledFeatureTest, SunsetSunrise) {
  EXPECT_FALSE(GetEnabled());
  EXPECT_EQ(feature()->current_checkpoint(), ScheduleCheckpoint::kDisabled);

  // Set time now to 10:00 AM.
  FastForwardTo(MakeTimeOfDay(10, AmPm::kAM));
  const CheckpointObserver checkpoint_observer(feature(), this);
  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(GetEnabled());

  const PrefChangeObserver change_log(user1_pref_service(), this);

  // Set time now to 4:00 PM.
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kScheduled));
  FastForwardTo(MakeTimeOfDay(4, AmPm::kPM));
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_FALSE(GetEnabled());

  // Firing a timer should to advance the time to sunset and automatically turn
  // on the feature.
  const auto sunset = geolocation_controller()->GetSunsetTime();
  ASSERT_TRUE(sunset.has_value());
  const TimeOfDay sunset_time = TimeOfDay::FromTime(sunset.value());
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kScheduled));
  FastForwardTo(sunset_time);
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_TRUE(GetEnabled());

  // Firing a timer should advance the time to sunrise and automatically turn
  // off the feature.
  const auto sunrise = geolocation_controller()->GetSunriseTime();
  ASSERT_TRUE(sunrise.has_value());
  const TimeOfDay sunrise_time = TimeOfDay::FromTime(sunrise.value());
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kScheduled));
  FastForwardTo(sunrise_time);
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_FALSE(GetEnabled());

  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(sunset_time, true), Pair(sunrise_time, false)));
  EXPECT_THAT(
      checkpoint_observer.changes(),
      ElementsAre(
          Pair(MakeTimeOfDay(10, AmPm::kAM), ScheduleCheckpoint::kMorning),
          Pair(MakeTimeOfDay(4, AmPm::kPM), ScheduleCheckpoint::kLateAfternoon),
          Pair(sunset_time, ScheduleCheckpoint::kSunset),
          Pair(sunrise_time, ScheduleCheckpoint::kSunrise)));
}

// Tests that scheduled start time and end time of sunset-to-sunrise feature
// are updated correctly if the geoposition changes.
TEST_F(ScheduledFeatureTest, SunsetSunriseGeoposition) {
  constexpr double kFakePosition1_Latitude = 23.5;
  constexpr double kFakePosition1_Longitude = 55.88;
  constexpr double kFakePosition2_Latitude = 23.5;
  constexpr double kFakePosition2_Longitude = 10.9;
  // Position 1 sunset and sunrise times.
  //
  //    sunset-4
  // <----- + --------- + ---------------- + ------->
  //        |           |                  |
  //       now        sunset            sunrise
  //

  GeolocationControllerObserver observer1;
  geolocation_controller()->AddObserver(&observer1);
  EXPECT_TRUE(timer_ptr()->IsRunning());
  EXPECT_FALSE(observer1.possible_change_in_timezone());

  // Set and fetch position update.
  factory()->SetValidPosition(kFakePosition1_Latitude, kFakePosition1_Longitude,
                              Now());
  FireTimerToFetchGeoposition();
  EXPECT_TRUE(observer1.possible_change_in_timezone());
  const auto sunset_time1 = geolocation_controller()->GetSunsetTime();
  const auto sunrise_time1 = geolocation_controller()->GetSunriseTime();
  ASSERT_TRUE(sunset_time1.has_value());
  ASSERT_TRUE(sunrise_time1.has_value());
  // Our assumption is that GeolocationController gives us sunrise time
  // earlier in the same day before sunset.
  ASSERT_GT(sunset_time1.value(), sunrise_time1.value());
  ASSERT_LT(sunset_time1.value() - base::Days(1), sunrise_time1.value());

  // Set time now to be 4 hours before sunset.
  FastForwardTo(TimeOfDay::FromTime(sunset_time1.value() - base::Hours(4)));

  // Expect that timer is running and the start is scheduled after 4 hours.
  EXPECT_FALSE(feature()->GetEnabled());

  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(feature()->GetEnabled());
  EXPECT_TRUE(IsFeatureObservingGeoposition());

  // A small delta used to help forwarding the time to be a little bit behind
  // the target time. Used to avoid test flaky because of the time issue.
  const base::TimeDelta delta = base::Minutes(5);
  // Simulate reaching sunset.
  FastForwardBy(base::Hours(4) +
                delta);  // Now is sunset time of the position1.
  EXPECT_TRUE(feature()->GetEnabled());

  // Simulate reaching sunrise.
  FastForwardTo(TimeOfDay::FromTime(
      sunrise_time1.value() + delta));  // Now is sunrise time of the position1
  EXPECT_FALSE(feature()->GetEnabled());

  // Now simulate user changing position.
  // Position 2 sunset and sunrise times.
  //
  // <----- + --------- + ---------------- + ------->
  //        |           |                  |
  //      sunset2      now (sunrise1)     sunrise2
  //

  // Replace a response `position` with `position2`.
  factory()->ClearResponses();
  factory()->SetValidPosition(kFakePosition2_Latitude, kFakePosition2_Longitude,
                              Now());
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kReset));
  FireTimerToFetchGeoposition();
  Mock::VerifyAndClearExpectations(feature());
  EXPECT_TRUE(observer1.possible_change_in_timezone());
  EXPECT_TRUE(IsFeatureObservingGeoposition());

  const auto sunset_time2 = geolocation_controller()->GetSunsetTime();
  const auto sunrise_time2 = geolocation_controller()->GetSunriseTime();
  ASSERT_TRUE(sunset_time2.has_value());
  ASSERT_TRUE(sunrise_time2.has_value());

  // Expect that the scheduled end delay has been updated to sunrise of location
  // 2, and the status has changed to enabled even though time has not advanced.
  EXPECT_TRUE(feature()->GetEnabled());

  // Simulate reaching sunrise.
  FastForwardTo(TimeOfDay::FromTime(
      sunrise_time2.value() + delta));  // Now is sunrise time of the position2.
  EXPECT_FALSE(feature()->GetEnabled());
  // Timer is running scheduling the start at the sunset of the next day.
  FastForwardTo(TimeOfDay::FromTime(sunset_time2.value() + delta));
  EXPECT_TRUE(feature()->GetEnabled());
}

// Tests that the feature is disabled and there are no crashes/unpredictable
// behavior if there is 24 hours of daylight.
TEST_F(ScheduledFeatureTest, SunsetSunriseAllDaylight) {
  // 24 hours of daylight (Kiruna, Sweden)
  constexpr double kTestLatitude = 67.855800;
  constexpr double kTestLongitude = 20.225282;

  SetWallClockOrigin("07 Jun 2023 20:30:00.000");

  // Set and fetch position update.
  factory()->SetValidPosition(kTestLatitude, kTestLongitude, Now());
  FireTimerToFetchGeoposition();

  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(feature()->GetEnabled());
  FastForwardBy(base::Days(1));
  EXPECT_FALSE(feature()->GetEnabled());
}

// Tests that on device resume from sleep, the feature status is updated
// correctly if the time has changed meanwhile.
TEST_F(ScheduledFeatureTest, CustomScheduleOnResume) {
  // Now is 4:00 PM.
  FastForwardTo(MakeTimeOfDay(4, AmPm::kPM));
  feature()->SetEnabled(false);
  // Start time is at 6:00 PM and end time is at 10:00 PM. The feature should be
  // off.
  //      16:00         18:00         22:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now          start          end
  //
  feature()->SetCustomStartTime(MakeTimeOfDay(6, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(10, AmPm::kPM));
  feature()->SetScheduleType(ScheduleType::kCustom);
  ASSERT_FALSE(feature()->GetEnabled());

  PrefChangeObserver change_log(user1_pref_service(), this);

  // Now simulate that the device was suspended for 3 hours, and the time now
  // is 7:00 PM when the devices was resumed. Expect that the feature turns on.
  AdvanceTimeBy(base::Hours(3));  // 7:00 PM
  EXPECT_CALL(*feature(), RefreshFeatureState(RefreshReason::kReset));
  feature()->SuspendDone(base::TimeDelta::Max());
  Mock::VerifyAndClearExpectations(feature());

  EXPECT_TRUE(feature()->GetEnabled());
  // The feature should be disabled at originally scheduled time.
  FastForwardTo(MakeTimeOfDay(10, AmPm::kPM));
  EXPECT_FALSE(feature()->GetEnabled());

  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(7, AmPm::kPM), true),
                          Pair(MakeTimeOfDay(10, AmPm::kPM), false)));
}

// The following tests ensure that the feature schedule is correctly
// refreshed when the start and end times are inverted (i.e. the "start time" as
// a time of day today is in the future with respect to the "end time" also as a
// time of day today).
//
// Case 1: "Now" is less than both "end" and "start".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase1) {
  // Now is 4:00 AM.
  FastForwardTo(MakeTimeOfDay(4, AmPm::kAM));
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 6:00 AM. "Now" is less than
  // both. The feature should be on.
  //       4:00          6:00         21:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now           end          start
  //
  feature()->SetCustomStartTime(MakeTimeOfDay(9, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(6, AmPm::kAM));
  feature()->SetScheduleType(ScheduleType::kCustom);

  EXPECT_TRUE(GetEnabled());
  PrefChangeObserver change_log(user1_pref_service(), this);
  FastForwardBy(base::Days(1));
  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(6, AmPm::kAM), false),
                          Pair(MakeTimeOfDay(9, AmPm::kPM), true)));
}

// Case 2: "Now" is between "end" and "start".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase2) {
  // Now is 6:00 AM.
  FastForwardTo(MakeTimeOfDay(6, AmPm::kAM));
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 4:00 AM. "Now" is between both.
  // The feature should be off.
  //       4:00          6:00         21:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       end           now          start
  //
  feature()->SetCustomStartTime(MakeTimeOfDay(9, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(4, AmPm::kAM));
  feature()->SetScheduleType(ScheduleType::kCustom);

  EXPECT_FALSE(GetEnabled());
  PrefChangeObserver change_log(user1_pref_service(), this);
  FastForwardBy(base::Days(1));
  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(9, AmPm::kPM), true),
                          Pair(MakeTimeOfDay(4, AmPm::kAM), false)));
}

// Case 3: "Now" is greater than both "start" and "end".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase3) {
  // Now is 11:00 PM.
  FastForwardTo(MakeTimeOfDay(11, AmPm::kPM));
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 4:00 AM. "Now" is greater than
  // both. the feature should be on.
  //       4:00         21:00         23:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       end          start          now
  //
  feature()->SetCustomStartTime(MakeTimeOfDay(9, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(4, AmPm::kAM));
  feature()->SetScheduleType(ScheduleType::kCustom);

  EXPECT_TRUE(GetEnabled());
  PrefChangeObserver change_log(user1_pref_service(), this);
  FastForwardBy(base::Days(1));
  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(4, AmPm::kAM), false),
                          Pair(MakeTimeOfDay(9, AmPm::kPM), true)));
}

// Tests that manual changes to the feature status while a schedule is being
// used will be remembered and reapplied across user switches.
TEST_F(ScheduledFeatureTest, MultiUserManualStatusToggleWithSchedules) {
  // Setup user 1 to use a custom schedule from 3pm till 8pm, and user 2 to use
  // a sunset-to-sunrise schedule from 6pm till 6am.
  //
  //
  //          |<--- User 1 NL on --->|
  //          |                      |
  // <--------+-------------+--------+----------------------------+----------->
  //         3pm           6pm      8pm                          6am
  //                        |                                     |
  //                        |<----------- User 2 NL on ---------->|
  //
  // Test cases at:
  //
  // <---+---------+------------+------------+----------------------------+--->
  //    2pm       4pm         7pm           10pm                         9am
  //

  FastForwardTo(MakeTimeOfDay(2, kPM));
  feature()->SetCustomStartTime(MakeTimeOfDay(3, kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, kPM));
  feature()->SetScheduleType(ScheduleType::kCustom);
  SwitchActiveUser(kUser2Email);
  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);
  SwitchActiveUser(kUser1Email);

  struct {
    TimeOfDay fake_now;
    bool user_1_expected_status;
    bool user_2_expected_status;
  } kTestCases[] = {
      {MakeTimeOfDay(2, kPM), false, false},
      {MakeTimeOfDay(4, kPM), true, false},
      {MakeTimeOfDay(7, kPM), true, true},
      {MakeTimeOfDay(10, kPM), false, true},
      {MakeTimeOfDay(9, kAM),  // 9:00 AM tomorrow.
       false, false},
  };

  for (const auto& test_case : kTestCases) {
    // Each test case begins when user_1 is active.
    const bool user_1_toggled_status = !test_case.user_1_expected_status;
    const bool user_2_toggled_status = !test_case.user_2_expected_status;

    // Apply the test's case fake time, and fire the timer if there's a change
    // expected in the feature's status.
    FastForwardTo(test_case.fake_now);

    // The untoggled states for both users should match the expected ones
    // according to their schedules.
    EXPECT_EQ(test_case.user_1_expected_status, feature()->GetEnabled());
    SwitchActiveUser(kUser2Email);
    EXPECT_EQ(test_case.user_2_expected_status, feature()->GetEnabled());

    // Manually toggle the feature for user_2 and expect that it will be
    // remembered when we switch to user_1 and back.
    feature()->SetEnabled(user_2_toggled_status);
    EXPECT_EQ(user_2_toggled_status, feature()->GetEnabled());
    SwitchActiveUser(kUser1Email);
    EXPECT_EQ(test_case.user_1_expected_status, feature()->GetEnabled());
    SwitchActiveUser(kUser2Email);
    EXPECT_EQ(user_2_toggled_status, feature()->GetEnabled());

    // Toggle it for user_1 as well, and expect it will be remembered and won't
    // affect the already toggled state for user_2.
    SwitchActiveUser(kUser1Email);
    EXPECT_EQ(test_case.user_1_expected_status, feature()->GetEnabled());
    feature()->SetEnabled(user_1_toggled_status);
    EXPECT_EQ(user_1_toggled_status, feature()->GetEnabled());
    SwitchActiveUser(kUser2Email);
    EXPECT_EQ(user_2_toggled_status, feature()->GetEnabled());

    // Toggle both users back to their original states in preparation for the
    // next test case.
    feature()->SetEnabled(test_case.user_2_expected_status);
    EXPECT_EQ(test_case.user_2_expected_status, feature()->GetEnabled());
    SwitchActiveUser(kUser1Email);
    EXPECT_EQ(user_1_toggled_status, feature()->GetEnabled());
    feature()->SetEnabled(test_case.user_1_expected_status);
    EXPECT_EQ(test_case.user_1_expected_status, feature()->GetEnabled());
  }
}

TEST_F(ScheduledFeatureTest,
       ManualStatusToggleCanPersistAfterResumeFromSuspend) {
  FastForwardTo(MakeTimeOfDay(11, kAM));

  feature()->SetCustomStartTime(MakeTimeOfDay(3, kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, kPM));
  feature()->SetScheduleType(ScheduleType::kCustom);
  EXPECT_FALSE(feature()->GetEnabled());

  // Toggle the status manually and expect that the feature is scheduled to
  // turn back off at 8:00 PM.
  feature()->SetEnabled(true);
  EXPECT_TRUE(feature()->GetEnabled());

  PrefChangeObserver change_log(user1_pref_service(), this);

  // Simulate suspend and then resume at 2:00 PM (which is outside the user's
  // custom schedule). However, the manual toggle to on should be kept.
  AdvanceTimeBy(base::Hours(3));
  feature()->SuspendDone(base::TimeDelta{});
  EXPECT_TRUE(feature()->GetEnabled());

  // Suspend again and resume at 5:00 PM (which is within the user's custom
  // schedule). The schedule should be applied normally.
  AdvanceTimeBy(base::Hours(3));
  feature()->SuspendDone(base::TimeDelta{});
  EXPECT_TRUE(feature()->GetEnabled());

  // Suspend and resume at 9:00 PM and expect the feature to be off.
  AdvanceTimeBy(base::Hours(4));
  feature()->SuspendDone(base::TimeDelta{});
  EXPECT_FALSE(feature()->GetEnabled());

  EXPECT_THAT(change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(9, AmPm::kPM), false)));
}

TEST_F(ScheduledFeatureTest, CurrentCheckpointForNoneSchedule) {
  ASSERT_EQ(feature()->current_checkpoint(), ScheduleCheckpoint::kDisabled);

  const CheckpointObserver checkpoint_observer(feature(), this);

  feature()->SetEnabled(true);
  feature()->SetEnabled(true);
  feature()->SetEnabled(false);
  feature()->SetEnabled(false);
  EXPECT_THAT(checkpoint_observer.changes(),
              ElementsAre(Pair(_, ScheduleCheckpoint::kEnabled),
                          Pair(_, ScheduleCheckpoint::kDisabled)));
}

TEST_F(ScheduledFeatureTest, CurrentCheckpointForCustomSchedule) {
  FastForwardTo(MakeTimeOfDay(12, kAM));
  ASSERT_EQ(feature()->current_checkpoint(), ScheduleCheckpoint::kDisabled);

  const CheckpointObserver checkpoint_observer(feature(), this);
  // Checkpoint 0:
  // Custom schedule is enabled from 6 PM to 6 AM, so it should immediately
  // flip to enabled.
  feature()->SetScheduleType(ScheduleType::kCustom);
  // Checkpoint 1:
  // Fast forward to sunrise
  FastForwardTo(MakeTimeOfDay(6, kAM));
  // Checkpoint 2:
  // Fast forward to sunset.
  FastForwardTo(MakeTimeOfDay(6, kPM));
  EXPECT_THAT(
      checkpoint_observer.changes(),
      ElementsAre(Pair(MakeTimeOfDay(12, kAM), ScheduleCheckpoint::kEnabled),
                  Pair(MakeTimeOfDay(6, kAM), ScheduleCheckpoint::kDisabled),
                  Pair(MakeTimeOfDay(6, kPM), ScheduleCheckpoint::kEnabled)));
}

// These reflect real-world combinations of schedule type + feature enabled pref
// changes that can happen with D/L mode.
TEST_F(ScheduledFeatureTest, CurrentCheckpointForSwitchingScheduleTypes) {
  FastForwardTo(MakeTimeOfDay(12, kAM));
  ASSERT_EQ(feature()->current_checkpoint(), ScheduleCheckpoint::kDisabled);

  const CheckpointObserver checkpoint_observer(feature(), this);

  // Checkpoint 0:
  // Sunset is 6 PM and sunrise is 6 AM, so it should immediately flip to
  // enabled.
  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);

  // Checkpoint 1:
  // Flip back to no schedule type. It should stay enabled.
  feature()->SetScheduleType(ScheduleType::kNone);

  // Checkpoint 2:
  // Fast forward to 10 AM and flip back to sunset to sunrise schedule type.
  // It should automatically flip to disabled, but at the morning checkpoint.
  FastForwardTo(MakeTimeOfDay(10, kAM));
  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);

  // Checkpoint 2:
  // Flip back to no schedule type. It should stay disabled, but the default
  // checkpoint for disabled is sunrise, not morning, so the checkpoint should
  // change again.
  feature()->SetScheduleType(ScheduleType::kNone);

  // Checkpoint 3:
  // Fast forward to 12 AM again and switch to sunset to sunrise. Feature should
  // automatically flip to enabled.
  FastForwardTo(MakeTimeOfDay(12, kAM));
  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);

  // Checkpoint 4:
  // Now manually toggle the feature to disabled (opposite of what the schedule
  // says). The current checkpoint should reflect the feature being disabled.
  feature()->SetEnabled(false);

  EXPECT_THAT(
      checkpoint_observer.changes(),
      ElementsAre(Pair(MakeTimeOfDay(12, kAM), ScheduleCheckpoint::kSunset),
                  Pair(MakeTimeOfDay(12, kAM), ScheduleCheckpoint::kEnabled),
                  Pair(MakeTimeOfDay(10, kAM), ScheduleCheckpoint::kMorning),
                  Pair(MakeTimeOfDay(10, kAM), ScheduleCheckpoint::kDisabled),
                  Pair(MakeTimeOfDay(12, kAM), ScheduleCheckpoint::kSunset),
                  Pair(MakeTimeOfDay(12, kAM), ScheduleCheckpoint::kSunrise)));
}

// Tests that the feature gracefully handles failures to get local time:
// b/285187343
TEST_F(ScheduledFeatureTest, HandlesLocalTimeFailuresSunsetToSunrise) {
  // Give test initial start time of 9:00 AM.
  FastForwardTo(MakeTimeOfDay(9, AmPm::kAM));

  const CheckpointObserver checkpoint_observer(feature(), this);
  const PrefChangeObserver pref_change_log(user1_pref_service(), this);

  const FailingLocalTimeConverter failing_local_time_converter;
  SetLocalTimeConverter(&failing_local_time_converter);
  ASSERT_EQ(
      geolocation_controller()->GetSunsetTime(),
      base::unexpected(GeolocationController::SunRiseSetError::kUnavailable));
  ASSERT_EQ(
      geolocation_controller()->GetSunriseTime(),
      base::unexpected(GeolocationController::SunRiseSetError::kUnavailable));

  // Normally, this would retrieve a default sunrise/sunset of 6 AM/PM. But
  // due to local time failure, this should keep the current state (disabled)
  // and started scheduling retries with backoff.
  ASSERT_FALSE(GetEnabled());
  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(GetEnabled());

  // Fast forward to 8:00 PM. Normally, sunset is 6:00 PM, so the feature should
  // be enabled now, but since local time is still failing, current state
  // should still be maintained.
  FastForwardTo(MakeTimeOfDay(8, AmPm::kPM));
  EXPECT_FALSE(GetEnabled());

  // Now local time comes back and starts working again.
  SetLocalTimeConverter(nullptr);

  // At the next refresh retry, the schedule should resume normally.
  FastForwardBy(kMaxRefreshBackoff);
  EXPECT_TRUE(GetEnabled());

  // Test a full day of the schedule (24 hours)to make sure schedule resumed
  // normally.
  FastForwardBy(base::Days(1));

  EXPECT_THAT(pref_change_log.changes(),
              // When local time starts working again, we know that it is
              // somewhere between 8:00 PM and 8:00 PM + `kMaxRefreshBackoff`.
              // Due to backoff jitter and other variables, it is difficult to
              // pinpoint the exact timestamp for a test expectation, but that's
              // not critical here. The important thing is that it ultimately
              // updates to the correct state gracefully.
              ElementsAre(Pair(_, true), Pair(MakeTimeOfDay(6, kAM), false),
                          Pair(MakeTimeOfDay(6, kPM), true)));
  EXPECT_THAT(
      checkpoint_observer.changes(),
      // 9 AM: Change to `kSunsetToSunrise` schedule type. Feature is disabled,
      // which defaults to sunrise.
      ElementsAre(
          Pair(MakeTimeOfDay(9, kAM), ScheduleCheckpoint::kSunrise),
          // Timestamp of initial change (when local time starts working
          // again) is not precise. See comment above.
          Pair(_, ScheduleCheckpoint::kSunset),
          Pair(MakeTimeOfDay(6, kAM), ScheduleCheckpoint::kSunrise),
          Pair(MakeTimeOfDay(10, kAM), ScheduleCheckpoint::kMorning),
          Pair(MakeTimeOfDay(4, kPM), ScheduleCheckpoint::kLateAfternoon),
          Pair(MakeTimeOfDay(6, kPM), ScheduleCheckpoint::kSunset)));
}

// Tests that the feature gracefully handles failures to get local time:
// b/285187343
TEST_F(ScheduledFeatureTest, HandlesLocalTimeFailuresCustom) {
  // Start time is at 9:00 AM and end time is at 9:00 PM.
  feature()->SetCustomStartTime(MakeTimeOfDay(9, AmPm::kAM));
  feature()->SetCustomEndTime(MakeTimeOfDay(9, AmPm::kPM));
  // Give test initial start time of 9:00 AM.
  FastForwardTo(MakeTimeOfDay(9, AmPm::kAM));

  const CheckpointObserver checkpoint_observer(feature(), this);
  const PrefChangeObserver pref_change_log(user1_pref_service(), this);

  // Feature should be enabled immediately since it's 9:00 AM.
  feature()->SetScheduleType(ScheduleType::kCustom);
  EXPECT_TRUE(GetEnabled());

  const FailingLocalTimeConverter failing_local_time_converter;
  SetLocalTimeConverter(&failing_local_time_converter);

  // At 9 PM, we switch the feature off as previously scheduled, but local time
  // has started failing. Thus, the feature switches state correctly, but the
  // next refresh (which should come at 9 AM tomorrow) is not scheduled
  // correctly. Retries start.
  FastForwardTo(MakeTimeOfDay(9, AmPm::kPM));
  EXPECT_FALSE(GetEnabled());

  // 9 AM tomorrow. Feature is still disabled due to local time failure.
  FastForwardTo(MakeTimeOfDay(9, AmPm::kAM));
  EXPECT_FALSE(GetEnabled());

  // Local time starts working again. Next refresh should return the schedule to
  // normal.
  SetLocalTimeConverter(nullptr);
  FastForwardBy(kMaxRefreshBackoff);
  EXPECT_TRUE(GetEnabled());

  // Test a full day of the schedule (24 hours) to make sure schedule resumed
  // normally.
  FastForwardBy(base::Days(1));

  EXPECT_THAT(pref_change_log.changes(),
              ElementsAre(Pair(MakeTimeOfDay(9, kAM), true),
                          Pair(MakeTimeOfDay(9, kPM), false),
                          // Local time starts working slightly after 9 AM. See
                          // comments in
                          // `HandlesLocalTimeFailuresSunsetToSunrise` for why
                          // an exact timestamp is not specified.
                          Pair(_, true),
                          // Schedule resumes like normal:
                          Pair(MakeTimeOfDay(9, kPM), false),
                          Pair(MakeTimeOfDay(9, kAM), true)));
  EXPECT_THAT(
      checkpoint_observer.changes(),
      ElementsAre(Pair(MakeTimeOfDay(9, kAM), ScheduleCheckpoint::kEnabled),
                  Pair(MakeTimeOfDay(9, kPM), ScheduleCheckpoint::kDisabled),
                  // Local time starts working slightly after 9 AM. See
                  // comments in
                  // `HandlesLocalTimeFailuresSunsetToSunrise` for why
                  // an exact timestamp is not specified.
                  Pair(_, ScheduleCheckpoint::kEnabled),
                  // Schedule resumes like normal:
                  Pair(MakeTimeOfDay(9, kPM), ScheduleCheckpoint::kDisabled),
                  Pair(MakeTimeOfDay(9, kAM), ScheduleCheckpoint::kEnabled)));
}

TEST_P(ScheduledFeatureGeopositionTest, CyclesThroughCheckpoints) {
  // Sunrise, morning, late afternoon, and sunset
  static constexpr size_t kNumCheckpointsPerDay = 4;

  const CheckpointObserver checkpoint_observer(feature(), this);
  FastForwardBy(base::Days(1));

  // This legitimately happens in regions with no daylight/darkness.
  if (checkpoint_observer.changes().empty()) {
    return;
  }

  const size_t num_checkpoints_observed = checkpoint_observer.changes().size();
  // There are a couple of corner cases where 3 or 5 checkpoints are observed in
  // 24 hours.
  //
  // Example of 5:
  // Now: 5:59 AM
  // Sunrise today: 6:00 AM
  // Sunrise tomorrow: 5:58 AM
  //
  // Expected checkpoint changes:
  // * Sunrise 1 (6 AM)
  // * Morning (10 AM)
  // * Late Afternoon (4 PM)
  // * Sunset (6 PM)
  // * Sunrise 2 (5:58 AM)
  //
  // Example of 3:
  // Now: 6:01 AM
  // Sunrise today: 6:00 AM
  // Sunrise tomorrow: 6:02 AM
  //
  // Expected checkpoint changes:
  // * Morning (10 AM)
  // * Late Afternoon (4 PM)
  // * Sunset (6 PM)
  ASSERT_GE(num_checkpoints_observed, kNumCheckpointsPerDay - 1);
  ASSERT_LE(num_checkpoints_observed, kNumCheckpointsPerDay + 1);
  for (size_t i = 1; i < num_checkpoints_observed; ++i) {
    EXPECT_EQ(
        checkpoint_observer.changes()[i].second,
        GetNextExpectedCheckpoint(checkpoint_observer.changes()[i - 1].second));
  }
}

TEST_F(ScheduledFeatureTest, RecordsScheduleTypeHistogram) {
  const std::string test_histogram_name = "Ash.Test.ScheduleType";
  base::HistogramTester histogram_tester;
  feature()->SetScheduleType(ScheduleType::kCustom);
  histogram_tester.ExpectTotalCount(test_histogram_name, 0);

  feature()->set_schedule_type_histogram_name(test_histogram_name);
  feature()->SetScheduleType(ScheduleType::kSunsetToSunrise);
  feature()->SetScheduleType(ScheduleType::kNone);
  feature()->SetScheduleType(ScheduleType::kCustom);
  histogram_tester.ExpectBucketCount(test_histogram_name, ScheduleType::kNone,
                                     1);
  histogram_tester.ExpectBucketCount(test_histogram_name,
                                     ScheduleType::kSunsetToSunrise, 1);
  histogram_tester.ExpectBucketCount(test_histogram_name, ScheduleType::kCustom,
                                     1);

  // Switching users should not count as a schedule type change even if second
  // user's schedule type is different from the first.
  base::HistogramTester histogram_tester_2;
  const ScheduleType user_1_schedule_type = feature()->GetScheduleType();
  SwitchActiveUser(kUser2Email);
  ASSERT_NE(feature()->GetScheduleType(), user_1_schedule_type);
  histogram_tester_2.ExpectTotalCount(test_histogram_name, 0);
}

}  // namespace

}  // namespace ash
