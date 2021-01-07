// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_orientation_controller.h"

#include <memory>
#include <vector>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/window_factory.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/numerics/math_constants.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

using base::kMeanGravityFloat;

const float kDegreesToRadians = 3.1415926f / 180.0f;

display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                              const gfx::Rect& bounds) {
  display::ManagedDisplayInfo info(id, "dummy", false);
  info.SetBounds(bounds);
  return info;
}

void EnableTabletMode(bool enable) {
  Shell::Get()->tablet_mode_controller()->ForceUiTabletModeState(enable);
}

bool RotationLocked() {
  return Shell::Get()->screen_orientation_controller()->rotation_locked();
}

bool UserRotationLocked() {
  return Shell::Get()->screen_orientation_controller()->user_rotation_locked();
}

void SetDisplayRotationById(int64_t display_id,
                            display::Display::Rotation rotation) {
  Shell::Get()->display_manager()->SetDisplayRotation(
      display_id, rotation, display::Display::RotationSource::USER);
}

void SetInternalDisplayRotation(display::Display::Rotation rotation) {
  SetDisplayRotationById(display::Display::InternalDisplayId(), rotation);
}

void TriggerLidUpdate(const gfx::Vector3dF& lid) {
  AccelerometerUpdate update;
  update.Set(ACCELEROMETER_SOURCE_SCREEN, lid.x(), lid.y(), lid.z());
  Shell::Get()->screen_orientation_controller()->OnAccelerometerUpdated(update);
}

// Shows |child| and adds |child| to |parent|.
void AddWindowAndShow(aura::Window* child, aura::Window* parent) {
  child->Show();
  parent->AddChild(child);
}

// Adds |child| to |parent| and activates |parent|.
void AddWindowAndActivateParent(aura::Window* child, aura::Window* parent) {
  AddWindowAndShow(child, parent);
  Shell::Get()->activation_client()->ActivateWindow(parent);
}

void Lock(aura::Window* window, OrientationLockType orientation_lock) {
  Shell::Get()->screen_orientation_controller()->LockOrientationForWindow(
      window, orientation_lock);
}

void Unlock(aura::Window* window) {
  Shell::Get()->screen_orientation_controller()->UnlockOrientationForWindow(
      window);
}

// Creates a window of type WINDOW_TYPE_CONTROL.
std::unique_ptr<aura::Window> CreateControlWindow() {
  std::unique_ptr<aura::Window> window = window_factory::NewWindow(
      nullptr, aura::client::WindowType::WINDOW_TYPE_CONTROL);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->set_owned_by_parent(false);
  return window;
}

}  // namespace

class ScreenOrientationControllerTest : public AshTestBase {
 public:
  ScreenOrientationControllerTest() = default;
  ~ScreenOrientationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kUseFirstDisplayAsInternal);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletMode);
    AshTestBase::SetUp();
  }

 protected:
  aura::Window* CreateAppWindowInShellWithId(int id) {
    aura::Window* window = CreateTestWindowInShellWithId(id);
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(AppType::CHROME_APP));
    return window;
  }

  void SetSystemRotationLocked(bool rotation_locked) {
    ScreenOrientationControllerTestApi(
        Shell::Get()->screen_orientation_controller())
        .SetRotationLocked(rotation_locked);
  }

  void SetUserRotationLocked(bool rotation_locked) {
    if (Shell::Get()->screen_orientation_controller()->user_rotation_locked() !=
        rotation_locked) {
      Shell::Get()->screen_orientation_controller()->ToggleUserRotationLock();
    }
  }

  OrientationLockType UserLockedOrientation() const {
    ScreenOrientationControllerTestApi test_api(
        Shell::Get()->screen_orientation_controller());
    return test_api.UserLockedOrientation();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationControllerTest);
};

// Tests that a Window can lock rotation.
TEST_F(ScreenOrientationControllerTest, LockOrientation) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window(CreateAppWindowInShellWithId(0));
  ASSERT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  ASSERT_FALSE(RotationLocked());

  AddWindowAndActivateParent(child_window.get(), focus_window.get());
  Lock(child_window.get(), OrientationLockType::kLandscape);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  auto modal = CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  modal->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());
}

