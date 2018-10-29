// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login_status.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/accessibility/tray_accessibility.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

using testing::Return;
using testing::_;
using testing::WithParamInterface;

namespace chromeos {

enum PrefSettingMechanism {
  PREF_SERVICE,
  POLICY,
};

////////////////////////////////////////////////////////////////////////////////
// Changing accessibility settings may change preferences, so these helpers spin
// the message loop to ensure ash sees the change.

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableSpokenFeedback(bool enabled) {
  AccessibilityManager::Get()->EnableSpokenFeedback(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableSelectToSpeak(bool enabled) {
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableHighContrast(bool enabled) {
  AccessibilityManager::Get()->EnableHighContrast(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableAutoclick(bool enabled) {
  AccessibilityManager::Get()->EnableAutoclick(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableVirtualKeyboard(bool enabled) {
  AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableLargeCursor(bool enabled) {
  AccessibilityManager::Get()->EnableLargeCursor(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableMonoAudio(bool enabled) {
  AccessibilityManager::Get()->EnableMonoAudio(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetCaretHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetCaretHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetCursorHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetCursorHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetFocusHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetFocusHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableStickyKeys(bool enabled) {
  AccessibilityManager::Get()->EnableStickyKeys(enabled);
  base::RunLoop().RunUntilIdle();
}

// Uses InProcessBrowserTest instead of OobeBaseTest because most of the tests
// don't need to test the login screen.
class TrayAccessibilityTest
    : public InProcessBrowserTest,
      public WithParamInterface<PrefSettingMechanism> {
 public:
  TrayAccessibilityTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~TrayAccessibilityTest() override = default;

  // The profile which should be used by these tests.
  Profile* GetProfile() { return ProfileManager::GetActiveUserProfile(); }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetShowAccessibilityOptionsInSystemTrayMenu(bool value) {
    if (GetParam() == PREF_SERVICE) {
      PrefService* prefs = GetProfile()->GetPrefs();
      prefs->SetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu, value);
      // Prefs are sent to ash asynchronously.
      base::RunLoop().RunUntilIdle();
    } else if (GetParam() == POLICY) {
      policy::PolicyMap policy_map;
      policy_map.Set(policy::key::kShowAccessibilityOptionsInSystemTrayMenu,
                     policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                     policy::POLICY_SOURCE_CLOUD,
                     std::make_unique<base::Value>(value), nullptr);
      provider_.UpdateChromePolicy(policy_map);
      base::RunLoop().RunUntilIdle();
    } else {
      FAIL() << "Unknown test parameterization";
    }
  }

  static ash::TrayAccessibility* tray() {
    return ash::SystemTrayTestApi(ash::Shell::Get()->GetPrimarySystemTray())
        .tray_accessibility();
  }

  // The "tray view" is the icon.
  bool IsTrayIconVisible() const { return tray()->tray_view()->visible(); }

  views::View* CreateMenuItem() {
    return tray()->CreateDefaultView(GetLoginStatus());
  }

  void DestroyMenuItem() { return tray()->OnDefaultViewDestroyed(); }

  bool CanCreateMenuItem() {
    views::View* menu_item_view = CreateMenuItem();
    DestroyMenuItem();
    return menu_item_view != nullptr;
  }

  void SetLoginStatus(ash::LoginStatus status) {
    tray()->UpdateAfterLoginStatusChange(status);
  }

  ash::LoginStatus GetLoginStatus() { return tray()->login_; }

  bool CreateDetailedMenu() {
    tray()->ShowDetailedView(0);
    return tray()->detailed_menu_ != nullptr;
  }

  ash::tray::AccessibilityDetailedView* GetDetailedMenu() {
    return tray()->detailed_menu_;
  }

  void CloseDetailMenu() {
    ASSERT_TRUE(tray()->detailed_menu_);
    tray()->OnDetailedViewDestroyed();
    EXPECT_FALSE(tray()->detailed_menu_);
  }

  // These helpers may change prefs in ash, so they must spin the message loop
  // to wait for chrome to observe the change.
  void ClickSpokenFeedbackOnDetailMenu() {
    ash::HoverHighlightView* view =
        tray()->detailed_menu_->spoken_feedback_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickHighContrastOnDetailMenu() {
    ash::HoverHighlightView* view = tray()->detailed_menu_->high_contrast_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickScreenMagnifierOnDetailMenu() {
    ash::HoverHighlightView* view =
        tray()->detailed_menu_->screen_magnifier_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickAutoclickOnDetailMenu() {
    ash::HoverHighlightView* view = tray()->detailed_menu_->autoclick_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickVirtualKeyboardOnDetailMenu() {
    ash::HoverHighlightView* view =
        tray()->detailed_menu_->virtual_keyboard_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickLargeMouseCursorOnDetailMenu() {
    ash::HoverHighlightView* view = tray()->detailed_menu_->large_cursor_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickMonoAudioOnDetailMenu() {
    ash::HoverHighlightView* view = tray()->detailed_menu_->mono_audio_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickCaretHighlightOnDetailMenu() {
    ash::HoverHighlightView* view =
        tray()->detailed_menu_->caret_highlight_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickHighlightMouseCursorOnDetailMenu() {
    ash::HoverHighlightView* view =
        tray()->detailed_menu_->highlight_mouse_cursor_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickHighlightKeyboardFocusOnDetailMenu() {
    ash::HoverHighlightView* view =
        tray()->detailed_menu_->highlight_keyboard_focus_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickStickyKeysOnDetailMenu() {
    ash::HoverHighlightView* view = tray()->detailed_menu_->sticky_keys_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  void ClickSelectToSpeakOnDetailMenu() {
    ash::HoverHighlightView* view =
        tray()->detailed_menu_->select_to_speak_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  bool IsSpokenFeedbackEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->spoken_feedback_enabled_;
  }

  bool IsSelectToSpeakEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->select_to_speak_enabled_;
  }

  bool IsHighContrastEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->high_contrast_enabled_;
  }

  bool IsScreenMagnifierEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->screen_magnifier_enabled_;
  }

  bool IsLargeCursorEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->large_cursor_enabled_;
  }

  bool IsAutoclickEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->autoclick_enabled_;
  }

  bool IsVirtualKeyboardEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->virtual_keyboard_enabled_;
  }

  bool IsMonoAudioEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->mono_audio_enabled_;
  }

  bool IsCaretHighlightEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->caret_highlight_enabled_;
  }

  bool IsHighlightMouseCursorEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->highlight_mouse_cursor_enabled_;
  }

  bool IsHighlightKeyboardFocusEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->highlight_keyboard_focus_enabled_;
  }

  bool IsStickyKeysEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->sticky_keys_enabled_;
  }

  // Disable animations so that tray icons hide immediately.
  ui::ScopedAnimationDurationScaleMode disable_animations_;

  policy::MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowTrayIcon) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  // Confirms that the icon is invisible just after login.
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling spoken feedback changes the visibility of the icon.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableSpokenFeedback(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling high contrast changes the visibility of the icon.
  EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableHighContrast(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling magnifier changes the visibility of the icon.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling automatic clicks changes the visibility of the icon.
  EnableAutoclick(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableAutoclick(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling the virtual keyboard setting changes the visibility of the a11y
  // icon.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableVirtualKeyboard(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling large cursor changes the visibility of the icon.
  EnableLargeCursor(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableLargeCursor(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling mono audio changes the visibility of the icon.
  EnableMonoAudio(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableMonoAudio(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling caret highlight changes the visibility of the icon.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling highlight mouse cursor changes the visibility of the icon.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling highlight keyboard focus changes the visibility of the icon.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling sticky keys changes the visibility of the icon.
  EnableStickyKeys(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableStickyKeys(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling select-to-speak changes the visibility of the icon.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableSelectToSpeak(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableLargeCursor(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableMonoAudio(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableStickyKeys(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsTrayIconVisible());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsTrayIconVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableStickyKeys(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Confirms that ash::prefs::kShouldAlwaysShowAccessibilityMenu doesn't affect
  // the icon on the tray.
  SetShowAccessibilityOptionsInSystemTrayMenu(true);
  EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  EnableHighContrast(false);
  EXPECT_FALSE(IsTrayIconVisible());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenu) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is hidden.
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling spoken feedback changes the visibility of the menu.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling high contrast changes the visibility of the menu.
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling screen magnifier changes the visibility of the menu.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling autoclick changes the visibility of the menu.
  EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling virtual keyboard changes the visibility of the menu.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling large mouse cursor changes the visibility of the menu.
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling mono audio changes the visibility of the menu.
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling caret highlight changes the visibility of the menu.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling highlight mouse cursor changes the visibility of the menu.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling highlight keyboard focus changes the visibility of the menu.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling sticky keys changes the visibility of the menu.
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling select-to-speak dragging changes the visibility of the menu.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenuWithShowMenuOption) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu is visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling autoclick.
  EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling large mouse cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of the toggling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling select-to-speak.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is invisible.
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenuWithShowOnLoginScreen) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  SetLoginStatus(ash::LoginStatus::NOT_LOGGED_IN);

  // Confirms that the menu is visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling large mouse cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(CanCreateMenuItem());
}

// TODO: Move to ash_unittests.
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, KeepMenuVisibilityOnLockScreen) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  // Enables high contrast mode.
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());

  // Locks the screen.
  SetLoginStatus(ash::LoginStatus::LOCKED);
  EXPECT_TRUE(CanCreateMenuItem());

  // Disables high contrast mode.
  EnableHighContrast(false);

  // Confirms that the menu is still visible.
  EXPECT_TRUE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ClickDetailMenu) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  SetLoginStatus(ash::LoginStatus::USER);

  // Confirms that the check item toggles the spoken feedback.
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickSpokenFeedbackOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickSpokenFeedbackOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  // Confirms that the check item toggles the high contrast.
  EXPECT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighContrastOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsHighContrastEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighContrastOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());

  // Confirms that the check item toggles the magnifier.
  EXPECT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());

  EXPECT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());
  EXPECT_TRUE(CreateDetailedMenu());
  ClickScreenMagnifierOnDetailMenu();
  EXPECT_TRUE(MagnificationManager::Get()->IsMagnifierEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickScreenMagnifierOnDetailMenu();
  EXPECT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());

  // Confirms that the check item toggles autoclick.
  EXPECT_FALSE(AccessibilityManager::Get()->IsAutoclickEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickAutoclickOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsAutoclickEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickAutoclickOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsAutoclickEnabled());

  // Confirms that the check item toggles on-screen keyboard.
  EXPECT_FALSE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickVirtualKeyboardOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickVirtualKeyboardOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());

  // Confirms that the check item toggles large mouse cursor.
  EXPECT_FALSE(AccessibilityManager::Get()->IsLargeCursorEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickLargeMouseCursorOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsLargeCursorEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickLargeMouseCursorOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsLargeCursorEnabled());

  // Confirms that the check item toggles mono audio.
  EXPECT_FALSE(AccessibilityManager::Get()->IsMonoAudioEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickMonoAudioOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsMonoAudioEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickMonoAudioOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsMonoAudioEnabled());

  // Confirms that the check item toggles caret highlight.
  EXPECT_FALSE(AccessibilityManager::Get()->IsCaretHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickCaretHighlightOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsCaretHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickCaretHighlightOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsCaretHighlightEnabled());

  // Confirms that the check item toggles highlight mouse cursor.
  EXPECT_FALSE(AccessibilityManager::Get()->IsCursorHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighlightMouseCursorOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsCursorHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighlightMouseCursorOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsCursorHighlightEnabled());

  // Confirms that the check item toggles highlight keyboard focus.
  EXPECT_FALSE(AccessibilityManager::Get()->IsFocusHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighlightKeyboardFocusOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsFocusHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighlightKeyboardFocusOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsFocusHighlightEnabled());

  // Confirms that the check item toggles sticky keys.
  EXPECT_FALSE(AccessibilityManager::Get()->IsStickyKeysEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickStickyKeysOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsStickyKeysEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickStickyKeysOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsStickyKeysEnabled());

  // Confirms that the check item toggles select-to-speak.
  EXPECT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickSelectToSpeakOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickSelectToSpeakOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
}

// TODO: Move to ash_unittests.
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, CheckMarksOnDetailMenu) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  SetLoginStatus(ash::LoginStatus::NOT_LOGGED_IN);

  // At first, all of the check is unchecked.
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling spoken feedback.
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling high contrast.
  EnableHighContrast(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling full screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling screen magnifier.
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling large cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling large cursor.
  EnableLargeCursor(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enable on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disable on-screen keyboard.
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling mono audio.
  EnableMonoAudio(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling caret highlight.
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling highlight mouse cursor.
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling highlight keyboard focus.
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling sticky keys.
  EnableStickyKeys(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling all of the a11y features.
  EnableSpokenFeedback(true);
  EnableHighContrast(true);
  SetMagnifierEnabled(true);
  EnableLargeCursor(true);
  EnableVirtualKeyboard(true);
  EnableAutoclick(true);
  EnableMonoAudio(true);
  SetCaretHighlightEnabled(true);
  SetCursorHighlightEnabled(true);
  SetFocusHighlightEnabled(true);
  EnableStickyKeys(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorEnabledOnDetailMenu());
  // Focus highlighting can't be on when spoken feedback is on
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling all of the a11y features.
  EnableSpokenFeedback(false);
  EnableHighContrast(false);
  SetMagnifierEnabled(false);
  EnableLargeCursor(false);
  EnableVirtualKeyboard(false);
  EnableAutoclick(false);
  EnableMonoAudio(false);
  SetCaretHighlightEnabled(false);
  SetCursorHighlightEnabled(false);
  SetFocusHighlightEnabled(false);
  EnableStickyKeys(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling autoclick.
  EnableAutoclick(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling autoclick.
  EnableAutoclick(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  CloseDetailMenu();
}

// Verify that the accessiblity system detailed menu remains open when an item
// is selected or deselected.
// TODO: Move to ash_unittests.
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, DetailMenuRemainsOpen) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  EXPECT_TRUE(CreateDetailedMenu());

  ClickAutoclickOnDetailMenu();
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(GetDetailedMenu());

  ClickAutoclickOnDetailMenu();
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(GetDetailedMenu());
}

INSTANTIATE_TEST_CASE_P(TrayAccessibilityTestInstance,
                        TrayAccessibilityTest,
                        testing::Values(PREF_SERVICE,
                                        POLICY));

}  // namespace chromeos
