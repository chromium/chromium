// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_controller.h"

#include <string>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {
namespace {

Shelf* GetShelfForDisplay(int64_t display_id) {
  return Shell::GetRootWindowControllerWithDisplayId(display_id)->shelf();
}

void BuildAndSendNotification(message_center::MessageCenter* message_center,
                              const std::string& app_id,
                              const std::string& notification_id) {
  const message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, app_id);
  std::unique_ptr<message_center::Notification> notification =
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          base::ASCIIToUTF16("Test Web Notification"),
          base::ASCIIToUTF16("Notification message body."), gfx::Image(),
          base::ASCIIToUTF16("www.test.org"), GURL(), notifier_id,
          message_center::RichNotificationData(), nullptr /* delegate */);
  message_center->AddNotification(std::move(notification));
}

using ShelfControllerTest = AshTestBase;

TEST_F(ShelfControllerTest, Shutdown) {
  // Simulate a display change occurring during shutdown (e.g. due to a screen
  // rotation animation being canceled).
  Shell::Get()->shelf_controller()->Shutdown();
  display_manager()->SetDisplayRotation(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  // Ash does not crash during cleanup.
}

TEST_F(ShelfControllerTest, ShelfIDUpdate) {
  ShelfModel* model = Shell::Get()->shelf_controller()->model();

  const ShelfID id1("id1");
  const ShelfID id2("id2");

  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  window->SetProperty(kShelfIDKey, id1.Serialize());
  wm::ActivateWindow(window.get());
  EXPECT_EQ(id1, model->active_shelf_id());

  window->SetProperty(kShelfIDKey, id2.Serialize());
  EXPECT_EQ(id2, model->active_shelf_id());

  window->ClearProperty(kShelfIDKey);
  EXPECT_NE(id1, model->active_shelf_id());
  EXPECT_NE(id2, model->active_shelf_id());
}

class ShelfControllerNotificationIndicatorTest : public AshTestBase {
 public:
  ShelfControllerNotificationIndicatorTest() = default;
  ~ShelfControllerNotificationIndicatorTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kNotificationIndicator},
                                          {});
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ShelfControllerNotificationIndicatorTest);
};

// Tests that the ShelfController keeps the ShelfModel updated on new
// notifications.
TEST_F(ShelfControllerNotificationIndicatorTest, HasNotificationBasic) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  const std::string app_id("app_id");
  ShelfItem item;
  item.type = TYPE_APP;
  item.id = ShelfID(app_id);
  const int index = controller->model()->Add(item);
  EXPECT_FALSE(controller->model()->items()[index].has_notification);

  // Add a notification for |item|.
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  const std::string notification_id("notification_id");
  BuildAndSendNotification(message_center, app_id, notification_id);

  EXPECT_TRUE(controller->model()->items()[index].has_notification);

  // Remove the app and pin it, the notification should persist.
  controller->model()->RemoveItemAt(index);
  controller->model()->PinAppWithID(app_id);

  EXPECT_TRUE(controller->model()->items()[index].has_notification);

  message_center->RemoveNotification(notification_id, true);

  EXPECT_FALSE(controller->model()->items()[index].has_notification);
}

class ShelfControllerPrefsTest : public AshTestBase {
 public:
  ShelfControllerPrefsTest() = default;
  ~ShelfControllerPrefsTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfControllerPrefsTest);
};

// Ensure shelf settings are updated on preference changes.
TEST_F(ShelfControllerPrefsTest, ShelfRespectsPrefs) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetString(prefs::kShelfAlignmentLocal, "Left");
  prefs->SetString(prefs::kShelfAutoHideBehaviorLocal, "Always");

  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
}

// Ensure shelf settings are updated on per-display preference changes.
TEST_F(ShelfControllerPrefsTest, ShelfRespectsPerDisplayPrefs) {
  UpdateDisplay("1024x768,800x600");
  base::RunLoop().RunUntilIdle();
  const int64_t id1 = GetPrimaryDisplay().id();
  const int64_t id2 = GetSecondaryDisplay().id();
  Shelf* shelf1 = GetShelfForDisplay(id1);
  Shelf* shelf2 = GetShelfForDisplay(id2);

  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf1->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf2->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf1->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf2->auto_hide_behavior());

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, id1, SHELF_ALIGNMENT_LEFT);
  SetShelfAlignmentPref(prefs, id2, SHELF_ALIGNMENT_RIGHT);
  SetShelfAutoHideBehaviorPref(prefs, id1, SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  SetShelfAutoHideBehaviorPref(prefs, id2, SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf1->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, shelf2->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf1->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf2->auto_hide_behavior());
}

