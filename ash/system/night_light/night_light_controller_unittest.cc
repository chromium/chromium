// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>
#include <sstream>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/time/time_of_day.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/pattern.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@nightlight";
constexpr char kUser2Email[] = "user2@nightlight";

enum AmPm { kAM, kPM };

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

  return TimeOfDay(hour * 60);
}

NightLightControllerImpl* GetController() {
  return Shell::Get()->night_light_controller();
}

// Returns RGB scaling factors already applied on the display's compositor in a
// Vector3df given a |display_id|.
gfx::Vector3dF GetDisplayCompositorRGBScaleFactors(int64_t display_id) {
  WindowTreeHostManager* wth_manager = Shell::Get()->window_tree_host_manager();
  aura::Window* root_window =
      wth_manager->GetRootWindowForDisplayId(display_id);
  DCHECK(root_window);
  aura::WindowTreeHost* host = root_window->GetHost();
  DCHECK(host);
  ui::Compositor* compositor = host->compositor();
  DCHECK(compositor);

  const SkM44& matrix = compositor->display_color_matrix();
  return gfx::Vector3dF(matrix.rc(0, 0), matrix.rc(1, 1), matrix.rc(2, 2));
}

// Returns a vector with a Vector3dF for each compositor.
// Each element contains RGB scaling factors.
std::vector<gfx::Vector3dF> GetAllDisplaysCompositorsRGBScaleFactors() {
  std::vector<gfx::Vector3dF> scale_factors;
  for (int64_t display_id :
       Shell::Get()->display_manager()->GetConnectedDisplayIdList()) {
    scale_factors.push_back(GetDisplayCompositorRGBScaleFactors(display_id));
  }
  return scale_factors;
}

// Tests that the given display with |display_id| has the expected color matrix
// on its compositor that corresponds to the given |temperature|.
void TestDisplayCompositorTemperature(int64_t display_id, float temperature) {
  const gfx::Vector3dF& scaling_factors =
      GetDisplayCompositorRGBScaleFactors(display_id);
  const float blue_scale = scaling_factors.z();
  const float green_scale = scaling_factors.y();
  EXPECT_FLOAT_EQ(
      blue_scale,
      NightLightControllerImpl::BlueColorScaleFromTemperature(temperature));
  EXPECT_FLOAT_EQ(green_scale,
                  NightLightControllerImpl::GreenColorScaleFromTemperature(
                      temperature, false /* in_linear_gamma_space */));
}

// Tests that the display color matrices of all compositors correctly correspond
// to the given |temperature|.
void TestCompositorsTemperature(float temperature) {
  for (int64_t display_id :
       Shell::Get()->display_manager()->GetConnectedDisplayIdList()) {
    TestDisplayCompositorTemperature(display_id, temperature);
  }
}

class TestObserver : public NightLightController::Observer {
 public:
  TestObserver() { GetController()->AddObserver(this); }
  TestObserver(const TestObserver& other) = delete;
  TestObserver& operator=(const TestObserver& rhs) = delete;
  ~TestObserver() override { GetController()->RemoveObserver(this); }

  // ash::NightLightController::Observer:
  void OnNightLightEnabledChanged(bool enabled) override { status_ = enabled; }

  bool status() const { return status_; }

 private:
  bool status_ = false;
};

constexpr double kFakePosition1_Latitude = 23.5;
constexpr double kFakePosition1_Longitude = 35.88;
constexpr int kFakePosition1_SunsetOffset = 20 * 60;
constexpr int kFakePosition1_SunriseOffset = 4 * 60;

constexpr double kFakePosition2_Latitude = 37.5;
constexpr double kFakePosition2_Longitude = -100.5;
constexpr int kFakePosition2_SunsetOffset = 17 * 60;
constexpr int kFakePosition2_SunriseOffset = 3 * 60;

class TestDelegate : public NightLightControllerImpl::Delegate {
 public:
  TestDelegate() = default;
  TestDelegate(const TestDelegate& other) = delete;
  TestDelegate& operator=(const TestDelegate& rhs) = delete;
  ~TestDelegate() override = default;

  void SetFakeNow(base::Time time) { fake_now_ = time; }
  void SetFakeNow(TimeOfDay time) { fake_now_ = time.ToTimeToday(); }
  void SetFakeSunset(TimeOfDay time) { fake_sunset_ = time.ToTimeToday(); }
  void SetFakeSunrise(TimeOfDay time) { fake_sunrise_ = time.ToTimeToday(); }

  // ash::NightLightControllerImpl::Delegate
  base::Time GetNow() const override { return fake_now_; }
  base::Time GetSunsetTime() const override { return fake_sunset_; }
  base::Time GetSunriseTime() const override { return fake_sunrise_; }
  bool SetGeoposition(
      const NightLightController::SimpleGeoposition& position) override {
    has_geoposition_ = true;
    if (position == NightLightController::SimpleGeoposition{
                        kFakePosition1_Latitude, kFakePosition1_Longitude}) {
      // Set sunset and sunrise times associated with fake position 1.
      SetFakeSunset(TimeOfDay(kFakePosition1_SunsetOffset));
      SetFakeSunrise(TimeOfDay(kFakePosition1_SunriseOffset));
    } else if (position ==
               NightLightController::SimpleGeoposition{
                   kFakePosition2_Latitude, kFakePosition2_Longitude}) {
      // Set sunset and sunrise times associated with fake position 2.
      SetFakeSunset(TimeOfDay(kFakePosition2_SunsetOffset));
      SetFakeSunrise(TimeOfDay(kFakePosition2_SunriseOffset));
    }
    return true;
  }
  bool HasGeoposition() const override { return has_geoposition_; }

 private:
  base::Time fake_now_;
  base::Time fake_sunset_;
  base::Time fake_sunrise_;
  bool has_geoposition_ = false;
};

class NightLightTest : public NoSessionAshTestBase {
 public:
  NightLightTest() : delegate_(new TestDelegate) {}
  NightLightTest(const NightLightTest& other) = delete;
  NightLightTest& operator=(const NightLightTest& rhs) = delete;
  ~NightLightTest() override = default;

  PrefService* user1_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  PrefService* user2_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser2Email));
  }

  TestDelegate* delegate() const { return delegate_; }

  // AshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    GetController()->SetDelegateForTesting(base::WrapUnique(delegate_.get()));

    CreateTestUserSessions();

    // Simulate user 1 login.
    SimulateNewUserFirstLogin(kUser1Email);

    // Start with ambient color pref disabled.
    SetAmbientColorPrefEnabled(false);
  }

  void CreateTestUserSessions() {
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  void SetNightLightEnabled(bool enabled) {
    GetController()->SetEnabled(
        enabled, NightLightControllerImpl::AnimationDuration::kShort);
  }

  void SetAmbientColorPrefEnabled(bool enabled) {
    GetController()->SetAmbientColorEnabled(enabled);
  }

  // Simulate powerd sending multiple times an ambient temperature of
  // |powerd_temperature|. The remapped ambient temperature should eventually
  // reach |target_remapped_temperature|.
  float SimulateAmbientColorFromPowerd(int32_t powerd_temperature,
                                       float target_remapped_temperature) {
    auto* controller = GetController();
    int max_steps = 1000;
    float ambient_temperature = 0.0f;
    const float initial_difference =
        controller->ambient_temperature() - target_remapped_temperature;
    do {
      controller->AmbientColorChanged(powerd_temperature);
      ambient_temperature = controller->ambient_temperature();
    } while (max_steps-- &&
             ((ambient_temperature - target_remapped_temperature) *
              initial_difference) > 0.0f);
    // We should reach the expected remapped temperature.
    EXPECT_GT(max_steps, 0);

    return ambient_temperature;
  }

 private:
  raw_ptr<TestDelegate, ExperimentalAsh> delegate_ = nullptr;  // Not owned.
};

// Tests toggling NightLight on / off and makes sure the observer is updated and
// the layer temperatures are modified.
TEST_F(NightLightTest, TestToggle) {
  UpdateDisplay("800x600,800x600");

  TestObserver observer;
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(false);
  ASSERT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  controller->Toggle();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_TRUE(observer.status());
  TestCompositorsTemperature(GetController()->GetColorTemperature());
  controller->Toggle();
  EXPECT_FALSE(controller->GetEnabled());
  EXPECT_FALSE(observer.status());
  TestCompositorsTemperature(0.0f);
}

// Tests setting the temperature in various situations.
TEST_F(NightLightTest, TestSetTemperature) {
  UpdateDisplay("800x600,800x600");

  TestObserver observer;
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(false);
  ASSERT_FALSE(controller->GetEnabled());

  // Setting the temperature while NightLight is disabled only changes the
  // color temperature field, but root layers temperatures are not affected, nor
  // the status of NightLight itself.
  const float temperature1 = 0.2f;
  controller->SetColorTemperature(temperature1);
  EXPECT_EQ(temperature1, controller->GetColorTemperature());
  TestCompositorsTemperature(0.0f);

  // When NightLight is enabled, temperature changes actually affect the root
  // layers temperatures.
  SetNightLightEnabled(true);
  ASSERT_TRUE(controller->GetEnabled());
  const float temperature2 = 0.7f;
  controller->SetColorTemperature(temperature2);
  EXPECT_EQ(temperature2, controller->GetColorTemperature());
  TestCompositorsTemperature(temperature2);

  // When NightLight is disabled, the value of the color temperature field
  // doesn't change, however the temperatures set on the root layers are all
  // 0.0f. Observers only receive an enabled status change notification; no
  // temperature change notification.
  SetNightLightEnabled(false);
  ASSERT_FALSE(controller->GetEnabled());
  EXPECT_FALSE(observer.status());
  EXPECT_EQ(temperature2, controller->GetColorTemperature());
  TestCompositorsTemperature(0.0f);

  // When re-enabled, the stored temperature is re-applied.
  SetNightLightEnabled(true);
  EXPECT_TRUE(observer.status());
  ASSERT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(temperature2);
}

