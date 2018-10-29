// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/tray_accessibility.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item_detailed_view_delegate.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

// Simulates changing the large cursor setting via menu.
void SetLargeCursorEnabledFromMenu(bool enabled) {
  Shell::Get()->accessibility_controller()->SetLargeCursorEnabled(enabled);
}

// Simulates changing the large cursor setting via webui settings.
void SetLargeCursorEnabledFromSettings(bool enabled) {
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, enabled);
}

}  // namespace

class TrayAccessibilityTest : public AshTestBase {
 protected:
  TrayAccessibilityTest() = default;
  ~TrayAccessibilityTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    if (features::IsSystemTrayUnifiedEnabled())
      return;

    tray_item_ = SystemTrayTestApi(Shell::Get()->GetPrimarySystemTray())
                     .tray_accessibility();
  }

  void CreateDetailedMenu() {
    // TODO(tetsui): Use UnifiedDetailedViewDelegate, or create a delegate for
    // unit testing, when removing SystemTrayItemDetailedViewDelegate.
    delegate_ = std::make_unique<SystemTrayItemDetailedViewDelegate>(nullptr);
    detailed_menu_ =
        std::make_unique<tray::AccessibilityDetailedView>(delegate_.get());
  }

  void CloseDetailMenu() {
    detailed_menu_.reset();
    delegate_.reset();
  }

  bool IsSpokenFeedbackMenuShownOnDetailMenu() const {
    return detailed_menu_->spoken_feedback_view_;
  }

  bool IsSelectToSpeakShownOnDetailMenu() const {
    return detailed_menu_->select_to_speak_view_;
  }

  bool IsHighContrastMenuShownOnDetailMenu() const {
    return detailed_menu_->high_contrast_view_;
  }

  bool IsScreenMagnifierMenuShownOnDetailMenu() const {
    return detailed_menu_->screen_magnifier_view_;
  }

  bool IsLargeCursorMenuShownOnDetailMenu() const {
    return detailed_menu_->large_cursor_view_;
  }

  bool IsAutoclickMenuShownOnDetailMenu() const {
    return detailed_menu_->autoclick_view_;
  }

  bool IsVirtualKeyboardMenuShownOnDetailMenu() const {
    return detailed_menu_->virtual_keyboard_view_;
  }

  bool IsMonoAudioMenuShownOnDetailMenu() const {
    return detailed_menu_->mono_audio_view_;
  }

  bool IsCaretHighlightMenuShownOnDetailMenu() const {
    return detailed_menu_->caret_highlight_view_;
  }

  bool IsHighlightMouseCursorMenuShownOnDetailMenu() const {
    return detailed_menu_->highlight_mouse_cursor_view_;
  }

  bool IsHighlightKeyboardFocusMenuShownOnDetailMenu() const {
    return detailed_menu_->highlight_keyboard_focus_view_;
  }

  bool IsStickyKeysMenuShownOnDetailMenu() const {
    return detailed_menu_->sticky_keys_view_;
  }

  // In material design we show the help button but theme it as disabled if
  // it is not possible to load the help page.
  bool IsHelpAvailableOnDetailMenu() {
    return detailed_menu_->help_view_->state() == views::Button::STATE_NORMAL;
  }

  // In material design we show the settings button but theme it as disabled if
  // it is not possible to load the settings page.
  bool IsSettingsAvailableOnDetailMenu() {
    return detailed_menu_->settings_view_->state() ==
           views::Button::STATE_NORMAL;
  }

  TrayAccessibility* tray_item_;  // Not owned.

 private:
  std::unique_ptr<DetailedViewDelegate> delegate_;
  std::unique_ptr<tray::AccessibilityDetailedView> detailed_menu_;

  DISALLOW_COPY_AND_ASSIGN(TrayAccessibilityTest);
};

// Tests that the icon becomes visible when the tray menu toggles a feature.
TEST_F(TrayAccessibilityTest, VisibilityFromMenu) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // By default the icon isn't visible.
  EXPECT_FALSE(tray_item_->tray_view()->visible());

  // Turning on an accessibility feature shows the icon.
  SetLargeCursorEnabledFromMenu(true);
  EXPECT_TRUE(tray_item_->tray_view()->visible());

  // Turning off all accessibility features hides the icon.
  SetLargeCursorEnabledFromMenu(false);
  EXPECT_FALSE(tray_item_->tray_view()->visible());
}

// Tests that the icon becomes visible when webui settings toggles a feature.
TEST_F(TrayAccessibilityTest, VisibilityFromSettings) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // By default the icon isn't visible.
  EXPECT_FALSE(tray_item_->tray_view()->visible());

  // Turning on an accessibility pref shows the icon.
  SetLargeCursorEnabledFromSettings(true);
  EXPECT_TRUE(tray_item_->tray_view()->visible());

  // Turning off all accessibility prefs hides the icon.
  SetLargeCursorEnabledFromSettings(false);
  EXPECT_FALSE(tray_item_->tray_view()->visible());
}

TEST_F(TrayAccessibilityTest, CheckMenuVisibilityOnDetailMenu) {
  // Except help & settings, others should be kept the same
  // in LOGIN | NOT LOGIN | LOCKED. https://crbug.com/632107.
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHelpAvailableOnDetailMenu());
  EXPECT_TRUE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  CloseDetailMenu();

  // Simulate screen lock.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  CloseDetailMenu();
  UnblockUserSession();

  // Simulate adding multiprofile user.
  BlockUserSession(BLOCKED_BY_USER_ADDING_SCREEN);
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  CloseDetailMenu();
  UnblockUserSession();
}

class TrayAccessibilityLoginScreenTest : public NoSessionAshTestBase {
 protected:
  TrayAccessibilityLoginScreenTest() = default;
  ~TrayAccessibilityLoginScreenTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    // TODO(tetsui): Remove after UnifiedSystemTray launch.
    // https://crbug.com/847104
    if (features::IsSystemTrayUnifiedEnabled())
      return;

    tray_item_ = SystemTrayTestApi(Shell::Get()->GetPrimarySystemTray())
                     .tray_accessibility();
  }

  // In material design we show the help button but theme it as disabled if
  // it is not possible to load the help page.
  bool IsHelpAvailableOnDetailMenu() {
    return tray_item_->detailed_menu_->help_view_->state() ==
           views::Button::STATE_NORMAL;
  }

  // In material design we show the settings button but theme it as disabled if
  // it is not possible to load the settings page.
  bool IsSettingsAvailableOnDetailMenu() {
    return tray_item_->detailed_menu_->settings_view_->state() ==
           views::Button::STATE_NORMAL;
  }

  TrayAccessibility* tray_item_;  // Not owned.

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayAccessibilityLoginScreenTest);
};

TEST_F(TrayAccessibilityLoginScreenTest, LoginStatus) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // By default the icon is not visible at the login screen.
  views::View* tray_icon = tray_item_->tray_view();
  EXPECT_FALSE(tray_icon->visible());

  // Enabling an accessibility feature shows the icon.
  SetLargeCursorEnabledFromMenu(true);
  EXPECT_TRUE(tray_icon->visible());

  // Disabling the accessibility feature hides the icon.
  SetLargeCursorEnabledFromMenu(false);
  EXPECT_FALSE(tray_icon->visible());

  // Settings and help are not available on the login screen because they use
  // webui.
  tray_item_->ShowDetailedView(0);
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
}

}  // namespace ash
