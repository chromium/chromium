// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_controller.h"

#include <math.h>
#include <utility>
#include <vector>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "services/ws/public/cpp/input_devices/input_device_client_test_api.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

constexpr float kMeanGravity = TabletModeControllerTestApi::kMeanGravity;

// The strings are "Touchview" as they're already used in metrics.
constexpr char kTabletModeInitiallyDisabled[] = "Touchview_Initially_Disabled";
constexpr char kTabletModeEnabled[] = "Touchview_Enabled";
constexpr char kTabletModeDisabled[] = "Touchview_Disabled";

}  // namespace

// Test accelerometer data taken with the lid at less than 180 degrees while
// shaking the device around. The data is to be interpreted in groups of 6 where
// each 6 values corresponds to the base accelerometer (-y / g, -x / g, -z / g)
// followed by the lid accelerometer (-y / g , x / g, z / g).
extern const float kAccelerometerLaptopModeTestData[];
extern const size_t kAccelerometerLaptopModeTestDataLength;

// Test accelerometer data taken with the lid open 360 degrees while
// shaking the device around. The data is to be interpreted in groups of 6 where
// each 6 values corresponds to the base accelerometer (-y / g, -x / g, -z / g)
// followed by the lid accelerometer (-y / g , x / g, z / g).
extern const float kAccelerometerFullyOpenTestData[];
extern const size_t kAccelerometerFullyOpenTestDataLength;

// Test accelerometer data taken with the lid open 360 degrees while the device
// hinge was nearly vertical, while shaking the device around. The data is to be
// interpreted in groups of 6 where each 6 values corresponds to the X, Y, and Z
// readings from the base and lid accelerometers in this order.
extern const float kAccelerometerVerticalHingeTestData[];
extern const size_t kAccelerometerVerticalHingeTestDataLength;
extern const float kAccelerometerVerticalHingeUnstableAnglesTestData[];
extern const size_t kAccelerometerVerticalHingeUnstableAnglesTestDataLength;

class TabletModeControllerTest : public AshTestBase {
 public:
  TabletModeControllerTest() = default;
  ~TabletModeControllerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletMode);
    AshTestBase::SetUp();
    chromeos::AccelerometerReader::GetInstance()->RemoveObserver(
        tablet_mode_controller());

    // Set the first display to be the internal display for the accelerometer
    // screen rotation tests.
    display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
        .SetFirstDisplayAsInternalDisplay();

    test_api_ = std::make_unique<TabletModeControllerTestApi>();
  }

  void TearDown() override {
    chromeos::AccelerometerReader::GetInstance()->AddObserver(
        tablet_mode_controller());
    AshTestBase::TearDown();
  }

  TabletModeController* tablet_mode_controller() {
    return Shell::Get()->tablet_mode_controller();
  }

  void TriggerLidUpdate(const gfx::Vector3dF& lid) {
    test_api_->TriggerLidUpdate(lid);
  }

  void TriggerBaseAndLidUpdate(const gfx::Vector3dF& base,
                               const gfx::Vector3dF& lid) {
    test_api_->TriggerBaseAndLidUpdate(base, lid);
  }

  bool IsTabletModeStarted() const { return test_api_->IsTabletModeStarted(); }

  // Attaches a SimpleTestTickClock to the TabletModeController with a non
  // null value initial value.
  void AttachTickClockForTest() {
    test_tick_clock_.Advance(base::TimeDelta::FromSeconds(1));
    test_api_->set_tick_clock(&test_tick_clock_);
  }

  void AdvanceTickClock(const base::TimeDelta& delta) {
    test_tick_clock_.Advance(delta);
  }

  void OpenLidToAngle(float degrees) { test_api_->OpenLidToAngle(degrees); }
  void HoldDeviceVertical() { test_api_->HoldDeviceVertical(); }
  void OpenLid() { test_api_->OpenLid(); }
  void CloseLid() { test_api_->CloseLid(); }
  bool CanUseUnstableLidAngle() { return test_api_->CanUseUnstableLidAngle(); }

  void SetTabletMode(bool on) { test_api_->SetTabletMode(on); }

  bool AreEventsBlocked() const { return test_api_->AreEventsBlocked(); }

  TabletModeController::UiMode forced_ui_mode() const {
    return test_api_->force_ui_mode();
  }

  base::UserActionTester* user_action_tester() { return &user_action_tester_; }

 private:
  std::unique_ptr<TabletModeControllerTestApi> test_api_;

  base::SimpleTestTickClock test_tick_clock_;

  // Tracks user action counts.
  base::UserActionTester user_action_tester_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeControllerTest);
};

