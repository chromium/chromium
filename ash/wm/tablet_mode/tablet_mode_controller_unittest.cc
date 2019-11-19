// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_controller.h"

#include <math.h>

#include <utility>
#include <vector>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/fps_counter.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_wallpaper_controller.h"
#include "ash/wm/splitview/multi_display_overview_and_split_view_test.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/numerics/math_constants.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using base::kMeanGravityFloat;

// The strings are "Touchview" as they're already used in metrics.
constexpr char kTabletModeInitiallyDisabled[] = "Touchview_Initially_Disabled";
constexpr char kTabletModeEnabled[] = "Touchview_Enabled";
constexpr char kTabletModeDisabled[] = "Touchview_Disabled";

constexpr char kEnterHistogram[] = "Ash.TabletMode.AnimationSmoothness.Enter";
constexpr char kExitHistogram[] = "Ash.TabletMode.AnimationSmoothness.Exit";

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

class TabletModeControllerTest : public MultiDisplayOverviewAndSplitViewTest {
 public:
  TabletModeControllerTest() = default;
  ~TabletModeControllerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletMode);
    MultiDisplayOverviewAndSplitViewTest::SetUp();
    AccelerometerReader::GetInstance()->RemoveObserver(
        tablet_mode_controller());
    FpsCounter::SetForceReportZeroAnimationForTest(true);

    // Set the first display to be the internal display for the accelerometer
    // screen rotation tests.
    display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
        .SetFirstDisplayAsInternalDisplay();

    test_api_ = std::make_unique<TabletModeControllerTestApi>();
  }

  void TearDown() override {
    FpsCounter::SetForceReportZeroAnimationForTest(false);
    AccelerometerReader::GetInstance()->AddObserver(tablet_mode_controller());
    MultiDisplayOverviewAndSplitViewTest::TearDown();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
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

  base::UserActionTester* user_action_tester() { return &user_action_tester_; }

  void SuspendImminent() { test_api_->SuspendImminent(); }
  void SuspendDone(const base::TimeDelta& sleep_duration) {
    test_api_->SuspendDone(sleep_duration);
  }

  bool IsScreenshotShown() const { return test_api_->IsScreenshotShown(); }

  // Creates a test window snapped on the left in desktop mode.
  std::unique_ptr<aura::Window> CreateDesktopWindowSnappedLeft() {
    std::unique_ptr<aura::Window> window = CreateTestWindow();
    WMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_LEFT);
    WindowState::Get(window.get())->OnWMEvent(&snap_to_left);
    return window;
  }

  // Creates a test window snapped on the right in desktop mode.
  std::unique_ptr<aura::Window> CreateDesktopWindowSnappedRight() {
    std::unique_ptr<aura::Window> window = CreateTestWindow();
    WMEvent snap_to_right(WM_EVENT_CYCLE_SNAP_RIGHT);
    WindowState::Get(window.get())->OnWMEvent(&snap_to_right);
    return window;
  }

 private:
  std::unique_ptr<TabletModeControllerTestApi> test_api_;

  base::SimpleTestTickClock test_tick_clock_;

  // Tracks user action counts.
  base::UserActionTester user_action_tester_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeControllerTest);
};

// Verify TabletMode enabled/disabled user action metrics are recorded.
TEST_P(TabletModeControllerTest, VerifyTabletModeEnabledDisabledCounts) {
  ASSERT_EQ(1,
            user_action_tester()->GetActionCount(kTabletModeInitiallyDisabled));
  ASSERT_EQ(0, user_action_tester()->GetActionCount(kTabletModeEnabled));
  ASSERT_EQ(0, user_action_tester()->GetActionCount(kTabletModeDisabled));

  user_action_tester()->ResetCounts();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTabletModeEnabled));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTabletModeDisabled));
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTabletModeEnabled));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTabletModeDisabled));

  user_action_tester()->ResetCounts();
  tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTabletModeEnabled));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTabletModeDisabled));
  tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTabletModeEnabled));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTabletModeDisabled));
}

// Verify that closing the lid will exit tablet mode.
TEST_P(TabletModeControllerTest, CloseLidWhileInTabletMode) {
  OpenLidToAngle(315.0f);
  ASSERT_TRUE(IsTabletModeStarted());

  CloseLid();
  EXPECT_FALSE(IsTabletModeStarted());
}