// Ensures that pre-Unified Mode per-display shelf settings don't prevent us
// from changing the shelf settings in unified mode.
TEST_F(ShelfControllerPrefsTest, ShelfRespectsPerDisplayPrefsUnified) {
  UpdateDisplay("1024x768,800x600");

  // Before enabling Unified Mode, set the shelf alignment for one of the two
  // displays, so that we have a per-display shelf alignment setting.
  ASSERT_FALSE(display_manager()->IsInUnifiedMode());
  const int64_t non_unified_primary_id = GetPrimaryDisplay().id();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  Shelf* shelf = GetShelfForDisplay(non_unified_primary_id);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  SetShelfAlignmentPref(prefs, non_unified_primary_id, SHELF_ALIGNMENT_LEFT);
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf->alignment());

  // Switch to Unified Mode, and expect to be able to change the shelf
  // alignment.
  display_manager()->SetUnifiedDesktopEnabled(true);
  ASSERT_TRUE(display_manager()->IsInUnifiedMode());
  const int64_t unified_id = display::kUnifiedDisplayId;
  ASSERT_EQ(unified_id, GetPrimaryDisplay().id());

  shelf = GetShelfForDisplay(unified_id);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  SetShelfAlignmentPref(prefs, unified_id, SHELF_ALIGNMENT_LEFT);
  SetShelfAutoHideBehaviorPref(prefs, unified_id,
                               SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  SetShelfAlignmentPref(prefs, unified_id, SHELF_ALIGNMENT_RIGHT);
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, shelf->alignment());
}

// Ensure shelf settings are correct after display swap at login screen, see
// crbug.com/748291
TEST_F(ShelfControllerPrefsTest,
       ShelfSettingsValidAfterDisplaySwapAtLoginScreen) {
  // Simulate adding an external display at the lock screen.
  GetSessionControllerClient()->RequestLockScreen();
  UpdateDisplay("1024x768,800x600");
  base::RunLoop().RunUntilIdle();
  const int64_t internal_display_id = GetPrimaryDisplay().id();
  const int64_t external_display_id = GetSecondaryDisplay().id();

  // The primary shelf should be on the internal display.
  EXPECT_EQ(GetPrimaryShelf(), GetShelfForDisplay(internal_display_id));
  EXPECT_NE(GetPrimaryShelf(), GetShelfForDisplay(external_display_id));

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  // Check for the default shelf preferences.
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfAutoHideBehaviorPref(prefs, internal_display_id));
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfAutoHideBehaviorPref(prefs, external_display_id));
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM,
            GetShelfAlignmentPref(prefs, internal_display_id));
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM,
            GetShelfAlignmentPref(prefs, external_display_id));

  // Check the current state; shelves have locked alignments in the lock screen.
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(external_display_id)->alignment());

  // Set some shelf prefs to differentiate the two shelves, check state.
  SetShelfAlignmentPref(prefs, internal_display_id, SHELF_ALIGNMENT_LEFT);
  SetShelfAlignmentPref(prefs, external_display_id, SHELF_ALIGNMENT_RIGHT);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(external_display_id)->alignment());

  SetShelfAutoHideBehaviorPref(prefs, external_display_id,
                               SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());

  // Simulate the external display becoming the primary display. The shelves are
  // swapped (each instance now has a different display id), check state.
  SwapPrimaryDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());

  // After screen unlock the shelves should have the expected alignment values.
  GetSessionControllerClient()->UnlockScreen();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
}

// Test display swap while logged in, which was causing a crash (see
// crbug.com/1022852)
TEST_F(ShelfControllerPrefsTest,
       ShelfSettingsValidAfterDisplaySwapWhileLoggedIn) {
  // Simulate adding an external display at the lock screen.
  GetSessionControllerClient()->RequestLockScreen();
  UpdateDisplay("1024x768,800x600");
  base::RunLoop().RunUntilIdle();
  const int64_t internal_display_id = GetPrimaryDisplay().id();
  const int64_t external_display_id = GetSecondaryDisplay().id();

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  // Set some shelf prefs to differentiate the two shelves.
  SetShelfAlignmentPref(prefs, internal_display_id, SHELF_ALIGNMENT_LEFT);
  SetShelfAlignmentPref(prefs, external_display_id, SHELF_ALIGNMENT_RIGHT);
  SetShelfAutoHideBehaviorPref(prefs, external_display_id,
                               SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Unlock the screen.
  GetSessionControllerClient()->UnlockScreen();
  base::RunLoop().RunUntilIdle();

  // Simulate the external display becoming the primary display. The shelves are
  // swapped (each instance now has a different display id), check state.
  SwapPrimaryDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
}

TEST_F(ShelfControllerPrefsTest, ShelfSettingsInTabletMode) {
  Shelf* shelf = GetPrimaryShelf();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, GetPrimaryDisplay().id(), SHELF_ALIGNMENT_LEFT);
  SetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplay().id(),
                               SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  ASSERT_EQ(SHELF_ALIGNMENT_LEFT, shelf->alignment());
  ASSERT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Verify after entering tablet mode, the shelf alignment is bottom and the
  // auto hide behavior has not changed.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Verify that screen rotation does not change alignment or auto-hide.
  display_manager()->SetDisplayRotation(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Verify after exiting tablet mode, the shelf alignment and auto hide
  // behavior get their stored pref values.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
}

using ShelfControllerAppModeTest = NoSessionAshTestBase;

// Tests that shelf auto hide behavior is always hidden in app mode.
TEST_F(ShelfControllerAppModeTest, AutoHideBehavior) {
  SimulateKioskMode(user_manager::USER_TYPE_KIOSK_APP);

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_ALWAYS_HIDDEN, shelf->auto_hide_behavior());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_ALWAYS_HIDDEN, shelf->auto_hide_behavior());

  display_manager()->SetDisplayRotation(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(SHELF_AUTO_HIDE_ALWAYS_HIDDEN, shelf->auto_hide_behavior());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_ALWAYS_HIDDEN, shelf->auto_hide_behavior());
}

}  // namespace
}  // namespace ash
