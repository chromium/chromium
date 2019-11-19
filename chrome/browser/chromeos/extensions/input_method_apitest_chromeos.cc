// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/chromeos/input_method_whitelist.h"
#include "ui/base/ime/ime_bridge.h"

using chromeos::input_method::InputMethodManager;

namespace {

const char kLoginScreenUILanguage[] = "fr";
const char kInitialInputMethodOnLoginScreen[] = "xkb:us::eng";
const char kBackgroundReady[] = "ready";
const char kTestIMEID[] = "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest";
const char kTestIMEID2[] = "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest2";

// Class that listens for the JS message.
class TestListener : public content::NotificationObserver {
 public:
  TestListener() {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  ~TestListener() override {}

  // Implements the content::NotificationObserver interface.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    const std::string& message =
        content::Details<std::pair<std::string, bool*>>(details).ptr()->first;
    if (message == kBackgroundReady) {
      // Initializes IMF for testing when receives ready message from
      // background.
      InputMethodManager* manager = InputMethodManager::Get();
      manager->GetInputMethodUtil()->InitXkbInputMethodsForTesting(
          *chromeos::input_method::InputMethodWhitelist()
               .GetSupportedInputMethods());

      std::vector<std::string> keyboard_layouts;
      keyboard_layouts.push_back(
          chromeos::extension_ime_util::GetInputMethodIDByEngineID(
              kInitialInputMethodOnLoginScreen));
      manager->GetActiveIMEState()->EnableLoginLayouts(kLoginScreenUILanguage,
                                                       keyboard_layouts);
    }
  }

 private:
  content::NotificationRegistrar registrar_;
};

class ExtensionInputMethodApiTest : public extensions::ExtensionApiTest {
 public:
  ExtensionInputMethodApiTest() {}
  ~ExtensionInputMethodApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ilanclmaeigfpnmdlgelmhkpkegdioip");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInputMethodApiTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, Basic) {
  // Listener for extension's background ready.
  TestListener listener;

  ASSERT_TRUE(RunExtensionTest("input_method/basic")) << message_;
}

// TODO(https://crbug.com/997888): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, DISABLED_Typing) {
  // Enable the test IME from the test extension.
  std::vector<std::string> extension_ime_ids = {
      "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest"};
  InputMethodManager::Get()->GetActiveIMEState()->SetEnabledExtensionImes(
      &extension_ime_ids);

  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath("extensions/api_test/input_method/typing/"),
      base::FilePath("test_page.html"));
  ui_test_utils::NavigateToURL(browser(), test_url);

  ASSERT_TRUE(RunExtensionTest("input_method/typing")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, ImeMenuActivation) {
  // Listener for IME menu initial state ready.
  ExtensionTestMessageListener config_listener("config_ready", false);
  // Listener for IME menu event ready.
  ExtensionTestMessageListener event_listener("event_ready", false);

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLanguageImeMenuActivated,
                                               true);

  // Test the initial state and add listener for IME menu activation change.
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("input_method/ime_menu")));
  ASSERT_TRUE(config_listener.WaitUntilSatisfied()) << message_;

  // Trigger chrome.inputMethodPrivate.onImeMenuActivationChanged() event.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLanguageImeMenuActivated,
                                               false);
  // Test that the extension gets the IME activation change event properly.
  ASSERT_TRUE(event_listener.WaitUntilSatisfied()) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, ImeMenuAPITest) {
  ExtensionTestMessageListener activated_listener("activated", false);
  ExtensionTestMessageListener menu_listener("get_menu_update", false);
  ExtensionTestMessageListener item_activated_listenter("get_menu_activated",
                                                        false);
  ExtensionTestMessageListener list_listenter("list_change", false);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLanguageImeMenuActivated,
                                               true);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("input_method/ime_menu2")));

  std::vector<std::string> extension_ime_ids;
  extension_ime_ids.push_back(kTestIMEID);
  extension_ime_ids.push_back(kTestIMEID2);
  InputMethodManager::Get()->GetActiveIMEState()->SetEnabledExtensionImes(
      &extension_ime_ids);
  chromeos::input_method::InputMethodDescriptors extension_imes;
  InputMethodManager::Get()->GetActiveIMEState()->GetInputMethodExtensions(
      &extension_imes);
  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kTestIMEID, false /* show_message */);
  ui::IMEEngineHandlerInterface* engine_handler =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);
  engine_handler->Enable("test");

  ASSERT_TRUE(activated_listener.WaitUntilSatisfied()) << message_;
  ASSERT_TRUE(menu_listener.WaitUntilSatisfied()) << message_;
  ASSERT_TRUE(item_activated_listenter.WaitUntilSatisfied()) << message_;

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kTestIMEID2, false /* show_message */);
  engine_handler->Enable("test2");
  ASSERT_TRUE(list_listenter.WaitUntilSatisfied()) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, Settings) {
  ASSERT_TRUE(RunExtensionTest("input_method/settings")) << message_;
}