// Tests that a Window can unlock rotation.
TEST_F(ScreenOrientationControllerTest, Unlock) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window(CreateAppWindowInShellWithId(0));
  ASSERT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  ASSERT_FALSE(RotationLocked());

  AddWindowAndActivateParent(child_window.get(), focus_window.get());
  Lock(child_window.get(), OrientationLockType::kLandscape);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  Unlock(child_window.get());
  EXPECT_FALSE(RotationLocked());
}

// Tests that a Window is able to change the orientation of the display after
// having locked rotation.
TEST_F(ScreenOrientationControllerTest, OrientationChanges) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window(CreateAppWindowInShellWithId(0));
  ASSERT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  ASSERT_FALSE(RotationLocked());

  AddWindowAndActivateParent(child_window.get(), focus_window.get());
  Lock(child_window.get(), OrientationLockType::kPortrait);
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  Lock(child_window.get(), OrientationLockType::kLandscape);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that orientation can only be set by the first Window that has set a
// rotation lock.
TEST_F(ScreenOrientationControllerTest, SecondContentCannotChangeOrientation) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window1 = CreateControlWindow();
  std::unique_ptr<aura::Window> child_window2 = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window1(CreateAppWindowInShellWithId(0));
  std::unique_ptr<aura::Window> focus_window2(CreateAppWindowInShellWithId(1));

  AddWindowAndActivateParent(child_window1.get(), focus_window1.get());
  AddWindowAndShow(child_window2.get(), focus_window2.get());
  Lock(child_window1.get(), OrientationLockType::kLandscape);
  Lock(child_window2.get(), OrientationLockType::kPortrait);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that only the Window that set a rotation lock can perform an unlock.
TEST_F(ScreenOrientationControllerTest, SecondContentCannotUnlock) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window1 = CreateControlWindow();
  std::unique_ptr<aura::Window> child_window2 = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window1(CreateAppWindowInShellWithId(0));
  std::unique_ptr<aura::Window> focus_window2(CreateAppWindowInShellWithId(1));

  AddWindowAndActivateParent(child_window1.get(), focus_window1.get());
  AddWindowAndShow(child_window2.get(), focus_window2.get());
  Lock(child_window1.get(), OrientationLockType::kLandscape);
  Unlock(child_window2.get());
  EXPECT_TRUE(RotationLocked());
}

// Tests that a rotation lock is applied only while the Window are a part of the
// active window.
TEST_F(ScreenOrientationControllerTest, ActiveWindowChangesUpdateLock) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window1(CreateAppWindowInShellWithId(0));
  std::unique_ptr<aura::Window> focus_window2(CreateAppWindowInShellWithId(1));

  AddWindowAndActivateParent(child_window.get(), focus_window1.get());
  Lock(child_window.get(), OrientationLockType::kLandscape);
  ASSERT_TRUE(RotationLocked());

  ::wm::ActivationClient* activation_client = Shell::Get()->activation_client();
  activation_client->ActivateWindow(focus_window2.get());
  EXPECT_FALSE(RotationLocked());

  activation_client->ActivateWindow(focus_window1.get());
  EXPECT_TRUE(RotationLocked());
}

