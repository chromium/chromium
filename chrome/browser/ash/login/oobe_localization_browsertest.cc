// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace base {
class TaskRunner;
}

namespace ash {
namespace {

// OOBE constants.
const char kLanguageSelect[] = "languageSelect";
const char kKeyboardSelect[] = "keyboardSelect";

std::string GetGetSelectStatement(const std::string& selectId) {
  return "document.getElementById('connect').$." + selectId + ".$.select";
}

const char kUSLayout[] = "xkb:us::eng";

class LanguageListWaiter : public WelcomeScreen::Observer {
 public:
  LanguageListWaiter()
      : welcome_screen_(WizardController::default_controller()
                            ->GetScreen<WelcomeScreen>()) {
    welcome_screen_->AddObserver(this);
    CheckLanguageList();
  }

  ~LanguageListWaiter() override { welcome_screen_->RemoveObserver(this); }

  // WelcomeScreen::Observer implementation:
  void OnLanguageListReloaded() override { CheckLanguageList(); }

  // Run the loop until the list is ready or the default Run() timeout expires.
  void RunUntilLanguageListReady() { loop_.Run(); }

 private:
  bool LanguageListReady() const {
    return welcome_screen_->language_list_updated_for_testing();
  }

  void CheckLanguageList() {
    if (LanguageListReady())
      loop_.Quit();
  }

