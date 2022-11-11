// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

constexpr char kTestSupportPath[] =
    "chrome/browser/resources/chromeos/accessibility/switch_access/"
    "test_support.js";

}

class SwitchAccessTest : public InProcessBrowserTest {
 public:
  void SendVirtualKeyPress(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, key, false, false, false, false)));
  }

  void EnableSwitchAccess(const std::set<int>& select_key_codes,
                          const std::set<int>& next_key_codes,
                          const std::set<int>& previous_key_codes) {
    AccessibilityManager* manager = AccessibilityManager::Get();

    extensions::ExtensionHostTestHelper host_helper(
        manager->profile(), extension_misc::kSwitchAccessExtensionId);
    manager->SetSwitchAccessEnabled(true);
    host_helper.WaitForHostCompletedFirstLoad();

    manager->SetSwitchAccessKeysForTest(
        select_key_codes,
        prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes);
    manager->SetSwitchAccessKeysForTest(
        next_key_codes, prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes);
    manager->SetSwitchAccessKeysForTest(
        previous_key_codes,
        prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes);

    EXPECT_TRUE(manager->IsSwitchAccessEnabled());

    InjectFocusRingWatcher();
  }

  std::string GetInputString() {
    std::string output;
    std::string script =
        "window.domAutomationController.send("
        "document.getElementById('in').value)";
    CHECK(ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetWebContentsAt(0), script, &output));
    return output;
  }

  void SetUpOnMainThread() override {
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->profile(), extension_misc::kSwitchAccessExtensionId);
  }

 protected:
  SwitchAccessTest() = default;
  ~SwitchAccessTest() override = default;

  void InjectFocusRingWatcher() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath source_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
    auto test_support_path = source_dir.AppendASCII(kTestSupportPath);
    std::string script;
    ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
        << test_support_path;

    std::string result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            browser()->profile(), extension_misc::kSwitchAccessExtensionId,
            script);
    ASSERT_EQ("ready", result);
  }

  // Run js snippet and wait for it to finish.
  void WaitForJS(const std::string& js_to_eval) {
    std::string result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            browser()->profile(), extension_misc::kSwitchAccessExtensionId,
            js_to_eval,
            extensions::browsertest_util::ScriptUserActivation::kDontActivate);
    ASSERT_EQ(result, "ok");
  }

  // Waits for a focus ring of type |type| (primary or preview) with a
  // role of |role| and a name of |name| to appear and then returns.
  void WaitForFocusRing(const std::string& type,
                        const std::string& role,
                        const std::string& name) {
    ASSERT_TRUE(type == "primary" || type == "preview");
    std::string script = base::StringPrintf(
        R"JS(
          waitForFocusRing("%s", "%s", "%s", () => {
            window.domAutomationController.send('ok');
          });
        )JS",
        type.c_str(), role.c_str(), name.c_str());
    WaitForJS(script);
  }

  // Performs default action on node with |name|.
  void DoDefault(const std::string& name) {
    std::string script = base::StringPrintf(
        R"JS(
          doDefault("%s", () => {
            window.domAutomationController.send('ok');
          });
        )JS",
        name.c_str());
    WaitForJS(script);
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

  // Clicks at location (in DIPs) |x| and |y| using point scanning.
  void PointScanClick(const int x, const int y) {
    std::string script = base::StringPrintf(
        R"JS(
          pointScanClick("%d", "%d", () => {
            window.domAutomationController.send('ok');
          });
        )JS",
        x, y);
    WaitForJS(script);
  }

  // Waits for an automation event of type |eventType| on an automation node
  // with a name of |name| to occur and then returns.
  void WaitForEventOnAutomationNode(const std::string& eventType,
                                    const std::string& name) {
    std::string script = base::StringPrintf(
        R"JS(
          waitForEventOnAutomationNode("%s", "%s", () => {
            window.domAutomationController.send('ok');
          });
        )JS",
        eventType.c_str(), name.c_str());
    WaitForJS(script);
  }

 private:
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
};

// Flaky. See https://crbug.com/1224254.
IN_PROC_BROWSER_TEST_F(SwitchAccessTest, DISABLED_ConsumesKeyEvents) {
  EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                     {'3', 'C'} /* previous */);
  // Load a webpage with a text box.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;charset=utf-8,<input type=text id=in>")));

  // Put focus in the text box.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_TAB);

  // Send a key event for a character consumed by Switch Access.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Check that the text field did not receive the character.
  EXPECT_STREQ("", GetInputString().c_str());

  // Send a key event for a character not consumed by Switch Access.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_X);

  // Check that the text field received the character.
  EXPECT_STREQ("x", GetInputString().c_str());
}

