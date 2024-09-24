// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_tree_host_lookup.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/ash/accessibility/switch_access_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"

namespace ash {

class SwitchAccessTest : public AccessibilityFeatureBrowserTest {
 protected:
  SwitchAccessTest() = default;
  ~SwitchAccessTest() override = default;
  SwitchAccessTest(const SwitchAccessTest&) = delete;
  SwitchAccessTest& operator=(const SwitchAccessTest&) = delete;

  void SetUpOnMainThread() override {
    switch_access_test_utils_ = std::make_unique<SwitchAccessTestUtils>(
        AccessibilityManager::Get()->profile());
  }

  void SendVirtualKeyPress(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, key, false, false, false, false)));
  }

  // Returns cursor client for root window at location (in DIPs) |x| and |y|.
  aura::client::CursorClient* GetCursorClient(const int x, const int y) {
    gfx::Point location_in_screen(x, y);
    const display::Display& display =
        display::Screen::GetScreen()->GetDisplayNearestPoint(
            location_in_screen);
    auto* host = GetWindowTreeHostForDisplay(display.id());
    CHECK(host);

    aura::Window* root_window = host->window();
    CHECK(root_window);

    return aura::client::GetCursorClient(root_window);
  }

  // Enables mouse events for root window at location (in DIPs) |x| and |y|.
  void EnableMouseEvents(const int x, const int y) {
    GetCursorClient(x, y)->EnableMouseEvents();
  }

  // Disables mouse events for root window at location (in DIPs) |x| and |y|.
  void DisableMouseEvents(const int x, const int y) {
    GetCursorClient(x, y)->DisableMouseEvents();
  }

  // Checks if mouse events are enabled for root window at location (in DIPs)
  // |x| and |y|.
  bool IsMouseEventsEnabled(const int x, const int y) {
    return GetCursorClient(x, y)->IsMouseEventsEnabled();
  }

  SwitchAccessTestUtils* utils() { return switch_access_test_utils_.get(); }

 private:
  std::unique_ptr<SwitchAccessTestUtils> switch_access_test_utils_;
};

// Flaky. See https://crbug.com/1224254.
IN_PROC_BROWSER_TEST_F(SwitchAccessTest, DISABLED_ConsumesKeyEvents) {
  utils()->EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                              {'3', 'C'} /* previous */);
  AutomationTestUtils test_utils(extension_misc::kSwitchAccessExtensionId);
  test_utils.SetUpTestSupport();

  // Load a webpage with a text box.
  NavigateToUrl(GURL(
      "data:text/html;charset=utf-8,<input type='text' class='sa-input'>"));

  // Put focus in the text box.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_TAB);

  // Send a key event for a character consumed by Switch Access.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Check that the text field did not receive the character.
  EXPECT_STREQ("", test_utils.GetValueForNodeWithClassName("sa_input").c_str());

  // Send a key event for a character not consumed by Switch Access.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_X);

  // Check that the text field received the character.
  EXPECT_STREQ("x",
               test_utils.GetValueForNodeWithClassName("sa_input").c_str());
}

IN_PROC_BROWSER_TEST_F(SwitchAccessTest, NavigateGroupings) {
  utils()->EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                              {'3', 'C'} /* previous */);

  // Load a webpage with two groups of controls.
  NavigateToUrl(GURL(R"HTML(data:text/html,
      <div role="group" aria-label="Top">
        <button autofocus>Northwest</button>
        <button>Northeast</button>
      </div>
      <div role="group" aria-label="Bottom">
        <button>Southwest</button>
        <button>Southeast</button>
      </div>
      )HTML"));

  // Wait for switch access to focus on the first button.
  utils()->WaitForFocusRing("primary", "button", "Northwest");

  // Go to the next element by pressing the next switch.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  utils()->WaitForFocusRing("primary", "button", "Northeast");

  // Next is the back button.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  utils()->WaitForFocusRing("primary", "back", "");

  // Press the select key to press the back button, which should focus
  // on the Top container, with Northwest as the preview.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);
  utils()->WaitForFocusRing("primary", "group", "Top");
  utils()->WaitForFocusRing("preview", "button", "Northwest");

  // Navigate to the next group by pressing the next switch.
  // Now we should be focused on the Bottom container, with
  // Southwest as the preview.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  utils()->WaitForFocusRing("primary", "group", "Bottom");
  utils()->WaitForFocusRing("preview", "button", "Southwest");

  // Press the select key to enter the container, which should focus
  // Southwest.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);
  utils()->WaitForFocusRing("primary", "button", "Southwest");

  // Go to the next element by pressing the next switch. That should
  // focus Southeast.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  utils()->WaitForFocusRing("primary", "button", "Southeast");
}