// Verify that tablet mode will not be entered when the lid is closed.
TEST_P(TabletModeControllerTest, HingeAnglesWithLidClosed) {
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
TEST_P(TabletModeControllerTest, OpenLidUnstableLidAngle) {
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
TEST_P(TabletModeControllerTest, TabletModeSwitchOnWithOpenUnstableLidAngle) {
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
TEST_P(TabletModeControllerTest, CloseLidUnstableLidAngle) {
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
TEST_P(TabletModeControllerTest, TabletModeSwitchOnWithCloseUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  SetTabletMode(true /*on*/);
  EXPECT_TRUE(IsTabletModeStarted());

  CloseLid();
  EXPECT_TRUE(IsTabletModeStarted());

  SetTabletMode(false /*on*/);
  EXPECT_FALSE(IsTabletModeStarted());
}

TEST_P(TabletModeControllerTest, TabletModeTransition) {
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
TEST_P(TabletModeControllerTest, TabletModeTransitionNoKeyboardAccelerometer) {
  ASSERT_FALSE(IsTabletModeStarted());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat));
  ASSERT_FALSE(IsTabletModeStarted());

  SetTabletMode(true);
  EXPECT_TRUE(IsTabletModeStarted());

  // Single sensor reading should not change mode.
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat));
  EXPECT_TRUE(IsTabletModeStarted());

  // With a single sensor we should exit immediately on the tablet mode switch
  // rather than waiting for stabilized accelerometer readings.
  SetTabletMode(false);
  EXPECT_FALSE(IsTabletModeStarted());
}

// Verify the tablet mode enter/exit thresholds for stable angles.
TEST_P(TabletModeControllerTest, StableHingeAnglesWithLidOpened) {
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
TEST_P(TabletModeControllerTest, EnterTabletModeWithUnstableLidAngle) {
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
TEST_P(TabletModeControllerTest, NotExitTabletModeWithUnstableLidAngle) {
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
TEST_P(TabletModeControllerTest, HingeAligned) {
  // Laptop in normal orientation lid open 90 degrees.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, -kMeanGravityFloat),
                          gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_FALSE(IsTabletModeStarted());

  // Completely vertical.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f),
                          gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_FALSE(IsTabletModeStarted());

  // Close to vertical but with hinge appearing to be open 270 degrees.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, -0.1f),
                          gfx::Vector3dF(kMeanGravityFloat, 0.1f, 0.0f));
  EXPECT_FALSE(IsTabletModeStarted());

  // Flat and open 270 degrees should start tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, -kMeanGravityFloat),
                          gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_TRUE(IsTabletModeStarted());

  // Normal 90 degree orientation but near vertical should stay in maximize
  // mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, -0.1f),
                          gfx::Vector3dF(kMeanGravityFloat, -0.1f, 0.0f));
  EXPECT_TRUE(IsTabletModeStarted());
}

TEST_P(TabletModeControllerTest, LaptopTest) {
  // Feeds in sample accelerometer data and verifies that there are no
  // transitions into tabletmode / tablet mode while shaking the device around
  // with the hinge at less than 180 degrees. Note the conversion from device
  // data to accelerometer updates consistent with accelerometer_reader.cc.
  ASSERT_EQ(0u, kAccelerometerLaptopModeTestDataLength % 6);
  for (size_t i = 0; i < kAccelerometerLaptopModeTestDataLength / 6; ++i) {
    gfx::Vector3dF base(-kAccelerometerLaptopModeTestData[i * 6 + 1],
                        -kAccelerometerLaptopModeTestData[i * 6],
                        -kAccelerometerLaptopModeTestData[i * 6 + 2]);
    base.Scale(kMeanGravityFloat);
    gfx::Vector3dF lid(-kAccelerometerLaptopModeTestData[i * 6 + 4],
                       kAccelerometerLaptopModeTestData[i * 6 + 3],
                       kAccelerometerLaptopModeTestData[i * 6 + 5]);
    lid.Scale(kMeanGravityFloat);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_FALSE(IsTabletModeStarted());
  }
}

TEST_P(TabletModeControllerTest, TabletModeTest) {
  // Trigger tablet mode by opening to 270 to begin the test in tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat),
                          gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
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
    base.Scale(kMeanGravityFloat);
    gfx::Vector3dF lid(-kAccelerometerFullyOpenTestData[i * 6 + 4],
                       kAccelerometerFullyOpenTestData[i * 6 + 3],
                       kAccelerometerFullyOpenTestData[i * 6 + 5]);
    lid.Scale(kMeanGravityFloat);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_TRUE(IsTabletModeStarted());
  }
}

TEST_P(TabletModeControllerTest, VerticalHingeTest) {
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
TEST_P(TabletModeControllerTest, DisplayDisconnectionDuringOverview) {
  // Do not animate wallpaper on entering overview.
  OverviewWallpaperController::SetDoNotChangeWallpaperForTests();

  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithBounds(gfx::Rect(800, 0, 100, 100)));
  ASSERT_NE(w1->GetRootWindow(), w2->GetRootWindow());
  ASSERT_FALSE(IsTabletModeStarted());

  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(Shell::Get()->overview_controller()->StartOverview());

  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(w1->GetRootWindow(), w2->GetRootWindow());
}

// Test that the disabling of the internal display exits tablet mode, and that
// while disabled we do not re-enter tablet mode.
TEST_P(TabletModeControllerTest, NoTabletModeWithDisabledInternalDisplay) {
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
TEST_P(TabletModeControllerTest, TabletModeAfterExitingDockedMode) {
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
TEST_P(TabletModeControllerTest, VerticalHingeUnstableAnglesTest) {
  // Trigger tablet mode by opening to 270 to begin the test in tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat),
                          gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
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
class TabletModeControllerInitedFromPowerManagerClientTest
    : public TabletModeControllerTest {
 public:
  TabletModeControllerInitedFromPowerManagerClientTest() = default;
  ~TabletModeControllerInitedFromPowerManagerClientTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    power_manager_client()->SetTabletMode(
        chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks::Now());
    TabletModeControllerTest::SetUp();
    // Remove TabletModeController as an observer of input device events to
    // prevent interfering with the test.
    ui::DeviceDataManager::GetInstance()->RemoveObserver(
        tablet_mode_controller());
  }
};

TEST_P(TabletModeControllerInitedFromPowerManagerClientTest,
       InitializedWhileTabletModeSwitchOn) {
  EXPECT_FALSE(tablet_mode_controller()->InTabletMode());
  // PowerManagerClient callback is a posted task.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tablet_mode_controller()->InTabletMode());
}

TEST_P(TabletModeControllerTest, RestoreAfterExit) {
  UpdateDisplay("1000x600");
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 10, 900, 300)));
  tablet_mode_controller()->SetEnabledForTest(true);
  Shell::Get()->screen_orientation_controller()->SetLockToRotation(
      display::Display::ROTATE_90);
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(display::Display::ROTATE_90, display.rotation());
  EXPECT_LT(display.size().width(), display.size().height());
  tablet_mode_controller()->SetEnabledForTest(false);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  // Sanity checks.
  EXPECT_EQ(display::Display::ROTATE_0, display.rotation());
  EXPECT_GT(display.size().width(), display.size().height());

  // The bounds should be restored to the original bounds, and
  // should not be clamped by the portrait display in touch view.
  EXPECT_EQ(gfx::Rect(10, 10, 900, 300), w1->bounds());
}