IN_PROC_BROWSER_TEST_F(SwitchAccessTest, NavigateGroupings) {
  EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                     {'3', 'C'} /* previous */);

  // Load a webpage with two groups of controls.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(R"HTML(data:text/html,
      <div role="group" aria-label="Top">
        <button autofocus>Northwest</button>
        <button>Northeast</button>
      </div>
      <div role="group" aria-label="Bottom">
        <button>Southwest</button>
        <button>Southeast</button>
      </div>
      )HTML")));

  // Wait for switch access to focus on the first button.
  WaitForFocusRing("primary", "button", "Northwest");

  // Go to the next element by pressing the next switch.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  WaitForFocusRing("primary", "button", "Northeast");

  // Next is the back button.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  WaitForFocusRing("primary", "back", "");

  // Press the select key to press the back button, which should focus
  // on the Top container, with Northwest as the preview.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);
  WaitForFocusRing("primary", "group", "Top");
  WaitForFocusRing("preview", "button", "Northwest");

  // Navigate to the next group by pressing the next switch.
  // Now we should be focused on the Bottom container, with
  // Southwest as the preview.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  WaitForFocusRing("primary", "group", "Bottom");
  WaitForFocusRing("preview", "button", "Southwest");

  // Press the select key to enter the container, which should focus
  // Southwest.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);
  WaitForFocusRing("primary", "button", "Southwest");

  // Go to the next element by pressing the next switch. That should
  // focus Southeast.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  WaitForFocusRing("primary", "button", "Southeast");
}

IN_PROC_BROWSER_TEST_F(SwitchAccessTest, NavigateButtonsInTextFieldMenu) {
  EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                     {'3', 'C'} /* previous */);

  // Load a webpage with a text box.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<input autofocus aria-label=MyTextField>")));

  // Wait for switch access to focus on the text field.
  WaitForFocusRing("primary", "textField", "MyTextField");

  // Send "select", which opens the switch access menu.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Wait for the switch access menu to appear and for focus to land on
  // the first item, the "keyboard" button.
  //
  // Note that we don't try to also call WaitForSwitchAccessMenuAndGetActions
  // here because by the time it returns, we may have already received the focus
  // ring for the menu and so the following WaitForFocusRing would fail / loop
  // forever.
  WaitForFocusRing("primary", "button", "Keyboard");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // The next menu item is the "dictation" button.
  WaitForFocusRing("primary", "button", "Dictation");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // The next menu item is the "point scanning" button.
  WaitForFocusRing("primary", "button", "Point scanning");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // The next menu item is the "settings" button.
  WaitForFocusRing("primary", "button", "Settings");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // Finally is the back button. Note that it has a role of "back" so we
  // can tell it's the special Switch Access back button.
  WaitForFocusRing("primary", "back", "");

  // Send "next".
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);

  // Wrap back around to the "keyboard" button.
  WaitForFocusRing("primary", "button", "Keyboard");
}

IN_PROC_BROWSER_TEST_F(SwitchAccessTest, TypeIntoVirtualKeyboard) {
  EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                     {'3', 'C'} /* previous */);

  // Load a webpage with a text box.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<input autofocus aria-label=MyTextField>")));

  // Wait for switch access to focus on the text field.
  WaitForFocusRing("primary", "textField", "MyTextField");

  // Send "select", which opens the switch access menu.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Wait for the switch access menu to appear and for focus to land on
  // the first item, the "keyboard" button.
  //
  // Note that we don't try to also call WaitForSwitchAccessMenuAndGetActions
  // here because by the time it returns, we may have already received the focus
  // ring for the menu and so the following WaitForFocusRing would fail / loop
  // forever.
  WaitForFocusRing("primary", "button", "Keyboard");

  // Send "select", which opens the virtual keyboard.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Finally, we should land on a keyboard key.
  WaitForFocusRing("primary", "keyboard", "");

  // Actually typing and verifying text field value should be covered by
  // js-based tests that have the ability to ask the text field for its value.
}

IN_PROC_BROWSER_TEST_F(SwitchAccessTest, PointScanClickWhenMouseEventsEnabled) {
  EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                     {'3', 'C'} /* previous */);

  // Load a webpage with a checkbox.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<input autofocus type=checkbox title='checkbox'"
           "style='width: 800px; height: 800px;'>")));

  // Wait for switch access to focus on the checkbox.
  WaitForFocusRing("primary", "checkBox", "checkbox");

  // Enable mouse events (within root window containing checkbox).
  EnableMouseEvents(600, 600);

  // Perform default action on the checkbox.
  DoDefault("checkbox");

  // Verify checkbox state changes.
  WaitForEventOnAutomationNode("checkedStateChanged", "checkbox");

  // Use Point Scan to click on the checkbox.
  PointScanClick(600, 600);

  // Verify checkbox state changes.
  WaitForEventOnAutomationNode("checkedStateChanged", "checkbox");

  // Verify mouse events are still enabled.
  ASSERT_TRUE(IsMouseEventsEnabled(600, 600));
}

IN_PROC_BROWSER_TEST_F(SwitchAccessTest,
                       PointScanClickWhenMouseEventsDisabled) {
  EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                     {'3', 'C'} /* previous */);

  // Load a webpage with a checkbox.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<input autofocus type=checkbox title='checkbox'"
           "style='width: 800px; height: 800px;'>")));

  // Wait for switch access to focus on the checkbox.
  WaitForFocusRing("primary", "checkBox", "checkbox");

  // Disable mouse events (within root window containing checkbox).
  DisableMouseEvents(600, 600);

  // Perform default action on the checkbox.
  DoDefault("checkbox");

  // Verify checkbox state changes.
  WaitForEventOnAutomationNode("checkedStateChanged", "checkbox");

  // Use Point Scan to click on the checkbox.
  PointScanClick(600, 600);

  // Verify checkbox state changes.
  WaitForEventOnAutomationNode("checkedStateChanged", "checkbox");

  // Verify mouse events are not enabled.
  ASSERT_FALSE(IsMouseEventsEnabled(600, 600));
}

}  // namespace ash