TEST_F(NightLightTest, TestNightLightWithDisplayConfigurationChanges) {
  // Start with one display and enable NightLight.
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(true);
  ASSERT_TRUE(controller->GetEnabled());
  const float temperature = 0.2f;
  controller->SetColorTemperature(temperature);
  EXPECT_EQ(temperature, controller->GetColorTemperature());
  TestCompositorsTemperature(temperature);

  // Add a new display, and expect that its compositor gets the already set from
  // before color temperature.
  display_manager()->AddRemoveDisplay();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, RootWindowController::root_window_controllers().size());
  TestCompositorsTemperature(temperature);

  // While we have the second display, enable mirror mode, the compositors
  // should still have the same temperature.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, absl::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  base::RunLoop().RunUntilIdle();
  TestCompositorsTemperature(temperature);

  // Exit mirror mode, temperature is still applied.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, absl::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  base::RunLoop().RunUntilIdle();
  TestCompositorsTemperature(temperature);

  // Enter unified mode, temperature is still applied.
  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());
  base::RunLoop().RunUntilIdle();
  TestCompositorsTemperature(temperature);
  // The unified host should never have a temperature on its compositor.
  TestDisplayCompositorTemperature(display::kUnifiedDisplayId, 0.0f);

  // Exit unified mode, and remove the display, temperature should remain the
  // same.
  display_manager()->SetUnifiedDesktopEnabled(false);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
  base::RunLoop().RunUntilIdle();
  TestCompositorsTemperature(temperature);

  display_manager()->AddRemoveDisplay();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, RootWindowController::root_window_controllers().size());
  TestCompositorsTemperature(temperature);
}

// Tests that switching users retrieves NightLight settings for the active
// user's prefs.
TEST_F(NightLightTest, TestUserSwitchAndSettingsPersistence) {
  // Test start with user1 logged in.
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(true);
  EXPECT_TRUE(controller->GetEnabled());
  const float user1_temperature = 0.8f;
  controller->SetColorTemperature(user1_temperature);
  EXPECT_EQ(user1_temperature, controller->GetColorTemperature());
  TestCompositorsTemperature(user1_temperature);

  // Switch to user 2, and expect NightLight to be disabled.
  SwitchActiveUser(kUser2Email);
  EXPECT_FALSE(controller->GetEnabled());
  // Changing user_2's color temperature shouldn't affect user_1's settings.
  const float user2_temperature = 0.2f;
  user2_pref_service()->SetDouble(prefs::kNightLightTemperature,
                                  user2_temperature);
  EXPECT_EQ(user2_temperature, controller->GetColorTemperature());
  TestCompositorsTemperature(0.0f);
  EXPECT_EQ(user1_temperature,
            user1_pref_service()->GetDouble(prefs::kNightLightTemperature));

  // Switch back to user 1, to find NightLight is still enabled, and the same
  // user's color temperature are re-applied.
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_EQ(user1_temperature, controller->GetColorTemperature());
  TestCompositorsTemperature(user1_temperature);
}

// Tests that changes from outside NightLightControlled to the NightLight
// Preferences are seen by the controlled and applied properly.
TEST_F(NightLightTest, TestOutsidePreferencesChangesAreApplied) {
  // Test start with user1 logged in.
  NightLightControllerImpl* controller = GetController();
  user1_pref_service()->SetBoolean(prefs::kNightLightEnabled, true);
  EXPECT_TRUE(controller->GetEnabled());
  const float temperature1 = 0.65f;
  user1_pref_service()->SetDouble(prefs::kNightLightTemperature, temperature1);
  EXPECT_EQ(temperature1, controller->GetColorTemperature());
  TestCompositorsTemperature(temperature1);
  const float temperature2 = 0.23f;
  user1_pref_service()->SetDouble(prefs::kNightLightTemperature, temperature2);
  EXPECT_EQ(temperature2, controller->GetColorTemperature());
  TestCompositorsTemperature(temperature2);
  user1_pref_service()->SetBoolean(prefs::kNightLightEnabled, false);
  EXPECT_FALSE(controller->GetEnabled());
}

// Tests transitioning from kNone to kCustom and back to kNone schedule types.
TEST_F(NightLightTest, TestScheduleNoneToCustomTransition) {
  NightLightControllerImpl* controller = GetController();
  // Now is 6:00 PM.
  delegate()->SetFakeNow(TimeOfDay(18 * 60));
  SetNightLightEnabled(false);
  controller->SetScheduleType(NightLightController::ScheduleType::kNone);
  // Start time is at 3:00 PM and end time is at 8:00 PM.
  controller->SetCustomStartTime(TimeOfDay(15 * 60));
  controller->SetCustomEndTime(TimeOfDay(20 * 60));

  //      15:00         18:00         20:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //      start          now           end
  //
  // Even though "Now" is inside the NightLight interval, nothing should change,
  // since the schedule type is "none".
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);

  // Now change the schedule type to custom, NightLight should turn on
  // immediately with a short animation duration, and the timer should be
  // running with a delay of exactly 2 hours scheduling the end.
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);
  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(controller->GetColorTemperature());
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kShort,
            controller->last_animation_duration());
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(2), controller->timer()->GetCurrentDelay());

  // If the user changes the schedule type to "none", the NightLight status
  // should not change, but the timer should not be running.
  controller->SetScheduleType(NightLightController::ScheduleType::kNone);
  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(controller->GetColorTemperature());
  EXPECT_FALSE(controller->timer()->IsRunning());
}

// Tests what happens when the time now reaches the end of the NightLight
// interval when NightLight mode is on.
TEST_F(NightLightTest, TestCustomScheduleReachingEndTime) {
  NightLightControllerImpl* controller = GetController();
  delegate()->SetFakeNow(TimeOfDay(18 * 60));
  controller->SetCustomStartTime(TimeOfDay(15 * 60));
  controller->SetCustomEndTime(TimeOfDay(20 * 60));
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);
  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(controller->GetColorTemperature());

  // Simulate reaching the end time by triggering the timer's user task. Make
  // sure that NightLight ended with a long animation.
  //
  //      15:00                      20:00
  // <----- + ------------------------ + ----->
  //        |                          |
  //      start                    end & now
  //
  // Now is 8:00 PM.
  delegate()->SetFakeNow(TimeOfDay(20 * 60));
  controller->timer()->FireNow();
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kLong,
            controller->last_animation_duration());
  // The timer should still be running, but now scheduling the start at 3:00 PM
  // tomorrow which is 19 hours from "now" (8:00 PM).
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(19), controller->timer()->GetCurrentDelay());
}

// Tests that user toggles from the system menu or system settings override any
// status set by an automatic schedule.
TEST_F(NightLightTest, TestExplicitUserTogglesWhileScheduleIsActive) {
  // Start with the below custom schedule, where NightLight is off.
  //
  //      15:00               20:00          23:00
  // <----- + ----------------- + ------------ + ---->
  //        |                   |              |
  //      start                end            now
  //
  NightLightControllerImpl* controller = GetController();
  delegate()->SetFakeNow(TimeOfDay(23 * 60));
  controller->SetCustomStartTime(TimeOfDay(15 * 60));
  controller->SetCustomEndTime(TimeOfDay(20 * 60));
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);

  // What happens if the user manually turns NightLight on while the schedule
  // type says it should be off?
  // User toggles either from the system menu or the System Settings toggle
  // button must override any automatic schedule, and should be performed with
  // the short animation duration.
  controller->Toggle();
  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(controller->GetColorTemperature());
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kShort,
            controller->last_animation_duration());
  // The timer should still be running, but NightLight should automatically
  // turn off at 8:00 PM tomorrow, which is 21 hours from now (11:00 PM).
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(21), controller->timer()->GetCurrentDelay());

  // Manually turning it back off should also be respected, and this time the
  // start is scheduled at 3:00 PM tomorrow after 19 hours from "now" (8:00 PM).
  controller->Toggle();
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kShort,
            controller->last_animation_duration());
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(16), controller->timer()->GetCurrentDelay());
}

// Tests that changing the custom start and end times, in such a way that
// shouldn't change the current status, only updates the timer but doesn't
// change the status.
TEST_F(NightLightTest, TestChangingStartTimesThatDontChangeTheStatus) {
  //       16:00        18:00         22:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now          start          end
  //
  NightLightControllerImpl* controller = GetController();
  delegate()->SetFakeNow(TimeOfDay(16 * 60));  // 4:00 PM.
  SetNightLightEnabled(false);
  controller->SetScheduleType(NightLightController::ScheduleType::kNone);
  controller->SetCustomStartTime(TimeOfDay(18 * 60));  // 6:00 PM.
  controller->SetCustomEndTime(TimeOfDay(22 * 60));    // 10:00 PM.

  // Since now is outside the NightLight interval, changing the schedule type
  // to kCustom, shouldn't affect the status. Validate the timer is running with
  // a 2-hour delay.
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(2), controller->timer()->GetCurrentDelay());

  // Change the start time in such a way that doesn't change the status, but
  // despite that, confirm that schedule has been updated.
  controller->SetCustomStartTime(TimeOfDay(19 * 60));  // 7:00 PM.
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(3), controller->timer()->GetCurrentDelay());

  // Changing the end time in a similar fashion to the above and expect no
  // change.
  controller->SetCustomEndTime(TimeOfDay(23 * 60));  // 11:00 PM.
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(3), controller->timer()->GetCurrentDelay());
}