// Verify TabletMode enabled/disabled user action metrics are recorded.
TEST_F(TabletModeControllerTest, VerifyTabletModeEnabledDisabledCounts) {
  ASSERT_EQ(1,
            user_action_tester()->GetActionCount(kTabletModeInitiallyDisabled));
  ASSERT_EQ(0, user_action_tester()->GetActionCount(kTabletModeEnabled));
  ASSERT_EQ(0, user_action_tester()->GetActionCount(kTabletModeDisabled));

  user_action_tester()->ResetCounts();
  tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTabletModeEnabled));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTabletModeDisabled));
  tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTabletModeEnabled));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTabletModeDisabled));

  user_action_tester()->ResetCounts();
  tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTabletModeEnabled));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTabletModeDisabled));
  tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTabletModeEnabled));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTabletModeDisabled));
}

// Verify that closing the lid will exit tablet mode.
TEST_F(TabletModeControllerTest, CloseLidWhileInTabletMode) {
  OpenLidToAngle(315.0f);
  ASSERT_TRUE(IsTabletModeStarted());

  CloseLid();
  EXPECT_FALSE(IsTabletModeStarted());
}

// Verify that tablet mode will not be entered when the lid is closed.
TEST_F(TabletModeControllerTest, HingeAnglesWithLidClosed) {
  AttachTickClockForTest();

  CloseLid();

  OpenLidToAngle(270.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  OpenLidToAngle(315.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  OpenLidToAngle(355.0f);
  EXPECT_FALSE(IsTabletModeStarted());
}

// Verify the unstable lid angle is suppressed during opening the lid.
TEST_F(TabletModeControllerTest, OpenLidUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  // Simulate the erroneous accelerometer readings.
  OpenLidToAngle(355.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // Simulate the correct accelerometer readings.
  OpenLidToAngle(5.0f);
  EXPECT_FALSE(IsTabletModeStarted());
}

// Verify that suppressing unstable lid angle while opening the lid does not
// override tablet mode switch on value - if tablet mode switch is on, device
// should remain in tablet mode.
TEST_F(TabletModeControllerTest, TabletModeSwitchOnWithOpenUnstableLidAngle) {
  AttachTickClockForTest();

  SetTabletMode(true /*on*/);
  EXPECT_TRUE(IsTabletModeStarted());

  OpenLid();
  EXPECT_TRUE(IsTabletModeStarted());

  // Simulate the correct accelerometer readings.
  OpenLidToAngle(355.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  // Simulate the erroneous accelerometer readings.
  OpenLidToAngle(5.0f);
  EXPECT_TRUE(IsTabletModeStarted());
}

// Verify the unstable lid angle is suppressed during closing the lid.
TEST_F(TabletModeControllerTest, CloseLidUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  OpenLidToAngle(45.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // Simulate the correct accelerometer readings.
  OpenLidToAngle(5.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // Simulate the erroneous accelerometer readings.
  OpenLidToAngle(355.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  CloseLid();
  EXPECT_FALSE(IsTabletModeStarted());
}

// Verify that suppressing unstable lid angle when the lid is closed does not
// override tablet mode switch on value - if tablet mode switch is on, device
// should remain in tablet mode.
TEST_F(TabletModeControllerTest, TabletModeSwitchOnWithCloseUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  SetTabletMode(true /*on*/);
  EXPECT_TRUE(IsTabletModeStarted());

  CloseLid();
  EXPECT_TRUE(IsTabletModeStarted());

  SetTabletMode(false /*on*/);
  EXPECT_FALSE(IsTabletModeStarted());
}

TEST_F(TabletModeControllerTest, TabletModeTransition) {
  OpenLidToAngle(90.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // Unstable reading. This should not trigger tablet mode.
  HoldDeviceVertical();
  EXPECT_FALSE(IsTabletModeStarted());

  // When tablet mode switch is on it should force tablet mode even if the
  // reading is not stable.
  SetTabletMode(true);
  EXPECT_TRUE(IsTabletModeStarted());

  // After tablet mode switch is off it should stay in tablet mode if the
  // reading is not stable.
  SetTabletMode(false);
  EXPECT_TRUE(IsTabletModeStarted());

  // Should leave tablet mode when the lid angle is small enough.
  OpenLidToAngle(90.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  OpenLidToAngle(300.0f);
  EXPECT_TRUE(IsTabletModeStarted());
}

// When there is no keyboard accelerometer available tablet mode should solely
// rely on the tablet mode switch.
TEST_F(TabletModeControllerTest, TabletModeTransitionNoKeyboardAccelerometer) {
  ASSERT_FALSE(IsTabletModeStarted());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravity));
  ASSERT_FALSE(IsTabletModeStarted());

  SetTabletMode(true);
  EXPECT_TRUE(IsTabletModeStarted());

  // Single sensor reading should not change mode.
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravity));
  EXPECT_TRUE(IsTabletModeStarted());

  // With a single sensor we should exit immediately on the tablet mode switch
  // rather than waiting for stabilized accelerometer readings.
  SetTabletMode(false);
  EXPECT_FALSE(IsTabletModeStarted());
}

// Verify the tablet mode enter/exit thresholds for stable angles.
TEST_F(TabletModeControllerTest, StableHingeAnglesWithLidOpened) {
  ASSERT_FALSE(IsTabletModeStarted());

  OpenLidToAngle(180.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  OpenLidToAngle(315.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  OpenLidToAngle(180.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  OpenLidToAngle(45.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  OpenLidToAngle(270.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  OpenLidToAngle(90.0f);
  EXPECT_FALSE(IsTabletModeStarted());
}

// Verify entering tablet mode for unstable lid angles when a certain range of
// time has passed.
TEST_F(TabletModeControllerTest, EnterTabletModeWithUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  ASSERT_FALSE(IsTabletModeStarted());

  OpenLidToAngle(5.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  EXPECT_FALSE(CanUseUnstableLidAngle());
  OpenLidToAngle(355.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // 1 second after entering unstable angle zone.
  AdvanceTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(CanUseUnstableLidAngle());
  OpenLidToAngle(355.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // 2 seconds after entering unstable angle zone.
  AdvanceTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(CanUseUnstableLidAngle());
  OpenLidToAngle(355.0f);
  EXPECT_TRUE(IsTabletModeStarted());
}

// Verify not exiting tablet mode for unstable lid angles even after a certain
// range of time has passed.
TEST_F(TabletModeControllerTest, NotExitTabletModeWithUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  ASSERT_FALSE(IsTabletModeStarted());

  OpenLidToAngle(280.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  OpenLidToAngle(5.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  // 1 second after entering unstable angle zone.
  AdvanceTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(CanUseUnstableLidAngle());
  OpenLidToAngle(5.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  // 2 seconds after entering unstable angle zone.
  AdvanceTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(CanUseUnstableLidAngle());
  OpenLidToAngle(5.0f);
  EXPECT_TRUE(IsTabletModeStarted());
}

// Tests that when the hinge is nearly vertically aligned, the current state
// persists as the computed angle is highly inaccurate in this orientation.
TEST_F(TabletModeControllerTest, HingeAligned) {
  // Laptop in normal orientation lid open 90 degrees.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, -kMeanGravity),
                          gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  EXPECT_FALSE(IsTabletModeStarted());

  // Completely vertical.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f),
                          gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f));
  EXPECT_FALSE(IsTabletModeStarted());

  // Close to vertical but with hinge appearing to be open 270 degrees.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, -0.1f),
                          gfx::Vector3dF(kMeanGravity, 0.1f, 0.0f));
  EXPECT_FALSE(IsTabletModeStarted());

  // Flat and open 270 degrees should start tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, -kMeanGravity),
                          gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_TRUE(IsTabletModeStarted());

  // Normal 90 degree orientation but near vertical should stay in maximize
  // mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, -0.1f),
                          gfx::Vector3dF(kMeanGravity, -0.1f, 0.0f));
  EXPECT_TRUE(IsTabletModeStarted());
}

TEST_F(TabletModeControllerTest, LaptopTest) {
  // Feeds in sample accelerometer data and verifies that there are no
  // transitions into tabletmode / tablet mode while shaking the device around
  // with the hinge at less than 180 degrees. Note the conversion from device
  // data to accelerometer updates consistent with accelerometer_reader.cc.
  ASSERT_EQ(0u, kAccelerometerLaptopModeTestDataLength % 6);
  for (size_t i = 0; i < kAccelerometerLaptopModeTestDataLength / 6; ++i) {
    gfx::Vector3dF base(-kAccelerometerLaptopModeTestData[i * 6 + 1],
                        -kAccelerometerLaptopModeTestData[i * 6],
                        -kAccelerometerLaptopModeTestData[i * 6 + 2]);
    base.Scale(kMeanGravity);
    gfx::Vector3dF lid(-kAccelerometerLaptopModeTestData[i * 6 + 4],
                       kAccelerometerLaptopModeTestData[i * 6 + 3],
                       kAccelerometerLaptopModeTestData[i * 6 + 5]);
    lid.Scale(kMeanGravity);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_FALSE(IsTabletModeStarted());
  }
}

TEST_F(TabletModeControllerTest, TabletModeTest) {
  // Trigger tablet mode by opening to 270 to begin the test in tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravity),
                          gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  ASSERT_TRUE(IsTabletModeStarted());

  // Feeds in sample accelerometer data and verifies that there are no
  // transitions out of tabletmode / tablet mode while shaking the device
  // around. Note the conversion from device data to accelerometer updates
  // consistent with accelerometer_reader.cc.
  ASSERT_EQ(0u, kAccelerometerFullyOpenTestDataLength % 6);
  for (size_t i = 0; i < kAccelerometerFullyOpenTestDataLength / 6; ++i) {
    gfx::Vector3dF base(-kAccelerometerFullyOpenTestData[i * 6 + 1],
                        -kAccelerometerFullyOpenTestData[i * 6],
                        -kAccelerometerFullyOpenTestData[i * 6 + 2]);
    base.Scale(kMeanGravity);
    gfx::Vector3dF lid(-kAccelerometerFullyOpenTestData[i * 6 + 4],
                       kAccelerometerFullyOpenTestData[i * 6 + 3],
                       kAccelerometerFullyOpenTestData[i * 6 + 5]);
    lid.Scale(kMeanGravity);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_TRUE(IsTabletModeStarted());
  }
}

TEST_F(TabletModeControllerTest, VerticalHingeTest) {
  // Feeds in sample accelerometer data and verifies that there are no
  // transitions out of tabletmode / tablet mode while shaking the device
  // around, while the hinge is nearly vertical. The data was captured from
  // maxmimize_mode_controller.cc and does not require conversion.
  ASSERT_EQ(0u, kAccelerometerVerticalHingeTestDataLength % 6);
  for (size_t i = 0; i < kAccelerometerVerticalHingeTestDataLength / 6; ++i) {
    gfx::Vector3dF base(kAccelerometerVerticalHingeTestData[i * 6],
                        kAccelerometerVerticalHingeTestData[i * 6 + 1],
                        kAccelerometerVerticalHingeTestData[i * 6 + 2]);
    gfx::Vector3dF lid(kAccelerometerVerticalHingeTestData[i * 6 + 3],
                       kAccelerometerVerticalHingeTestData[i * 6 + 4],
                       kAccelerometerVerticalHingeTestData[i * 6 + 5]);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_TRUE(IsTabletModeStarted());
  }
}

// Test if this case does not crash. See http://crbug.com/462806
TEST_F(TabletModeControllerTest, DisplayDisconnectionDuringOverview) {
  // Do not animate wallpaper on entering overview.
  WindowSelectorController::SetDoNotChangeWallpaperBlurForTests();

  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithBounds(gfx::Rect(800, 0, 100, 100)));
  ASSERT_NE(w1->GetRootWindow(), w2->GetRootWindow());
  ASSERT_FALSE(IsTabletModeStarted());

  tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->ToggleOverview());

  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(w1->GetRootWindow(), w2->GetRootWindow());
}

// Test that the disabling of the internal display exits tablet mode, and that
// while disabled we do not re-enter tablet mode.
TEST_F(TabletModeControllerTest, NoTabletModeWithDisabledInternalDisplay) {
  UpdateDisplay("200x200, 200x200");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  ASSERT_FALSE(IsTabletModeStarted());

  // Set up a mode with the internal display deactivated before switching to
  // tablet mode (which will enable mirror mode with only one display).
  std::vector<display::ManagedDisplayInfo> secondary_only;
  secondary_only.push_back(display_manager()->GetDisplayInfo(
      display_manager()->GetDisplayAt(1).id()));

  // Opening the lid to 270 degrees should start tablet mode.
  OpenLidToAngle(270.0f);
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Close lid and deactivate the internal display to simulate Docked Mode.
  CloseLid();
  display_manager()->OnNativeDisplaysChanged(secondary_only);
  ASSERT_FALSE(display_manager()->IsActiveDisplayId(internal_display_id));
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(270.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Tablet mode signal should also be ignored.
  SetTabletMode(true);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

// Tests that is a tablet mode signal is received while docked, that maximize
// mode is enabled upon exiting docked mode.
TEST_F(TabletModeControllerTest, TabletModeAfterExitingDockedMode) {
  UpdateDisplay("200x200, 200x200");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  ASSERT_FALSE(IsTabletModeStarted());

  // Deactivate internal display to simulate Docked Mode.
  std::vector<display::ManagedDisplayInfo> all_displays;
  all_displays.push_back(display_manager()->GetDisplayInfo(
      display_manager()->GetDisplayAt(0).id()));
  std::vector<display::ManagedDisplayInfo> secondary_only;
  display::ManagedDisplayInfo secondary_display =
      display_manager()->GetDisplayInfo(
          display_manager()->GetDisplayAt(1).id());
  all_displays.push_back(secondary_display);
  secondary_only.push_back(secondary_display);
  display_manager()->OnNativeDisplaysChanged(secondary_only);
  ASSERT_FALSE(display_manager()->IsActiveDisplayId(internal_display_id));

  // Tablet mode signal should also be ignored.
  SetTabletMode(true);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Exiting docked state
  display_manager()->OnNativeDisplaysChanged(all_displays);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  EXPECT_TRUE(IsTabletModeStarted());
}

// Verify that the device won't exit tabletmode / tablet mode for unstable
// angles when hinge is nearly vertical
TEST_F(TabletModeControllerTest, VerticalHingeUnstableAnglesTest) {
  // Trigger tablet mode by opening to 270 to begin the test in tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravity),
                          gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  ASSERT_TRUE(IsTabletModeStarted());

  // Feeds in sample accelerometer data and verifies that there are no
  // transitions out of tabletmode / tablet mode while shaking the device
  // around, while the hinge is nearly vertical. The data was captured
  // from maxmimize_mode_controller.cc and does not require conversion.
  ASSERT_EQ(0u, kAccelerometerVerticalHingeUnstableAnglesTestDataLength % 6);
  for (size_t i = 0;
       i < kAccelerometerVerticalHingeUnstableAnglesTestDataLength / 6; ++i) {
    gfx::Vector3dF base(
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6],
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 1],
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 2]);
    gfx::Vector3dF lid(
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 3],
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 4],
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 5]);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_TRUE(IsTabletModeStarted());
  }
}

// Tests that when a TabletModeController is created that cached tablet mode
// state will trigger a mode update.
TEST_F(TabletModeControllerTest, InitializedWhileTabletModeSwitchOn) {
  base::RunLoop().RunUntilIdle();
  // FakePowerManagerClient is always installed for tests
  chromeos::FakePowerManagerClient* power_manager_client =
      static_cast<chromeos::FakePowerManagerClient*>(
          chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
  power_manager_client->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks::Now());

  // Clear the callback that was set by the original TabletModeController.
  TabletMode::SetCallback({});

  TabletModeController controller;
  controller.OnShellInitialized();
  EXPECT_FALSE(controller.IsTabletModeWindowManagerEnabled());
  // PowerManagerClient callback is a posted task.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller.IsTabletModeWindowManagerEnabled());
}

TEST_F(TabletModeControllerTest, RestoreAfterExit) {
  UpdateDisplay("1000x600");
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 10, 900, 300)));
  tablet_mode_controller()->EnableTabletModeWindowManager(true);
  Shell::Get()->screen_orientation_controller()->SetLockToRotation(
      display::Display::ROTATE_90);
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(display::Display::ROTATE_90, display.rotation());
  EXPECT_LT(display.size().width(), display.size().height());
  tablet_mode_controller()->EnableTabletModeWindowManager(false);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  // Sanity checks.
  EXPECT_EQ(display::Display::ROTATE_0, display.rotation());
  EXPECT_GT(display.size().width(), display.size().height());

  // The bounds should be restored to the original bounds, and
  // should not be clamped by the portrait display in touch view.
  EXPECT_EQ(gfx::Rect(10, 10, 900, 300), w1->bounds());
}

TEST_F(TabletModeControllerTest, RecordLidAngle) {
  // The timer shouldn't be running before we've received accelerometer data.
  EXPECT_FALSE(
      tablet_mode_controller()->TriggerRecordLidAngleTimerForTesting());

  base::HistogramTester histogram_tester;
  OpenLidToAngle(300.0f);
  ASSERT_TRUE(tablet_mode_controller()->TriggerRecordLidAngleTimerForTesting());
  histogram_tester.ExpectBucketCount(
      TabletModeController::kLidAngleHistogramName, 300, 1);

  ASSERT_TRUE(tablet_mode_controller()->TriggerRecordLidAngleTimerForTesting());
  histogram_tester.ExpectBucketCount(
      TabletModeController::kLidAngleHistogramName, 300, 2);

  OpenLidToAngle(90.0f);
  ASSERT_TRUE(tablet_mode_controller()->TriggerRecordLidAngleTimerForTesting());
  histogram_tester.ExpectBucketCount(
      TabletModeController::kLidAngleHistogramName, 90, 1);

  // The timer should be stopped in response to a lid-only update since we can
  // no longer compute an angle.
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravity));
  EXPECT_FALSE(
      tablet_mode_controller()->TriggerRecordLidAngleTimerForTesting());
  histogram_tester.ExpectTotalCount(
      TabletModeController::kLidAngleHistogramName, 3);

  // When lid and base data is received, the timer should be started again.
  OpenLidToAngle(180.0f);
  ASSERT_TRUE(tablet_mode_controller()->TriggerRecordLidAngleTimerForTesting());
  histogram_tester.ExpectBucketCount(
      TabletModeController::kLidAngleHistogramName, 180, 1);
}

// Tests that when an external mouse is connected, flipping the
// lid of the chromebook will not enter tablet mode.
TEST_F(TabletModeControllerTest, CannotEnterTabletModeWithExternalMouse) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  base::RunLoop().RunUntilIdle();
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();

  OpenLidToAngle(300.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // Attach a external mouse.
  ws::InputDeviceClientTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());

  // Open lid to tent mode. Verify that tablet mode is not started.
  OpenLidToAngle(300.0f);
  EXPECT_FALSE(IsTabletModeStarted());
}

