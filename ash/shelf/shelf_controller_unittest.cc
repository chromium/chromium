// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_controller.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
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
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

Shelf* GetShelfForDisplay(int64_t display_id) {
  return Shell::GetRootWindowControllerWithDisplayId(display_id)->shelf();
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

  ShelfControllerNotificationIndicatorTest(
      const ShelfControllerNotificationIndicatorTest&) = delete;
  ShelfControllerNotificationIndicatorTest& operator=(
      const ShelfControllerNotificationIndicatorTest&) = delete;

  ~ShelfControllerNotificationIndicatorTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    account_id_ = AccountId::FromUserEmail("test@gmail.com");
  }

  void SendAppUpdate(bool app_has_badge) {
    ShelfController* controller = Shell::Get()->shelf_controller();

    auto test_app = std::make_unique<apps::App>(apps::AppType::kArc, "app_id");
    test_app->has_badge = app_has_badge;
    apps::AppUpdate test_update(nullptr, /*delta=*/test_app.get(), account_id_);
    static_cast<apps::AppRegistryCache::Observer*>(controller)
        ->OnAppUpdate(test_update);
  }

 private:
  AccountId account_id_;
};

// Tests that the ShelfController keeps the ShelfModel updated on calls to
// OnAppUpdate().
TEST_F(ShelfControllerNotificationIndicatorTest, HasNotificationBasic) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  const std::string app_id("app_id");
  ShelfItem item;
  item.type = TYPE_APP;
  item.id = ShelfID(app_id);
  const int index = controller->model()->Add(
      item, std::make_unique<TestShelfItemDelegate>(item.id));
  EXPECT_FALSE(controller->model()->items()[index].has_notification);

  // Send an app update to ShelfController for adding a notification badge.
  SendAppUpdate(true /* app_has_badge */);

  EXPECT_TRUE(controller->model()->items()[index].has_notification);

  // Send an app update to ShelfController for removing a notification badge.
  SendAppUpdate(false /* app_has_badge */);

  EXPECT_FALSE(controller->model()->items()[index].has_notification);
}

class ShelfControllerPrefsTest
    : public AshTestBase,
      public testing::WithParamInterface<
          /*is_shelf_auto_hide_separation_enabled*/ bool> {
 public:
  ShelfControllerPrefsTest()
      : is_shelf_auto_hide_separation_enabled_(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(
        features::kShelfAutoHideSeparation,
        is_shelf_auto_hide_separation_enabled_);
  }

  ShelfControllerPrefsTest(const ShelfControllerPrefsTest&) = delete;
  ShelfControllerPrefsTest& operator=(const ShelfControllerPrefsTest&) = delete;

  ~ShelfControllerPrefsTest() override = default;

  void SetAutoHideBehaviorPrefForMode(PrefService* prefs,
                                      int64_t display_id,
                                      const std::string& value,
                                      bool tablet_mode) {
    SetPerDisplayShelfPref(prefs, display_id,
                           tablet_mode ? prefs::kShelfAutoHideTabletModeBehavior
                                       : prefs::kShelfAutoHideBehavior,
                           value);
    prefs->SetString(tablet_mode ? prefs::kShelfAutoHideTabletModeBehaviorLocal
                                 : prefs::kShelfAutoHideBehaviorLocal,
                     value);
    prefs->SetString(tablet_mode ? prefs::kShelfAutoHideTabletModeBehavior
                                 : prefs::kShelfAutoHideBehavior,
                     value);
  }

  bool is_shelf_auto_hide_separation_enabled() {
    return is_shelf_auto_hide_separation_enabled_;
  }

 private:
  const bool is_shelf_auto_hide_separation_enabled_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ShelfAutoHideSeparation,
                         ShelfControllerPrefsTest,
                         testing::Bool());