// Tests the behavior of the sunset to sunrise automatic schedule type.
TEST_F(NightLightTest, TestSunsetSunrise) {
  //      16:00         18:00     20:00      22:00              5:00
  // <----- + ----------- + ------- + -------- + --------------- + ------->
  //        |             |         |          |                 |
  //       now      custom start  sunset   custom end         sunrise
  //
  NightLightControllerImpl* controller = GetController();
  delegate()->SetFakeNow(TimeOfDay(16 * 60));     // 4:00 PM.
  delegate()->SetFakeSunset(TimeOfDay(20 * 60));  // 8:00 PM.
  delegate()->SetFakeSunrise(TimeOfDay(5 * 60));  // 5:00 AM.
  SetNightLightEnabled(false);
  controller->SetScheduleType(NightLightController::ScheduleType::kNone);
  controller->SetCustomStartTime(TimeOfDay(18 * 60));  // 6:00 PM.
  controller->SetCustomEndTime(TimeOfDay(22 * 60));    // 10:00 PM.

  // Custom times should have no effect when the schedule type is sunset to
  // sunrise.
  controller->SetScheduleType(
      NightLightController::ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(4), controller->timer()->GetCurrentDelay());

  // Simulate reaching sunset.
  delegate()->SetFakeNow(TimeOfDay(20 * 60));  // Now is 8:00 PM.
  controller->timer()->FireNow();
  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(controller->GetColorTemperature());
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kLong,
            controller->last_animation_duration());
  // Timer is running scheduling the end at sunrise.
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(9), controller->timer()->GetCurrentDelay());

  // Simulate reaching sunrise.
  delegate()->SetFakeNow(TimeOfDay(5 * 60));  // Now is 5:00 AM.
  controller->timer()->FireNow();
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kLong,
            controller->last_animation_duration());
  // Timer is running scheduling the start at the next sunset.
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(15), controller->timer()->GetCurrentDelay());
}

// Tests the behavior of the sunset to sunrise automatic schedule type when the
// client sets the geoposition.
TEST_F(NightLightTest, TestSunsetSunriseGeoposition) {
  // Position 1 sunset and sunrise times.
  //
  //      16:00       20:00               4:00
  // <----- + --------- + ---------------- + ------->
  //        |           |                  |
  //       now        sunset            sunrise
  //
  NightLightControllerImpl* controller = GetController();
  delegate()->SetFakeNow(TimeOfDay(16 * 60));  // 4:00PM.
  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition1_Latitude, kFakePosition1_Longitude});

  // Expect that timer is running and the start is scheduled after 4 hours.
  controller->SetScheduleType(
      NightLightController::ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(4), controller->timer()->GetCurrentDelay());

  // Simulate reaching sunset.
  delegate()->SetFakeNow(TimeOfDay(20 * 60));  // Now is 8:00 PM.
  controller->timer()->FireNow();
  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(controller->GetColorTemperature());
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kLong,
            controller->last_animation_duration());
  // Timer is running scheduling the end at sunrise.
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(8), controller->timer()->GetCurrentDelay());

  // Now simulate user changing position.
  // Position 2 sunset and sunrise times.
  //
  //      17:00       20:00               3:00
  // <----- + --------- + ---------------- + ------->
  //        |           |                  |
  //      sunset       now               sunrise
  //
  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition2_Latitude, kFakePosition2_Longitude});

  // Expect that the scheduled end delay has been updated, and the status hasn't
  // changed.
  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(controller->GetColorTemperature());
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(7), controller->timer()->GetCurrentDelay());

  // Simulate reaching sunrise.
  delegate()->SetFakeNow(TimeOfDay(3 * 60));  // Now is 5:00 AM.
  controller->timer()->FireNow();
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kLong,
            controller->last_animation_duration());
  // Timer is running scheduling the start at the next sunset.
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(14), controller->timer()->GetCurrentDelay());
}

// Tests the behavior when the client sets the geoposition while in custom
// schedule setting. Current time is simulated to be updated accordingly. The
// current time change should bring the controller into or take it out of the
// night light mode accordingly if necessary, based on the settings.

// Failed on 5 linux chromeos builds. http://crbug.com/1059626
TEST_F(NightLightTest, DISABLED_TestCustomScheduleGeopositionChanges) {
  constexpr int kCustom_Start = 19 * 60;
  constexpr int kCustom_End = 2 * 60;

  // Returns the positive difference in minutes given t1 and t2 in minutes
  auto time_diff = [](int t1, int t2) {
    int t = t2 - t1;
    return t < 0 ? 24 * 60 + t : t;
  };

  NightLightControllerImpl* controller = GetController();
  controller->SetCustomStartTime(TimeOfDay(kCustom_Start));
  controller->SetCustomEndTime(TimeOfDay(kCustom_End));

  // Position 1 current time and custom start and end time.
  //
  //      16:00       19:00             2:00
  // <----- + --------- + --------------- + ------------->
  //        |           |                 |
  //       now     custom start      custom end
  //

  int fake_now = 16 * 60;
  delegate()->SetFakeNow(TimeOfDay(fake_now));
  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition1_Latitude, kFakePosition1_Longitude});

  // Expect that timer is running and is scheduled at next custom start time.
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Minutes(time_diff(fake_now, kCustom_Start)),
            controller->timer()->GetCurrentDelay());

  // Simulate a timezone change by changing geoposition.
  // Current time updates to 9PM.
  //      19:00       21:00       2:00
  // <----- + --------- + -------- + --------------------->
  //        |           |          |
  //   custom start    now      custom end
  //
  fake_now = 21 * 60;
  delegate()->SetFakeNow(TimeOfDay(fake_now));
  controller->timer()->FireNow();
  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition2_Latitude, kFakePosition2_Longitude});

  // Expect the controller to enter night light mode and  the scheduled end
  // delay has been updated.
  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(controller->GetColorTemperature());
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kShort,
            controller->last_animation_duration());
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Minutes(time_diff(fake_now, kCustom_End)),
            controller->timer()->GetCurrentDelay());

  // Simulate user changing position back to location 1 and current time goes
  // back to 4PM.
  fake_now = 16 * 60;
  delegate()->SetFakeNow(TimeOfDay(fake_now));
  controller->timer()->FireNow();

  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition1_Latitude, kFakePosition1_Longitude});
  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_EQ(NightLightControllerImpl::AnimationDuration::kShort,
            controller->last_animation_duration());
  // Timer is running and is scheduled at next custom start time.
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Minutes(time_diff(fake_now, kCustom_Start)),
            controller->timer()->GetCurrentDelay());
}

// Tests the behavior when there is no valid geoposition for example due to lack
// of connectivity.
TEST_F(NightLightTest, AbsentValidGeoposition) {
  NightLightControllerImpl* controller = GetController();
  ASSERT_FALSE(delegate()->HasGeoposition());

  // Initially, no values are stored in either of the two users' prefs.
  ASSERT_FALSE(
      user1_pref_service()->HasPrefPath(prefs::kNightLightCachedLatitude));
  ASSERT_FALSE(
      user1_pref_service()->HasPrefPath(prefs::kNightLightCachedLongitude));
  ASSERT_FALSE(
      user2_pref_service()->HasPrefPath(prefs::kNightLightCachedLatitude));
  ASSERT_FALSE(
      user2_pref_service()->HasPrefPath(prefs::kNightLightCachedLongitude));

  // Store fake geoposition 2 in user 2's prefs.
  user2_pref_service()->SetDouble(prefs::kNightLightCachedLatitude,
                                  kFakePosition2_Latitude);
  user2_pref_service()->SetDouble(prefs::kNightLightCachedLongitude,
                                  kFakePosition2_Longitude);

  // Switch to user 2 and expect that the delegate now has a geoposition, but
  // the controller knows that it's from a cached value.
  SwitchActiveUser(kUser2Email);
  EXPECT_TRUE(delegate()->HasGeoposition());
  EXPECT_TRUE(controller->is_current_geoposition_from_cache());
  const TimeOfDay kSunset2{kFakePosition2_SunsetOffset};
  const TimeOfDay kSunrise2{kFakePosition2_SunriseOffset};
  EXPECT_EQ(delegate()->GetSunsetTime(), kSunset2.ToTimeToday());
  EXPECT_EQ(delegate()->GetSunriseTime(), kSunrise2.ToTimeToday());

  // Store fake geoposition 1 in user 1's prefs.
  user1_pref_service()->SetDouble(prefs::kNightLightCachedLatitude,
                                  kFakePosition1_Latitude);
  user1_pref_service()->SetDouble(prefs::kNightLightCachedLongitude,
                                  kFakePosition1_Longitude);

  // Switching to user 1 should ignore the current geoposition since it's
  // a cached value from user 2's prefs rather than a newly-updated value.
  // User 1's cached values should be loaded.
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(delegate()->HasGeoposition());
  EXPECT_TRUE(controller->is_current_geoposition_from_cache());
  const TimeOfDay kSunset1{kFakePosition1_SunsetOffset};
  const TimeOfDay kSunrise1{kFakePosition1_SunriseOffset};
  EXPECT_EQ(delegate()->GetSunsetTime(), kSunset1.ToTimeToday());
  EXPECT_EQ(delegate()->GetSunriseTime(), kSunrise1.ToTimeToday());

  // Now simulate receiving a geoposition update of fake geoposition 2.
  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition2_Latitude, kFakePosition2_Longitude});
  EXPECT_TRUE(delegate()->HasGeoposition());
  EXPECT_FALSE(controller->is_current_geoposition_from_cache());
  EXPECT_EQ(delegate()->GetSunsetTime(), kSunset2.ToTimeToday());
  EXPECT_EQ(delegate()->GetSunriseTime(), kSunrise2.ToTimeToday());

  // Update user 2's prefs with fake geoposition 1.
  user2_pref_service()->SetDouble(prefs::kNightLightCachedLatitude,
                                  kFakePosition1_Latitude);
  user2_pref_service()->SetDouble(prefs::kNightLightCachedLongitude,
                                  kFakePosition1_Longitude);

  // Now switching to user 2 should completely ignore their cached geopsoition,
  // since from now on we have a valid newly-retrieved value.
  SwitchActiveUser(kUser2Email);
  EXPECT_TRUE(delegate()->HasGeoposition());
  EXPECT_FALSE(controller->is_current_geoposition_from_cache());
  EXPECT_EQ(delegate()->GetSunsetTime(), kSunset2.ToTimeToday());
  EXPECT_EQ(delegate()->GetSunriseTime(), kSunrise2.ToTimeToday());

  // Clear all cached geoposition prefs for all users, just to make sure getting
  // a new geoposition with persist it for all users not just the active one.
  user1_pref_service()->ClearPref(prefs::kNightLightCachedLatitude);
  user1_pref_service()->ClearPref(prefs::kNightLightCachedLongitude);
  user2_pref_service()->ClearPref(prefs::kNightLightCachedLatitude);
  user2_pref_service()->ClearPref(prefs::kNightLightCachedLongitude);

  // Now simulate receiving a geoposition update of fake geoposition 1.
  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition1_Latitude, kFakePosition1_Longitude});
  EXPECT_TRUE(delegate()->HasGeoposition());
  EXPECT_FALSE(controller->is_current_geoposition_from_cache());
  EXPECT_EQ(delegate()->GetSunsetTime(), kSunset1.ToTimeToday());
  EXPECT_EQ(delegate()->GetSunriseTime(), kSunrise1.ToTimeToday());
  EXPECT_EQ(kFakePosition1_Latitude,
            user1_pref_service()->GetDouble(prefs::kNightLightCachedLatitude));
  EXPECT_EQ(kFakePosition1_Longitude,
            user1_pref_service()->GetDouble(prefs::kNightLightCachedLongitude));
  EXPECT_EQ(kFakePosition1_Latitude,
            user2_pref_service()->GetDouble(prefs::kNightLightCachedLatitude));
  EXPECT_EQ(kFakePosition1_Longitude,
            user2_pref_service()->GetDouble(prefs::kNightLightCachedLongitude));
}