TEST_P(TabletModeControllerTest, RecordLidAngle) {
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
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat));
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
TEST_P(TabletModeControllerTest, CannotEnterTabletModeWithExternalMouse) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});

  OpenLidToAngle(300.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // Attach a external mouse.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  EXPECT_FALSE(IsTabletModeStarted());

  // Open lid to tent mode. Verify that tablet mode is not started.
  OpenLidToAngle(300.0f);
  EXPECT_FALSE(IsTabletModeStarted());
}

// Tests that when we plug in a external mouse the device will
// leave tablet mode.
TEST_P(TabletModeControllerTest, LeaveTabletModeWhenExternalMouseConnected) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});

  // Start in tablet mode.
  OpenLidToAngle(300.0f);
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach external mouse and keyboard. Verify that tablet mode has ended, but
  // events are still blocked because the keyboard is still facing the bottom.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Verify that after unplugging the mouse, tablet mode will resume.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());
}

// Test that plug in or out a mouse in laptop mode will not change current
// laptop mode.
TEST_P(TabletModeControllerTest, ExternalMouseInLaptopMode) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});

  // Start in laptop mode.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the external mouse. It still should maintain in laptop mode
  // because its lid angle is still in laptop mode.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

// Test that docked mode prevents entering tablet mode on detaching an external
// mouse while in tablet position.
TEST_P(TabletModeControllerTest, ExternalMouseInDockedMode) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  base::RunLoop().RunUntilIdle();

  UpdateDisplay("800x600, 800x600");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  // Set the current list of devices with an external mouse.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});

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

  // Enter tablet position.
  SetTabletMode(true);
  ASSERT_FALSE(IsTabletModeStarted());

  // Detach the external mouse.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  // Still expect clamshell mode.
  EXPECT_FALSE(IsTabletModeStarted());
}