IN_PROC_BROWSER_TEST_F(SwitchAccessTest, NavigateButtonsInTextFieldMenu) {
  utils()->EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                              {'3', 'C'} /* previous */);

  // Load a webpage with a text box.
  NavigateToUrl(
      GURL("data:text/html,<input autofocus aria-label=MyTextField>"));

  // Wait for switch access to focus on the text field.
  utils()->WaitForFocusRing("primary", "textField", "MyTextField");

  // Send "select", which opens the switch access menu.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Wait for the switch access menu to appear and for focus to land on
  // the first item, the "keyboard" button.
  //
  // Note that we don't try to also call WaitForSwitchAccessMenuAndGetActions
  // here because by the time it returns, we may have already received the focus
  // ring for the menu and so the following WaitForFocusRing would fail / loop
  // forever.
  utils()->WaitForFocusRing("primary", "button", "Keyboard");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // The next menu item is the "dictation" button.
  utils()->WaitForFocusRing("primary", "button", "Dictation");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // The next menu item is the "enter" button.
  utils()->WaitForFocusRing("primary", "button", "Drill down");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // The next menu item is the "point scanning" button.
  utils()->WaitForFocusRing("primary", "button", "Point scanning");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // The next menu item is the "settings" button.
  utils()->WaitForFocusRing("primary", "button", "Settings");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // Finally is the back button. Note that it has a role of "back" so we
  // can tell it's the special Switch Access back button.
  utils()->WaitForFocusRing("primary", "back", "");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // Wrap back around to the "keyboard" button.
  utils()->WaitForFocusRing("primary", "button", "Keyboard");
}

// TODO(crbug.com/40926594): Enable after fixing flakiness.
IN_PROC_BROWSER_TEST_F(SwitchAccessTest, DISABLED_TypeIntoVirtualKeyboard) {
  utils()->EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                              {'3', 'C'} /* previous */);

  // Load a webpage with a text box.
  NavigateToUrl(
      GURL("data:text/html,<input autofocus aria-label=MyTextField>"));

  // Wait for switch access to focus on the text field.
  utils()->WaitForFocusRing("primary", "textField", "MyTextField");

  // Send "select", which opens the switch access menu.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Wait for the switch access menu to appear and for focus to land on
  // the first item, the "keyboard" button.
  //
  // Note that we don't try to also call WaitForSwitchAccessMenuAndGetActions
  // here because by the time it returns, we may have already received the focus
  // ring for the menu and so the following WaitForFocusRing would fail / loop
  // forever.
  utils()->WaitForFocusRing("primary", "button", "Keyboard");

  // Send "select", which opens the virtual keyboard.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Finally, we should land on a keyboard key.
  utils()->WaitForFocusRing("primary", "keyboard", "");

  // Actually typing and verifying text field value should be covered by
  // js-based tests that have the ability to ask the text field for its value.
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PointScanClickWhenMouseEventsEnabled \
  DISABLED_PointScanClickWhenMouseEventsEnabled
#else
#define MAYBE_PointScanClickWhenMouseEventsEnabled \
  PointScanClickWhenMouseEventsEnabled
#endif
IN_PROC_BROWSER_TEST_F(SwitchAccessTest,
                       MAYBE_PointScanClickWhenMouseEventsEnabled) {
  utils()->EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                              {'3', 'C'} /* previous */);

  // Load a webpage with a checkbox.
  NavigateToUrl(
      GURL("data:text/html,<input autofocus type=checkbox title='checkbox'"
           "style='width: 800px; height: 800px;'>"));

  // Wait for switch access to focus on the checkbox.
  utils()->WaitForFocusRing("primary", "checkBox", "checkbox");

  // Enable mouse events (within root window containing checkbox).
  EnableMouseEvents(600, 600);

  // Perform default action on the checkbox.
  utils()->DoDefault("checkbox");

  // Verify checkbox state changes.
  utils()->WaitForEventOnAutomationNode("checkedStateChanged", "checkbox");

  // Use Point Scan to click on the checkbox.
  utils()->PointScanClick(600, 600);

  // Verify checkbox state changes.
  utils()->WaitForEventOnAutomationNode("checkedStateChanged", "checkbox");

  // Verify mouse events are still enabled.
  ASSERT_TRUE(IsMouseEventsEnabled(600, 600));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PointScanClickWhenMouseEventsDisabled \
  DISABLED_PointScanClickWhenMouseEventsDisabled
#else
#define MAYBE_PointScanClickWhenMouseEventsDisabled \
  PointScanClickWhenMouseEventsDisabled
#endif
IN_PROC_BROWSER_TEST_F(SwitchAccessTest,
                       MAYBE_PointScanClickWhenMouseEventsDisabled) {
  utils()->EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                              {'3', 'C'} /* previous */);

  // Load a webpage with a checkbox.
  NavigateToUrl(
      GURL("data:text/html,<input autofocus type=checkbox title='checkbox'"
           "style='width: 800px; height: 800px;'>"));

  // Wait for switch access to focus on the checkbox.
  utils()->WaitForFocusRing("primary", "checkBox", "checkbox");

  // Disable mouse events (within root window containing checkbox).
  DisableMouseEvents(600, 600);

  // Perform default action on the checkbox.
  utils()->DoDefault("checkbox");

  // Verify checkbox state changes.
  utils()->WaitForEventOnAutomationNode("checkedStateChanged", "checkbox");

  // Use Point Scan to click on the checkbox.
  utils()->PointScanClick(600, 600);

  // Verify checkbox state changes.
  utils()->WaitForEventOnAutomationNode("checkedStateChanged", "checkbox");

  // Verify mouse events are not enabled.
  ASSERT_FALSE(IsMouseEventsEnabled(600, 600));
}

}  // namespace ash