// Tests that on device resume from sleep, the NightLight status is updated
// correctly if the time has changed meanwhile.
TEST_F(NightLightTest, TestCustomScheduleOnResume) {
  NightLightControllerImpl* controller = GetController();
  // Now is 4:00 PM.
  delegate()->SetFakeNow(TimeOfDay(16 * 60));
  SetNightLightEnabled(false);
  // Start time is at 6:00 PM and end time is at 10:00 PM. NightLight should be
  // off.
  //      16:00         18:00         22:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now          start          end
  //
  controller->SetColorTemperature(0.4f);
  controller->SetCustomStartTime(TimeOfDay(18 * 60));
  controller->SetCustomEndTime(TimeOfDay(22 * 60));
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);

  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  // NightLight should start in 2 hours.
  EXPECT_EQ(base::Hours(2), controller->timer()->GetCurrentDelay());

  // Now simulate that the device was suspended for 3 hours, and the time now
  // is 7:00 PM when the devices was resumed. Expect that NightLight turns on.
  delegate()->SetFakeNow(TimeOfDay(19 * 60));
  controller->SuspendDone(base::TimeDelta::Max());

  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(0.4f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  // NightLight should end in 3 hours.
  EXPECT_EQ(base::Hours(3), controller->timer()->GetCurrentDelay());
}

// The following tests ensure that the NightLight schedule is correctly
// refreshed when the start and end times are inverted (i.e. the "start time" as
// a time of day today is in the future with respect to the "end time" also as a
// time of day today).
//
// Case 1: "Now" is less than both "end" and "start".
TEST_F(NightLightTest, TestCustomScheduleInvertedStartAndEndTimesCase1) {
  NightLightControllerImpl* controller = GetController();
  // Now is 4:00 AM.
  delegate()->SetFakeNow(TimeOfDay(4 * 60));
  SetNightLightEnabled(false);
  // Start time is at 9:00 PM and end time is at 6:00 AM. "Now" is less than
  // both. NightLight should be on.
  //       4:00          6:00         21:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now           end          start
  //
  controller->SetColorTemperature(0.4f);
  controller->SetCustomStartTime(TimeOfDay(21 * 60));
  controller->SetCustomEndTime(TimeOfDay(6 * 60));
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);

  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(0.4f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  // NightLight should end in two hours.
  EXPECT_EQ(base::Hours(2), controller->timer()->GetCurrentDelay());
}

// Case 2: "Now" is between "end" and "start".
TEST_F(NightLightTest, TestCustomScheduleInvertedStartAndEndTimesCase2) {
  NightLightControllerImpl* controller = GetController();
  // Now is 6:00 AM.
  delegate()->SetFakeNow(TimeOfDay(6 * 60));
  SetNightLightEnabled(false);
  // Start time is at 9:00 PM and end time is at 4:00 AM. "Now" is between both.
  // NightLight should be off.
  //       4:00          6:00         21:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       end           now          start
  //
  controller->SetColorTemperature(0.4f);
  controller->SetCustomStartTime(TimeOfDay(21 * 60));
  controller->SetCustomEndTime(TimeOfDay(4 * 60));
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);

  EXPECT_FALSE(controller->GetEnabled());
  TestCompositorsTemperature(0.0f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  // NightLight should start in 15 hours.
  EXPECT_EQ(base::Hours(15), controller->timer()->GetCurrentDelay());
}

// Case 3: "Now" is greater than both "start" and "end".
TEST_F(NightLightTest, TestCustomScheduleInvertedStartAndEndTimesCase3) {
  NightLightControllerImpl* controller = GetController();
  // Now is 11:00 PM.
  delegate()->SetFakeNow(TimeOfDay(23 * 60));
  SetNightLightEnabled(false);
  // Start time is at 9:00 PM and end time is at 4:00 AM. "Now" is greater than
  // both. NightLight should be on.
  //       4:00         21:00         23:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       end          start          now
  //
  controller->SetColorTemperature(0.4f);
  controller->SetCustomStartTime(TimeOfDay(21 * 60));
  controller->SetCustomEndTime(TimeOfDay(4 * 60));
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);

  EXPECT_TRUE(controller->GetEnabled());
  TestCompositorsTemperature(0.4f);
  EXPECT_TRUE(controller->timer()->IsRunning());
  // NightLight should end in 5 hours.
  EXPECT_EQ(base::Hours(5), controller->timer()->GetCurrentDelay());
}

TEST_F(NightLightTest, TestAmbientLightEnabledSetting_FeatureOn) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAllowAmbientEQ);

  // Feature enabled, Pref disabled -> disabled
  SetAmbientColorPrefEnabled(false);
  EXPECT_FALSE(GetController()->GetAmbientColorEnabled());

  // Feature enabled, Pref enabled -> enabled
  SetAmbientColorPrefEnabled(true);
  EXPECT_TRUE(GetController()->GetAmbientColorEnabled());
}

TEST_F(NightLightTest, TestAmbientLightEnabledSetting_FeatureOff) {
  // With the feature disabled it should always be disabled.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAllowAmbientEQ);

  // Feature disabled, Pref disabled -> disabled
  SetAmbientColorPrefEnabled(false);
  EXPECT_FALSE(GetController()->GetAmbientColorEnabled());

  // Feature disabled, Pref enabled -> disabled
  SetAmbientColorPrefEnabled(true);
  EXPECT_FALSE(GetController()->GetAmbientColorEnabled());
}

TEST_F(NightLightTest, TestAmbientLightRemappingTemperature) {
  NightLightControllerImpl* controller = GetController();

  // Test that at the beginning the ambient temperature is neutral.
  constexpr float kNeutralColorTemperatureInKelvin = 6500;
  EXPECT_EQ(kNeutralColorTemperatureInKelvin,
            controller->ambient_temperature());

  controller->SetAmbientColorEnabled(true);
  EXPECT_EQ(kNeutralColorTemperatureInKelvin,
            controller->ambient_temperature());

  // Simulate powerd sending multiple times an ambient temperature of 8000.
  // The remapped ambient temperature should grow and eventually reach ~7400.
  float ambient_temperature = SimulateAmbientColorFromPowerd(8000, 7400.0f);

  // If powerd sends the same temperature, the remapped temperature should not
  // change.
  controller->AmbientColorChanged(8000);
  EXPECT_EQ(ambient_temperature, controller->ambient_temperature());

  // Simulate powerd sending multiple times an ambient temperature of 2700.
  // The remapped ambient temperature should grow and eventually reach 4500.
  ambient_temperature = SimulateAmbientColorFromPowerd(2700, 4500.0f);

  // Disabling ambient color should not affect the returned temperature.
  controller->SetAmbientColorEnabled(false);
  EXPECT_EQ(ambient_temperature, controller->ambient_temperature());

  // Re-enabling should still keep the same temperature.
  controller->SetAmbientColorEnabled(true);
  EXPECT_EQ(ambient_temperature, controller->ambient_temperature());
}