// Tests that switching between windows with different orientation locks change
// the orientation.
TEST_F(ScreenOrientationControllerTest, ActiveWindowChangesUpdateOrientation) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window1 = CreateControlWindow();
  std::unique_ptr<aura::Window> child_window2 = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window1(CreateAppWindowInShellWithId(0));
  std::unique_ptr<aura::Window> focus_window2(CreateAppWindowInShellWithId(1));
  AddWindowAndActivateParent(child_window1.get(), focus_window1.get());
  AddWindowAndShow(child_window2.get(), focus_window2.get());

  Lock(child_window1.get(), OrientationLockType::kLandscape);
  Lock(child_window2.get(), OrientationLockType::kPortrait);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  ::wm::ActivationClient* activation_client = Shell::Get()->activation_client();
  activation_client->ActivateWindow(focus_window2.get());
  EXPECT_TRUE(RotationLocked());
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());

  activation_client->ActivateWindow(focus_window1.get());
  EXPECT_TRUE(RotationLocked());
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that a rotation lock is removed when the setting window is hidden, and
// that it is reapplied when the window becomes visible.
TEST_F(ScreenOrientationControllerTest, VisibilityChangesLock) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window(CreateAppWindowInShellWithId(0));
  AddWindowAndActivateParent(child_window.get(), focus_window.get());
  Lock(child_window.get(), OrientationLockType::kLandscape);
  EXPECT_TRUE(RotationLocked());

  child_window->Hide();
  EXPECT_FALSE(RotationLocked());

  child_window->Show();
  EXPECT_TRUE(RotationLocked());
}

// Tests that when a window is destroyed that its rotation lock is removed, and
// window activations no longer change the lock
TEST_F(ScreenOrientationControllerTest, WindowDestructionRemovesLock) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window1(CreateAppWindowInShellWithId(0));
  std::unique_ptr<aura::Window> focus_window2(CreateAppWindowInShellWithId(1));

  AddWindowAndActivateParent(child_window.get(), focus_window1.get());
  Lock(child_window.get(), OrientationLockType::kLandscape);
  ASSERT_TRUE(RotationLocked());

  focus_window1->RemoveChild(child_window.get());
  child_window.reset();
  EXPECT_FALSE(RotationLocked());

  ::wm::ActivationClient* activation_client = Shell::Get()->activation_client();
  activation_client->ActivateWindow(focus_window2.get());
  EXPECT_FALSE(RotationLocked());

  activation_client->ActivateWindow(focus_window1.get());
  EXPECT_FALSE(RotationLocked());
}

TEST_F(ScreenOrientationControllerTest, SplitViewPreventsLock) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window1 = CreateControlWindow();
  std::unique_ptr<aura::Window> child_window2 = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window1(CreateAppWindowInShellWithId(0));
  std::unique_ptr<aura::Window> focus_window2(CreateAppWindowInShellWithId(1));

  AddWindowAndActivateParent(child_window1.get(), focus_window1.get());
  AddWindowAndShow(child_window2.get(), focus_window2.get());
  Lock(child_window1.get(), OrientationLockType::kLandscape);
  Lock(child_window2.get(), OrientationLockType::kPortrait);
  ASSERT_TRUE(RotationLocked());

  split_view_controller()->SnapWindow(focus_window1.get(),
                                      SplitViewController::LEFT);
  split_view_controller()->SnapWindow(focus_window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_FALSE(RotationLocked());

  split_view_controller()->EndSplitView();
  EXPECT_TRUE(RotationLocked());
}

// Tests that accelerometer readings in each of the screen angles will trigger a
// rotation of the internal display.
TEST_F(ScreenOrientationControllerTest, DisplayRotation) {
  EnableTabletMode(true);

  // Now test rotating in all directions.
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that low angles are ignored by the accelerometer (i.e. when the device
// is almost laying flat).
TEST_F(ScreenOrientationControllerTest, RotationIgnoresLowAngles) {
  EnableTabletMode(true);

  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravityFloat, kMeanGravityFloat));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(-2.0f, 0.0f, kMeanGravityFloat));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 2.0f, kMeanGravityFloat));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(2.0f, 0.0f, kMeanGravityFloat));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -2.0f, kMeanGravityFloat));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that the display will stick to the current orientation beyond the
// halfway point, preventing frequent updates back and forth.
TEST_F(ScreenOrientationControllerTest, RotationSticky) {
  EnableTabletMode(true);

  gfx::Vector3dF gravity(0.0f, kMeanGravityFloat, 0.0f);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  // Turn past half-way point to next direction and rotation should remain
  // the same.
  float degrees = 50.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * -kMeanGravityFloat);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * -kMeanGravityFloat);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  // Turn more and the screen should rotate.
  degrees = 70.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * -kMeanGravityFloat);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * -kMeanGravityFloat);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  // Turn back just beyond the half-way point and the new rotation should
  // still be in effect.
  degrees = 40.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * -kMeanGravityFloat);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * -kMeanGravityFloat);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
}

