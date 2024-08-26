// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/system/accessibility/accessibility_detailed_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

using testing::_;
using testing::Return;
using testing::WithParamInterface;

namespace ash {

enum PrefSettingMechanism {
  PREF_SERVICE,
  POLICY,
};

namespace {

////////////////////////////////////////////////////////////////////////////////
// Changing accessibility settings may change preferences, so these helpers spin
// the message loop to ensure ash sees the change.

void SetScreenMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetDockedMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetDockedMagnifierEnabled(enabled);
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

void EnableDictation(bool enabled) {
  bool already_enabled = AccessibilityManager::Get()->IsDictationEnabled();
  if (enabled == already_enabled) {
    return;
  }
  AccessibilityManager::Get()->ToggleDictation();
  base::RunLoop().RunUntilIdle();
}

void EnableSwitchAccess(bool enabled) {
  AccessibilityManager::Get()->SetSwitchAccessEnabled(enabled);
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

void EnableLiveCaption(bool enabled) {
  AccessibilityManager::Get()->EnableLiveCaption(enabled);
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

}  // namespace

// Uses InProcessBrowserTest instead of OobeBaseTest because most of the tests
// don't need to test the login screen.
class TrayAccessibilityTest : public InProcessBrowserTest,
                              public WithParamInterface<PrefSettingMechanism> {
 public:
  TrayAccessibilityTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~TrayAccessibilityTest() override = default;

  // The profile which should be used by these tests.
  Profile* GetProfile() { return ProfileManager::GetActiveUserProfile(); }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    tray_test_api_ = ash::SystemTrayTestApi::Create();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
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
                     policy::POLICY_SOURCE_CLOUD, base::Value(value), nullptr);
      provider_.UpdateChromePolicy(policy_map);
      base::RunLoop().RunUntilIdle();
    } else {
      FAIL() << "Unknown test parameterization";
    }
  }

  bool IsMenuButtonVisible() {
    bool visible = tray_test_api_->IsBubbleViewVisible(
        ash::VIEW_ID_FEATURE_TILE_ACCESSIBILITY, true /* open_tray */);
    tray_test_api_->CloseBubble();
    return visible;
  }

  void CreateDetailedMenu() { tray_test_api_->ShowAccessibilityDetailedView(); }

  bool IsBubbleOpen() { return tray_test_api_->IsTrayBubbleOpen(); }

  void ClickVirtualKeyboardOnDetailMenu() {
    // Scroll the detailed view to show the virtual keyboard option.
    tray_test_api_->ScrollToShowView(
        tray_test_api_->GetAccessibilityDetailedView()
            ->scroll_view_for_testing(),
        ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD);
    tray_test_api_->ClickBubbleView(
        ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD);
  }

  bool IsVirtualKeyboardEnabledOnDetailMenu() const {
    return tray_test_api_->IsToggleOn(
        ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD_ENABLED);
  }

  // Disable animations so that tray icons hide immediately.
  ui::ScopedAnimationDurationScaleMode disable_animations_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  std::unique_ptr<ash::SystemTrayTestApi> tray_test_api_;
};

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, DISABLED_ShowMenu) {
  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is hidden.
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling spoken feedback changes the visibility of the menu.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling high contrast changes the visibility of the menu.
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling screen magnifier changes the visibility of the menu.
  SetScreenMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetScreenMagnifierEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling docked magnifier changes the visibility of the menu.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetDockedMagnifierEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling autoclick changes the visibility of the menu.
  EnableAutoclick(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling virtual keyboard changes the visibility of the menu.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling large mouse cursor changes the visibility of the menu.
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling Live Caption changes the visibility of the menu.
  EnableLiveCaption(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling mono audio changes the visibility of the menu.
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling caret highlight changes the visibility of the menu.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling highlight mouse cursor changes the visibility of the menu.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling highlight keyboard focus changes the visibility of the menu.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling sticky keys changes the visibility of the menu.
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling select-to-speak dragging changes the visibility of the menu.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling dictation changes the visibility of the menu.
  EnableDictation(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling switch access changes the visibility of the menu.
  EnableSwitchAccess(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Enabling all accessibility features.
  SetScreenMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetScreenMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetDockedMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_FALSE(IsMenuButtonVisible());
}

// Fails on linux-chromeos-dbg see crbug/1027919.
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest,
                       DISABLED_ShowMenuWithShowMenuOption) {
  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu is visible.
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling screen magnifier.
  SetScreenMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetScreenMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling docked magnifier.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetDockedMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling autoclick.
  EnableAutoclick(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling large mouse cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling Live Caption.
  EnableLiveCaption(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of the toggling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling select-to-speak.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling dictation.
  EnableDictation(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling switch access.
  EnableSwitchAccess(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // Enabling all accessibility features.
  SetScreenMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetScreenMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetDockedMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is invisible.
  EXPECT_FALSE(IsMenuButtonVisible());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, KeepMenuVisibilityOnLockScreen) {
  // Enables high contrast mode.
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());

  // Locks the screen.
  SessionControllerClientImpl::Get()->RequestLockScreen();

  // Resets the test API because UnifiedSystemTray is recreated.
  tray_test_api_ = ash::SystemTrayTestApi::Create();
  EXPECT_TRUE(IsMenuButtonVisible());

  // Disables high contrast mode.
  EnableHighContrast(false);

  // Confirms that the menu is still visible.
  EXPECT_TRUE(IsMenuButtonVisible());
}

// Verify that the accessibility system detailed menu remains open when an item
// is selected or deselected.
// Do not use a feature which requires an enable/disable confirmation dialog
// here, as the dialogs change focus and close the detail menu.
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, DetailMenuRemainsOpen) {
  CreateDetailedMenu();

  ClickVirtualKeyboardOnDetailMenu();
  EXPECT_TRUE(IsVirtualKeyboardEnabledOnDetailMenu());
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(IsBubbleOpen());

  ClickVirtualKeyboardOnDetailMenu();
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(IsBubbleOpen());
}

class TrayAccessibilityLoginTest : public TrayAccessibilityTest {
 public:
  TrayAccessibilityLoginTest() = default;

  TrayAccessibilityLoginTest(const TrayAccessibilityLoginTest&) = delete;
  TrayAccessibilityLoginTest& operator=(const TrayAccessibilityLoginTest&) =
      delete;

  ~TrayAccessibilityLoginTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TrayAccessibilityTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }
};

IN_PROC_BROWSER_TEST_P(TrayAccessibilityLoginTest,
                       ShowMenuWithShowOnLoginScreen) {
  EXPECT_FALSE(user_manager::UserManager::Get()->IsUserLoggedIn());

  // Confirms that the menu is visible.
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling screen magnifier.
  SetScreenMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetScreenMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling docked magnifier.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetDockedMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling select-to-speak.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling dictation.
  EnableDictation(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling switch access.
  EnableSwitchAccess(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling large mouse cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling Live Caption.
  EnableLiveCaption(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // Enabling all accessibility features.
  SetScreenMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableDictation(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSwitchAccess(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetScreenMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLiveCaption(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(IsMenuButtonVisible());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(IsMenuButtonVisible());
}

INSTANTIATE_TEST_SUITE_P(TrayAccessibilityTestInstance,
                         TrayAccessibilityTest,
                         testing::Values(PREF_SERVICE, POLICY));
INSTANTIATE_TEST_SUITE_P(TrayAccessibilityLoginTestInstance,
                         TrayAccessibilityLoginTest,
                         testing::Values(PREF_SERVICE, POLICY));

}  // namespace ash