// Tests that manual changes to NightLight status while a schedule is being used
// will be remembered and reapplied across user switches.
TEST_F(NightLightTest, MultiUserManualStatusToggleWithSchedules) {
  // Setup user 1 to use a custom schedule from 3pm till 8pm, and user 2 to use
  // a sunset-to-sunrise schedule from 5pm till 4am.
  //
  //
  //          |<--- User 1 NL on --->|
  //          |                      |
  // <--------+--------+-------------+----------------------------+----------->
  //         3pm      5pm           8pm                          4am
  //                   |                                          |
  //                   |<-------------- User 2 NL on ------------>|
  //
  // Test cases at:
  //
  // <---+---------+------------+------------+----------------------------+--->
  //    2pm       4pm         7pm           10pm                         9am
  //

  delegate()->SetFakeNow(MakeTimeOfDay(2, kPM));
  delegate()->SetFakeSunset(MakeTimeOfDay(5, kPM));
  delegate()->SetFakeSunrise(MakeTimeOfDay(4, kAM));

  constexpr float kUser1Temperature = 0.6f;
  constexpr float kUser2Temperature = 0.8f;

  NightLightControllerImpl* controller = GetController();
  controller->SetCustomStartTime(MakeTimeOfDay(3, kPM));
  controller->SetCustomEndTime(MakeTimeOfDay(8, kPM));
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);
  controller->SetColorTemperature(kUser1Temperature);
  SwitchActiveUser(kUser2Email);
  controller->SetScheduleType(
      NightLightController::ScheduleType::kSunsetToSunrise);
  controller->SetColorTemperature(kUser2Temperature);
  SwitchActiveUser(kUser1Email);

  struct {
    base::Time fake_now;
    bool user_1_expected_status;
    bool user_2_expected_status;
  } kTestCases[] = {
      {MakeTimeOfDay(2, kPM).ToTimeToday(), false, false},
      {MakeTimeOfDay(4, kPM).ToTimeToday(), true, false},
      {MakeTimeOfDay(7, kPM).ToTimeToday(), true, true},
      {MakeTimeOfDay(10, kPM).ToTimeToday(), false, true},
      {MakeTimeOfDay(9, kAM).ToTimeToday() +
           base::Days(1),  // 9:00 AM tomorrow.
       false, false},
  };

  // Verifies that NightLight status is |expected_status| and the given
  // |user_temperature| is applied only when NightLight is expected to be
  // enabled.
  auto verify_night_light_state = [controller](bool expected_status,
                                               float user_temperature) {
    EXPECT_EQ(expected_status, controller->GetEnabled());
    TestCompositorsTemperature(expected_status ? user_temperature : 0.0f);
  };

  bool user_1_previous_status = false;
  for (const auto& test_case : kTestCases) {
    // Each test case begins when user_1 is active.
    SCOPED_TRACE(TimeOfDay::FromTime(test_case.fake_now).ToString());

    const bool user_1_toggled_status = !test_case.user_1_expected_status;
    const bool user_2_toggled_status = !test_case.user_2_expected_status;

    // Apply the test's case fake time, and fire the timer if there's a change
    // expected in NightLight's status.
    delegate()->SetFakeNow(test_case.fake_now);
    if (user_1_previous_status != test_case.user_1_expected_status)
      controller->timer()->FireNow();
    user_1_previous_status = test_case.user_1_expected_status;

    // The untoggled states for both users should match the expected ones
    // according to their schedules.
    verify_night_light_state(test_case.user_1_expected_status,
                             kUser1Temperature);
    SwitchActiveUser(kUser2Email);
    verify_night_light_state(test_case.user_2_expected_status,
                             kUser2Temperature);

    // Manually toggle NightLight for user_2 and expect that it will be
    // remembered when we switch to user_1 and back.
    controller->Toggle();
    verify_night_light_state(user_2_toggled_status, kUser2Temperature);
    SwitchActiveUser(kUser1Email);
    verify_night_light_state(test_case.user_1_expected_status,
                             kUser1Temperature);
    SwitchActiveUser(kUser2Email);
    verify_night_light_state(user_2_toggled_status, kUser2Temperature);

    // Toggle it for user_1 as well, and expect it will be remembered and won't
    // affect the already toggled state for user_2.
    SwitchActiveUser(kUser1Email);
    verify_night_light_state(test_case.user_1_expected_status,
                             kUser1Temperature);
    controller->Toggle();
    verify_night_light_state(user_1_toggled_status, kUser1Temperature);
    SwitchActiveUser(kUser2Email);
    verify_night_light_state(user_2_toggled_status, kUser2Temperature);

    // Toggle both users back to their original states in preparation for the
    // next test case.
    controller->Toggle();
    verify_night_light_state(test_case.user_2_expected_status,
                             kUser2Temperature);
    SwitchActiveUser(kUser1Email);
    verify_night_light_state(user_1_toggled_status, kUser1Temperature);
    controller->Toggle();
    verify_night_light_state(test_case.user_1_expected_status,
                             kUser1Temperature);
  }
}

TEST_F(NightLightTest, ManualStatusToggleCanPersistAfterResumeFromSuspend) {
  delegate()->SetFakeNow(MakeTimeOfDay(11, kAM));
  NightLightControllerImpl* controller = GetController();
  controller->SetCustomStartTime(MakeTimeOfDay(3, kPM));
  controller->SetCustomEndTime(MakeTimeOfDay(8, kPM));
  controller->SetScheduleType(NightLightController::ScheduleType::kCustom);
  EXPECT_FALSE(controller->GetEnabled());

  // Toggle the status manually and expect that NightLight is scheduled to
  // turn back off at 8:00 PM.
  controller->Toggle();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_TRUE(controller->timer()->IsRunning());
  EXPECT_EQ(base::Hours(9), controller->timer()->GetCurrentDelay());

  // Simulate suspend and then resume at 2:00 PM (which is outside the user's
  // custom schedule). However, the manual toggle to on should be kept.
  delegate()->SetFakeNow(MakeTimeOfDay(2, kPM));
  controller->SuspendDone(base::TimeDelta{});
  EXPECT_TRUE(controller->GetEnabled());

  // Suspend again and resume at 5:00 PM (which is within the user's custom
  // schedule). The schedule should be applied normally.
  delegate()->SetFakeNow(MakeTimeOfDay(5, kPM));
  controller->SuspendDone(base::TimeDelta{});
  EXPECT_TRUE(controller->GetEnabled());

  // Suspend and resume at 9:00 PM and expect NightLight to be off.
  delegate()->SetFakeNow(MakeTimeOfDay(9, kPM));
  controller->SuspendDone(base::TimeDelta{});
  EXPECT_FALSE(controller->GetEnabled());
}

// Fixture for testing behavior of Night Light when displays support hardware
// CRTC matrices.
class NightLightCrtcTest : public NightLightTest {
 public:
  NightLightCrtcTest()
      : logger_(std::make_unique<display::test::ActionLogger>()) {}
  NightLightCrtcTest(const NightLightCrtcTest& other) = delete;
  NightLightCrtcTest& operator=(const NightLightCrtcTest& rhs) = delete;
  ~NightLightCrtcTest() override = default;

  static constexpr gfx::Size kDisplaySize{1024, 768};
  static constexpr int64_t kId1 = 123;
  static constexpr int64_t kId2 = 456;

  // NightLightTest:
  void SetUp() override {
    NightLightTest::SetUp();

    native_display_delegate_ =
        new display::test::TestNativeDisplayDelegate(logger_.get());
    display_manager()->configurator()->SetDelegateForTesting(
        std::unique_ptr<display::NativeDisplayDelegate>(
            native_display_delegate_));
    display_change_observer_ =
        std::make_unique<display::DisplayChangeObserver>(display_manager());
    test_api_ = std::make_unique<display::DisplayConfigurator::TestApi>(
        display_manager()->configurator());
  }

  void TearDown() override {
    // DisplayChangeObserver access DeviceDataManager in its destructor, so
    // destroy it first.
    display_change_observer_ = nullptr;
    NightLightTest::TearDown();
  }

  struct TestSnapshotParams {
    bool has_ctm_support;
    bool correction_in_linear_space;
  };

  // Builds two displays snapshots into |owned_snapshots_| and return a list of
  // unowned pointers to them. |snapshot_params| should contain exactly 2
  // elements that correspond to capabilities of both displays.
  std::vector<display::DisplaySnapshot*> BuildAndGetDisplaySnapshots(
      const std::vector<TestSnapshotParams>& snapshot_params) {
    DCHECK_EQ(2u, snapshot_params.size());
    owned_snapshots_.clear();
    owned_snapshots_.emplace_back(
        display::FakeDisplaySnapshot::Builder()
            .SetId(kId1)
            .SetNativeMode(kDisplaySize)
            .SetCurrentMode(kDisplaySize)
            .SetHasColorCorrectionMatrix(snapshot_params[0].has_ctm_support)
            .SetColorCorrectionInLinearSpace(
                snapshot_params[0].correction_in_linear_space)
            .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
            .Build());
    owned_snapshots_.back()->set_origin({0, 0});
    owned_snapshots_.emplace_back(
        display::FakeDisplaySnapshot::Builder()
            .SetId(kId2)
            .SetNativeMode(kDisplaySize)
            .SetCurrentMode(kDisplaySize)
            .SetHasColorCorrectionMatrix(snapshot_params[1].has_ctm_support)
            .SetColorCorrectionInLinearSpace(
                snapshot_params[1].correction_in_linear_space)
            .Build());
    owned_snapshots_.back()->set_origin({1030, 0});
    std::vector<display::DisplaySnapshot*> outputs = {
        owned_snapshots_[0].get(), owned_snapshots_[1].get()};
    return outputs;
  }

  // Updates the display configurator and display manager with the given list of
  // display snapshots.
  void UpdateDisplays(const std::vector<display::DisplaySnapshot*>& outputs) {
    native_display_delegate_->set_outputs(outputs);
    display_manager()->configurator()->OnConfigurationChanged();
    EXPECT_TRUE(test_api_->TriggerConfigureTimeout());
    display_change_observer_->GetStateForDisplayIds(outputs);
    display_change_observer_->OnDisplayModeChanged(outputs);
  }

  // Returns true if the software cursor is turned on.
  bool IsCursorCompositingEnabled() const {
    return Shell::Get()
        ->window_tree_host_manager()
        ->cursor_window_controller()
        ->is_cursor_compositing_enabled();
  }

  std::string GetLoggerActionsAndClear() {
    return logger_->GetActionsAndClear();
  }

  bool VerifyCrtcMatrix(int64_t display_id,
                        float temperature,
                        bool in_linear_gamma_space,
                        const std::string& logger_actions_string) const {
    if (in_linear_gamma_space)
      temperature =
          NightLightControllerImpl::GetNonLinearTemperature(temperature);
    constexpr float kRedScale = 1.0f;
    const float blue_scale =
        NightLightControllerImpl::BlueColorScaleFromTemperature(temperature);
    const float green_scale =
        NightLightControllerImpl::GreenColorScaleFromTemperature(
            temperature, in_linear_gamma_space);
    std::stringstream pattern_stream;
    pattern_stream << "*set_color_matrix(id=" << display_id
                   << ",ctm[0]=" << kRedScale << "*ctm[4]=" << green_scale
                   << "*ctm[8]=" << blue_scale << "*)*";
    return base::MatchPattern(logger_actions_string, pattern_stream.str());
  }