// Tests that the display will stick to its current orientation when the
// rotation lock has been set.
TEST_F(ScreenOrientationControllerTest, RotationLockPreventsRotation) {
  EnableTabletMode(true);
  SetUserRotationLocked(true);

  // Turn past the threshold for rotation.
  float degrees = 90.0;
  gfx::Vector3dF gravity(-sin(degrees * kDegreesToRadians) * -kMeanGravityFloat,
                         -cos(degrees * kDegreesToRadians) * -kMeanGravityFloat,
                         0.0f);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  SetUserRotationLocked(false);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
}

// The ScreenLayoutObserver class that is responsible for adding/updating
// MessageCenter notifications is only added to the SystemTray on ChromeOS.
// Tests that the screen rotation notifications are suppressed when
// triggered by the accelerometer.
TEST_F(ScreenOrientationControllerTest, BlockRotationNotifications) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kReduceDisplayNotifications);

  EnableTabletMode(true);
  Shell::Get()->screen_layout_observer()->set_show_notifications_for_testing(
      true);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are still displayed when
  // adjusting the screen rotation directly when in tablet mode
  ASSERT_NE(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  SetInternalDisplayRotation(display::Display::ROTATE_270);
  SetSystemRotationLocked(false);
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasPopupNotifications());

  // Clear all notifications
  message_center->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are blocked when adjusting the screen rotation
  // via the accelerometer while in tablet mode
  // Rotate the screen 90 degrees
  ASSERT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  ASSERT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are still displayed when
  // adjusting the screen rotation directly when not in tablet mode
  EnableTabletMode(false);
  // Reset the screen rotation.
  SetInternalDisplayRotation(display::Display::ROTATE_0);
  // Clear all notifications
  message_center->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
  ASSERT_NE(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  ASSERT_EQ(0u, message_center->NotificationCount());
  ASSERT_FALSE(message_center->HasPopupNotifications());
  SetInternalDisplayRotation(display::Display::ROTATE_180);
  EXPECT_EQ(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasPopupNotifications());
}

// Tests that if a user has set a display rotation that it is restored upon
// exiting tablet mode.
TEST_F(ScreenOrientationControllerTest, ResetUserRotationUponExit) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  SetInternalDisplayRotation(display::Display::ROTATE_90);
  EnableTabletMode(true);

  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());

  EnableTabletMode(false);
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
}

// Tests that if a user changes the display rotation, while rotation is locked,
// that the updates are recorded. Upon exiting tablet mode the latest user
// rotation should be applied.
TEST_F(ScreenOrientationControllerTest, UpdateUserRotationWhileRotationLocked) {
  EnableTabletMode(true);
  SetInternalDisplayRotation(display::Display::ROTATE_270);
  // User sets rotation to the same rotation that the display was at when
  // tablet mode was activated.
  SetInternalDisplayRotation(display::Display::ROTATE_0);
  EnableTabletMode(false);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that when the orientation lock is set to Landscape, that rotation can
// be done between the two angles of the orientation.
TEST_F(ScreenOrientationControllerTest, LandscapeOrientationAllowsRotation) {
  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window(CreateAppWindowInShellWithId(0));
  EnableTabletMode(true);

  AddWindowAndActivateParent(child_window.get(), focus_window.get());
  Lock(child_window.get(), OrientationLockType::kLandscape);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  // Inverse of orientation is allowed
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());

  // Display rotations between are not allowed
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
}

// Tests that when the orientation lock is set to Portrait, that rotation can be
// done between the two angles of the orientation.
TEST_F(ScreenOrientationControllerTest, PortraitOrientationAllowsRotation) {
  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window(CreateAppWindowInShellWithId(0));
  EnableTabletMode(true);

  AddWindowAndActivateParent(child_window.get(), focus_window.get());
  Lock(child_window.get(), OrientationLockType::kPortrait);
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  // Inverse of orientation is allowed
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  // Display rotations between are not allowed
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
}

