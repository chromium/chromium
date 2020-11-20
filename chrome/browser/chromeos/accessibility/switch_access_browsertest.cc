// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accessibility_controller.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/extension_load_waiter_one_shot.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"

namespace chromeos {

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
    manager->SetSwitchAccessEnabled(true);
    manager->SetSwitchAccessKeysForTest(select_key_codes, next_key_codes,
                                        previous_key_codes);

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

 protected:
  SwitchAccessTest() = default;
  ~SwitchAccessTest() override = default;

  void InjectFocusRingWatcher() {
    std::string script = R"JS(
      let focusRingState = {
        'primary': {
          'role': '',
          'name': ''
        },
        'preview': {
          'role': '',
          'name': ''
        }
      };
      let expectedType = '';
      let expectedRole = '';
      let expectedName = '';
      let successCallback = null;
      let transcript = [];

      function checkFocusRingState() {
        if (expectedType != '' &&
            focusRingState[expectedType].role == expectedRole &&
            focusRingState[expectedType].name == expectedName) {
          if (successCallback) {
            transcript.push(`Success type=${expectedType} ` +
                            `role=${expectedRole} name=${expectedName}`);
            successCallback();
            successCallback = null;
          }
        }
      }

      function waitForFocusRing(type, role, name, callback) {
        transcript.push(`Waiting for type=${type} role=${role} name=${name}`);
        expectedType = type;
        expectedRole = role;
        expectedName = name;
        successCallback = callback;
        checkFocusRingState();
      }

      FocusRingManager.setObserver((primary, preview) => {
        if (primary && primary instanceof BackButtonNode) {
          focusRingState['primary']['role'] = 'back';
          focusRingState['primary']['name'] = '';
        } else if (primary && primary.automationNode) {
          let node = primary.automationNode;
          focusRingState['primary']['role'] = node.role;
          focusRingState['primary']['name'] = node.name;
        } else {
          focusRingState['primary']['role'] = '';
          focusRingState['primary']['name'] = '';
        }
        if (preview && preview.automationNode) {
          let node = preview.automationNode;
          focusRingState['preview']['role'] = node.role;
          focusRingState['preview']['name'] = node.name;
        } else {
          focusRingState['preview']['role'] = '';
          focusRingState['preview']['name'] = '';
        }
        transcript.push(`Focus ring state: ${JSON.stringify(focusRingState)}`);
        checkFocusRingState();
      });
      window.domAutomationController.send('ready');

      setInterval(() => {
        console.error('Test still running. Transcript so far:\n' +
                      transcript.join('\n'));
      }, 5000);
      )JS";

    // Wait for the extension to load.
    base::RunLoop loop;
    ExtensionLoadWaiterOneShot waiter;
    waiter.WaitForExtension(extension_misc::kSwitchAccessExtensionId,
                            loop.QuitClosure());
    loop.Run();

    std::string result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            browser()->profile(), extension_misc::kSwitchAccessExtensionId,
            script);
    ASSERT_EQ("ready", result);
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

    std::string result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            browser()->profile(), extension_misc::kSwitchAccessExtensionId,
            script,
            extensions::browsertest_util::ScriptUserActivation::kDontActivate);
    ASSERT_EQ(result, "ok");
  }
};

// TODO(anastasi): Add a test for typing with the virtual keyboard.

IN_PROC_BROWSER_TEST_F(SwitchAccessTest, ConsumesKeyEvents) {
  EnableSwitchAccess({'1', 'A'} /* select */, {'2', 'B'} /* next */,
                     {'3', 'C'} /* previous */);
  // Load a webpage with a text box.
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;charset=utf-8,<input type=text id=in>"));

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
  ui_test_utils::NavigateToURL(browser(), GURL(R"HTML(data:text/html,
      <div aria-label=Top>
        <button autofocus>Northwest</button>
        <button>Northeast</button>
      </div>
      <div aria-label=Bottom>
        <button>Southwest</button>
        <button>Southeast</button>
      </div>
      )HTML"));

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
  WaitForFocusRing("primary", "genericContainer", "Top");
  WaitForFocusRing("preview", "button", "Northwest");

  // Navigate to the next group by pressing the next switch.
  // Now we should be focused on the Bottom container, with
  // Southwest as the preview.
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_2);
  WaitForFocusRing("primary", "genericContainer", "Bottom");
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

}  // namespace chromeos