// Test that the ui mode and input event blocker should be both correctly
// updated when there is a change in external mouse and lid angle.
TEST_P(TabletModeControllerTest, ExternalMouseWithLidAngleTest) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});

  // Start in laptop mode.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
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
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach the mouse again should enter clamshell mode again.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Flip the device back to clamshell angle. The device should stay in
  // clamshell mode and the internal input events should not be blocked.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the mouse. The device should stay in clamshell mode and the
  // internal events should not be blocked.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

// Test that the ui mode and input event blocker should be both correctly
// updated when there is a change in external mouse and tablet mode switch
// value.
TEST_P(TabletModeControllerTest, ExternalMouseWithTabletModeSwithTest) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});

  // Start in laptop mode.
  SetTabletMode(false);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
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
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach the mouse again should enter clamshell mode again.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Set tablet mode switch value to false. The device should stay in
  // clamshell mode and the internal input events should not be blocked.
  SetTabletMode(false);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the mouse. The device should stay in clamshell mode and the
  // internal events should not be blocked.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

// Tests that when an external touchpad is connected, the device should exit
// tablet mode and enter clamshell mode.
TEST_P(TabletModeControllerTest, ExternalTouchPadTest) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  ui::DeviceDataManagerTestApi().SetTouchpadDevices({});

  OpenLidToAngle(300.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  OpenLidToAngle(30.0f);
  EXPECT_FALSE(IsTabletModeStarted());

  // Attach a external touchpad.
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "touchpad")});
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Open lid to tent mode. Verify that tablet mode is not started.
  OpenLidToAngle(300.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Verify that after unplugging the touchpad, tablet mode will resume.
  ui::DeviceDataManagerTestApi().SetTouchpadDevices({});
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());
}