// Tests that for an orientation lock which does not allow rotation, that the
// display rotation remains constant.
TEST_F(ScreenOrientationControllerTest, OrientationLockDisallowsRotation) {
  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window(CreateAppWindowInShellWithId(0));
  EnableTabletMode(true);

  AddWindowAndActivateParent(child_window.get(), focus_window.get());
  Lock(child_window.get(), OrientationLockType::kPortraitPrimary);
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  // Rotation does not change.
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
}

// Tests that after a Window has applied an orientation lock which supports
// rotation, that a user rotation lock does not allow rotation.
TEST_F(ScreenOrientationControllerTest, UserRotationLockDisallowsRotation) {
  std::unique_ptr<aura::Window> child_window = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window(CreateAppWindowInShellWithId(0));
  EnableTabletMode(true);

  AddWindowAndActivateParent(child_window.get(), focus_window.get());
  Lock(child_window.get(), OrientationLockType::kLandscape);
  Unlock(child_window.get());

  SetUserRotationLocked(true);
  EXPECT_TRUE(RotationLocked());
  EXPECT_TRUE(UserRotationLocked());

  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Verifies rotating an inactive Display is successful.
TEST_F(ScreenOrientationControllerTest, RotateInactiveDisplay) {
  const int64_t kInternalDisplayId = 9;
  const int64_t kExternalDisplayId = 10;
  const display::Display::Rotation kNewRotation = display::Display::ROTATE_180;

  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(kInternalDisplayId, gfx::Rect(0, 0, 500, 500));
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(kExternalDisplayId, gfx::Rect(1, 1, 500, 500));

  std::vector<display::ManagedDisplayInfo> display_info_list_two_active;
  display_info_list_two_active.push_back(internal_display_info);
  display_info_list_two_active.push_back(external_display_info);

  std::vector<display::ManagedDisplayInfo> display_info_list_one_active;
  display_info_list_one_active.push_back(external_display_info);

  // The display::ManagedDisplayInfo list with two active displays needs to be
  // added first so that the DisplayManager can track the
  // |internal_display_info| as inactive instead of non-existent.
  display_manager()->UpdateDisplaysWith(display_info_list_two_active);
  display_manager()->UpdateDisplaysWith(display_info_list_one_active);

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         kInternalDisplayId);

  ASSERT_NE(kNewRotation, display_manager()
                              ->GetDisplayInfo(kInternalDisplayId)
                              .GetActiveRotation());
  ScreenOrientationControllerTestApi(
      Shell::Get()->screen_orientation_controller())
      .SetDisplayRotation(kNewRotation,
                          display::Display::RotationSource::ACTIVE);

  EXPECT_EQ(kNewRotation, display_manager()
                              ->GetDisplayInfo(kInternalDisplayId)
                              .GetActiveRotation());
}

