// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/chromeos/input_method_whitelist.h"

namespace base {
class TaskRunner;
}

namespace chromeos {

namespace {

// Timeout for RunLoop::Run() in this test.
const int kTimeoutSeconds = 2;

// OOBE constants.
const char kLanguageSelect[] = "languageSelect";
const char kKeyboardSelect[] = "keyboardSelect";

std::string GetGetSelectStatement(const std::string& selectId) {
  return "document.getElementById('connect').$." + selectId + ".$.select";
}

const char kUSLayout[] = "xkb:us::eng";
class TimedRunLoop {
 public:
  TimedRunLoop(const base::TimeDelta& timeout,
               const std::string& failure_message)
      : timeout_(timeout), message_(failure_message) {}

  // Returns true if Run() successfully finished,
  // Returns false on timeout.
  bool Run() {
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, timeout_,
                base::Bind(&TimedRunLoop::OnTimeout, base::Unretained(this)));
    loop_.Run();
    return result_;
  }

  void Quit() {
    result_ = true;
    loop_.Quit();
  }

  base::Closure QuitClosure() {
    return base::Bind(&TimedRunLoop::Quit, base::Unretained(this));
  }

 private:
  void OnTimeout() {
    LOG(ERROR) << "Timeout waiting for: " << message_;
    result_ = false;
    loop_.Quit();
  }

  bool result_ = false;
  const base::TimeDelta timeout_;
  const std::string message_;
  base::RunLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(TimedRunLoop);
};

class LanguageListWaiter : public WelcomeScreen::Observer {
 public:
  LanguageListWaiter()
      : welcome_screen_(WelcomeScreen::Get(
            WizardController::default_controller()->screen_manager())),
        loop_(base::TimeDelta::FromSeconds(kTimeoutSeconds), "LanguageList") {
    welcome_screen_->AddObserver(this);
    CheckLanguageList();
  }

  ~LanguageListWaiter() override { welcome_screen_->RemoveObserver(this); }

  // WelcomeScreen::Observer implementation:
  void OnLanguageListReloaded() override { CheckLanguageList(); }

  // Returns true on success, false on timeout.
  bool Wait() {
    if (LanguageListReady())
      return true;

    return loop_.Run();
  }

 private:
  bool LanguageListReady() const { return welcome_screen_->language_list(); }

  void CheckLanguageList() {
    if (LanguageListReady())
      loop_.Quit();
  }

