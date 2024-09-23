// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/extensions/input_method_event_router.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace {

using ::ash::extension_ime_util::GetInputMethodIDByEngineID;
using ::ash::input_method::InputMethodDescriptor;
using ::ash::input_method::InputMethodManager;

const char kLoginScreenUILanguage[] = "fr";
const char kInitialInputMethodOnLoginScreen[] = "xkb:us::eng";
const char kTestIMEID[] = "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest";
const char kTestIMEID2[] = "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest2";

const InputMethodDescriptor CreateInputMethodDescriptor(
    const std::string& engineId,
    const std::string& indicator,
    const std::string& layout,
    const std::vector<std::string>& language_codes) {
  return InputMethodDescriptor(GetInputMethodIDByEngineID(engineId), "",
                               indicator, {layout}, language_codes, true,
                               GURL(), GURL(),
                               /*handwriting_language=*/std::nullopt);
}

class ExtensionInputMethodApiTest : public extensions::ExtensionApiTest {
 public:
  ExtensionInputMethodApiTest() {}

  ExtensionInputMethodApiTest(const ExtensionInputMethodApiTest&) = delete;
  ExtensionInputMethodApiTest& operator=(const ExtensionInputMethodApiTest&) =
      delete;

  ~ExtensionInputMethodApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "ilanclmaeigfpnmdlgelmhkpkegdioip");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, Basic) {
  const std::vector<InputMethodDescriptor> xkb_input_method_descriptors = {
      CreateInputMethodDescriptor("xkb:us::eng", "US", "us",
                                  {"en", "en-US", "en-AU", "en-NZ"}),
      CreateInputMethodDescriptor("xkb:fr::fra", "FR", "fr(oss)",
                                  {"fr", "fr-FR"}),
      CreateInputMethodDescriptor("xkb:fr:bepo:fra", "FR", "fr(bepo)",
                                  {"fr", "fr-FR"}),
      CreateInputMethodDescriptor("xkb:be::fra", "BE", "fr(be)", {"fr"}),
      CreateInputMethodDescriptor("xkb:ca::fra", "CA", "ca", {"fr", "fr-CA"}),
      CreateInputMethodDescriptor("xkb:ch:fr::fra", "CH", "ch(fr)",
                                  {"fr", "fr-CH"}),
      CreateInputMethodDescriptor("xkb:ca:multix:fra", "CA", "ca(multix)",
                                  {"fr", "fr-CA"}),
  };

  // Initializes IMF for testing.
  InputMethodManager* manager = InputMethodManager::Get();
  manager->GetInputMethodUtil()->InitXkbInputMethodsForTesting(
      xkb_input_method_descriptors);

  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back(
      GetInputMethodIDByEngineID(kInitialInputMethodOnLoginScreen));
  manager->GetActiveIMEState()->EnableLoginLayouts(kLoginScreenUILanguage,
                                                   keyboard_layouts);

  ASSERT_TRUE(RunExtensionTest("input_method/basic")) << message_;
}

// TODO(crbug.com/41478266): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, DISABLED_Typing) {
  // Enable the test IME from the test extension.
  std::vector<std::string> extension_ime_ids = {
      "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest"};
  InputMethodManager::Get()->GetActiveIMEState()->SetEnabledExtensionImes(
      extension_ime_ids);

  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath("extensions/api_test/input_method/typing/"),
      base::FilePath("test_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  ASSERT_TRUE(RunExtensionTest("input_method/typing")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, ImeMenuActivation) {
  // Listener for IME menu initial state ready.
  ExtensionTestMessageListener config_listener("config_ready");
  // Listener for IME menu event ready.
  ExtensionTestMessageListener event_listener("event_ready");

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
  ExtensionTestMessageListener activated_listener("activated");
  ExtensionTestMessageListener menu_listener("get_menu_update");
  ExtensionTestMessageListener list_listenter("list_change");
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLanguageImeMenuActivated,
                                               true);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("input_method/ime_menu2")));

  std::vector<std::string> extension_ime_ids;
  extension_ime_ids.push_back(kTestIMEID);
  extension_ime_ids.push_back(kTestIMEID2);
  InputMethodManager::Get()->GetActiveIMEState()->SetEnabledExtensionImes(
      extension_ime_ids);
  ash::input_method::InputMethodDescriptors extension_imes;
  InputMethodManager::Get()->GetActiveIMEState()->GetInputMethodExtensions(
      &extension_imes);
  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kTestIMEID, false /* show_message */);
  ash::TextInputMethod* engine_handler =
      ash::IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);

  ASSERT_TRUE(activated_listener.WaitUntilSatisfied()) << message_;
  ASSERT_TRUE(menu_listener.WaitUntilSatisfied()) << message_;

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kTestIMEID2, false /* show_message */);
  ASSERT_TRUE(list_listenter.WaitUntilSatisfied()) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionInputMethodApiTest, Settings) {
  ASSERT_TRUE(RunExtensionTest("input_method/settings")) << message_;
}