TEST_F(ScreenOrientationControllerTest, UserRotationLockedOrientation) {
  ScreenOrientationController* orientation_controller =
      Shell::Get()->screen_orientation_controller();
  orientation_controller->ToggleUserRotationLock();
  EXPECT_TRUE(orientation_controller->user_rotation_locked());
  EXPECT_EQ(OrientationLockType::kLandscapePrimary, UserLockedOrientation());

  orientation_controller->ToggleUserRotationLock();
  SetInternalDisplayRotation(display::Display::ROTATE_270);
  orientation_controller->ToggleUserRotationLock();
  EXPECT_EQ(OrientationLockType::kPortraitPrimary, UserLockedOrientation());

  orientation_controller->ToggleUserRotationLock();
  SetInternalDisplayRotation(display::Display::ROTATE_180);
  orientation_controller->ToggleUserRotationLock();
  EXPECT_EQ(OrientationLockType::kLandscapeSecondary, UserLockedOrientation());

  orientation_controller->ToggleUserRotationLock();
  SetInternalDisplayRotation(display::Display::ROTATE_90);
  orientation_controller->ToggleUserRotationLock();
  EXPECT_EQ(OrientationLockType::kPortraitSecondary, UserLockedOrientation());
  orientation_controller->ToggleUserRotationLock();

  SetInternalDisplayRotation(display::Display::ROTATE_270);

  UpdateDisplay("800x1280");
  orientation_controller->ToggleUserRotationLock();
  EXPECT_TRUE(orientation_controller->user_rotation_locked());
  EXPECT_EQ(OrientationLockType::kPortraitPrimary, UserLockedOrientation());

  orientation_controller->ToggleUserRotationLock();
  SetInternalDisplayRotation(display::Display::ROTATE_90);
  orientation_controller->ToggleUserRotationLock();
  EXPECT_EQ(OrientationLockType::kLandscapePrimary, UserLockedOrientation());

  orientation_controller->ToggleUserRotationLock();
  SetInternalDisplayRotation(display::Display::ROTATE_180);
  orientation_controller->ToggleUserRotationLock();
  EXPECT_EQ(OrientationLockType::kPortraitSecondary, UserLockedOrientation());

  orientation_controller->ToggleUserRotationLock();
  SetInternalDisplayRotation(display::Display::ROTATE_270);
  orientation_controller->ToggleUserRotationLock();
  EXPECT_EQ(OrientationLockType::kLandscapeSecondary, UserLockedOrientation());
  orientation_controller->ToggleUserRotationLock();
}

TEST_F(ScreenOrientationControllerTest, UserRotationLock) {
  EnableTabletMode(true);

  std::unique_ptr<aura::Window> child_window1 = CreateControlWindow();
  std::unique_ptr<aura::Window> child_window2 = CreateControlWindow();
  std::unique_ptr<aura::Window> focus_window1(CreateAppWindowInShellWithId(0));
  std::unique_ptr<aura::Window> focus_window2(CreateAppWindowInShellWithId(1));

  AddWindowAndActivateParent(child_window2.get(), focus_window2.get());
  AddWindowAndActivateParent(child_window1.get(), focus_window1.get());

  ASSERT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  ASSERT_FALSE(RotationLocked());
  ASSERT_FALSE(UserRotationLocked());

  ScreenOrientationController* orientation_controller =
      Shell::Get()->screen_orientation_controller();
  ASSERT_FALSE(orientation_controller->user_rotation_locked());
  orientation_controller->ToggleUserRotationLock();
  ASSERT_TRUE(orientation_controller->user_rotation_locked());

  Lock(child_window1.get(), OrientationLockType::kPortrait);

  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());

  ::wm::ActivationClient* activation_client = Shell::Get()->activation_client();
  // Activating any will switch to the natural orientation.
  activation_client->ActivateWindow(focus_window2.get());
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  // Activating the portrait window will rotate to the portrait.
  activation_client->ActivateWindow(focus_window1.get());
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());

  // User locked to the 90 dig.
  orientation_controller->ToggleUserRotationLock();
  orientation_controller->ToggleUserRotationLock();

  // Switching to Any orientation will stay to the user locked orientation.
  activation_client->ActivateWindow(focus_window2.get());
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());

  // Application forced to be landscape.
  Lock(child_window2.get(), OrientationLockType::kLandscape);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  Lock(child_window1.get(), OrientationLockType::kAny);
  activation_client->ActivateWindow(focus_window1.get());
  // Switching back to any will rotate to user rotation.
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
}

TEST_F(ScreenOrientationControllerTest, ClamshellPhysicalTabletState) {
  // Auto-rotation is disabled while the device is not physically used as a
  // tablet.
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  EXPECT_FALSE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  // Once the device goes into tablet mode, it becomes possible to auto-rotate.
  tablet_mode_controller_test_api.OpenLidToAngle(270);
  EXPECT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_mode_controller_test_api.IsTabletModeStarted());
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  // Hooking an external pointing device will exits tablet UI mode, but the
  // device is still in a physical tablet state, which means auto-rotation is
  // still possible.
  tablet_mode_controller_test_api.AttachExternalMouse();
  EXPECT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
  EXPECT_FALSE(tablet_mode_controller_test_api.IsTabletModeStarted());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
}