  WelcomeScreen* welcome_screen_;
  TimedRunLoop loop_;
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
    : public LoginManagerTest,
      public testing::WithParamInterface<const LocalizationTestParams*> {
 public:
  OobeLocalizationTest();

  // Verifies that the comma-separated |values| corresponds with the first
  // values in |select_id|, optionally checking for an options group label after
  // the first set of options.
  bool VerifyInitialOptions(const char* select_id,
                            const char* values,
                            bool check_separator);

  // Verifies that |value| exists in |select_id|.
  bool VerifyOptionExists(const char* select_id, const char* value);

  // Dumps OOBE select control (language or keyboard) to string.
  std::string DumpOptions(const char* select_id);

 protected:
  // Runs the test for the given locale and keyboard layout.
  void RunLocalizationTest();

  // Returns true on success, false on error.
  bool WaitUntilJSIsReady() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    if (!host)
      return false;

    OobeUI* oobe_ui = host->GetOobeUI();
    if (!oobe_ui)
      return false;

    TimedRunLoop run_loop(base::TimeDelta::FromSeconds(kTimeoutSeconds),
                          "WaitUntilJSIsReady()");
    const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
    if (oobe_ui_ready)
      return true;

    return run_loop.Run();
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  DISALLOW_COPY_AND_ASSIGN(OobeLocalizationTest);
};

OobeLocalizationTest::OobeLocalizationTest() : LoginManagerTest(false, true) {
  fake_statistics_provider_.SetMachineStatistic("initial_locale",
                                                GetParam()->initial_locale);
  fake_statistics_provider_.SetMachineStatistic("keyboard_layout",
                                                GetParam()->keyboard_layout);
}

bool OobeLocalizationTest::VerifyInitialOptions(const char* select_id,
                                                const char* values,
                                                bool check_separator) {
  const std::string expression = base::StringPrintf(
      "(function () {\n"
      "  let select = %s;\n"
      "  if (!select)\n"
      "    return false;\n"
      "  let values = '%s'.split(',');\n"
      "  let correct = select.selectedIndex == 0;\n"
      "  for (var i = 0; i < values.length && correct; i++) {\n"
      "    if (select.options[i].value != values[i])\n"
      "      correct = false;\n"
      "  }\n"
      "  if (%d && correct)\n"
      "    correct = select.children[values.length].tagName === 'OPTGROUP';\n"
      "  return correct;\n"
      "})()",
      GetGetSelectStatement(select_id).c_str(), values, check_separator);
  const bool execute_status = test::OobeJS().GetBool(expression);
  EXPECT_TRUE(execute_status) << expression;
  return execute_status;
}

bool OobeLocalizationTest::VerifyOptionExists(const char* select_id,
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
  const bool execute_status = test::OobeJS().GetBool(expression);
  EXPECT_TRUE(execute_status) << expression;
  return execute_status;
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
  const std::string initial_locale(GetParam()->initial_locale);
  const std::string keyboard_layout(GetParam()->keyboard_layout);
  const std::string expected_locale(GetParam()->expected_locale);
  const std::string expected_keyboard_layout(
      GetParam()->expected_keyboard_layout);
  const std::string expected_keyboard_select_control(
      GetParam()->expected_keyboard_select_control);

  const std::string expected_keyboard_select =
      TranslateXKB2Extension(expected_keyboard_select_control);

  ASSERT_TRUE(LanguageListWaiter().Wait());

  ASSERT_TRUE(WaitUntilJSIsReady());

  const std::string first_language =
      expected_locale.substr(0, expected_locale.find(','));
  bool done = false;
  const std::string get_select_statement =
      GetGetSelectStatement(kLanguageSelect);
  const std::string waiting_script = base::StringPrintf(
      "function SendReplyIfAcceptEnabled() {"
      "  if (%s.value != '%s')"
      "    return false;"
      "  domAutomationController.send(true);"
      "  observer.disconnect();"
      "  return true;"
      "}"
      "var observer = new MutationObserver(SendReplyIfAcceptEnabled);"
      "if (!SendReplyIfAcceptEnabled()) {"
      "  let options = { attributes: true };"
      "  observer.observe(%s, options);"
      "}",
      get_select_statement.c_str(), first_language.c_str(),
      get_select_statement.c_str());

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      LoginDisplayHost::default_host()->GetOobeUI()->web_ui()->GetWebContents(),
      waiting_script, &done));

  if (!VerifyInitialOptions(kLanguageSelect, expected_locale.c_str(), true)) {
    LOG(ERROR) << "Actual value of " << kLanguageSelect << ":\n"
               << DumpOptions(kLanguageSelect);
    EXPECT_TRUE(false);
  }
  if (!VerifyInitialOptions(
          kKeyboardSelect,
          TranslateXKB2Extension(expected_keyboard_layout).c_str(), false)) {
    LOG(ERROR) << "Actual value of " << kKeyboardSelect << ":\n"
               << DumpOptions(kKeyboardSelect);
    EXPECT_TRUE(false);
  }

  // Make sure we have a fallback keyboard.
  if (!VerifyOptionExists(
          kKeyboardSelect,
          extension_ime_util::GetInputMethodIDByEngineID(kUSLayout).c_str())) {
    LOG(ERROR) << "Actual value of " << kKeyboardSelect << ":\n"
               << DumpOptions(kKeyboardSelect);
    EXPECT_TRUE(false);
  }

  // Note, that sort order is locale-specific, but is unlikely to change.
  // Especially for keyboard layouts.
  EXPECT_EQ(expected_keyboard_select, DumpOptions(kKeyboardSelect));

  // Shut down the display host.
  LoginDisplayHost::default_host()->Finalize(base::OnceClosure());
  base::RunLoop().RunUntilIdle();

  // Clear the locale pref so the statistics provider is pinged next time.
  g_browser_process->local_state()->SetString(
      language::prefs::kApplicationLocale, std::string());
}

IN_PROC_BROWSER_TEST_P(OobeLocalizationTest, LocalizationTest) {
  RunLocalizationTest();
}

INSTANTIATE_TEST_SUITE_P(
    StructSequence,
    OobeLocalizationTest,
    testing::Range(&oobe_localization_test_parameters[0],
                   &oobe_localization_test_parameters[base::size(
                       oobe_localization_test_parameters)]));
}  // namespace chromeos