  raw_ptr<WelcomeScreen> welcome_screen_;
  base::RunLoop loop_;
};

}  // namespace

// These test data depend on the IME extension manifest which differs between
// Chromium OS and Chrome OS.
struct LocalizationTestParams {
  const char* initial_locale;
  const char* keyboard_layout;
  const char* expected_locale;
  const char* expected_keyboard_layout;
  const char* expected_keyboard_select_control;
} const oobe_localization_test_parameters[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // ------------------ Non-Latin setup
    // For a non-Latin keyboard layout like Russian, we expect to see the US
    // keyboard.
    {"ru", "xkb:ru::rus", "ru", kUSLayout, "xkb:us::eng"},
    {"ru", "xkb:us::eng,xkb:ru::rus", "ru", kUSLayout, "xkb:us::eng"},

    // IMEs do not load at OOBE, so we just expect to see the (Latin) Japanese
    // keyboard.
    {"ja", "xkb:jp::jpn", "ja", "xkb:jp::jpn", "xkb:jp::jpn,[xkb:us::eng]"},

    // We don't use the Icelandic locale but the Icelandic keyboard layout
    // should still be selected when specified as the default.
    {"en-US", "xkb:is::ice", "en-US", "xkb:is::ice",
     "xkb:is::ice,[xkb:us::eng,xkb:us:intl:eng,xkb:us:intl_pc:eng,"
     "xkb:us:altgr-intl:eng,xkb:us:dvorak:eng,xkb:us:dvp:eng,"
     "xkb:us:colemak:eng,xkb:us:workman:eng,xkb:us:workman-intl:eng]"},
    // ------------------ Full Latin setup
    // French Swiss keyboard.
    {"fr", "xkb:ch:fr:fra", "fr", "xkb:ch:fr:fra",
     "xkb:ch:fr:fra,[xkb:fr::fra,xkb:fr:bepo:fra,xkb:be::fra,xkb:ca::fra,"
     "xkb:ca:multix:fra,xkb:us::eng]"},

    // German Swiss keyboard.
    {"de", "xkb:ch::ger", "de", "xkb:ch::ger",
     "xkb:ch::ger,[xkb:de::ger,xkb:de:neo:ger,xkb:be::ger,xkb:us::eng]"},

    // WelcomeScreenMultipleLocales
    {"es,en-US,nl", "xkb:be::nld", "es,en-US,nl", "xkb:be::nld",
     "xkb:be::nld,[xkb:es::spa,xkb:latam::spa,xkb:us::eng]"},

    {"ru,de", "xkb:ru::rus", "ru,de", kUSLayout, "xkb:us::eng"},

    // ------------------ Regional Locales
    // Synthetic example to test correct merging of different locales.
    {"fr-CH,it-CH,de-CH", "xkb:fr::fra,xkb:it::ita,xkb:de::ger",
     "fr-CH,it-CH,de-CH", "xkb:fr::fra",
     "xkb:fr::fra,xkb:it::ita,xkb:de::ger,"
     "[xkb:fr:bepo:fra,xkb:be::fra,xkb:ca::fra,"
     "xkb:ch:fr:fra,xkb:ca:multix:fra,xkb:us::eng]"},

    // Another synthetic example. Check that british keyboard is available.
    {"en-AU", "xkb:us::eng", "en-AU", "xkb:us::eng",
     "xkb:us::eng,[xkb:gb:extd:eng,xkb:gb:dvorak:eng]"},
#else
    // ------------------ Non-Latin setup
    // For a non-Latin keyboard layout like Russian, we expect to see the US
    // keyboard.
    {"ru", "xkb:ru::rus", "ru", kUSLayout, "xkb:us::eng"},
    {"ru", "xkb:us::eng,xkb:ru::rus", "ru", kUSLayout, "xkb:us::eng"},

    // IMEs do not load at OOBE, so we just expect to see the (Latin) Japanese
    // keyboard.
    {"ja", "xkb:jp::jpn", "ja", "xkb:jp::jpn", "xkb:jp::jpn,[xkb:us::eng]"},

    // We don't use the Icelandic locale but the Icelandic keyboard layout
    // should still be selected when specified as the default.
    {"en-US", "xkb:is::ice", "en-US", "xkb:is::ice",
     "xkb:is::ice,[xkb:us::eng,xkb:us:intl:eng,xkb:us:altgr-intl:eng,"
     "xkb:us:dvorak:eng,xkb:us:dvp:eng,xkb:us:colemak:eng,"
     "xkb:us:workman:eng,xkb:us:workman-intl:eng]"},
    // ------------------ Full Latin setup
    // French Swiss keyboard.
    {"fr", "xkb:ch:fr:fra", "fr", "xkb:ch:fr:fra",
     "xkb:ch:fr:fra,[xkb:fr::fra,xkb:be::fra,xkb:ca::fra,"
     "xkb:ca:multix:fra,xkb:us::eng]"},

    // German Swiss keyboard.
    {"de", "xkb:ch::ger", "de", "xkb:ch::ger",
     "xkb:ch::ger,[xkb:de::ger,xkb:de:neo:ger,xkb:be::ger,xkb:us::eng]"},

    // WelcomeScreenMultipleLocales
    {"es,en-US,nl", "xkb:be::nld", "es,en-US,nl", "xkb:be::nld",
     "xkb:be::nld,[xkb:es::spa,xkb:latam::spa,xkb:us::eng]"},

    {"ru,de", "xkb:ru::rus", "ru,de", kUSLayout, "xkb:us::eng"},

    // ------------------ Regional Locales
    // Synthetic example to test correct merging of different locales.
    {"fr-CH,it-CH,de-CH", "xkb:fr::fra,xkb:it::ita,xkb:de::ger",
     "fr-CH,it-CH,de-CH", "xkb:fr::fra",
     "xkb:fr::fra,xkb:it::ita,xkb:de::ger,[xkb:be::fra,xkb:ca::fra,"
     "xkb:ch:fr:fra,xkb:ca:multix:fra,xkb:us::eng]"},

    // Another synthetic example. Check that british keyboard is available.
    {"en-AU", "xkb:us::eng", "en-AU", "xkb:us::eng",
     "xkb:us::eng,[xkb:gb:extd:eng,xkb:gb:dvorak:eng]"},
#endif
};

class OobeLocalizationTest
    : public OobeBaseTest,
      public testing::WithParamInterface<const LocalizationTestParams*> {
 public:
  OobeLocalizationTest();

  OobeLocalizationTest(const OobeLocalizationTest&) = delete;
  OobeLocalizationTest& operator=(const OobeLocalizationTest&) = delete;

  // Verifies that the comma-separated `values` corresponds with the first
  // values in `select_id`, optionally checking for an options group label after
  // the first set of options.
  void VerifyInitialOptions(const char* select_id,
                            const char* values,
                            bool check_separator);

  // Verifies that `value` exists in `select_id`.
  void VerifyOptionExists(const char* select_id, const char* value);

  // Dumps OOBE select control (language or keyboard) to string.
  std::string DumpOptions(const char* select_id);

 protected:
  // Runs the test for the given locale and keyboard layout.
  void RunLocalizationTest();

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

OobeLocalizationTest::OobeLocalizationTest() : OobeBaseTest() {
  fake_statistics_provider_.SetMachineStatistic("initial_locale",
                                                GetParam()->initial_locale);
  fake_statistics_provider_.SetMachineStatistic("keyboard_layout",
                                                GetParam()->keyboard_layout);
}

void OobeLocalizationTest::VerifyInitialOptions(const char* select_id,
                                                const char* values,
                                                bool check_separator) {
  const std::string select = GetGetSelectStatement(select_id);
  const std::string expression = base::StringPrintf(
      "(function () {\n"
      "  let select = %s;\n"
      "  if (!select) {\n"
      "    console.error('Could not find ' + `%s`);\n"
      "    return false;\n"
      "  }\n"
      "  let values = '%s'.split(',');\n"
      "  let correct = select.selectedIndex == 0;\n"
      "  if (!correct)\n"
      "    console.error('Wrong selected index ' + select.selectedIndex);\n"
      "  for (var i = 0; i < values.length && correct; i++) {\n"
      "    if (select.options[i].value != values[i]) {\n"
      "      correct = false;\n"
      "      console.error('Values mismatch ' + "
      "                     select.options[i].value + ' ' + values[i]);\n"
      "    }\n"
      "  }\n"
      "  if (%d && correct) {\n"
      "    correct = select.children[values.length].tagName === 'OPTGROUP';\n"
      "    if (!correct)\n"
      "      console.error('Wrong tagname ' + "
      "                     select.children[values.length].tagName);\n"
      "  }\n"
      "  return correct;\n"
      "})()",
      select.c_str(), select.c_str(), values, check_separator);
  test::OobeJS().ExpectTrue(expression);
}

void OobeLocalizationTest::VerifyOptionExists(const char* select_id,
                                              const char* value) {
  const std::string expression = base::StringPrintf(
      "(function () {\n"
      "  var select = %s;\n"
      "  if (!select)\n"
      "    return false;\n"
      "  for (var i = 0; i < select.options.length; i++) {\n"
      "    if (select.options[i].value == '%s')\n"
      "      return true;\n"
      "  }\n"
      "  return false;\n"
      "})()",
      GetGetSelectStatement(select_id).c_str(), value);
  test::OobeJS().ExpectTrue(expression);
}

std::string OobeLocalizationTest::DumpOptions(const char* select_id) {
  const std::string expression = base::StringPrintf(
      "(function () {\n"
      "  var select = %s;\n"
      "  var divider = ',';\n"
      "  if (!select)\n"
      "    return 'select statement for \"%s\" failed.';\n"
      "  var dumpOptgroup = function(group) {\n"
      "    var result = '';\n"
      "    for (var i = 0; i < group.children.length; i++) {\n"
      "      if (i > 0) {\n"
      "        result += divider;\n"
      "      }\n"
      "      if (group.children[i].value) {\n"
      "        result += group.children[i].value;\n"
      "      } else {\n"
      "        result += '__NO_VALUE__';\n"
      "      }\n"
      "    }\n"
      "    return result;\n"
      "  };\n"
      "  var result = '';\n"
      "  if (select.selectedIndex != 0) {\n"
      "    result += '(selectedIndex=' + select.selectedIndex + \n"
      "        ', selected \"' + select.options[select.selectedIndex].value +\n"
      "        '\")';\n"
      "  }\n"
      "  var children = select.children;\n"
      "  for (var i = 0; i < children.length; i++) {\n"
      "    if (i > 0) {\n"
      "      result += divider;\n"
      "    }\n"
      "    if (children[i].value) {\n"
      "      result += children[i].value;\n"
      "    } else if (children[i].tagName === 'OPTGROUP') {\n"
      "      result += '[' + dumpOptgroup(children[i]) + ']';\n"
      "    } else {\n"
      "      result += '__NO_VALUE__';\n"
      "    }\n"
      "  }\n"
      "  return result;\n"
      "})()\n",
      GetGetSelectStatement(select_id).c_str(), select_id);
  return test::OobeJS().GetString(expression);
}

std::string TranslateXKB2Extension(const std::string& src) {
  std::string result(src);
  // Modifies the expected keyboard select control options for the new
  // extension based xkb id.
  size_t pos = 0;
  std::string repl_old = "xkb:";
  std::string repl_new = extension_ime_util::GetInputMethodIDByEngineID("xkb:");
  while ((pos = result.find(repl_old, pos)) != std::string::npos) {
    result.replace(pos, repl_old.length(), repl_new);
    pos += repl_new.length();
  }
  return result;
}

void OobeLocalizationTest::RunLocalizationTest() {
  WaitForOobeUI();
  const std::string expected_locale(GetParam()->expected_locale);
  const std::string expected_keyboard_layout(
      GetParam()->expected_keyboard_layout);
  const std::string expected_keyboard_select_control(
      GetParam()->expected_keyboard_select_control);

  const std::string expected_keyboard_select =
      TranslateXKB2Extension(expected_keyboard_select_control);

  ASSERT_NO_FATAL_FAILURE(LanguageListWaiter().RunUntilLanguageListReady());

  const std::string first_language =
      expected_locale.substr(0, expected_locale.find(','));
  const std::string get_select_statement =
      GetGetSelectStatement(kLanguageSelect);

  ASSERT_NO_FATAL_FAILURE(
      VerifyInitialOptions(kLanguageSelect, expected_locale.c_str(), true))
      << "Actual value of " << kLanguageSelect << ":\n"
      << DumpOptions(kLanguageSelect);

  ASSERT_NO_FATAL_FAILURE(VerifyInitialOptions(
      kKeyboardSelect, TranslateXKB2Extension(expected_keyboard_layout).c_str(),
      false))
      << "Actual value of " << kKeyboardSelect << ":\n"
      << DumpOptions(kKeyboardSelect);

  // Make sure we have a fallback keyboard.
  ASSERT_NO_FATAL_FAILURE(VerifyOptionExists(
      kKeyboardSelect,
      extension_ime_util::GetInputMethodIDByEngineID(kUSLayout).c_str()))
      << "Actual value of " << kKeyboardSelect << ":\n"
      << DumpOptions(kKeyboardSelect);

  // Note, that sort order is locale-specific, but is unlikely to change.
  // Especially for keyboard layouts.
  EXPECT_EQ(expected_keyboard_select, DumpOptions(kKeyboardSelect));
}

IN_PROC_BROWSER_TEST_P(OobeLocalizationTest, LocalizationTest) {
  RunLocalizationTest();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OobeLocalizationTest,
    testing::Range(&oobe_localization_test_parameters[0],
                   &oobe_localization_test_parameters[std::size(
                       oobe_localization_test_parameters)]));
}  // namespace ash
