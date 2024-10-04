// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/tablet_mode/tablet_mode_controller.h"

#include <math.h>

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/math_constants.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/message_center.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/wm/core/cursor_manager.h"
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

constexpr char kLsbReleaseContent[] = "CHROMEOS_RELEASE_NAME=Chromium OS\n";

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

  TabletModeControllerTest(const TabletModeControllerTest&) = delete;
  TabletModeControllerTest& operator=(const TabletModeControllerTest&) = delete;

  ~TabletModeControllerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletMode);
    AshTestBase::SetUp();
    AccelerometerReader::GetInstance()->RemoveObserver(
        tablet_mode_controller());

    // Set the first display to be the internal display for the accelerometer
    // screen rotation tests.
    display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
        .SetFirstDisplayAsInternalDisplay();

    test_api_ = std::make_unique<TabletModeControllerTestApi>();

    // Unit tests are supposed to be in reference to a hypothetical computer,
    // but they can detect a mouse connected to the actual computer on which
    // they are run. That is relevant here because external pointing devices
    // prevent tablet mode. Detach all mice, so that unit tests will produce the
    // same results whether the host machine has a mouse or not.
    DetachAllMice();
  }

  void TearDown() override {
    AccelerometerReader::GetInstance()->AddObserver(tablet_mode_controller());
    // Rset before Shell destruction.
    test_api_.reset();
    AshTestBase::TearDown();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  TabletModeController* tablet_mode_controller() {
    return Shell::Get()->tablet_mode_controller();
  }

  void AttachExternalMouse() { test_api_->AttachExternalMouse(); }
  void AttachExternalTouchpad() { test_api_->AttachExternalTouchpad(); }
  void DetachAllMice() { test_api_->DetachAllMice(); }
  void DetachAllTouchpads() { test_api_->DetachAllTouchpads(); }

  void TriggerLidUpdate(const gfx::Vector3dF& lid) {
    test_api_->TriggerLidUpdate(lid);
  }

  void TriggerBaseAndLidUpdate(const gfx::Vector3dF& base,
                               const gfx::Vector3dF& lid) {
    test_api_->TriggerBaseAndLidUpdate(base, lid);
  }

  bool IsInPhysicalTabletState() const {
    return test_api_->IsInPhysicalTabletState();
  }

  // Attaches a SimpleTestTickClock to the TabletModeController with a non
  // null value initial value.
  void AttachTickClockForTest() {
    test_tick_clock_.Advance(base::Seconds(1));
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
  float GetLidAngle() const { return test_api_->GetLidAngle(); }

  // Creates a test window snapped on the left in desktop mode.
  std::unique_ptr<aura::Window> CreateDesktopWindowSnappedLeft(
      const gfx::Rect& bounds = gfx::Rect()) {
    std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);
    WindowSnapWMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_PRIMARY);
    WindowState::Get(window.get())->OnWMEvent(&snap_to_left);
    return window;
  }

  // Creates a test window snapped on the right in desktop mode.
  std::unique_ptr<aura::Window> CreateDesktopWindowSnappedRight(
      const gfx::Rect& bounds = gfx::Rect()) {
    std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);
    WindowSnapWMEvent snap_to_right(WM_EVENT_CYCLE_SNAP_SECONDARY);
    WindowState::Get(window.get())->OnWMEvent(&snap_to_right);
    return window;
  }

  // Waits for |window|'s animation to finish.
  void WaitForWindowAnimation(aura::Window* window) {
    auto* compositor = window->layer()->GetCompositor();

    while (window->layer()->GetAnimator()->is_animating()) {
      EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
    }
  }

  // Wait one more frame presented for the metrics to get recorded.
  // std::ignore and timeout is because the frame could already be
  // presented.
  void WaitForSmoothnessMetrics() {
    std::ignore = ui::WaitForNextFrameToBePresented(
        Shell::GetPrimaryRootWindow()->layer()->GetCompositor(),
        base::Milliseconds(100));
  }

 private:
  std::unique_ptr<TabletModeControllerTestApi> test_api_;

  base::SimpleTestTickClock test_tick_clock_;

  // Tracks user action counts.
  base::UserActionTester user_action_tester_;
};