// Tests that when we plug in a external mouse the device will
// leave tablet mode.
TEST_F(TabletModeControllerTest, LeaveTabletModeWhenExternalMouseConnected) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  base::RunLoop().RunUntilIdle();
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();

  // Start in tablet mode.
  OpenLidToAngle(300.0f);
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach external mouse and keyboard. Verify that tablet mode has ended, but
  // events are still blocked because the keyboard is still facing the bottom.
  ws::InputDeviceClientTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Verify that after unplugging the mouse, tablet mode will resume.
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());
}

// Test that plug in or out a mouse in laptop mode will not change current
// laptop mode.
TEST_F(TabletModeControllerTest, ExternalMouseInLaptopMode) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  base::RunLoop().RunUntilIdle();
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();

  // Start in laptop mode.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  ws::InputDeviceClientTestApi().SetMouseDevices({ui::InputDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the external mouse. It still should maintain in laptop mode
  // because its lid angle is still in laptop mode.
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

// Test that the ui mode and input event blocker should be both correctly
// updated when there is a change in external mouse and lid angle.
TEST_F(TabletModeControllerTest, ExternalMouseWithLidAngleTest) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  base::RunLoop().RunUntilIdle();
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();

  // Start in laptop mode.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  ws::InputDeviceClientTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Now flip the device to tablet mode angle. The device should stay in
  // clamshell mode because of the external mouse. But the internal input events
  // should be blocked.
  OpenLidToAngle(300.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Remove the external mouse should enter tablet mode now. The internal input
  // events should still be blocked.
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach the mouse again should enter clamshell mode again.
  ws::InputDeviceClientTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Flip the device back to clamshell angle. The device should stay in
  // clamshell mode and the internal input events should not be blocked.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the mouse. The device should stay in clamshell mode and the
  // internal events should not be blocked.
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

// Test that the ui mode and input event blocker should be both correctly
// updated when there is a change in external mouse and tablet mode switch
// value.
TEST_F(TabletModeControllerTest, ExternalMouseWithTabletModeSwithTest) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  base::RunLoop().RunUntilIdle();
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();

  // Start in laptop mode.
  SetTabletMode(false);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  ws::InputDeviceClientTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Now set tablet mode switch value to true. The device should stay in
  // clamshell mode because of the external mouse. But the internal input events
  // should be blocked.
  SetTabletMode(true);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Remove the external mouse should enter tablet mode now. The internal input
  // events should still be blocked.
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach the mouse again should enter clamshell mode again.
  ws::InputDeviceClientTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Set tablet mode switch value to false. The device should stay in
  // clamshell mode and the internal input events should not be blocked.
  SetTabletMode(false);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the mouse. The device should stay in clamshell mode and the
  // internal events should not be blocked.
  ws::InputDeviceClientTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

class TabletModeControllerForceTabletModeTest
    : public TabletModeControllerTest {
 public:
  TabletModeControllerForceTabletModeTest() = default;
  ~TabletModeControllerForceTabletModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshUiMode, switches::kAshUiModeTablet);
    TabletModeControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModeControllerForceTabletModeTest);
};