// Test that internal keyboard and mouse are not disabled in docked mode.
TEST_P(TabletModeControllerTest, InternalKeyboardMouseInDockedModeTest) {
  UpdateDisplay("800x600, 800x600");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  EXPECT_FALSE(IsTabletModeStarted());
  // Input devices events are unblocked.
  EXPECT_FALSE(AreEventsBlocked());
  EXPECT_TRUE(display::Display::HasInternalDisplay());
  EXPECT_TRUE(
      Shell::Get()->display_manager()->IsActiveDisplayId(internal_display_id));

  // Enter tablet mode first.
  SetTabletMode(true);
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

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
  // We should now enter in clamshell mode when the device is docked.
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Exiting docked state should enter tablet mode again.
  display_manager()->OnNativeDisplaysChanged(all_displays);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_TRUE(AreEventsBlocked());
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
// mode to off will not turn off tablet mode. The internal keyboard and trackpad
// should still work as it makes testing easier.
TEST_P(TabletModeControllerForceTabletModeTest, ForceTabletModeTest) {
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(30.0f);
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  SetTabletMode(false);
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Tests that attaching a external mouse will not change the mode.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  EXPECT_TRUE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

TEST_P(TabletModeControllerForceTabletModeTest, DockInForcedTabletMode) {
  UpdateDisplay("800x600, 800x600");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

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

  // Still expect tablet mode.
  EXPECT_TRUE(IsTabletModeStarted());
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
TEST_P(TabletModeControllerForceClamshellModeTest, ForceClamshellModeTest) {
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(200.0f);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  SetTabletMode(true);
  EXPECT_FALSE(IsTabletModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

// Test that if the active window is not snapped before tablet mode, then split
// view is not activated.
TEST_P(TabletModeControllerTest, StartTabletActiveNoSnap) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Test that if the active window is snapped on the left before tablet mode,
// then split view is activated with the active window on the left.
TEST_P(TabletModeControllerTest, StartTabletActiveLeftSnap) {
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedLeft();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Test that if the active window is snapped on the right before tablet mode,
// then split view is activated with the active window on the right.
TEST_P(TabletModeControllerTest, StartTabletActiveRightSnap) {
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedRight();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->right_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is snapped on the left and
// the previous window is snapped on the right, then split view is activated
// with the active window on the left and the previous window on the right.
TEST_P(TabletModeControllerTest, StartTabletActiveLeftSnapPreviousRightSnap) {
  std::unique_ptr<aura::Window> left_window = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> right_window =
      CreateDesktopWindowSnappedRight();
  wm::ActivateWindow(left_window.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_EQ(left_window.get(), split_view_controller()->left_window());
  EXPECT_EQ(right_window.get(), split_view_controller()->right_window());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(left_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is snapped on the right
// and the previous window is snapped on the left, then split view is activated
// with the active window on the right and the previous window on the left.
TEST_P(TabletModeControllerTest, StartTabletActiveRightSnapPreviousLeftSnap) {
  std::unique_ptr<aura::Window> left_window = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> right_window =
      CreateDesktopWindowSnappedRight();
  ASSERT_EQ(right_window.get(), window_util::GetActiveWindow());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_EQ(left_window.get(), split_view_controller()->left_window());
  EXPECT_EQ(right_window.get(), split_view_controller()->right_window());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is a transient child of a
// window snapped on the left, then split view is activated with the parent
// snapped on the left.
TEST_P(TabletModeControllerTest, StartTabletActiveTransientChildOfLeftSnap) {
  std::unique_ptr<aura::Window> parent = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> child =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  ::wm::AddTransientChild(parent.get(), child.get());
  wm::ActivateWindow(child.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(parent.get(), split_view_controller()->left_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(child.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is the app list and the
// previous window is snapped on the left, then split view is activated with the
// previous window on the left.
TEST_P(TabletModeControllerTest, StartTabletActiveAppListPreviousLeftSnap) {
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedLeft();
  Shell::Get()->app_list_controller()->ShowAppList();
  ASSERT_TRUE(wm::IsActiveWindow(
      GetAppListTestHelper()->GetAppListView()->GetWidget()->GetNativeView()));
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is being dragged and the
// previous window is snapped on the left, then split view is activated with the
// previous window on the left.
TEST_P(TabletModeControllerTest, StartTabletActiveDraggedPreviousLeftSnap) {
  std::unique_ptr<aura::Window> dragged_window = CreateTestWindow();
  std::unique_ptr<aura::Window> snapped_window =
      CreateDesktopWindowSnappedLeft();
  wm::ActivateWindow(dragged_window.get());
  ASSERT_TRUE(Shell::Get()->toplevel_window_event_handler()->AttemptToStartDrag(
      dragged_window.get(), gfx::Point(), HTCAPTION,
      ash::ToplevelWindowEventHandler::EndClosure()));
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(snapped_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is hidden from overview
// and the previous window is snapped on the left, then split view is activated
// with the previous window on the left.
TEST_P(TabletModeControllerTest,
       StartTabletActiveHiddenFromOverviewPreviousLeftSnap) {
  std::unique_ptr<aura::Window> window_hidden_from_overview =
      CreateTestWindow();
  window_hidden_from_overview->SetProperty(kHideInOverviewKey, true);
  std::unique_ptr<aura::Window> snapped_window =
      CreateDesktopWindowSnappedLeft();
  wm::ActivateWindow(window_hidden_from_overview.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(snapped_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is being dragged and the
// previous window is a transient child of a window snapped on the left, then
// split view is activated with the parent on the left.
TEST_P(TabletModeControllerTest,
       StartTabletActiveDraggedPreviousTransientChildOfLeftSnap) {
  std::unique_ptr<aura::Window> dragged_window = CreateTestWindow();
  std::unique_ptr<aura::Window> parent = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> child =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  ::wm::AddTransientChild(parent.get(), child.get());
  wm::ActivateWindow(child.get());
  wm::ActivateWindow(dragged_window.get());
  ASSERT_TRUE(Shell::Get()->toplevel_window_event_handler()->AttemptToStartDrag(
      dragged_window.get(), gfx::Point(), HTCAPTION,
      ash::ToplevelWindowEventHandler::EndClosure()));
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(parent.get(), split_view_controller()->left_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(parent.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is snapped on the left but
// does not meet the requirements to be snapped in split view, and the previous
// window is snapped on the right, then split view is not activated.
TEST_P(TabletModeControllerTest,
       StartTabletActiveDesktopOnlyLeftSnapPreviousRightSnap) {
  aura::test::TestWindowDelegate left_window_delegate;
  std::unique_ptr<aura::Window> left_window(CreateTestWindowInShellWithDelegate(
      &left_window_delegate, /*id=*/-1, /*bounds=*/gfx::Rect(0, 0, 400, 400)));
  const gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          left_window.get());
  left_window_delegate.set_minimum_size(
      gfx::Size(display_bounds.width() * 0.67f, display_bounds.height()));
  WindowState* left_window_state = WindowState::Get(left_window.get());
  ASSERT_TRUE(left_window_state->CanSnap());
  ASSERT_FALSE(CanSnapInSplitview(left_window.get()));
  WMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_LEFT);
  left_window_state->OnWMEvent(&snap_to_left);
  std::unique_ptr<aura::Window> right_window =
      CreateDesktopWindowSnappedRight();
  wm::ActivateWindow(left_window.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Test that if before tablet mode, the active window is snapped on the right
// but does not meet the requirements to be snapped in split view, and the
// previous window is snapped on the left, then split view is not activated.
TEST_P(TabletModeControllerTest,
       StartTabletActiveDesktopOnlyRightSnapPreviousLeftSnap) {
  std::unique_ptr<aura::Window> left_window = CreateDesktopWindowSnappedLeft();
  aura::test::TestWindowDelegate right_window_delegate;
  std::unique_ptr<aura::Window> right_window(
      CreateTestWindowInShellWithDelegate(
          &right_window_delegate, /*id=*/-1,
          /*bounds=*/gfx::Rect(0, 0, 400, 400)));
  const gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          right_window.get());
  right_window_delegate.set_minimum_size(
      gfx::Size(display_bounds.width() * 0.67f, display_bounds.height()));
  WindowState* right_window_state = WindowState::Get(right_window.get());
  ASSERT_TRUE(right_window_state->CanSnap());
  ASSERT_FALSE(CanSnapInSplitview(right_window.get()));
  WMEvent snap_to_right(WM_EVENT_CYCLE_SNAP_RIGHT);
  right_window_state->OnWMEvent(&snap_to_right);
  wm::ActivateWindow(right_window.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Test that if before tablet mode, the active window is snapped on the left and
// the previous window is snapped on the right but does not meet the
// requirements to be snapped in split view, then split view is activated with
// the active window on the left.
TEST_P(TabletModeControllerTest,
       StartTabletActiveLeftSnapPreviousDesktopOnlyRightSnap) {
  std::unique_ptr<aura::Window> left_window = CreateDesktopWindowSnappedLeft();
  aura::test::TestWindowDelegate right_window_delegate;
  std::unique_ptr<aura::Window> right_window(
      CreateTestWindowInShellWithDelegate(
          &right_window_delegate, /*id=*/-1,
          /*bounds=*/gfx::Rect(0, 0, 400, 400)));
  const gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          right_window.get());
  right_window_delegate.set_minimum_size(
      gfx::Size(display_bounds.width() * 0.67f, display_bounds.height()));
  WindowState* right_window_state = WindowState::Get(right_window.get());
  ASSERT_TRUE(right_window_state->CanSnap());
  ASSERT_FALSE(CanSnapInSplitview(right_window.get()));
  WMEvent snap_to_right(WM_EVENT_CYCLE_SNAP_RIGHT);
  right_window_state->OnWMEvent(&snap_to_right);
  ASSERT_EQ(left_window.get(), window_util::GetActiveWindow());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(left_window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(left_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is snapped on the right
// and the previous window is snapped on the left but does not meet the
// requirements to be snapped in split view, then split view is activated with
// the active window on the right.
TEST_P(TabletModeControllerTest,
       StartTabletActiveRightSnapPreviousDesktopOnlyLeftSnap) {
  aura::test::TestWindowDelegate left_window_delegate;
  std::unique_ptr<aura::Window> left_window(CreateTestWindowInShellWithDelegate(
      &left_window_delegate, /*id=*/-1, /*bounds=*/gfx::Rect(0, 0, 400, 400)));
  const gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          left_window.get());
  left_window_delegate.set_minimum_size(
      gfx::Size(display_bounds.width() * 0.67f, display_bounds.height()));
  WindowState* left_window_state = WindowState::Get(left_window.get());
  ASSERT_TRUE(left_window_state->CanSnap());
  ASSERT_FALSE(CanSnapInSplitview(left_window.get()));
  WMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_LEFT);
  left_window_state->OnWMEvent(&snap_to_left);
  std::unique_ptr<aura::Window> right_window =
      CreateDesktopWindowSnappedRight();
  ASSERT_EQ(right_window.get(), window_util::GetActiveWindow());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  EXPECT_EQ(right_window.get(), split_view_controller()->right_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
}

// Test that when entering tablet mode with a left snapped window, the applist
// is not visible because overview is shown.
TEST_P(TabletModeControllerTest,
       AppListNotSeenAfterEnteringTabletModeWithLeftSnappedWindow) {
  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedLeft();
  tablet_mode_controller()->SetEnabledForTest(true);
  app_list_controller->ShowAppList();
  EXPECT_FALSE(app_list_controller->IsVisible());
}

// Test that if both the active window and the previous window are snapped on
// the left before tablet mode, then split view is activated with the active
// window on the left.
TEST_P(TabletModeControllerTest, StartTabletActiveLeftSnapPreviousLeftSnap) {
  std::unique_ptr<aura::Window> window1 = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> window2 = CreateDesktopWindowSnappedLeft();
  wm::ActivateWindow(window1.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(window1.get(), split_view_controller()->left_window());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());
}

// Test that tablet mode controller does not respond to the input device changes
// during its suspend.
TEST_P(TabletModeControllerTest, DoNotObserverInputDeviceChangeDuringSuspend) {
  // Set the current list of devices to empty so that they don't interfere
  // with the test.
  ui::DeviceDataManagerTestApi().SetMouseDevices({});

  // Start in tablet mode.
  OpenLidToAngle(300.0f);
  EXPECT_TRUE(IsTabletModeStarted());

  // Attaching external mouse will end tablet mode.
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  EXPECT_FALSE(IsTabletModeStarted());

  // Now suspend the device. Input device changes are no longer be observed.
  SuspendImminent();
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  EXPECT_FALSE(IsTabletModeStarted());

  // Resume the device. Input device changes are being observed again.
  SuspendDone(base::TimeDelta::Max());
  EXPECT_TRUE(IsTabletModeStarted());

  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  EXPECT_FALSE(IsTabletModeStarted());
}

TEST_P(TabletModeControllerTest, TabletModeTransitionHistogramsNotLogged) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::HistogramTester histogram_tester;

  // Tests that we get no animation smoothness histograms when entering or
  // exiting tablet mode with no windows.
  {
    SCOPED_TRACE("No window");
    histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
    histogram_tester.ExpectTotalCount(kExitHistogram, 0);
    tablet_mode_controller()->SetEnabledForTest(true);
    tablet_mode_controller()->SetEnabledForTest(false);
    histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
    histogram_tester.ExpectTotalCount(kExitHistogram, 0);
  }

  // The workspace size changes when going between clamshell and tablet mode.
  // This means there will be an animation during the transition.
  if (chromeos::switches::ShouldShowShelfHotseat())
    return;

  // Test that we get no animation smoothness histograms when entering or
  // exiting tablet mode with a maximized window as no animation will take
  // place.
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  {
    SCOPED_TRACE("Window is maximized");
    WindowState::Get(window.get())->Maximize();
    window->layer()->GetAnimator()->StopAnimating();
    tablet_mode_controller()->SetEnabledForTest(true);
    EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
    histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
    histogram_tester.ExpectTotalCount(kExitHistogram, 0);
    tablet_mode_controller()->SetEnabledForTest(false);
    EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
    histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
    histogram_tester.ExpectTotalCount(kExitHistogram, 0);
  }
}

TEST_P(TabletModeControllerTest, TabletModeTransitionHistogramsLogged) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::HistogramTester histogram_tester;
  // We have two windows, which both animated into tablet mode, but we only
  // observe and record smoothness for one.
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto window2 = CreateTestWindow(gfx::Rect(300, 200));
  // Tests that we get one enter and one exit animation smoothess histogram when
  // entering and exiting tablet mode with a normal window.
  ui::Layer* layer = window->layer();
  ui::Layer* layer2 = window2->layer();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(window->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  layer->GetAnimator()->StopAnimating();
  layer2->GetAnimator()->StopAnimating();
  histogram_tester.ExpectTotalCount(kEnterHistogram, 1);
  histogram_tester.ExpectTotalCount(kExitHistogram, 0);

  layer = window->layer();
  layer2 = window2->layer();
  tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(layer->GetAnimator()->is_animating());
  EXPECT_TRUE(layer2->GetAnimator()->is_animating());
  layer->GetAnimator()->StopAnimating();
  layer2->GetAnimator()->StopAnimating();
  histogram_tester.ExpectTotalCount(kEnterHistogram, 1);
  histogram_tester.ExpectTotalCount(kExitHistogram, 1);
}

TEST_P(TabletModeControllerTest, TabletModeTransitionHistogramsSnappedWindows) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::HistogramTester histogram_tester;

  // Snap a window on either side.
  auto window = CreateDesktopWindowSnappedLeft();
  auto window2 = CreateDesktopWindowSnappedRight();
  window->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();

  // Tests that we have no logged metrics since nothing animates.
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window2->layer()->GetAnimator()->is_animating());
  histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
  histogram_tester.ExpectTotalCount(kExitHistogram, 0);

  tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window2->layer()->GetAnimator()->is_animating());
  window2->layer()->GetAnimator()->StopAnimating();
  histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
  histogram_tester.ExpectTotalCount(kExitHistogram, 0);
}

class TabletModeControllerScreenshotTest : public TabletModeControllerTest {
 public:
  TabletModeControllerScreenshotTest() = default;
  ~TabletModeControllerScreenshotTest() override = default;

  void SetUp() override {
    TabletModeControllerTest::SetUp();
    TabletModeController::SetUseScreenshotForTest(true);

    // Remove TabletModeController as an observer of input device events to
    // prevent interfering with the test.
    ui::DeviceDataManager::GetInstance()->RemoveObserver(
        tablet_mode_controller());

    scoped_animation_duration_scale_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    // PowerManagerClient callback is a posted task.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    scoped_animation_duration_scale_mode_.reset();
    ui::DeviceDataManager::GetInstance()->AddObserver(tablet_mode_controller());
    TabletModeControllerTest::TearDown();
  }

 private:
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      scoped_animation_duration_scale_mode_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeControllerScreenshotTest);
};

// Tests that when there are no animations, no screenshot is taken.
TEST_P(TabletModeControllerScreenshotTest, NoAnimationNoScreenshot) {
  // Tests that no windows means no screenshot.
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());
  SetTabletMode(false);

  // If the top window is already maximized, there is no animation, so no
  // screenshot should be shown.
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  WindowState::Get(window.get())->Maximize();
  window->layer()->GetAnimator()->StopAnimating();

  TabletMode::Waiter waiter(/*enable=*/true);
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());

  waiter.Wait();
  EXPECT_FALSE(IsScreenshotShown());
  // The window will animate if the hotseat is enabled because the workspace
  // area will change. As long as a screenshot is not shown, this is ok.
  if (chromeos::switches::ShouldShowShelfHotseat())
    return;
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
}

// Regression test for screenshot staying visible when entering tablet mode when
// already in overview mode. See https://crbug.com/1002735.
// Flaky in remote builds, see: crbug.com/1007961
TEST_P(TabletModeControllerScreenshotTest, DISABLED_FromOverviewNoScreenshot) {
  // Create two maximized windows.
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto window2 = CreateTestWindow(gfx::Rect(200, 200));
  WindowState::Get(window.get())->Maximize();
  WindowState::Get(window2.get())->Maximize();
  window->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();

  // Enter overview.
  Shell::Get()->overview_controller()->StartOverview();
  window->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();

  // Enter tablet mode.
  TabletMode::Waiter waiter(/*enable=*/true);
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());

  waiter.Wait();
  EXPECT_TRUE(IsScreenshotShown());

  // Tests that after ending the overview animation, the screenshot is
  // destroyed.
  window->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(IsScreenshotShown());
}

// Tests that the screenshot is visible when a window animation happens when
// entering tablet mode.
TEST_P(TabletModeControllerScreenshotTest, ScreenshotVisibility) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto window2 = CreateTestWindow(gfx::Rect(300, 200));
  ui::Layer* layer = window2->layer();
  ASSERT_FALSE(IsScreenshotShown());

  TabletMode::Waiter waiter(/*enable=*/true);
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());

  // Tests that after waiting for the async tablet mode entry, the screenshot is
  // shown.
  waiter.Wait();
  EXPECT_TRUE(IsScreenshotShown());
  EXPECT_TRUE(layer->GetAnimator()->is_animating());

  // Tests that the screenshot is destroyed after the window is done animating.
  layer->GetAnimator()->StopAnimating();
  EXPECT_FALSE(IsScreenshotShown());
}

// Tests that if we exit tablet mode before the screenshot is taken, there is no
// crash. (See https://crbug.com/1012879).
TEST_P(TabletModeControllerScreenshotTest, NoCrashWhenExitingWithoutWaiting) {
  // One non-maximized window is needed for screenshot to be taken.
  auto window = CreateTestWindow(gfx::Rect(200, 200));

  SetTabletMode(true);
  SetTabletMode(false);
  EXPECT_FALSE(IsScreenshotShown());

  // Tests that reentering tablet mode without waiting causes no crash either.
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());
}

INSTANTIATE_TEST_SUITE_P(, TabletModeControllerTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(,
                         TabletModeControllerInitedFromPowerManagerClientTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(,
                         TabletModeControllerForceTabletModeTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(,
                         TabletModeControllerForceClamshellModeTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(, TabletModeControllerScreenshotTest, testing::Bool());

}  // namespace ash