// Ensure relevant shelf preferences have been registsered.
TEST_P(ShelfControllerPrefsTest, PrefsAreRegistered) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kShelfAutoHideBehavior));
  EXPECT_TRUE(prefs->FindPreference(prefs::kShelfAutoHideBehaviorLocal));
  EXPECT_TRUE(prefs->FindPreference(prefs::kShelfAlignment));
  EXPECT_TRUE(prefs->FindPreference(prefs::kShelfAlignmentLocal));
  EXPECT_TRUE(prefs->FindPreference(prefs::kShelfPreferences));
  if (is_shelf_auto_hide_separation_enabled()) {
    EXPECT_TRUE(prefs->FindPreference(prefs::kShelfAutoHideTabletModeBehavior));
    EXPECT_TRUE(
        prefs->FindPreference(prefs::kShelfAutoHideTabletModeBehaviorLocal));
  }
}

// Ensure shelf settings are updated on preference changes.
TEST_P(ShelfControllerPrefsTest, ShelfRespectsPrefs) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetString(prefs::kShelfAlignmentLocal, "Left");
  EXPECT_EQ(ShelfAlignment::kLeft, shelf->alignment());

  if (is_shelf_auto_hide_separation_enabled()) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
    prefs->SetString(prefs::kShelfAutoHideTabletModeBehaviorLocal, "Always");
    EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  } else {
    EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
    prefs->SetString(prefs::kShelfAutoHideBehaviorLocal, "Always");
    EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  }
}

// Ensure shelf settings are updated on per-display preference changes.
TEST_P(ShelfControllerPrefsTest, ShelfRespectsPerDisplayPrefs) {
  UpdateDisplay("1024x768,800x600");
  base::RunLoop().RunUntilIdle();
  const int64_t id1 = GetPrimaryDisplay().id();
  const int64_t id2 = GetSecondaryDisplay().id();
  Shelf* shelf1 = GetShelfForDisplay(id1);
  Shelf* shelf2 = GetShelfForDisplay(id2);

  EXPECT_EQ(ShelfAlignment::kBottom, shelf1->alignment());
  EXPECT_EQ(ShelfAlignment::kBottom, shelf2->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf1->auto_hide_behavior());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf2->auto_hide_behavior());

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, id1, ShelfAlignment::kLeft);
  SetShelfAlignmentPref(prefs, id2, ShelfAlignment::kRight);
  SetShelfAutoHideBehaviorPref(prefs, id1, ShelfAutoHideBehavior::kAlways);
  SetShelfAutoHideBehaviorPref(prefs, id2, ShelfAutoHideBehavior::kAlways);

  EXPECT_EQ(ShelfAlignment::kLeft, shelf1->alignment());
  EXPECT_EQ(ShelfAlignment::kRight, shelf2->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf1->auto_hide_behavior());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf2->auto_hide_behavior());
}