 private:
  std::unique_ptr<display::test::ActionLogger> logger_;
  // Not owned.
  raw_ptr<display::test::TestNativeDisplayDelegate, ExperimentalAsh>
      native_display_delegate_;
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;
  std::unique_ptr<display::DisplayConfigurator::TestApi> test_api_;

  std::vector<std::unique_ptr<display::DisplaySnapshot>> owned_snapshots_;
};

// static
constexpr gfx::Size NightLightCrtcTest::kDisplaySize;

// All displays support CRTC matrices.
TEST_F(NightLightCrtcTest, TestAllDisplaysSupportCrtcMatrix) {
  // Create two displays with both having support for CRTC matrices.
  std::vector<display::DisplaySnapshot*> outputs =
      BuildAndGetDisplaySnapshots({{true, true}, {true, true}});
  UpdateDisplays(outputs);

  EXPECT_EQ(2u, display_manager()->GetNumDisplays());
  ASSERT_EQ(2u, RootWindowController::root_window_controllers().size());

  // Turn on Night Light:
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(true);
  float temperature = 0.2f;
  GetLoggerActionsAndClear();
  controller->SetColorTemperature(temperature);
  EXPECT_EQ(temperature, controller->GetColorTemperature());

  // Since both displays support CRTC matrices, no compositor matrix should be
  // set (i.e. compositor matrix is identity which corresponds to the zero
  // temperature).
  TestCompositorsTemperature(0.0f);
  // Hence software cursor should not be used.
  EXPECT_FALSE(IsCursorCompositingEnabled());
  // Verify correct matrix has been set on both crtcs.
  std::string logger_actions = GetLoggerActionsAndClear();
  EXPECT_TRUE(VerifyCrtcMatrix(kId1, temperature, true, logger_actions));
  EXPECT_TRUE(VerifyCrtcMatrix(kId2, temperature, true, logger_actions));

  // Setting a new temperature is applied.
  temperature = 0.65f;
  controller->SetColorTemperature(temperature);
  EXPECT_EQ(temperature, controller->GetColorTemperature());
  TestCompositorsTemperature(0.0f);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  logger_actions = GetLoggerActionsAndClear();
  EXPECT_TRUE(VerifyCrtcMatrix(kId1, temperature, true, logger_actions));
  EXPECT_TRUE(VerifyCrtcMatrix(kId2, temperature, true, logger_actions));

  // Test the cursor compositing behavior when Night Light is on (and doesn't
  // require the software cursor) while other accessibility settings that affect
  // the cursor are toggled.
  for (const auto* const pref : {prefs::kAccessibilityLargeCursorEnabled,
                                 prefs::kAccessibilityHighContrastEnabled}) {
    user1_pref_service()->SetBoolean(pref, true);
    EXPECT_TRUE(IsCursorCompositingEnabled());

    // Disabling the accessibility feature should revert back to the hardware
    // cursor.
    user1_pref_service()->SetBoolean(pref, false);
    EXPECT_FALSE(IsCursorCompositingEnabled());
  }
}

// All displays support CRTC matrices in the compressed gamma space.
TEST_F(NightLightCrtcTest,
       TestAllDisplaysSupportCrtcMatrixCompressedGammaSpace) {
  // Create two displays with both having support for CRTC matrices that are
  // applied in the compressed gamma space.
  std::vector<display::DisplaySnapshot*> outputs =
      BuildAndGetDisplaySnapshots({{true, false}, {true, false}});
  UpdateDisplays(outputs);

  EXPECT_EQ(2u, display_manager()->GetNumDisplays());
  ASSERT_EQ(2u, RootWindowController::root_window_controllers().size());

  // Turn on Night Light:
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(true);
  float temperature = 0.2f;
  GetLoggerActionsAndClear();
  controller->SetColorTemperature(temperature);
  EXPECT_EQ(temperature, controller->GetColorTemperature());

  // Since both displays support CRTC matrices, no compositor matrix should be
  // set (i.e. compositor matrix is identity which corresponds to the zero
  // temperature).
  TestCompositorsTemperature(0.0f);
  // Hence software cursor should not be used.
  EXPECT_FALSE(IsCursorCompositingEnabled());
  // Verify compressed gamma space matrix has been set on both crtcs.
  std::string logger_actions = GetLoggerActionsAndClear();
  EXPECT_TRUE(VerifyCrtcMatrix(kId1, temperature, false, logger_actions));
  EXPECT_TRUE(VerifyCrtcMatrix(kId2, temperature, false, logger_actions));

  // Setting a new temperature is applied.
  temperature = 0.65f;
  controller->SetColorTemperature(temperature);
  EXPECT_EQ(temperature, controller->GetColorTemperature());
  TestCompositorsTemperature(0.0f);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  logger_actions = GetLoggerActionsAndClear();
  EXPECT_TRUE(VerifyCrtcMatrix(kId1, temperature, false, logger_actions));
  EXPECT_TRUE(VerifyCrtcMatrix(kId2, temperature, false, logger_actions));
}

// One display supports CRTC matrix and the other doesn't.
TEST_F(NightLightCrtcTest, TestMixedCrtcMatrixSupport) {
  std::vector<display::DisplaySnapshot*> outputs =
      BuildAndGetDisplaySnapshots({{true, true}, {false, false}});
  UpdateDisplays(outputs);

  EXPECT_EQ(2u, display_manager()->GetNumDisplays());
  ASSERT_EQ(2u, RootWindowController::root_window_controllers().size());

  // Turn on Night Light:
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(true);
  const float temperature = 0.2f;
  GetLoggerActionsAndClear();
  controller->SetColorTemperature(temperature);
  EXPECT_EQ(temperature, controller->GetColorTemperature());

  // The first display supports CRTC matrix, so its compositor has identity
  // matrix.
  TestDisplayCompositorTemperature(kId1, 0.0f);
  // However, the second display doesn't support CRTC matrix, Night Light is
  // using the compositor matrix on this display.
  TestDisplayCompositorTemperature(kId2, temperature);
  // With mixed CTRC support, software cursor must be on.
  EXPECT_TRUE(IsCursorCompositingEnabled());
  // Verify correct matrix has been set on both crtcs.
  const std::string logger_actions = GetLoggerActionsAndClear();
  EXPECT_TRUE(VerifyCrtcMatrix(kId1, temperature, true, logger_actions));
  EXPECT_FALSE(VerifyCrtcMatrix(kId2, temperature, false, logger_actions));
}

// All displays don't support CRTC matrices.
TEST_F(NightLightCrtcTest, TestNoCrtcMatrixSupport) {
  std::vector<display::DisplaySnapshot*> outputs =
      BuildAndGetDisplaySnapshots({{false, false}, {false, false}});
  UpdateDisplays(outputs);

  EXPECT_EQ(2u, display_manager()->GetNumDisplays());
  ASSERT_EQ(2u, RootWindowController::root_window_controllers().size());

  // Turn on Night Light:
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(true);
  const float temperature = 0.2f;
  GetLoggerActionsAndClear();
  controller->SetColorTemperature(temperature);
  EXPECT_EQ(temperature, controller->GetColorTemperature());

  // Compositor matrices are used on both displays.
  TestCompositorsTemperature(temperature);
  // With no CTRC support, software cursor must be on.
  EXPECT_TRUE(IsCursorCompositingEnabled());
  // No CRTC matrices have been set.
  const std::string logger_actions = GetLoggerActionsAndClear();
  EXPECT_FALSE(VerifyCrtcMatrix(kId1, temperature, false, logger_actions));
  EXPECT_FALSE(VerifyCrtcMatrix(kId2, temperature, false, logger_actions));
}

// Tests that switching CRTC matrix support on while Night Light is enabled
// doesn't result in the matrix being applied twice.
TEST_F(NightLightCrtcTest, TestNoDoubleNightLightEffect) {
  std::vector<display::DisplaySnapshot*> outputs =
      BuildAndGetDisplaySnapshots({{false, false}, {false, false}});
  UpdateDisplays(outputs);

  EXPECT_EQ(2u, display_manager()->GetNumDisplays());
  ASSERT_EQ(2u, RootWindowController::root_window_controllers().size());

  // Turn on Night Light:
  NightLightControllerImpl* controller = GetController();
  SetNightLightEnabled(true);
  const float temperature = 0.2f;
  GetLoggerActionsAndClear();
  controller->SetColorTemperature(temperature);
  EXPECT_EQ(temperature, controller->GetColorTemperature());

  // Compositor matrices are used on both displays.
  TestCompositorsTemperature(temperature);
  // With no CTRC support, software cursor must be on.
  EXPECT_TRUE(IsCursorCompositingEnabled());
  // No CRTC matrices have been set.
  std::string logger_actions = GetLoggerActionsAndClear();
  EXPECT_FALSE(VerifyCrtcMatrix(kId1, temperature, false, logger_actions));
  EXPECT_FALSE(VerifyCrtcMatrix(kId2, temperature, false, logger_actions));

  // Simulate that the two displays suddenly became able to support CRTC matrix.
  // This shouldn't happen in practice, but we noticed multiple times on resume
  // from suspend, or after the display turned on after it was off as a result
  // of no user activity, we noticed that the display can get into a transient
  // state where it is wrongly believed to have no CTM matrix capability, then
  // later corrected. When this happened, we noticed that the Night Light effect
  // is applied twice; once via the CRTC CTM matrix, and another via the
  // compositor matrix. When this happens, we need to assert that the compositor
  // matrix is set to identity, and the cursor compositing is updated correctly.
  // TODO(afakhry): Investigate the root cause of this https://crbug.com/844067.
  std::vector<display::DisplaySnapshot*> outputs2 =
      BuildAndGetDisplaySnapshots({{true, true}, {true, true}});
  UpdateDisplays(outputs2);
  TestCompositorsTemperature(0.0f);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  logger_actions = GetLoggerActionsAndClear();
  EXPECT_TRUE(VerifyCrtcMatrix(kId1, temperature, true, logger_actions));
  EXPECT_TRUE(VerifyCrtcMatrix(kId2, temperature, true, logger_actions));
}