TEST_F(ScreenOrientationControllerTest,
       ApplyAppsRequestedLocksOnlyInUITabletMode) {
  std::unique_ptr<aura::Window> window(CreateAppWindowInShellWithId(0));
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  // Unit tests are supposed to be in reference to a hypothetical computer, but
  // they can detect a mouse connected to the actual computer on which they are
  // run. That is relevant here because external pointing devices prevent tablet
  // mode. Detach all mice, so that this unit test will produce the same results
  // whether the host machine has a mouse or not.
  tablet_mode_controller_test_api.DetachAllMice();

  tablet_mode_controller_test_api.OpenLidToAngle(270);
  EXPECT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_mode_controller_test_api.IsTabletModeStarted());
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  ScreenOrientationController* orientation_controller =
      Shell::Get()->screen_orientation_controller();
  orientation_controller->ToggleUserRotationLock();
  EXPECT_TRUE(orientation_controller->user_rotation_locked());
  EXPECT_EQ(OrientationLockType::kLandscapePrimary, UserLockedOrientation());

  // Apps' requested orientation locks are only applied in UI tablet mode.
  Lock(window.get(), OrientationLockType::kPortrait);
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());

  // Exiting to clamshell mode while the device is still physically a tablet
  // should restore the user rotation lock, and ignore the app-requested one.
  tablet_mode_controller_test_api.AttachExternalMouse();
  EXPECT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
  EXPECT_FALSE(tablet_mode_controller_test_api.IsTabletModeStarted());
  EXPECT_TRUE(orientation_controller->user_rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kLandscapePrimary, UserLockedOrientation());

  // Further requested orientation locks by apps will remain ignored.
  Lock(window.get(), OrientationLockType::kPortraitSecondary);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kLandscapePrimary, UserLockedOrientation());

  // When UI tablet mode triggers again, the most recent app requested
  // orientation lock for the active window will be applied.
  tablet_mode_controller_test_api.DetachAllMice();
  EXPECT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_mode_controller_test_api.IsTabletModeStarted());
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  // Orientation should be restored once the device exits the physical tablet
  // state.
  tablet_mode_controller_test_api.OpenLidToAngle(90);
  EXPECT_FALSE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
  EXPECT_FALSE(tablet_mode_controller_test_api.IsTabletModeStarted());
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

TEST_F(ScreenOrientationControllerTest, GetCurrentAppRequestedOrientationLock) {
  UpdateDisplay("0+0-400x300,+400+0-500x400");
  auto win0 = CreateAppWindow(gfx::Rect{100, 200});
  auto win1 = CreateAppWindow(gfx::Rect{460, 10, 100, 200});
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  EXPECT_EQ(win0->GetRootWindow(), roots[0]);
  EXPECT_EQ(win1->GetRootWindow(), roots[1]);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());

  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  screen_orientation_controller->LockOrientationForWindow(
      win0.get(), OrientationLockType::kPortraitPrimary);
  screen_orientation_controller->LockOrientationForWindow(
      win1.get(), OrientationLockType::kLandscape);
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());

  EXPECT_EQ(
      OrientationLockType::kAny,
      screen_orientation_controller->GetCurrentAppRequestedOrientationLock());

  // Enter tablet mode and expect nothing will change until we activate win0.
  TabletModeControllerTestApi().DetachAllMice();
  EnableTabletMode(true);
  // Run a loop for mirror mode to kick in which is triggered asynchronously.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(OrientationLockType::kLandscape,
            screen_orientation_controller->natural_orientation());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());

  wm::ActivateWindow(win0.get());
  EXPECT_EQ(
      OrientationLockType::kPortraitPrimary,
      screen_orientation_controller->GetCurrentAppRequestedOrientationLock());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());

  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  base::RunLoop().RunUntilIdle();

  roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  EXPECT_EQ(win0->GetRootWindow(), roots[0]);
  EXPECT_EQ(win1->GetRootWindow(), roots[1]);

  // `win1` belongs to the external display, so it is not allowed to lock the
  // rotation.
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  EXPECT_EQ(
      OrientationLockType::kPortraitPrimary,
      screen_orientation_controller->GetCurrentAppRequestedOrientationLock());
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());

  // Even if you activate `win1`, internal display is not affected and remain
  // locked to the rotation requested by `win0`.
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(
      OrientationLockType::kPortraitPrimary,
      screen_orientation_controller->GetCurrentAppRequestedOrientationLock());
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());

  // Once `win0` is snapped in splitview, it can no longer lock the rotation.
  SplitViewController::Get(win0->GetRootWindow())
      ->SnapWindow(win0.get(), SplitViewController::RIGHT);
  EXPECT_EQ(
      OrientationLockType::kAny,
      screen_orientation_controller->GetCurrentAppRequestedOrientationLock());
}