// Ensures that pre-Unified Mode per-display shelf settings don't prevent us
// from changing the shelf settings in unified mode.
TEST_P(ShelfControllerPrefsTest, ShelfRespectsPerDisplayPrefsUnified) {
  UpdateDisplay("1024x768,800x600");

  // Before enabling Unified Mode, set the shelf alignment for one of the two
  // displays, so that we have a per-display shelf alignment setting.
  ASSERT_FALSE(display_manager()->IsInUnifiedMode());
  const int64_t non_unified_primary_id = GetPrimaryDisplay().id();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  Shelf* shelf = GetShelfForDisplay(non_unified_primary_id);
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  SetShelfAlignmentPref(prefs, non_unified_primary_id, ShelfAlignment::kLeft);
  EXPECT_EQ(ShelfAlignment::kLeft, shelf->alignment());

  // Switch to Unified Mode, and expect to be able to change the shelf
  // alignment.
  display_manager()->SetUnifiedDesktopEnabled(true);
  ASSERT_TRUE(display_manager()->IsInUnifiedMode());
  const int64_t unified_id = display::kUnifiedDisplayId;
  ASSERT_EQ(unified_id, GetPrimaryDisplay().id());

  shelf = GetShelfForDisplay(unified_id);
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  SetShelfAlignmentPref(prefs, unified_id, ShelfAlignment::kLeft);
  SetShelfAutoHideBehaviorPref(prefs, unified_id,
                               ShelfAutoHideBehavior::kAlways);

  EXPECT_EQ(ShelfAlignment::kLeft, shelf->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  SetShelfAlignmentPref(prefs, unified_id, ShelfAlignment::kRight);
  EXPECT_EQ(ShelfAlignment::kRight, shelf->alignment());
}

// Ensure shelf settings are correct after display swap at login screen, see
// crbug.com/748291
TEST_P(ShelfControllerPrefsTest,
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
  EXPECT_EQ(ShelfAutoHideBehavior::kNever,
            GetShelfAutoHideBehaviorPref(prefs, internal_display_id));
  EXPECT_EQ(ShelfAutoHideBehavior::kNever,
            GetShelfAutoHideBehaviorPref(prefs, external_display_id));
  EXPECT_EQ(ShelfAlignment::kBottom,
            GetShelfAlignmentPref(prefs, internal_display_id));
  EXPECT_EQ(ShelfAlignment::kBottom,
            GetShelfAlignmentPref(prefs, external_display_id));

  // Check the current state; shelves have locked alignments in the lock screen.
  EXPECT_EQ(ShelfAutoHideBehavior::kNever,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
  EXPECT_EQ(ShelfAlignment::kBottomLocked,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(ShelfAlignment::kBottomLocked,
            GetShelfForDisplay(external_display_id)->alignment());

  // Set some shelf prefs to differentiate the two shelves, check state.
  SetShelfAlignmentPref(prefs, internal_display_id, ShelfAlignment::kLeft);
  SetShelfAlignmentPref(prefs, external_display_id, ShelfAlignment::kRight);
  EXPECT_EQ(ShelfAlignment::kBottomLocked,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(ShelfAlignment::kBottomLocked,
            GetShelfForDisplay(external_display_id)->alignment());

  SetShelfAutoHideBehaviorPref(prefs, external_display_id,
                               ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());

  // Simulate the external display becoming the primary display. The shelves are
  // swapped (each instance now has a different display id), check state.
  SwapPrimaryDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ShelfAlignment::kBottomLocked,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(ShelfAlignment::kBottomLocked,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());

  // After screen unlock the shelves should have the expected alignment values.
  GetSessionControllerClient()->UnlockScreen();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ShelfAlignment::kLeft,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(ShelfAlignment::kRight,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
}

// Test display swap while logged in, which was causing a crash (see
// crbug.com/1022852)
TEST_P(ShelfControllerPrefsTest,
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
  SetShelfAlignmentPref(prefs, internal_display_id, ShelfAlignment::kLeft);
  SetShelfAlignmentPref(prefs, external_display_id, ShelfAlignment::kRight);
  SetShelfAutoHideBehaviorPref(prefs, external_display_id,
                               ShelfAutoHideBehavior::kAlways);

  // Unlock the screen.
  GetSessionControllerClient()->UnlockScreen();
  base::RunLoop().RunUntilIdle();

  // Simulate the external display becoming the primary display. The shelves are
  // swapped (each instance now has a different display id), check state.
  SwapPrimaryDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ShelfAlignment::kLeft,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(ShelfAlignment::kRight,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
}

// Tests shelf settings behavior when switching between clamshell mode and
// tablet mode.
TEST_P(ShelfControllerPrefsTest, ShelfSettingsBetweenClamshellAndTabletMode) {
  Shelf* shelf = GetPrimaryShelf();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, GetPrimaryDisplay().id(), ShelfAlignment::kLeft);
  SetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplay().id(),
                               ShelfAutoHideBehavior::kAlways);
  ASSERT_EQ(ShelfAlignment::kLeft, shelf->alignment());
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  // Verify after entering tablet mode, the shelf alignment is bottom.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  // If shelf-auto-hide-separation is enabled then the auto hide behavior should
  // be never (the default value) because this is the first time that tablet
  // mode is entered. If shelf-auto-hide-separation is not enabled then the auto
  // hide behvaior should be whatever value it was before entering tablet mode.
  if (is_shelf_auto_hide_separation_enabled()) {
    EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  } else {
    EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  }

  // Verify that screen rotation does not change alignment or auto-hide.
  display_manager()->SetDisplayRotation(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  if (is_shelf_auto_hide_separation_enabled()) {
    EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  } else {
    EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  }

  // Verify after exiting tablet mode, the shelf alignment and auto hide
  // behavior get their stored pref values.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(ShelfAlignment::kLeft, shelf->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  // Change the clamshell-mode auto hide setting and then switch to tablet mode.
  SetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplay().id(),
                               ShelfAutoHideBehavior::kNever);
  ASSERT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // If shelf-auto-hide-separation is enabled then the auto hide behavior should
  // be whatever it was previously in tablet mode (never, in this case);
  // otherwise, it should follow the clamshell-mode behavior (still never, in
  // this case).
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  // Change the tablet-mode auto hide setting and then switch to clamshell mode.
  SetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplay().id(),
                               ShelfAutoHideBehavior::kAlways);
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  // If shelf-auto-hide-separation is enabled then the auto hide behavior should
  // be whatever it was previously in clamshell mode (never, in this case);
  // otherwise it should follow the tablet-mode behavior (always, in this case).
  if (is_shelf_auto_hide_separation_enabled()) {
    EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  } else {
    EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  }
}

using ShelfControllerPrefsWithSeparationTest = ShelfControllerPrefsTest;

INSTANTIATE_TEST_SUITE_P(ShelfAutoHideSeparation,
                         ShelfControllerPrefsWithSeparationTest,
                         testing::Values(true));

// Tests shelf auto hide behavior when changing tablet mode settings while in
// clamshell mode, and vice versa. Note that this test only makes sense when
// shelf-auto-hide-separation is enabled because there is no distinction between
// "clamshell-mode auto hide pref" vs. "tablet-mode auto hide pref" when
// shelf-auto-hide-separation is not enabled.
TEST_P(ShelfControllerPrefsWithSeparationTest,
       ShelfSettingsChangedInAnotherMode) {
  Shelf* shelf = GetPrimaryShelf();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  // Ensure the auto hide behavior is "Always" in both clamshell and tablet
  // mode. Then switch to tablet mode, and change the clamshell-mode auto hide
  // behavior. This should not affect the tablet-mode auto hide behavior.
  SetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplay().id(),
                               ShelfAutoHideBehavior::kAlways);
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  SetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplay().id(),
                               ShelfAutoHideBehavior::kAlways);
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  const auto display_id = GetPrimaryDisplay().id();
  SetAutoHideBehaviorPrefForMode(prefs, display_id, "Never",
                                 /*tablet_mode*/ false);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  // Ensure the auto hide behavior is "Never" in both clamshell and tablet
  // mode. Then switch to clamshell mode, and change the tablet-mode auto hide
  // behavior. This should not affect the clamshell-mode auto hide behavior.
  SetShelfAutoHideBehaviorPref(prefs, display_id,
                               ShelfAutoHideBehavior::kNever);
  ASSERT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  ASSERT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  SetAutoHideBehaviorPrefForMode(prefs, display_id, "Always",
                                 /*tablet_mode*/ true);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
}

class ShelfControllerAppModeTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<
          /*is_shelf_auto_hide_separation_enabled*/ bool> {
 public:
  ShelfControllerAppModeTest()
      : is_shelf_auto_hide_separation_enabled_(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(
        features::kShelfAutoHideSeparation,
        is_shelf_auto_hide_separation_enabled_);
  }

  ShelfControllerAppModeTest(const ShelfControllerAppModeTest&) = delete;
  ShelfControllerAppModeTest& operator=(const ShelfControllerAppModeTest&) =
      delete;

  ~ShelfControllerAppModeTest() override = default;

 private:
  const bool is_shelf_auto_hide_separation_enabled_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ShelfAutoHideSeparation,
                         ShelfControllerAppModeTest,
                         testing::Bool());

// Tests that shelf auto hide behavior is always hidden in app mode.
TEST_P(ShelfControllerAppModeTest, AutoHideBehavior) {
  SimulateKioskMode(user_manager::UserType::kKioskApp);

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAutoHideBehavior::kAlwaysHidden, shelf->auto_hide_behavior());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlwaysHidden, shelf->auto_hide_behavior());

  display_manager()->SetDisplayRotation(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlwaysHidden, shelf->auto_hide_behavior());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlwaysHidden, shelf->auto_hide_behavior());
}

}  // namespace ash