// The following tests are for ambient color temperature conversions
// needed to go from a powerd ambient temperature reading in Kelvin to three
// RGB factors that can be used for a CTM to match the ambient color
// temperature.
// The table for the mapping was created with internal user studies, refer to
// kTable in
// NightLightControllerImpl::RemapAmbientColorTemperature to
// verify the assertion in the following tests.
TEST(AmbientTemperature, RemapAmbientColorTemperature) {
  // Neutral temperature
  float temperature =
      NightLightControllerImpl::RemapAmbientColorTemperature(6500);
  EXPECT_GT(temperature, 6000);
  EXPECT_LT(temperature, 7000);

  // Warm color temperature
  temperature = NightLightControllerImpl::RemapAmbientColorTemperature(3000);
  EXPECT_GT(temperature, 4500);
  EXPECT_LT(temperature, 5000);

  // Daylight color temperature
  temperature = NightLightControllerImpl::RemapAmbientColorTemperature(7500);
  EXPECT_GT(temperature, 6800);
  EXPECT_LT(temperature, 7500);

  // Extremely high color temperature
  temperature = NightLightControllerImpl::RemapAmbientColorTemperature(20000);
  EXPECT_GT(temperature, 7000);
  EXPECT_LT(temperature, 8000);
}

// The following tests Kelvin temperatures to RGB scale factors.
// The values are from the calculation of white point based on Planckian locus.
// For each RGB vector we compute the distance from the expected value
// and check it's within a threshold of 0.01f;
TEST(AmbientTemperature, AmbientTemperatureToRGBScaleFactors) {
  constexpr float allowed_difference = 0.01f;
  // Netural temperature
  gfx::Vector3dF vec =
      NightLightControllerImpl::ColorScalesFromRemappedTemperatureInKevin(6500);
  EXPECT_LT((vec - gfx::Vector3dF(1.0f, 1.0f, 1.0f)).Length(),
            allowed_difference);
  // Warm
  vec =
      NightLightControllerImpl::ColorScalesFromRemappedTemperatureInKevin(4500);
  EXPECT_LT((vec - gfx::Vector3dF(1.0f, 0.8816f, 0.7313f)).Length(),
            allowed_difference);
  // Daylight
  vec =
      NightLightControllerImpl::ColorScalesFromRemappedTemperatureInKevin(7000);
  EXPECT_LT((vec - gfx::Vector3dF(0.949f, 0.971f, 1.0f)).Length(),
            allowed_difference);
}

class AutoNightLightTest : public NightLightTest {
 public:
  AutoNightLightTest() = default;
  AutoNightLightTest(const AutoNightLightTest& other) = delete;
  AutoNightLightTest& operator=(const AutoNightLightTest& rhs) = delete;
  ~AutoNightLightTest() override = default;

  // NightLightTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kAutoNightLight);

    delegate()->SetFakeNow(TimeOfDay(fake_now_));
    delegate()->SetFakeSunset(TimeOfDay(20 * 60));  // 8:00 PM.
    delegate()->SetFakeSunrise(TimeOfDay(5 * 60));  // 5:00 AM.

    NightLightTest::SetUp();
  }

 protected:
  // Now is at 4 PM.
  //
  //      16:00               20:00                      5:00
  // <----- + ----------------- + ----------------------- + ------->
  //        |                   |                         |
  //       now                sunset                   sunrise
  //
  int fake_now_ = 16 * 60;  // 4:00 PM.

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutoNightLightTest, Notification) {
  // Unblock the user session in order to be able to stop showing the Auto Night
  // Light notification.
  GetSessionControllerClient()->UnlockScreen();

  // Since Auto Night Light is enabled, the schedule should be automatically set
  // to sunset-to-sunrise, even though the user never set that pref.
  NightLightControllerImpl* controller = GetController();
  EXPECT_EQ(NightLightController::ScheduleType::kSunsetToSunrise,
            controller->GetScheduleType());
  EXPECT_FALSE(
      user1_pref_service()->HasPrefPath(prefs::kNightLightScheduleType));

  // Simulate reaching sunset.
  delegate()->SetFakeNow(TimeOfDay(20 * 60));  // Now is 8:00 PM.
  controller->timer()->FireNow();
  EXPECT_TRUE(controller->GetEnabled());
  auto* notification = controller->GetAutoNightLightNotificationForTesting();
  ASSERT_TRUE(notification);
  ASSERT_TRUE(notification->delegate());

  // Simulate the user clicking the notification body to go to settings, and
  // turning off Night Light manually for tonight. The notification should be
  // dismissed.
  notification->delegate()->Click(absl::nullopt, absl::nullopt);
  controller->SetEnabled(false,
                         NightLightControllerImpl::AnimationDuration::kShort);
  EXPECT_FALSE(controller->GetEnabled());
  EXPECT_FALSE(controller->GetAutoNightLightNotificationForTesting());

  // Simulate reaching next sunset. The notification should no longer show.
  delegate()->SetFakeNow(TimeOfDay(20 * 60));  // Now is 8:00 PM.
  controller->timer()->FireNow();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_FALSE(controller->GetAutoNightLightNotificationForTesting());
}

TEST_F(AutoNightLightTest, DismissNotificationOnTurningOff) {
  GetSessionControllerClient()->UnlockScreen();
  NightLightControllerImpl* controller = GetController();
  EXPECT_EQ(NightLightController::ScheduleType::kSunsetToSunrise,
            controller->GetScheduleType());

  // Use a fake geoposition with sunset/sunrise times at 5pm/3am.
  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition2_Latitude, kFakePosition2_Longitude});

  // Simulate reaching sunset.
  delegate()->SetFakeNow(TimeOfDay(17 * 60));  // Now is 5:00 PM.
  controller->timer()->FireNow();
  EXPECT_TRUE(controller->GetEnabled());
  auto* notification = controller->GetAutoNightLightNotificationForTesting();
  ASSERT_TRUE(notification);
  ASSERT_TRUE(notification->delegate());

  // Simulate receiving an updated geoposition with sunset/sunrise times at
  // 8pm/4am, so now is before sunset. Night Light should turn off, and the
  // stale notification from above should be removed. However, its removal
  // should not affect kAutoNightLightNotificationDismissed.
  controller->SetCurrentGeoposition(NightLightController::SimpleGeoposition{
      kFakePosition1_Latitude, kFakePosition1_Longitude});
  EXPECT_FALSE(controller->GetEnabled());
  EXPECT_FALSE(controller->GetAutoNightLightNotificationForTesting());

  // Simulate reaching next sunset. The notification should still show, since it
  // was never dismissed by the user.
  delegate()->SetFakeNow(TimeOfDay(20 * 60));  // Now is 8:00 PM.
  controller->timer()->FireNow();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_TRUE(controller->GetAutoNightLightNotificationForTesting());
}
TEST_F(AutoNightLightTest, CannotDisableNotificationWhenSessionIsBlocked) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(Shell::Get()->session_controller()->IsUserSessionBlocked());

  // Simulate reaching sunset.
  NightLightControllerImpl* controller = GetController();
  delegate()->SetFakeNow(TimeOfDay(20 * 60));  // Now is 8:00 PM.
  controller->timer()->FireNow();
  EXPECT_TRUE(controller->GetEnabled());
  auto* notification = controller->GetAutoNightLightNotificationForTesting();
  ASSERT_TRUE(notification);
  ASSERT_TRUE(notification->delegate());

  // Simulate user closing the notification.
  notification->delegate()->Close(/*by_user=*/true);
  EXPECT_FALSE(user1_pref_service()->GetBoolean(
      prefs::kAutoNightLightNotificationDismissed));
}

TEST_F(AutoNightLightTest, OverriddenByUser) {
  // Once the user sets the schedule to anything, even sunset-to-sunrise, the
  // auto-night light will never show.
  NightLightControllerImpl* controller = GetController();
  controller->SetScheduleType(
      NightLightController::ScheduleType::kSunsetToSunrise);

  // Simulate reaching sunset.
  delegate()->SetFakeNow(TimeOfDay(20 * 60));  // Now is 8:00 PM.
  controller->timer()->FireNow();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_FALSE(controller->GetAutoNightLightNotificationForTesting());
}

TEST_F(AutoNightLightTest, NoNotificationWhenManuallyEnabledFromSettings) {
  NightLightControllerImpl* controller = GetController();
  EXPECT_FALSE(controller->GetEnabled());
  user1_pref_service()->SetBoolean(prefs::kNightLightEnabled, true);
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_FALSE(controller->GetAutoNightLightNotificationForTesting());
}

TEST_F(AutoNightLightTest, NoNotificationWhenManuallyEnabledFromSystemMenu) {
  NightLightControllerImpl* controller = GetController();
  EXPECT_FALSE(controller->GetEnabled());
  controller->Toggle();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_FALSE(controller->GetAutoNightLightNotificationForTesting());
}

// Now is at 11 PM.
//
//      20:00               23:00                      5:00
// <----- + ----------------- + ----------------------- + ------->
//        |                   |                         |
//      sunset               now                     sunrise
//
// Tests that when the user logs in for the first time between sunset and
// sunrise with Auto Night Light enabled, and the notification has never been
// dismissed before, the notification should be shown.
class AutoNightLightOnFirstLogin : public AutoNightLightTest {
 public:
  AutoNightLightOnFirstLogin() { fake_now_ = 23 * 60; }
  AutoNightLightOnFirstLogin(const AutoNightLightOnFirstLogin& other) = delete;
  AutoNightLightOnFirstLogin& operator=(const AutoNightLightOnFirstLogin& rhs) =
      delete;
  ~AutoNightLightOnFirstLogin() override = default;
};

