// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/global_keyboard_shortcuts_mac.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#import "ui/events/test/cocoa_test_event_utils.h"

using cocoa_test_event_utils::SynthesizeKeyEvent;

class GlobalKeyboardShortcutsTest : public InProcessBrowserTest {
 public:
  GlobalKeyboardShortcutsTest() = default;
  void SetUpOnMainThread() override {
    // Many hotkeys are defined by the main menu. The value of these hotkeys
    // depends on the focused window. We must focus the browser window. This is
    // also why this test must be an interactive_ui_test rather than a browser
    // test.
    ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
        browser()->window()->GetNativeWindow()));
  }
};

namespace {

void SendEvent(NSEvent* ns_event) {
  [NSApp sendEvent:ns_event];
}

}  // namespace

IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, SwitchTabsMac) {
  NSWindow* ns_window =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Set up window with 2 tabs.
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_TRUE(tab_strip->IsTabSelected(1));

  // Ctrl+Tab goes to the next tab, which loops back to the first tab.
  SendEvent(SynthesizeKeyEvent(ns_window, true, ui::VKEY_TAB,
                               NSEventModifierFlagControl));
  EXPECT_TRUE(tab_strip->IsTabSelected(0));

  // Cmd+2 goes to the second tab.
  SendEvent(SynthesizeKeyEvent(ns_window, true, ui::VKEY_2,
                               NSEventModifierFlagCommand));

  // Wait for the tab to activate to be selected.
  while (true) {
    if (tab_strip->IsTabSelected(1))
      break;
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(tab_strip->IsTabSelected(1));

  // Cmd+{ goes to the previous tab.
  SendEvent(SynthesizeKeyEvent(
      ns_window, true, ui::VKEY_OEM_4,
      NSEventModifierFlagShift | NSEventModifierFlagCommand));
  EXPECT_TRUE(tab_strip->IsTabSelected(0));
}

// Test that cmd + left arrow can be used for history navigation.
IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, HistoryNavigation) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  NSWindow* ns_window =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();

  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  ASSERT_NE(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);

  // Navigate the active tab to a dummy URL.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url,
      /*number_of_navigations=*/1);
  ASSERT_EQ(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);

  // Focus the WebContents.
  tab_strip->GetActiveWebContents()->Focus();

  // Cmd + left arrow performs history navigation, but only after the
  // WebContents chooses not to handle the event.
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_LEFT,
                               NSEventModifierFlagCommand));
  while (true) {
    if (tab_strip->GetActiveWebContents()->GetLastCommittedURL() != test_url)
      break;
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_NE(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);
}

// Test that common hotkeys for editing the omnibox work.
IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, CopyPasteOmnibox) {
  BrowserWindow* window = browser()->window();
  ASSERT_TRUE(window);
  LocationBar* location_bar = window->GetLocationBar();
  ASSERT_TRUE(location_bar);
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);

  NSWindow* ns_window =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();

  // Cmd+L focuses the omnibox and selects all the text.
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_L,
                               NSEventModifierFlagCommand));

  // The first typed letter overrides the existing contents.
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_A,
                               /*flags=*/0));
  // The second typed letter just appends.
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_B,
                               /*flags=*/0));
  ASSERT_EQ(omnibox_view->GetText(), u"ab");

  // Cmd+A selects the contents.
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_A,
                               NSEventModifierFlagCommand));

  // Cmd+C copies the contents.
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_C,
                               NSEventModifierFlagCommand));

  // The first typed letter overrides the existing contents.
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_C,
                               /*flags=*/0));
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_D,
                               /*flags=*/0));
  ASSERT_EQ(omnibox_view->GetText(), u"cd");

  // Cmd + left arrow moves to the beginning. It should not perform history
  // navigation because the firstResponder is not a WebContents..
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_LEFT,
                               NSEventModifierFlagCommand));

  // Cmd+V pastes the contents.
  SendEvent(SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_V,
                               NSEventModifierFlagCommand));
  EXPECT_EQ(omnibox_view->GetText(), u"abcd");
}

// Tests that the shortcut to reopen a previous tab works.
IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, ReopenPreviousTab) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Set up window with 2 tabs.
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip->count());

  // Navigate the active tab to a dummy URL.
  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url,
      /*number_of_navigations=*/1);
  ASSERT_EQ(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);

  // Close a tab.
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      browser()->window()->GetNativeWindow(), ui::VKEY_W, false, false, false,
      true));
  EXPECT_EQ(1, tab_strip->count());
  ASSERT_NE(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);

  // Reopen a tab.
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      browser()->window()->GetNativeWindow(), ui::VKEY_T, false, true, false,
      true));
  EXPECT_EQ(2, tab_strip->count());
  ASSERT_EQ(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);
}

// Checks that manually configured hotkeys in the main menu have higher priority
// than unconfigurable hotkeys not present in the main menu.
IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, MenuCommandPriority) {
  NSWindow* ns_window =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Set up window with 4 tabs.
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  EXPECT_EQ(4, tab_strip->count());
  EXPECT_TRUE(tab_strip->IsTabSelected(3));

  // Use the cmd-2 hotkey to switch to the second tab.
  SendEvent(SynthesizeKeyEvent(ns_window, true, ui::VKEY_2,
                               NSEventModifierFlagCommand));
  EXPECT_TRUE(tab_strip->IsTabSelected(1));

  // Change the "Select Next Tab" menu item's key equivalent to be cmd-2, to
  // simulate what would happen if there was a user key equivalent for it. Note
  // that there is a readonly "userKeyEquivalent" property on NSMenuItem, but
  // this code can't modify it.
  NSMenu* main_menu = [NSApp mainMenu];
  ASSERT_NE(nil, main_menu);
  NSMenuItem* tab_menu = [main_menu itemWithTitle:@"Tab"];
  ASSERT_NE(nil, tab_menu);
  ASSERT_TRUE(tab_menu.hasSubmenu);
  NSMenuItem* next_item = [tab_menu.submenu itemWithTag:IDC_SELECT_NEXT_TAB];
  ASSERT_NE(nil, next_item);
  [next_item setKeyEquivalent:@"2"];
  [next_item setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
  ASSERT_TRUE([next_item isEnabled]);

  // Send cmd-2 again, and ensure the tab switches.
  SendEvent(SynthesizeKeyEvent(ns_window, true, ui::VKEY_2,
                               NSEventModifierFlagCommand));
  EXPECT_TRUE(tab_strip->IsTabSelected(2));
  SendEvent(SynthesizeKeyEvent(ns_window, true, ui::VKEY_2,
                               NSEventModifierFlagCommand));
  EXPECT_TRUE(tab_strip->IsTabSelected(3));
}