// Verify TabletMode enabled/disabled user action metrics are recorded.
TEST_F(TabletModeControllerTest, VerifyTabletModeEnabledDisabledCounts) {
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
TEST_F(TabletModeControllerTest, CloseLidWhileInTabletMode) {
  OpenLidToAngle(315.0f);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());

  CloseLid();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Verify that tablet mode will not be entered when the lid is closed.
TEST_F(TabletModeControllerTest, HingeAnglesWithLidClosed) {
  AttachTickClockForTest();

  CloseLid();

  OpenLidToAngle(270.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(315.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(355.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Verify the unstable lid angle is suppressed during opening the lid.
TEST_F(TabletModeControllerTest, OpenLidUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  // Simulate the erroneous accelerometer readings.
  OpenLidToAngle(355.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Simulate the correct accelerometer readings.
  OpenLidToAngle(5.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Verify that suppressing unstable lid angle while opening the lid does not
// override tablet mode switch on value - if tablet mode switch is on, device
// should remain in tablet mode.
TEST_F(TabletModeControllerTest, TabletModeSwitchOnWithOpenUnstableLidAngle) {
  AttachTickClockForTest();

  SetTabletMode(true /*on*/);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  OpenLid();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Simulate the correct accelerometer readings.
  OpenLidToAngle(355.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Simulate the erroneous accelerometer readings.
  OpenLidToAngle(5.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
}

// Verify the unstable lid angle is suppressed during closing the lid.
TEST_F(TabletModeControllerTest, CloseLidUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  OpenLidToAngle(45.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Simulate the correct accelerometer readings.
  OpenLidToAngle(5.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Simulate the erroneous accelerometer readings.
  OpenLidToAngle(355.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  CloseLid();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Verify that suppressing unstable lid angle when the lid is closed does not
// override tablet mode switch on value - if tablet mode switch is on, device
// should remain in tablet mode.
TEST_F(TabletModeControllerTest, TabletModeSwitchOnWithCloseUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  SetTabletMode(true /*on*/);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  CloseLid();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  SetTabletMode(false /*on*/);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

TEST_F(TabletModeControllerTest, TabletModeTransition) {
  OpenLidToAngle(90.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Unstable reading. This should not trigger tablet mode.
  HoldDeviceVertical();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // When tablet mode switch is on it should force tablet mode even if the
  // reading is not stable.
  SetTabletMode(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // After tablet mode switch is off it should stay in tablet mode if the
  // reading is not stable.
  SetTabletMode(false);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Should leave tablet mode when the lid angle is small enough.
  OpenLidToAngle(90.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(300.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
}

// When there is no keyboard accelerometer available tablet mode should solely
// rely on the tablet mode switch.
TEST_F(TabletModeControllerTest, TabletModeTransitionNoKeyboardAccelerometer) {
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat));
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  SetTabletMode(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Single sensor reading should not change mode.
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat));
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // With a single sensor we should exit immediately on the tablet mode switch
  // rather than waiting for stabilized accelerometer readings.
  SetTabletMode(false);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Verify the tablet mode enter/exit thresholds for stable angles.
TEST_F(TabletModeControllerTest, StableHingeAnglesWithLidOpened) {
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(180.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(315.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(180.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(45.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(270.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(90.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Verify entering tablet mode for unstable lid angles when a certain range of
// time has passed.
TEST_F(TabletModeControllerTest, EnterTabletModeWithUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(5.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  EXPECT_FALSE(CanUseUnstableLidAngle());
  OpenLidToAngle(355.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // 1 second after entering unstable angle zone.
  AdvanceTickClock(base::Seconds(1));
  EXPECT_FALSE(CanUseUnstableLidAngle());
  OpenLidToAngle(355.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // 2 seconds after entering unstable angle zone.
  AdvanceTickClock(base::Seconds(1));
  EXPECT_TRUE(CanUseUnstableLidAngle());
  OpenLidToAngle(355.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
}

// Verify not exiting tablet mode for unstable lid angles even after a certain
// range of time has passed.
TEST_F(TabletModeControllerTest, NotExitTabletModeWithUnstableLidAngle) {
  AttachTickClockForTest();

  OpenLid();

  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(280.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(5.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // 1 second after entering unstable angle zone.
  AdvanceTickClock(base::Seconds(1));
  EXPECT_FALSE(CanUseUnstableLidAngle());
  OpenLidToAngle(5.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // 2 seconds after entering unstable angle zone.
  AdvanceTickClock(base::Seconds(1));
  EXPECT_TRUE(CanUseUnstableLidAngle());
  OpenLidToAngle(5.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
}

// Test that when the device lid is closed, its lid angle is reset properly.
TEST_F(TabletModeControllerTest, ResetLidAngleWhenLidClosed) {
  AttachTickClockForTest();
  OpenLid();
  OpenLidToAngle(90.0f);
  EXPECT_FLOAT_EQ(GetLidAngle(), 90.f);

  CloseLid();
  EXPECT_FLOAT_EQ(GetLidAngle(), 0.f);
}

// Tests that when the hinge is nearly vertically aligned, the current state
// persists as the computed angle is highly inaccurate in this orientation.
TEST_F(TabletModeControllerTest, HingeAligned) {
  // Laptop in normal orientation lid open 90 degrees.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, -kMeanGravityFloat),
                          gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Completely vertical.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f),
                          gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Close to vertical but with hinge appearing to be open 270 degrees.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, -0.1f),
                          gfx::Vector3dF(kMeanGravityFloat, 0.1f, 0.0f));
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Flat and open 270 degrees should start tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, -kMeanGravityFloat),
                          gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Normal 90 degree orientation but near vertical should stay in maximize
  // mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, -0.1f),
                          gfx::Vector3dF(kMeanGravityFloat, -0.1f, 0.0f));
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
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
    base.Scale(kMeanGravityFloat);
    gfx::Vector3dF lid(-kAccelerometerLaptopModeTestData[i * 6 + 4],
                       kAccelerometerLaptopModeTestData[i * 6 + 3],
                       kAccelerometerLaptopModeTestData[i * 6 + 5]);
    lid.Scale(kMeanGravityFloat);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
  }
}

TEST_F(TabletModeControllerTest, TabletModeTest) {
  // Trigger tablet mode by opening to 270 to begin the test in tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat),
                          gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());

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
    ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
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
    ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  }
}

// Test if this case does not crash. See http://crbug.com/462806.
TEST_F(TabletModeControllerTest, DisplayDisconnectionDuringOverview) {
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithBounds(gfx::Rect(800, 0, 100, 100)));
  ASSERT_NE(w1->GetRootWindow(), w2->GetRootWindow());
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(EnterOverview());

  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(w1->GetRootWindow(), w2->GetRootWindow());
}

// Test that the disabling of the internal display exits tablet mode, and that
// while disabled we do not re-enter tablet mode.
TEST_F(TabletModeControllerTest, NoTabletModeWithDisabledInternalDisplay) {
  UpdateDisplay("300x200, 300x200");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Set up a mode with the internal display deactivated before switching to
  // tablet mode (which will enable mirror mode with only one display).
  std::vector<display::ManagedDisplayInfo> secondary_only;
  secondary_only.push_back(display_manager()->GetDisplayInfo(
      display_manager()->GetDisplayAt(1).id()));

  // Opening the lid to 270 degrees should start tablet mode.
  OpenLidToAngle(270.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Close lid and deactivate the internal display to simulate Docked Mode.
  CloseLid();
  display_manager()->OnNativeDisplaysChanged(secondary_only);
  ASSERT_FALSE(display_manager()->IsActiveDisplayId(internal_display_id));
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(270.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Tablet mode signal should also be ignored.
  SetTabletMode(true);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
}

// Tests that tablet mode change events are fired while in unified desktop mode.
TEST_F(TabletModeControllerTest,
       TabletModeChangeEventsFiredInUnifiedDesktopMode) {
  UpdateDisplay("300x200, 300x200");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Turn on unified desktop mode.
  display_manager()->SetUnifiedDesktopEnabled(true);
  ASSERT_TRUE(display_manager()->IsInUnifiedMode());

  // Opening the lid to 270 degrees should start tablet mode.
  OpenLidToAngle(270.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(IsInPhysicalTabletState());
  EXPECT_TRUE(AreEventsBlocked());

  // Opening the lid to 30 degrees should stop tablet mode.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(IsInPhysicalTabletState());
  EXPECT_FALSE(AreEventsBlocked());
}

// Tests that is a tablet mode signal is received while docked, that maximize
// mode is enabled upon exiting docked mode.
TEST_F(TabletModeControllerTest, TabletModeAfterExitingDockedMode) {
  UpdateDisplay("300x200, 300x200");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

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
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Exiting docked state
  display_manager()->OnNativeDisplaysChanged(all_displays);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
}

// Verify that the device won't exit tabletmode / tablet mode for unstable
// angles when hinge is nearly vertical
TEST_F(TabletModeControllerTest, VerticalHingeUnstableAnglesTest) {
  // Trigger tablet mode by opening to 270 to begin the test in tablet mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravityFloat),
                          gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());

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
    ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  }
}

// Verify that the Alert Message will be triggered when switching between tablet
// mode and laptop mode.
TEST_F(TabletModeControllerTest, AlertInAndOutTabletMode) {
  TestAccessibilityControllerClient client;

  SetTabletMode(true);
  EXPECT_TRUE(l10n_util::GetStringUTF8(IDS_ASH_SWITCH_TO_TABLET_MODE) ==
              client.last_alert_message());

  SetTabletMode(false);
  EXPECT_TRUE(l10n_util::GetStringUTF8(IDS_ASH_SWITCH_TO_LAPTOP_MODE) ==
              client.last_alert_message());
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
  }
};

TEST_F(TabletModeControllerInitedFromPowerManagerClientTest,
       InitializedWhileTabletModeSwitchOn) {
  // PowerManagerClient callback is a posted task.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
}

TEST_F(TabletModeControllerTest, RestoreAfterExit) {
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
TEST_F(TabletModeControllerTest, CannotEnterTabletModeWithExternalMouse) {
  OpenLidToAngle(300.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(30.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Open lid to tent mode. Verify that tablet mode is not started.
  OpenLidToAngle(300.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Tests that when we plug in a external mouse the device will
// leave tablet mode.
TEST_F(TabletModeControllerTest, LeaveTabletModeWhenExternalMouseConnected) {
  // Start in tablet mode.
  OpenLidToAngle(300.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach external mouse and verify that tablet mode has ended, but events are
  // still blocked because the keyboard is still facing the bottom.
  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Verify that after unplugging the mouse, tablet mode will resume.
  DetachAllMice();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());
}

// Test that plug in or out a mouse in laptop mode will not change current
// laptop mode.
TEST_F(TabletModeControllerTest, ExternalMouseInLaptopMode) {
  // Start in laptop mode.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the external mouse. It still should maintain in laptop mode
  // because its lid angle is still in laptop mode.
  DetachAllMice();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
}

// Test that docked mode prevents entering tablet mode on detaching an external
// mouse while in tablet position.
TEST_F(TabletModeControllerTest, ExternalMouseInDockedMode) {
  UpdateDisplay("800x600, 800x600");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  // Attach an external mouse, and deactivate the internal display to simulate
  // Docked Mode.
  AttachExternalMouse();
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
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Detach the external mouse, and still expect clamshell mode.
  DetachAllMice();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Test that the ui mode and input event blocker should be both correctly
// updated when there is a change in external mouse and lid angle.
TEST_F(TabletModeControllerTest, ExternalMouseWithLidAngleTest) {
  // Start in laptop mode.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Now flip the device to tablet mode angle. The device should stay in
  // clamshell mode because of the external mouse. But the internal input events
  // should be blocked.
  OpenLidToAngle(300.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Remove the external mouse should enter tablet mode now. The internal input
  // events should still be blocked.
  DetachAllMice();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach the mouse again should enter clamshell mode again.
  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Flip the device back to clamshell angle. The device should stay in
  // clamshell mode and the internal input events should not be blocked.
  OpenLidToAngle(30.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the mouse. The device should stay in clamshell mode and the
  // internal events should not be blocked.
  DetachAllMice();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
}

// Test that the ui mode and input event blocker should be both correctly
// updated when there is a change in external mouse and tablet mode switch
// value.
TEST_F(TabletModeControllerTest, ExternalMouseWithTabletModeSwithTest) {
  // Start in laptop mode.
  SetTabletMode(false);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Attach external mouse doesn't change the mode.
  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Now set tablet mode switch value to true. The device should stay in
  // clamshell mode because of the external mouse. But the internal input events
  // should be blocked.
  SetTabletMode(true);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Remove the external mouse should enter tablet mode now. The internal input
  // events should still be blocked.
  DetachAllMice();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Attach the mouse again should enter clamshell mode again.
  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Set tablet mode switch value to false. The device should stay in
  // clamshell mode and the internal input events should not be blocked.
  SetTabletMode(false);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Now remove the mouse. The device should stay in clamshell mode and the
  // internal events should not be blocked.
  DetachAllMice();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
}

// Tests that when an external touchpad is connected, the device should exit
// tablet mode and enter clamshell mode.
TEST_F(TabletModeControllerTest, ExternalTouchPadTest) {
  // Nix touchpads attached to the machine that is running unit tests.
  DetachAllTouchpads();

  OpenLidToAngle(300.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(30.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Attach a external touchpad.
  AttachExternalTouchpad();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Open lid to tent mode. Verify that tablet mode is not started.
  OpenLidToAngle(300.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  // Verify that after unplugging the touchpad, tablet mode will resume.
  DetachAllTouchpads();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());
}

// Test that internal keyboard and mouse are not disabled in docked mode.
TEST_F(TabletModeControllerTest, InternalKeyboardMouseInDockedModeTest) {
  UpdateDisplay("800x600, 800x600");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  // Input devices events are unblocked.
  EXPECT_FALSE(AreEventsBlocked());
  EXPECT_TRUE(display::HasInternalDisplay());
  EXPECT_TRUE(
      Shell::Get()->display_manager()->IsActiveDisplayId(internal_display_id));

  // Enter tablet mode first.
  SetTabletMode(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
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
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Exiting docked state should enter tablet mode again.
  display_manager()->OnNativeDisplaysChanged(all_displays);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());
}

// Test that the mouse cursor is hidden when entering tablet mode, and shown
// when exiting tablet mode.
TEST_F(TabletModeControllerTest, ShowAndHideMouseCursorTest) {
  wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

TEST_F(TabletModeControllerTest, StartingKioskSwitchesToUiClamshellMode) {
  SetTabletMode(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  SimulateKioskMode(user_manager::UserType::kKioskApp);

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  // When the device is in the physical tablet state, the internal events should
  // still be blocked even when the device is in the UI clamshell mode.
  EXPECT_TRUE(AreEventsBlocked());
}

TEST_F(TabletModeControllerTest, KioskBlocksEnteringTabletMode) {
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
  SimulateKioskMode(user_manager::UserType::kKioskApp);

  SetTabletMode(true);

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());
  EXPECT_TRUE(IsInPhysicalTabletState());
}

TEST_F(TabletModeControllerTest, DeviceReactsOnLidChangeInKioskSession) {
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
  SimulateKioskMode(user_manager::UserType::kKioskApp);

  // Opening the lid to 270 degrees should start tablet mode if not blocked.
  OpenLidToAngle(270.0f);

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());
  EXPECT_TRUE(IsInPhysicalTabletState());
}

TEST_F(TabletModeControllerTest,
       KioskBlocksUiTabletModeEvenAfterMultipleLidChange) {
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
  SimulateKioskMode(user_manager::UserType::kKioskApp);

  OpenLidToAngle(270.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());

  OpenLidToAngle(30.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(270.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(AreEventsBlocked());
}

class TabletModeControllerForceTabletModeTest
    : public TabletModeControllerTest {
 public:
  TabletModeControllerForceTabletModeTest() = default;

  TabletModeControllerForceTabletModeTest(
      const TabletModeControllerForceTabletModeTest&) = delete;
  TabletModeControllerForceTabletModeTest& operator=(
      const TabletModeControllerForceTabletModeTest&) = delete;

  ~TabletModeControllerForceTabletModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshUiMode, switches::kAshUiModeTablet);
    TabletModeControllerTest::SetUp();
  }
};

// Verify when the force touch view mode flag is turned on, tablet mode is on
// initially, and opening the lid to less than 180 degress or setting tablet
// mode to off will not turn off tablet mode. The internal keyboard and trackpad
// should still work as it makes testing easier.
TEST_F(TabletModeControllerForceTabletModeTest, ForceTabletModeTest) {
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(30.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  SetTabletMode(false);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  // Tests that attaching a external mouse will not change the mode.
  AttachExternalMouse();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
}

TEST_F(TabletModeControllerForceTabletModeTest,
       ForceTabletModeOverridenInKiosk) {
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  SimulateKioskMode(user_manager::UserType::kKioskApp);

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

TEST_F(TabletModeControllerForceTabletModeTest, DockInForcedTabletMode) {
  UpdateDisplay("800x600, 800x600");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  // Deactivate internal display to simulate Docked Mode.
  std::vector<display::ManagedDisplayInfo> secondary_only;
  secondary_only.push_back(display_manager()->GetDisplayInfo(
      display_manager()->GetMirroringDestinationDisplayIdList()[0]));
  display_manager()->OnNativeDisplaysChanged(secondary_only);
  ASSERT_FALSE(display_manager()->IsActiveDisplayId(internal_display_id));

  // Still expect tablet mode.
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
}

class TabletModeControllerForceClamshellModeTest
    : public TabletModeControllerTest {
 public:
  TabletModeControllerForceClamshellModeTest() = default;

  TabletModeControllerForceClamshellModeTest(
      const TabletModeControllerForceClamshellModeTest&) = delete;
  TabletModeControllerForceClamshellModeTest& operator=(
      const TabletModeControllerForceClamshellModeTest&) = delete;

  ~TabletModeControllerForceClamshellModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshUiMode, switches::kAshUiModeClamshell);
    TabletModeControllerTest::SetUp();
  }
};

// Tests that when the force touch view mode flag is set to clamshell, clamshell
// mode is on initially, and cannot be changed by lid angle or manually entering
// tablet mode.
TEST_F(TabletModeControllerForceClamshellModeTest, ForceClamshellModeTest) {
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(200.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());

  SetTabletMode(true);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(AreEventsBlocked());
}

// Test that if the active window is not snapped before tablet mode, then split
// view is not activated.
TEST_F(TabletModeControllerTest, StartTabletActiveNoSnap) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

// Test that if the active window is snapped on the left before tablet mode,
// then split view is activated with the active window on the left.
TEST_F(TabletModeControllerTest, StartTabletActiveLeftSnap) {
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedLeft();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Test that if the active window is snapped on the right before tablet mode,
// then split view is activated with the active window on the right.
TEST_F(TabletModeControllerTest, StartTabletActiveRightSnap) {
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedRight();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->secondary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is snapped on the left and
// the previous window is snapped on the right, then split view is activated
// with the active window on the left and the previous window on the right.
TEST_F(TabletModeControllerTest, StartTabletActiveLeftSnapPreviousRightSnap) {
  std::unique_ptr<aura::Window> left_window = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> right_window =
      CreateDesktopWindowSnappedRight();
  wm::ActivateWindow(left_window.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_EQ(left_window.get(), split_view_controller()->primary_window());
  EXPECT_EQ(right_window.get(), split_view_controller()->secondary_window());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(left_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is snapped on the right
// and the previous window is snapped on the left, then split view is activated
// with the active window on the right and the previous window on the left.
TEST_F(TabletModeControllerTest, StartTabletActiveRightSnapPreviousLeftSnap) {
  std::unique_ptr<aura::Window> left_window = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> right_window =
      CreateDesktopWindowSnappedRight();
  ASSERT_EQ(right_window.get(), window_util::GetActiveWindow());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_EQ(left_window.get(), split_view_controller()->primary_window());
  EXPECT_EQ(right_window.get(), split_view_controller()->secondary_window());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is a transient child of a
// window snapped on the left, then split view is activated with the parent
// snapped on the left.
TEST_F(TabletModeControllerTest, StartTabletActiveTransientChildOfLeftSnap) {
  std::unique_ptr<aura::Window> parent = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> child =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  ::wm::AddTransientChild(parent.get(), child.get());
  wm::ActivateWindow(child.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(parent.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(child.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is the app list and the
// previous window is snapped on the left, then split view is activated with the
// previous window on the left.
TEST_F(TabletModeControllerTest, StartTabletActiveAppListPreviousLeftSnap) {
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedLeft();
  auto* app_list_controller = Shell::Get()->app_list_controller();
  app_list_controller->ShowAppList(AppListShowSource::kSearchKey);
  aura::Window* app_list_window = app_list_controller->GetWindow();
  ASSERT_TRUE(app_list_window);
  ASSERT_TRUE(wm::IsActiveWindow(app_list_window));
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is being dragged and the
// previous window is snapped on the left, then split view is activated with the
// previous window on the left.
TEST_F(TabletModeControllerTest, StartTabletActiveDraggedPreviousLeftSnap) {
  std::unique_ptr<aura::Window> dragged_window = CreateTestWindow();
  std::unique_ptr<aura::Window> snapped_window =
      CreateDesktopWindowSnappedLeft();
  wm::ActivateWindow(dragged_window.get());

  GetEventGenerator()->PressTouch(
      dragged_window->GetBoundsInScreen().CenterPoint());

  ASSERT_TRUE(Shell::Get()->toplevel_window_event_handler()->AttemptToStartDrag(
      dragged_window.get(), gfx::PointF(), HTCAPTION,
      ToplevelWindowEventHandler::EndClosure()));
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(snapped_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is hidden from overview
// and the previous window is snapped on the left, then split view is activated
// with the previous window on the left.
TEST_F(TabletModeControllerTest,
       StartTabletActiveHiddenFromOverviewPreviousLeftSnap) {
  std::unique_ptr<aura::Window> window_hidden_from_overview =
      CreateTestWindow();
  window_hidden_from_overview->SetProperty(kHideInOverviewKey, true);
  std::unique_ptr<aura::Window> snapped_window =
      CreateDesktopWindowSnappedLeft();
  wm::ActivateWindow(window_hidden_from_overview.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(snapped_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is being dragged and the
// previous window is a transient child of a window snapped on the left, then
// split view is activated with the parent on the left.
TEST_F(TabletModeControllerTest,
       StartTabletActiveDraggedPreviousTransientChildOfLeftSnap) {
  std::unique_ptr<aura::Window> dragged_window = CreateTestWindow();
  std::unique_ptr<aura::Window> parent = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> child =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  ::wm::AddTransientChild(parent.get(), child.get());
  wm::ActivateWindow(child.get());
  wm::ActivateWindow(dragged_window.get());

  GetEventGenerator()->PressTouch(
      dragged_window->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(Shell::Get()->toplevel_window_event_handler()->AttemptToStartDrag(
      dragged_window.get(), gfx::PointF(), HTCAPTION,
      ToplevelWindowEventHandler::EndClosure()));
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(parent.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(parent.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is snapped on the left but
// does not meet the requirements to be snapped in split view, and the previous
// window is snapped on the right, then split view is not activated.
TEST_F(TabletModeControllerTest,
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
  ASSERT_FALSE(split_view_controller()->CanSnapWindow(
      left_window.get(), chromeos::kDefaultSnapRatio));
  WindowSnapWMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_PRIMARY);
  left_window_state->OnWMEvent(&snap_to_left);
  std::unique_ptr<aura::Window> right_window =
      CreateDesktopWindowSnappedRight();
  wm::ActivateWindow(left_window.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

// Test that if before tablet mode, the active window is snapped on the right
// but does not meet the requirements to be snapped in split view, and the
// previous window is snapped on the left, then split view is not activated.
TEST_F(TabletModeControllerTest,
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
  ASSERT_FALSE(split_view_controller()->CanSnapWindow(
      right_window.get(), chromeos::kDefaultSnapRatio));
  WindowSnapWMEvent snap_to_right(WM_EVENT_CYCLE_SNAP_SECONDARY);
  right_window_state->OnWMEvent(&snap_to_right);
  wm::ActivateWindow(right_window.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

// Test that if before tablet mode, the active window is snapped on the left and
// the previous window is snapped on the right but does not meet the
// requirements to be snapped in split view, then split view is activated with
// the active window on the left.
TEST_F(TabletModeControllerTest,
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
  ASSERT_FALSE(split_view_controller()->CanSnapWindow(
      right_window.get(), chromeos::kDefaultSnapRatio));
  WindowSnapWMEvent snap_to_right(WM_EVENT_CYCLE_SNAP_SECONDARY);
  right_window_state->OnWMEvent(&snap_to_right);
  ASSERT_EQ(left_window.get(), window_util::GetActiveWindow());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(left_window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(left_window.get(), window_util::GetActiveWindow());
}

// Test that if before tablet mode, the active window is snapped on the right
// and the previous window is snapped on the left but does not meet the
// requirements to be snapped in split view, then split view is activated with
// the active window on the right.
TEST_F(TabletModeControllerTest,
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
  ASSERT_FALSE(split_view_controller()->CanSnapWindow(
      left_window.get(), chromeos::kDefaultSnapRatio));
  WindowSnapWMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_PRIMARY);
  left_window_state->OnWMEvent(&snap_to_left);
  std::unique_ptr<aura::Window> right_window =
      CreateDesktopWindowSnappedRight();
  ASSERT_EQ(right_window.get(), window_util::GetActiveWindow());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(right_window.get(), split_view_controller()->secondary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
}

// Test that when entering tablet mode with a left snapped window, the applist
// is not visible because overview is shown.
TEST_F(TabletModeControllerTest,
       AppListNotSeenAfterEnteringTabletModeWithLeftSnappedWindow) {
  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedLeft();
  tablet_mode_controller()->SetEnabledForTest(true);
  app_list_controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(app_list_controller->IsVisible());
}

// Test that if both the active window and the previous window are snapped on
// the left before tablet mode, then split view is activated with the active
// window on the left.
TEST_F(TabletModeControllerTest, StartTabletActiveLeftSnapPreviousLeftSnap) {
  std::unique_ptr<aura::Window> window1 = CreateDesktopWindowSnappedLeft();
  std::unique_ptr<aura::Window> window2 = CreateDesktopWindowSnappedLeft();
  wm::ActivateWindow(window1.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(window1.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());
}

// Like TabletModeControllerTest.StartTabletActiveLeftSnap, but with an extra
// display which has no relevant windows on it.
TEST_F(TabletModeControllerTest,
       StartTabletActiveLeftSnapPlusExtraneousDisplay) {
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> window = CreateDesktopWindowSnappedLeft();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(2u, Shell::GetAllRootWindows().size());
  // Make sure display mirroring triggers without any crashes.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, Shell::GetAllRootWindows().size());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

TEST_F(TabletModeControllerTest, StartTabletActiveLeftSnapOnSecondaryDisplay) {
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> window =
      CreateDesktopWindowSnappedLeft(gfx::Rect(800, 0, 400, 400));
  EXPECT_NE(Shell::GetPrimaryRootWindow(), window->GetRootWindow());
  tablet_mode_controller()->SetEnabledForTest(true);
  // Make sure display mirroring triggers without any crashes.
  base::RunLoop().RunUntilIdle();
}

TEST_F(
    TabletModeControllerTest,
    StartTabletActiveLeftSnapOnPrimaryDisplayPreviousRightSnapOnSecondaryDisplay) {
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> window1 =
      CreateDesktopWindowSnappedLeft(gfx::Rect(0, 0, 400, 400));
  EXPECT_EQ(Shell::GetPrimaryRootWindow(), window1->GetRootWindow());
  std::unique_ptr<aura::Window> window2 =
      CreateDesktopWindowSnappedRight(gfx::Rect(800, 0, 400, 400));
  EXPECT_NE(Shell::GetPrimaryRootWindow(), window2->GetRootWindow());
  wm::ActivateWindow(window1.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  // Make sure display mirroring triggers without any crashes.
  base::RunLoop().RunUntilIdle();
}

TEST_F(TabletModeControllerTest,
       StartTabletActiveLeftSnapOnPrimaryDisplayPreviousOnSecondaryDisplay) {
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> window1 =
      CreateDesktopWindowSnappedLeft(gfx::Rect(0, 0, 400, 400));
  EXPECT_EQ(Shell::GetPrimaryRootWindow(), window1->GetRootWindow());
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(800, 0, 400, 400));
  EXPECT_NE(Shell::GetPrimaryRootWindow(), window2->GetRootWindow());
  wm::ActivateWindow(window1.get());
  tablet_mode_controller()->SetEnabledForTest(true);
  // After display mirroring triggers, as the split view state will still be
  // |SplitViewController::State::kPrimarySnapped|, check for overview mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
}

// Test that tablet mode controller does not respond to the input device changes
// during its suspend.
TEST_F(TabletModeControllerTest, DoNotObserverInputDeviceChangeDuringSuspend) {
  // Start in tablet mode.
  OpenLidToAngle(300.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Attaching external mouse will end tablet mode.
  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Now suspend the device. Input device changes are no longer be observed.
  SuspendImminent();
  DetachAllMice();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Resume the device. Input device changes are being observed again.
  SuspendDone(base::TimeDelta::Max());
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
}

// Tests that we get no animation smoothness histograms when entering or
// exiting tablet mode with no windows.
TEST_F(TabletModeControllerTest, TabletModeTransitionHistogramsNotLogged) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::HistogramTester histogram_tester;

  SCOPED_TRACE("No window");
  histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
  histogram_tester.ExpectTotalCount(kExitHistogram, 0);
  tablet_mode_controller()->SetEnabledForTest(true);
  tablet_mode_controller()->SetEnabledForTest(false);
  WaitForSmoothnessMetrics();
  histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
  histogram_tester.ExpectTotalCount(kExitHistogram, 0);
}

// TODO(crbug.com/40877227): Flaky on Linux Chromium OS ASan LSan Tests.
TEST_F(TabletModeControllerTest,
       DISABLED_TabletModeTransitionHistogramsLogged) {
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
  WaitForWindowAnimation(window.get());
  WaitForWindowAnimation(window2.get());
  WaitForSmoothnessMetrics();
  histogram_tester.ExpectTotalCount(kEnterHistogram, 1);
  histogram_tester.ExpectTotalCount(kExitHistogram, 0);

  layer = window->layer();
  layer2 = window2->layer();
  tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(layer->GetAnimator()->is_animating());
  EXPECT_TRUE(layer2->GetAnimator()->is_animating());
  WaitForWindowAnimation(window2.get());
  WaitForSmoothnessMetrics();
  histogram_tester.ExpectTotalCount(kEnterHistogram, 1);
  histogram_tester.ExpectTotalCount(kExitHistogram, 1);
}

TEST_F(TabletModeControllerTest, TabletModeTransitionHistogramsSnappedWindows) {
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
  WaitForSmoothnessMetrics();
  histogram_tester.ExpectTotalCount(kEnterHistogram, 0);
  histogram_tester.ExpectTotalCount(kExitHistogram, 0);
}

// Tests that closing a window during the tablet mode enter animation does not
// cause a crash.
TEST_F(TabletModeControllerTest, CloseWindowDuringEnterAnimation) {
  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(250, 100));

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  tablet_mode_controller()->SetEnabledForTest(true);
  window.reset();
}

// Tests that closing a window during the tablet mode exit animation does not
// cause a crash.
TEST_F(TabletModeControllerTest, CloseWindowDuringExitAnimation) {
  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(250, 100));
  tablet_mode_controller()->SetEnabledForTest(true);

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  tablet_mode_controller()->SetEnabledForTest(false);
  window.reset();
}

TEST_F(TabletModeControllerTest, TabletModeUsageMetricsTest) {
  // We haven't seen any accelerometer data or tablet mode event yet, so
  // no metrics should be logged.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletActiveTimeHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletInactiveTimeHistogramName, 0);

  // Start in clamshell mode by accelerometer data.
  OpenLidToAngle(60.0f);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletActiveTimeHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletInactiveTimeHistogramName, 0);

  // Enter in tablet mode by accelerometer data.
  OpenLidToAngle(300.0f);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletActiveTimeHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletInactiveTimeHistogramName, 1);

  // Exit tablet mode by accelerometer data.
  OpenLidToAngle(60.0f);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletActiveTimeHistogramName, 1);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletInactiveTimeHistogramName, 1);

  // Enter tablet mode by tablet mode event.
  SetTabletMode(true);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletActiveTimeHistogramName, 1);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletInactiveTimeHistogramName, 2);

  // Exit tablet mode by tablet mode event.
  SetTabletMode(false);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletActiveTimeHistogramName, 2);
  histogram_tester.ExpectTotalCount(
      TabletModeController::kTabletInactiveTimeHistogramName, 2);
}

// Tests that a title bar of a window should be auto hidden if the window is
// maximized or snapped.
TEST_F(TabletModeControllerTest, ShouldAutoHideTitlebars) {
  tablet_mode_controller()->SetEnabledForTest(true);
  TestWidgetBuilder widget_builder;
  std::unique_ptr<views::Widget> widget =
      widget_builder.SetWidgetType(views::Widget::InitParams::TYPE_WINDOW)
          .SetBounds(gfx::Rect(500, 300))
          .SetContext(GetContext())
          .SetShow(true)
          .BuildOwnsNativeWidget();
  auto* window = widget->GetNativeWindow();
  auto* window_state = WindowState::Get(window);
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize |
                          aura::client::kResizeBehaviorCanMaximize);

  // If the window is not maximized nor snapped, `ShouldAutoHideTitlebars()`
  // should return true.
  EXPECT_FALSE(Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
      widget.get()));
  EXPECT_TRUE(window_state->CanSnap());

  // Snap the window and check that `ShouldAutoHideTitlebars()` is true.
  WindowSnapWMEvent snap_to_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_to_left);
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_TRUE(Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
      widget.get()));

  // Minimize the window and check that `ShouldAutoHideTitlebars()` is false.
  WMEvent minimize(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimize);
  EXPECT_FALSE(Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
      widget.get()));

  // Maximize the window and check that `ShouldAutoHideTitlebars()` is true.
  window_state->Maximize();
  EXPECT_TRUE(Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
      widget.get()));
}

// Tests that `ShouldAutoHideTitlebars()` should not crash if the window state
// does not exist (crbug.com/1267778).
TEST_F(TabletModeControllerTest, ShouldAutoHideTitlebarsNoWindowState) {
  TestWidgetBuilder widget_builder;
  // Create a window type control which is an example of a window that its
  // state does not exist to test that `ShouldAutoHideTitlebars()` works.
  std::unique_ptr<views::Widget> widget =
      widget_builder.SetWidgetType(views::Widget::InitParams::TYPE_CONTROL)
          .SetBounds(gfx::Rect(500, 300))
          .SetContext(GetContext())
          .SetShow(true)
          .BuildOwnsNativeWidget();
  auto* window = widget->GetNativeWindow();
  tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(WindowState::Get(window));
  EXPECT_FALSE(Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
      widget.get()));
}

class TabletModeControllerOnDeviceTest : public TabletModeControllerTest {
 public:
  TabletModeControllerOnDeviceTest() = default;
  TabletModeControllerOnDeviceTest(const TabletModeControllerOnDeviceTest&) =
      delete;
  TabletModeControllerOnDeviceTest& operator=(
      const TabletModeControllerOnDeviceTest&) = delete;

  ~TabletModeControllerOnDeviceTest() override = default;

  void SetUp() override {
    // We need to simulate the real on-device behavior for some tests.
    scoped_version_info_ =
        std::make_unique<base::test::ScopedChromeOSVersionInfo>(
            kLsbReleaseContent, base::Time::Now());
    // TODO(oshima): Disable native events instead of adding offset.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kHostWindowBounds, "800x600");

    TabletModeControllerTest::SetUp();
    // PowerManagerClient callback is a posted task.
    base::RunLoop().RunUntilIdle();

    // Make sure we've seen accelerometer data.
    TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f),
                            gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  }

 private:
  std::unique_ptr<base::test::ScopedChromeOSVersionInfo> scoped_version_info_;
};

// Tests that if there is no internal and external input device, the device
// should stay in tablet mode.
TEST_F(TabletModeControllerOnDeviceTest, DoNotEnterClamshellWithNoInputDevice) {
  AttachExternalTouchpad();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  DetachAllTouchpads();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  SetTabletMode(false);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  SetTabletMode(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  OpenLidToAngle(30.f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  OpenLidToAngle(300.f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
}

class TabletModeControllerScreenshotTest : public TabletModeControllerTest {
 public:
  TabletModeControllerScreenshotTest() = default;
  TabletModeControllerScreenshotTest(
      const TabletModeControllerScreenshotTest&) = delete;
  TabletModeControllerScreenshotTest& operator=(
      const TabletModeControllerScreenshotTest&) = delete;
  ~TabletModeControllerScreenshotTest() override = default;

  // While taking the screenshot, we temporarily hide the shelf and float
  // containers. This helper helps us check if it gets hidden and reshown
  // correctly.
  bool IsShelfAndFloatContainerOpaque() const {
    aura::Window* root = Shell::GetPrimaryRootWindow();
    for (int id :
         {kShellWindowId_FloatContainer, kShellWindowId_ShelfContainer}) {
      const aura::Window* container = root->GetChildById(id);
      if (container->layer()->opacity() != 1.0f) {
        return false;
      }
    }
    return true;
  }

  // TabletModeControllerTest:
  void SetUp() override {
    TabletModeControllerTest::SetUp();
    TabletModeController::SetUseScreenshotForTest(true);

    // Remove TabletModeController as an observer of input device events to
    // prevent interfering with the test.
    ui::DeviceDataManager::GetInstance()->RemoveObserver(
        tablet_mode_controller());

    // Screenshot relies on the animation status of windows. These animations
    // can be triggered in multiple ways such as tablet enter/exit or overview
    // enter/exit. With a NONZERO_DURATION, occasionally they may trigger too
    // quickly in tests so use NORMAL_DURATION.
    scoped_animation_duration_scale_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

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
};

// Tests that when there are no animations, no screenshot is taken.
TEST_F(TabletModeControllerScreenshotTest, NoAnimationNoScreenshot) {
  // Tests that no windows means no screenshot.
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

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
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());
}

// Regression test for screenshot staying visible when entering tablet mode when
// already in overview mode. See https://crbug.com/1002735.
TEST_F(TabletModeControllerScreenshotTest, FromOverviewNoScreenshot) {
  // Create two maximized windows.
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto window2 = CreateTestWindow(gfx::Rect(200, 200));
  WindowState::Get(window.get())->Maximize();
  WindowState::Get(window2.get())->Maximize();
  window->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();

  // Enter overview.
  EnterOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);

  // Enter tablet mode while in overview. There should be no screenshot at any
  // time.
  TabletMode::Waiter waiter(/*enable=*/true);
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

  waiter.Wait();
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

  // Tests that after ending the window animation, the screenshot is still not
  // shown.
  window->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());
}

// Regression test for screenshot staying visible when entering tablet mode when
// a window creation animation is still underway. See https://crbug.com/1035356.
TEST_F(TabletModeControllerScreenshotTest, EnterTabletModeWhileAnimating) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  ASSERT_TRUE(window->layer()->GetAnimator()->is_animating());

  // Enter tablet mode.
  TabletMode::Waiter waiter(/*enable=*/true);
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

  waiter.Wait();
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());
}

namespace {

class LayerStartAnimationWaiter : public ui::LayerAnimationObserver {
 public:
  explicit LayerStartAnimationWaiter(ui::LayerAnimator* animator)
      : animator_(animator) {
    animator_->AddObserver(this);
    run_loop_.Run();
  }
  LayerStartAnimationWaiter(const LayerStartAnimationWaiter&) = delete;
  LayerStartAnimationWaiter& operator=(const LayerStartAnimationWaiter&) =
      delete;
  ~LayerStartAnimationWaiter() override { animator_->RemoveObserver(this); }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {
    run_loop_.Quit();
  }
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 private:
  raw_ptr<ui::LayerAnimator> animator_;
  base::RunLoop run_loop_;
};

}  // namespace

// Tests that the screenshot is visible when a window animation happens when
// entering tablet mode.
TEST_F(TabletModeControllerScreenshotTest, ScreenshotVisibility) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto window2 = CreateTestWindow(gfx::Rect(300, 200));

  window->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  ASSERT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_FALSE(IsShelfAndFloatContainerOpaque());

  // The layer we observe is actually the windows layer before starting the
  // animation. The animation performed is a cross-fade animation which
  // copies the window layer to another layer host. So cache them here for
  // later use. Wait until the animation has started, at this point the
  // screenshot should be visible.
  ui::LayerAnimator* old_animator = window2->layer()->GetAnimator();
  ASSERT_FALSE(old_animator->is_animating());
  { LayerStartAnimationWaiter waiter(old_animator); }
  EXPECT_TRUE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

  // Tests that the screenshot is destroyed after the window is done animating.
  old_animator->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());
}

// Tests that if we exit tablet mode before the screenshot is taken, there is no
// crash. (See https://crbug.com/1012879).
TEST_F(TabletModeControllerScreenshotTest, NoCrashWhenExitingWithoutWaiting) {
  // One non-maximized window is needed for screenshot to be taken.
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  window->layer()->GetAnimator()->StopAnimating();

  SetTabletMode(true);
  EXPECT_FALSE(IsShelfAndFloatContainerOpaque());

  SetTabletMode(false);
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

  // Tests that reentering tablet mode without waiting causes no crash either.
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_FALSE(IsShelfAndFloatContainerOpaque());
}

// Tests that the screenshot gets deleted after transition with a transient
// child as the top window that is not resizeable but positionable. Note that
// creating such windows is not desirable, but is possible so we need this
// regression test. See https://crbug.com/1096128.
TEST_F(TabletModeControllerScreenshotTest, TransientChildTypeWindow) {
  // Create a window with a transient child that is of WINDOW_TYPE_POPUP.
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto child = CreateTestWindow(gfx::Rect(200, 200));
  child->SetProperty(aura::client::kResizeBehaviorKey,
                     aura::client::kResizeBehaviorCanResize);
  ::wm::AddTransientChild(window.get(), child.get());

  window->layer()->GetAnimator()->StopAnimating();
  child->layer()->GetAnimator()->StopAnimating();

  SetTabletMode(true);
  ShellTestApi().WaitForWindowFinishAnimating(child.get());
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());
}

// Floated window in tablet mode only covers a portion of the work area, so we
// don't take a screenshot.
TEST_F(TabletModeControllerScreenshotTest, NoScreenshotFloatedWindow) {
  auto window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  window->layer()->GetAnimator()->StopAnimating();

  // Enter tablet mode. Test that there is no screenshot.
  TabletMode::Waiter waiter(/*enable=*/true);
  SetTabletMode(true);
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

  waiter.Wait();
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());

  // Tests that after ending `window`'s animation, the screenshot is still not
  // shown.
  window->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(IsScreenshotShown());
  EXPECT_TRUE(IsShelfAndFloatContainerOpaque());
}

}  // namespace ash
