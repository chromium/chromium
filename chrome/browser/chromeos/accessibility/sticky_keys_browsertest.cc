// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/sticky_keys/sticky_keys_overlay.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

class StickyKeysBrowserTest : public InProcessBrowserTest {
 protected:
  StickyKeysBrowserTest() = default;
  ~StickyKeysBrowserTest() override = default;

  void SetStickyKeysEnabled(bool enabled) {
    AccessibilityManager::Get()->EnableStickyKeys(enabled);
    // Spin the message loop to ensure ash sees the change.
    base::RunLoop().RunUntilIdle();
  }

  bool IsSystemTrayBubbleOpen() {
    return ash::Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->unified_system_tray()
        ->IsBubbleShown();
  }

  void CloseSystemTrayBubble() {
    ash::Shell::Get()
        ->GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->unified_system_tray()
        ->CloseBubble();
  }

  void SendKeyPress(ui::KeyboardCode key) {
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key, false, false,
                                                false, false));
  }

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(StickyKeysBrowserTest);
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
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  int tab_count = 1;
  for (; tab_count < 5; ++tab_count) {
    EXPECT_EQ(tab_count, tab_strip_model->count());
    SendKeyPress(ui::VKEY_T);
  }

  // Unlock the modifier key and shortcut should no longer activate.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  EXPECT_EQ(tab_count, tab_strip_model->count());

  // Shortcut should not work after disabling sticky keys.
  SetStickyKeysEnabled(false);
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  EXPECT_EQ(tab_count, tab_strip_model->count());
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, CtrlClickHomeButton) {
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

  OmniboxView* omnibox =
      browser()->window()->GetLocationBar()->GetOmniboxView();

  // Give the omnibox focus.
  omnibox->SetFocus(/*is_user_initiated=*/true);

  // Make sure that the AppList is not erroneously displayed and the omnibox
  // doesn't lose focus.
  EXPECT_TRUE(omnibox->GetNativeView()->HasFocus());

  // Type 'foo'.
  SendKeyPress(ui::VKEY_F);
  SendKeyPress(ui::VKEY_O);
  SendKeyPress(ui::VKEY_O);

  // Verify the location of the caret.
  size_t start, end;
  omnibox->GetSelectionBounds(&start, &end);
  ASSERT_EQ(3U, start);
  ASSERT_EQ(3U, end);

  EXPECT_TRUE(omnibox->GetNativeView()->HasFocus());

  // Hit Home by sequencing Search (left Windows) and Left (arrow).
  SendKeyPress(ui::VKEY_LWIN);
  SendKeyPress(ui::VKEY_LEFT);

  EXPECT_TRUE(omnibox->GetNativeView()->HasFocus());

  // Verify caret moved to the beginning.
  omnibox->GetSelectionBounds(&start, &end);
  ASSERT_EQ(0U, start);
  ASSERT_EQ(0U, end);
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, OverlayShown) {
  const ui::KeyboardCode modifier_keys[] = {ui::VKEY_CONTROL, ui::VKEY_SHIFT,
                                            ui::VKEY_MENU, ui::VKEY_COMMAND};

  // Overlay should not be visible if sticky keys is not enabled.
  ash::StickyKeysController* controller =
      ash::Shell::Get()->sticky_keys_controller();
  EXPECT_FALSE(controller->GetOverlayForTest());
  for (auto key_code : modifier_keys) {
    SendKeyPress(key_code);
    EXPECT_FALSE(controller->GetOverlayForTest());
  }

  // Cycle through the modifier keys and make sure each gets shown.
  SetStickyKeysEnabled(true);
  ash::StickyKeysOverlay* sticky_keys_overlay = controller->GetOverlayForTest();
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

}  // namespace chromeos
