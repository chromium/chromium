// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/accessibility/sticky_keys/sticky_keys_overlay.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/ash/accessibility/select_to_speak_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

class StickyKeysBrowserTest : public AccessibilityFeatureBrowserTest {
 protected:
  StickyKeysBrowserTest() = default;

  StickyKeysBrowserTest(const StickyKeysBrowserTest&) = delete;
  StickyKeysBrowserTest& operator=(const StickyKeysBrowserTest&) = delete;

  ~StickyKeysBrowserTest() override = default;

  void SetUpOnMainThread() override {
    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);
    Profile* profile = ProfileManager::GetActiveUserProfile();

    AccessibilityFeatureBrowserTest::SetUpOnMainThread();
    // Load Select to Speak so we have a Javascript context with access to the
    // Automation API to inject AutomationTestUtils. Select to Speak doesn't do
    // any work unless it is triggered, so this does not impact the test.
    sts_test_utils::TurnOnSelectToSpeakForTest(profile);
    utils_ = std::make_unique<AutomationTestUtils>(
        extension_misc::kSelectToSpeakExtensionId);
    utils_->SetUpTestSupport();
  }

  void SetStickyKeysEnabled(bool enabled) {
    AccessibilityManager::Get()->EnableStickyKeys(enabled);
    // Spin the message loop to ensure ash sees the change.
    base::RunLoop().RunUntilIdle();
  }

  bool IsSystemTrayBubbleOpen() {
    return Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->unified_system_tray()
        ->IsBubbleShown();
  }

  void CloseSystemTrayBubble() {
    Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->unified_system_tray()
        ->CloseBubble();
  }

  void SendKeyPress(ui::KeyboardCode key) {
    ui::test::EmulateFullKeyPressReleaseSequence(
        generator_.get(), key,
        /*control=*/false, /*shift=*/false, /*alt=*/false, /*command=*/false);
  }

  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<AutomationTestUtils> utils_;
};

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, OpenTrayMenu) {
  SetStickyKeysEnabled(true);

  // Open system tray bubble with shortcut.
  SendKeyPress(ui::VKEY_MENU);  // alt key.
  SendKeyPress(ui::VKEY_SHIFT);
  SendKeyPress(ui::VKEY_S);
  EXPECT_TRUE(IsSystemTrayBubbleOpen());

  // Hide system bubble.
  CloseSystemTrayBubble();
  EXPECT_FALSE(IsSystemTrayBubbleOpen());

  // Pressing S again should not reopen the bubble.
  SendKeyPress(ui::VKEY_S);
  EXPECT_FALSE(IsSystemTrayBubbleOpen());

  // With sticky keys disabled, we will fail to perform the shortcut.
  SetStickyKeysEnabled(false);
  SendKeyPress(ui::VKEY_MENU);  // alt key.
  SendKeyPress(ui::VKEY_SHIFT);
  SendKeyPress(ui::VKEY_S);
  EXPECT_FALSE(IsSystemTrayBubbleOpen());
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, OpenNewTabs) {
  // Lock the modifier key.
  SetStickyKeysEnabled(true);
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_CONTROL);

  // In the locked state, pressing 't' should open a new tab each time.
  for (int tab_count = 1; tab_count < 5; ++tab_count) {
    SendKeyPress(ui::VKEY_T);
    utils_->WaitForNumTabsWithRegexName(tab_count, "/New Tab*/");
  }

  // Unlock the modifier key and shortcut should no longer activate.
  // Instead, the omnibox is populated with the letter 't'.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  utils_->WaitForNodeWithClassNameAndValue("OmniboxViewViews", "t");

  // Shortcut should not work after disabling sticky keys.
  // Instead, another 't' is typed in the omnibox.
  SetStickyKeysEnabled(false);
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  utils_->WaitForNodeWithClassNameAndValue("OmniboxViewViews", "tt");
}