// Verify when the force touch view mode flag is turned on, tablet mode is on
// initially, and opening the lid to less than 180 degress or setting tablet
// mode to off will not turn off tablet mode.
TEST_F(TabletModeControllerForceTabletModeTest, ForceTabletModeTest) {
  EXPECT_EQ(TabletModeController::UiMode::kTabletMode, forced_ui_mode());
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  OpenLidToAngle(30.0f);
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  SetTabletMode(false);
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Tests that attaching a external mouse will not change the mode.
  ws::InputDeviceClientTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());
}

class TabletModeControllerForceClamshellModeTest
    : public TabletModeControllerTest {
 public:
  TabletModeControllerForceClamshellModeTest() = default;
  ~TabletModeControllerForceClamshellModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshUiMode, switches::kAshUiModeClamshell);
    TabletModeControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModeControllerForceClamshellModeTest);
};

// Tests that when the force touch view mode flag is set to clamshell, clamshell
// mode is on initially, and cannot be changed by lid angle or manually entering
// tablet mode.
TEST_F(TabletModeControllerForceClamshellModeTest, ForceClamshellModeTest) {
  EXPECT_EQ(TabletModeController::UiMode::kClamshell, forced_ui_mode());
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(200.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  SetTabletMode(true);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

}  // namespace ash