TEST_F(AutoNightLightOnFirstLogin, NotifyOnFirstLogin) {
  NightLightControllerImpl* controller = GetController();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_TRUE(controller->GetAutoNightLightNotificationForTesting());
}

// Fixture for testing Ambient EQ.
class AmbientEQTest : public NightLightTest {
 public:
  AmbientEQTest() : logger_(std::make_unique<display::test::ActionLogger>()) {}
  AmbientEQTest(const AmbientEQTest& other) = delete;
  AmbientEQTest& operator=(const AmbientEQTest& rhs) = delete;
  ~AmbientEQTest() override = default;

  static constexpr gfx::Vector3dF kDefaultScalingFactors{1.0f, 1.0f, 1.0f};
  static constexpr int64_t kInternalDisplayId = 123;
  static constexpr int64_t kExternalDisplayId = 456;

  // NightLightTest:
  void SetUp() override {
    NightLightTest::SetUp();

    features_.InitAndEnableFeature(features::kAllowAmbientEQ);
    controller_ = GetController();

    native_display_delegate_ =
        new display::test::TestNativeDisplayDelegate(logger_.get());
    display_manager()->configurator()->SetDelegateForTesting(
        std::unique_ptr<display::NativeDisplayDelegate>(
            native_display_delegate_));
    display_change_observer_ =
        std::make_unique<display::DisplayChangeObserver>(display_manager());
    test_api_ = std::make_unique<display::DisplayConfigurator::TestApi>(
        display_manager()->configurator());
  }

  void ConfigureMulipleDisplaySetup() {
    const gfx::Size kDisplaySize{1024, 768};
    owned_snapshots_.clear();
    owned_snapshots_.emplace_back(
        display::FakeDisplaySnapshot::Builder()
            .SetId(kInternalDisplayId)
            .SetNativeMode(kDisplaySize)
            .SetCurrentMode(kDisplaySize)
            .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
            .SetOrigin({0, 0})
            .Build());
    owned_snapshots_.emplace_back(display::FakeDisplaySnapshot::Builder()
                                      .SetId(kExternalDisplayId)
                                      .SetNativeMode(kDisplaySize)
                                      .SetCurrentMode(kDisplaySize)
                                      .SetOrigin({1030, 0})
                                      .Build());

    std::vector<display::DisplaySnapshot*> outputs = {
        owned_snapshots_[0].get(), owned_snapshots_[1].get()};

    native_display_delegate_->set_outputs(outputs);
    display_manager()->configurator()->OnConfigurationChanged();
    EXPECT_TRUE(test_api_->TriggerConfigureTimeout());
    display_change_observer_->GetStateForDisplayIds(outputs);
    display_change_observer_->OnDisplayModeChanged(outputs);
  }

  void TearDown() override {
    // DisplayChangeObserver access DeviceDataManager in its destructor, so
    // destroy it first.
    display_change_observer_ = nullptr;
    NightLightTest::TearDown();
  }

 protected:
  base::test::ScopedFeatureList features_;
  std::vector<std::unique_ptr<display::DisplaySnapshot>> owned_snapshots_;
  std::unique_ptr<display::test::ActionLogger> logger_;

  // Not owned.
  raw_ptr<NightLightControllerImpl, ExperimentalAsh> controller_;
  raw_ptr<display::test::TestNativeDisplayDelegate, ExperimentalAsh>
      native_display_delegate_;
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;
  std::unique_ptr<display::DisplayConfigurator::TestApi> test_api_;
};

// static
constexpr gfx::Vector3dF AmbientEQTest::kDefaultScalingFactors;

TEST_F(AmbientEQTest, TestAmbientRgbScalingUpdatesOnPrefChanged) {
  // Start with the pref disabled.
  controller_->SetAmbientColorEnabled(false);

  // Shift to the coolest temperature and the temperature updates even with the
  // pref disabled but the scaling factors don't.
  float ambient_temperature = SimulateAmbientColorFromPowerd(8000, 7350.0f);
  EXPECT_EQ(ambient_temperature, controller_->ambient_temperature());
  EXPECT_EQ(kDefaultScalingFactors, controller_->ambient_rgb_scaling_factors());

  // Enabling the pref and the scaling factors update.
  controller_->SetAmbientColorEnabled(true);
  const auto coolest_scaling_factors =
      controller_->ambient_rgb_scaling_factors();
  EXPECT_NE(kDefaultScalingFactors, coolest_scaling_factors);

  // Shift to the warmest temp and the the scaling factors should update along
  // with the temperature while the pref is enabled.
  ambient_temperature = SimulateAmbientColorFromPowerd(2700, 5800.0f);
  EXPECT_EQ(ambient_temperature, controller_->ambient_temperature());
  const auto warmest_scaling_factors =
      controller_->ambient_rgb_scaling_factors();
  EXPECT_NE(warmest_scaling_factors, coolest_scaling_factors);
  EXPECT_NE(warmest_scaling_factors, kDefaultScalingFactors);
}

TEST_F(AmbientEQTest, TestAmbientRgbScalingUpdatesOnUserChangedToEnabled) {
  // Start with user1 logged in with pref disabled.
  controller_->SetAmbientColorEnabled(false);

  // Shift to the coolest temperature and the temperature updates even with the
  // pref disabled but the scaling factors don't.
  float ambient_temperature = SimulateAmbientColorFromPowerd(8000, 7350.0f);
  EXPECT_EQ(ambient_temperature, controller_->ambient_temperature());
  EXPECT_EQ(kDefaultScalingFactors, controller_->ambient_rgb_scaling_factors());

  // Enable the pref for user 2 then switch to user2 and the factors update.
  user2_pref_service()->SetBoolean(prefs::kAmbientColorEnabled, true);
  SwitchActiveUser(kUser2Email);
  const auto coolest_scaling_factors =
      controller_->ambient_rgb_scaling_factors();
  EXPECT_NE(kDefaultScalingFactors, coolest_scaling_factors);
}

TEST_F(AmbientEQTest, TestAmbientRgbScalingUpdatesOnUserChangedBothDisabled) {
  // Start with user1 logged in with pref disabled.
  controller_->SetAmbientColorEnabled(false);

  // Shift to the coolest temperature and the temperature updates even with the
  // pref disabled but the scaling factors don't.
  float ambient_temperature = SimulateAmbientColorFromPowerd(8000, 7350.0f);
  EXPECT_EQ(ambient_temperature, controller_->ambient_temperature());
  EXPECT_EQ(kDefaultScalingFactors, controller_->ambient_rgb_scaling_factors());

  // Disable the pref for user 2 then switch to user2 and the factors still
  // shouldn't update.
  user2_pref_service()->SetBoolean(prefs::kAmbientColorEnabled, false);
  SwitchActiveUser(kUser2Email);
  EXPECT_EQ(kDefaultScalingFactors, controller_->ambient_rgb_scaling_factors());
}

TEST_F(AmbientEQTest, TestAmbientColorMatrix) {
  ConfigureMulipleDisplaySetup();
  SetNightLightEnabled(false);
  SetAmbientColorPrefEnabled(true);
  auto scaling_factors = GetAllDisplaysCompositorsRGBScaleFactors();
  // If no temperature is set, we expect 1.0 for each scaling factor.
  for (const gfx::Vector3dF& rgb : scaling_factors) {
    EXPECT_TRUE((rgb - gfx::Vector3dF(1.0f, 1.0f, 1.0f)).IsZero());
  }

  // Turn color temperature down.
  SimulateAmbientColorFromPowerd(8000, 7350.0f);
  auto internal_rgb = GetDisplayCompositorRGBScaleFactors(kInternalDisplayId);
  auto external_rgb = GetDisplayCompositorRGBScaleFactors(kExternalDisplayId);

  // A cool temperature should affect only red and green.
  EXPECT_LT(internal_rgb.x(), 1.0f);
  EXPECT_LT(internal_rgb.y(), 1.0f);
  EXPECT_EQ(internal_rgb.z(), 1.0f);

  // The external display should not be affected.
  EXPECT_TRUE((external_rgb - gfx::Vector3dF(1.0f, 1.0f, 1.0f)).IsZero());

  // Turn color temperature up.
  SimulateAmbientColorFromPowerd(2700, 5800.0f);
  internal_rgb = GetDisplayCompositorRGBScaleFactors(kInternalDisplayId);
  external_rgb = GetDisplayCompositorRGBScaleFactors(kExternalDisplayId);

  // A warm temperature should affect only green and blue.
  EXPECT_EQ(internal_rgb.x(), 1.0f);
  EXPECT_LT(internal_rgb.y(), 1.0f);
  EXPECT_LT(internal_rgb.z(), 1.0f);

  // The external display should not be affected.
  EXPECT_TRUE((external_rgb - gfx::Vector3dF(1.0f, 1.0f, 1.0f)).IsZero());
}

TEST_F(AmbientEQTest, TestNightLightAndAmbientColorInteraction) {
  ConfigureMulipleDisplaySetup();

  SetNightLightEnabled(true);

  auto night_light_rgb = GetAllDisplaysCompositorsRGBScaleFactors().front();

  SetAmbientColorPrefEnabled(true);

  auto night_light_and_ambient_rgb =
      GetDisplayCompositorRGBScaleFactors(kInternalDisplayId);
  // Ambient color with neutral temperature should not affect night light.
  EXPECT_TRUE((night_light_rgb - night_light_and_ambient_rgb).IsZero());

  SimulateAmbientColorFromPowerd(2700, 5800.0f);

  night_light_and_ambient_rgb =
      GetDisplayCompositorRGBScaleFactors(kInternalDisplayId);

  // Red should not be affected by a warmed ambient temperature.
  EXPECT_EQ(night_light_and_ambient_rgb.x(), night_light_rgb.x());
  // Green and blue should be lowered instead.
  EXPECT_LT(night_light_and_ambient_rgb.y(), night_light_rgb.y());
  EXPECT_LT(night_light_and_ambient_rgb.z(), night_light_rgb.z());
}

}  // namespace

}  // namespace ash