// Flaky. https://crbug.com/331433886.
IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, DISABLED_CtrlClickHomeButton) {
  // Show home page button.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  int tab_count = 1;
  EXPECT_EQ(tab_count, tab_strip_model->count());

  // Test sticky keys with modified mouse click action.
  SetStickyKeysEnabled(true);
  SendKeyPress(ui::VKEY_CONTROL);
  ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  EXPECT_EQ(++tab_count, tab_strip_model->count());
  ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  EXPECT_EQ(tab_count, tab_strip_model->count());

  // Test locked modifier key with mouse click.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_CONTROL);
  for (; tab_count < 5; ++tab_count) {
    EXPECT_EQ(tab_count, tab_strip_model->count());
    ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  }
  SendKeyPress(ui::VKEY_CONTROL);
  ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  EXPECT_EQ(tab_count, tab_strip_model->count());

  // Test disabling sticky keys prevent modified mouse click.
  SetStickyKeysEnabled(false);
  SendKeyPress(ui::VKEY_CONTROL);
  ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  EXPECT_EQ(tab_count, tab_strip_model->count());
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, SearchLeftOmnibox) {
  SetStickyKeysEnabled(true);

  // Give omnibox focus by opening a new tab with ctrl+t.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);

  utils_->WaitForNumTabsWithRegexName(1, "/New Tab*/");

  // Type 'foo'.
  SendKeyPress(ui::VKEY_F);
  SendKeyPress(ui::VKEY_O);
  SendKeyPress(ui::VKEY_O);

  utils_->WaitForNodeWithClassNameAndValue("OmniboxViewViews", "foo");

  // Hit Home by sequencing Search (left Windows) and Left (arrow).
  SendKeyPress(ui::VKEY_LWIN);
  SendKeyPress(ui::VKEY_LEFT);

  // Verify caret moved to the beginning by typing something else, this
  // should appear before "foo" in the omnibox.
  SendKeyPress(ui::VKEY_B);
  SendKeyPress(ui::VKEY_A);
  SendKeyPress(ui::VKEY_R);

  utils_->WaitForNodeWithClassNameAndValue("OmniboxViewViews", "barfoo");
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, OverlayShown) {
  const ui::KeyboardCode modifier_keys[] = {ui::VKEY_CONTROL, ui::VKEY_SHIFT,
                                            ui::VKEY_MENU, ui::VKEY_COMMAND};

  // Overlay should not be visible if sticky keys is not enabled.
  StickyKeysController* controller = Shell::Get()->sticky_keys_controller();
  EXPECT_FALSE(controller->GetOverlayForTest());
  for (auto key_code : modifier_keys) {
    SendKeyPress(key_code);
    EXPECT_FALSE(controller->GetOverlayForTest());
  }

  // Cycle through the modifier keys and make sure each gets shown.
  SetStickyKeysEnabled(true);
  StickyKeysOverlay* sticky_keys_overlay = controller->GetOverlayForTest();
  for (auto key_code : modifier_keys) {
    SendKeyPress(key_code);
    EXPECT_TRUE(sticky_keys_overlay->is_visible());
    SendKeyPress(key_code);
    EXPECT_TRUE(sticky_keys_overlay->is_visible());
    SendKeyPress(key_code);
    EXPECT_FALSE(sticky_keys_overlay->is_visible());
  }

  // Disabling sticky keys should hide the overlay.
  SendKeyPress(ui::VKEY_CONTROL);
  EXPECT_TRUE(sticky_keys_overlay->is_visible());
  SetStickyKeysEnabled(false);
  EXPECT_FALSE(controller->GetOverlayForTest());
  for (auto key_code : modifier_keys) {
    SendKeyPress(key_code);
    EXPECT_FALSE(controller->GetOverlayForTest());
  }
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, OpenIncognitoWindow) {
  SetStickyKeysEnabled(true);
  StickyKeysOverlay* overlay =
      Shell::Get()->sticky_keys_controller()->GetOverlayForTest();
  ASSERT_TRUE(overlay);

  // Overlay is shown on first modifier key press.
  EXPECT_FALSE(overlay->is_visible());
  SendKeyPress(ui::VKEY_SHIFT);
  EXPECT_TRUE(overlay->is_visible());
  SendKeyPress(ui::VKEY_CONTROL);
  EXPECT_TRUE(overlay->is_visible());
  SendKeyPress(ui::VKEY_N);
  EXPECT_FALSE(overlay->is_visible());

  utils_->WaitForNumTabsWithRegexName(1, "/New Incognito Tab*/");
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, CyclesWindows) {
  SetStickyKeysEnabled(true);

  // Ensure there is a normal browser window open with ctrl+t.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  int expected_tabs = 1;
  utils_->WaitForNumTabsWithRegexName(expected_tabs, "/New Tab*/");

  // Open an incognito browser.
  SendKeyPress(ui::VKEY_SHIFT);
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_N);
  utils_->WaitForNumTabsWithRegexName(1, "/New Incognito Tab*/");

  // Ctrl+t opens another incognito tab because the incognito window is focused.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  utils_->WaitForNumTabsWithRegexName(2, "/New Incognito Tab*/");

  // Cycle between windows.
  SendKeyPress(ui::VKEY_MENU);  // alt key.
  SendKeyPress(ui::VKEY_TAB);

  // Ctrl+t opens another non-incognito tab now, because the normal browser
  // window is focused.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  expected_tabs++;
  utils_->WaitForNumTabsWithRegexName(expected_tabs, "/New Tab*/");
}

}  // namespace ash