TEST_F(ScreenOrientationControllerTest,
       MoveWindowWithOrientationLockBetweenDisplays) {
  UpdateDisplay("400x300,500x400");
  // Enter tablet mode and expect nothing will change until we activate win0.
  TabletModeControllerTestApi().DetachAllMice();
  EnableTabletMode(true);
  // Run a loop for mirror mode to kick in which is triggered asynchronously.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  // Now switch mirror mode off so that we can have two displays in tablet mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  base::RunLoop().RunUntilIdle();
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  // Create a window that locks the orientation to portriat-primary.
  auto win0 = CreateAppWindow(gfx::Rect{100, 200});
  EXPECT_EQ(win0->GetRootWindow(), roots[0]);
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  EXPECT_EQ(OrientationLockType::kLandscape,
            screen_orientation_controller->natural_orientation());
  screen_orientation_controller->LockOrientationForWindow(
      win0.get(), OrientationLockType::kPortraitPrimary);

  // Even with an accelerometer update that would trigger a 0 degree rotation,
  // the rotation of the internal display is locked to 270.
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_EQ(
      OrientationLockType::kPortraitPrimary,
      screen_orientation_controller->GetCurrentAppRequestedOrientationLock());
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());

  // Triggers the move-active-window-between-displays shortcut.
  auto* event_generator = GetEventGenerator();
  auto trigger_shortcut = [event_generator]() {
    constexpr int kFlags = ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN;
    event_generator->PressKey(ui::VKEY_M, kFlags);
    event_generator->ReleaseKey(ui::VKEY_M, kFlags);
  };

  // Move the window to the external display, and expect that the internal
  // display's orientation is no longer locked.
  trigger_shortcut();
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_EQ(win0->GetRootWindow(), roots[1]);
  EXPECT_EQ(
      OrientationLockType::kAny,
      screen_orientation_controller->GetCurrentAppRequestedOrientationLock());
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());

  // Move the window back to the internal display, and expect that its
  // orientation is locked again by that window.
  trigger_shortcut();
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_EQ(win0->GetRootWindow(), roots[0]);
  EXPECT_EQ(
      OrientationLockType::kPortraitPrimary,
      screen_orientation_controller->GetCurrentAppRequestedOrientationLock());
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(OrientationLockType::kAny, UserLockedOrientation());
}

class ForceInPhysicalTabletStateTest : public ScreenOrientationControllerTest {
 public:
  ForceInPhysicalTabletStateTest() = default;
  ForceInPhysicalTabletStateTest(const ForceInPhysicalTabletStateTest&) =
      delete;
  ForceInPhysicalTabletStateTest& operator=(
      const ForceInPhysicalTabletStateTest&) = delete;
  ~ForceInPhysicalTabletStateTest() override = default;

  // ScreenOrientationControllerTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceInTabletPhysicalState);
    ScreenOrientationControllerTest::SetUp();
  }
};

// Tests that screen rotation is supported while the device is forced to stay
// in physical tablet state.
TEST_F(ForceInPhysicalTabletStateTest, ScreenRotation) {
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  ASSERT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());

  // Test rotating in all directions are supported.
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravityFloat, 0.0f, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravityFloat, 0.0f));
  EXPECT_EQ(display::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

}  // namespace ash
