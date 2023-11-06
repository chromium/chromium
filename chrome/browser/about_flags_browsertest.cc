// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/common/chrome_version.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"

namespace {

const char kSwitchName[] = "flag-system-test-switch";
const char kFlagName[] = "flag-system-test-flag-1";

const char kExpiredFlagName[] = "flag-system-test-flag-2";
const char kExpiredFlagSwitchName[] = "flag-system-test-expired-switch";

const char kFlagWithOptionSelectorName[] = "flag-system-test-flag-3";
const char kFlagWithOptionSelectorSwitchName[] = "flag-system-test-switch";

// Command line switch containing an invalid origin.
const char kUnsanitizedCommandLine[] =
    "http://example-cmdline.test,invalid-cmdline";

// Command line switch without the invalid origin.
const char kSanitizedCommandLine[] = "http://example-cmdline.test";

// User input containing invalid origins.
const char kUnsanitizedInput[] =
    "http://example.test/path    http://example2.test/?query\n"
    "invalid-value, filesystem:http://example.test.file, "
    "ws://example3.test http://&^.com";

// User input with invalid origins removed and formatted.
const char kSanitizedInput[] =
    "http://example.test,http://example2.test,ws://example3.test";

// Command Line + user input with invalid origins removed and formatted.
const char kSanitizedInputAndCommandLine[] =
    "http://example-cmdline.test,http://example.test,http://"
    "example2.test,ws://example3.test";

void SimulateTextType(content::WebContents* contents,
                      const char* experiment_id,
                      const char* text) {
  EXPECT_TRUE(content::ExecJs(
      contents,
      base::StringPrintf(
          "var parent = "
          "document.querySelector('flags-app').shadowRoot.getElementById('%s');"
          "var textarea = parent.getElementsByTagName('textarea')[0];"
          "textarea.focus();"
          "textarea.value = `%s`;"
          "textarea.dispatchEvent(new Event('change'));",
          experiment_id, text)));
}

void ToggleEnableDropdown(content::WebContents* contents,
                          const char* experiment_id,
                          bool enable) {
  EXPECT_TRUE(content::ExecJs(
      contents,
      base::StringPrintf(
          "var k = "
          "document.querySelector('flags-app').shadowRoot.getElementById('%s');"
          "var s = "
          "k.shadowRoot."
          "querySelector('.experiment-enable-disable');"
          "s.focus();"
          "s.selectedIndex = %d;"
          "s.dispatchEvent(new Event('change'));",
          experiment_id, enable ? 1 : 0)));
}

std::string GetOriginListText(content::WebContents* contents,
                              const char* experiment_id) {
  return content::EvalJs(
             contents,
             base::StringPrintf(
                 "var k = "
                 "document.querySelector('flags-app').shadowRoot."
                 "getElementById('%s');"
                 "var s = "
                 "k.getElementsByClassName('experiment-origin-list-value')[0];"
                 "s.value;",
                 experiment_id))
      .ExtractString();
}

bool IsDropdownEnabled(content::WebContents* contents,
                       const char* experiment_id) {
  return content::EvalJs(
             contents,
             base::StringPrintf(
                 "var k = "
                 "document.querySelector('flags-app').shadowRoot."
                 "getElementById('%s');"
                 "var s = "
                 "k.getElementsByClassName('experiment-enable-disable')[0];"
                 "s.value == 'enabled';",
                 experiment_id))
      .ExtractBool();
}

bool IsFlagPresent(content::WebContents* contents, const char* experiment_id) {
  return content::EvalJs(contents,
                         base::StringPrintf("var k = "
                                            "document.querySelector('flags-app'"
                                            ").shadowRoot.getElementById('%s');"
                                            "k != null;",
                                            experiment_id))
      .ExtractBool();
}

void WaitForExperimentalFeatures(content::WebContents* contents) {
  ASSERT_TRUE(content::ExecJs(
      contents,
      "var k = document.querySelector('flags-app');"
      "k.experimentalFeaturesReadyForTesting().then(() => true);"));
}

const std::vector<flags_ui::FeatureEntry> GetFeatureEntries(
    const std::string& unexpire_name) {
  std::vector<flags_ui::FeatureEntry> entries = {
      {kFlagName, "name-1", "description-1", static_cast<unsigned short>(-1),
       ORIGIN_LIST_VALUE_TYPE(kSwitchName, "")},
      {kExpiredFlagName, "name-2", "description-2",
       static_cast<unsigned short>(-1),
       SINGLE_VALUE_TYPE(kExpiredFlagSwitchName)},
      {kFlagWithOptionSelectorName, "name-3", "description-3",
       static_cast<unsigned short>(-1),
       SINGLE_VALUE_TYPE(kFlagWithOptionSelectorSwitchName)}};
  flags_ui::FeatureEntry expiry_entry = {
      unexpire_name.c_str(), "unexpire name", "unexpire desc",
      static_cast<unsigned short>(-1),
      SINGLE_VALUE_TYPE("unexpire-dummy-switch")};
  entries.push_back(expiry_entry);
  return entries;
}

// In these tests, valid origins in the existing command line flag will be
// appended to the list entered by the user in chrome://flags.
// The tests are run twice for each bool value: Once with an existing command
// line (provided in SetUpCommandLine) and once without.
class AboutFlagsBrowserTest : public InProcessBrowserTest,
                              public testing::WithParamInterface<bool> {
 public:
  AboutFlagsBrowserTest()
      : unexpire_name_(base::StringPrintf("temporary-unexpire-flags-m%d",
                                          CHROME_VERSION_MAJOR - 1)),
        scoped_feature_entries_(GetFeatureEntries(unexpire_name_)) {
    flags::testing::SetFlagExpiration(kExpiredFlagName,
                                      CHROME_VERSION_MAJOR - 1);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(kSwitchName, GetInitialCommandLine());
  }

 protected:
  bool has_initial_command_line() const { return GetParam(); }

  std::string GetInitialCommandLine() const {
    return has_initial_command_line() ? kUnsanitizedCommandLine : std::string();
  }

  std::string GetSanitizedCommandLine() const {
    return has_initial_command_line() ? kSanitizedCommandLine : std::string();
  }

  std::string GetSanitizedInputAndCommandLine() const {
    return has_initial_command_line() ? kSanitizedInputAndCommandLine
                                      : kSanitizedInput;
  }

  void NavigateToFlagsPage() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://flags")));
    WaitForExperimentalFeatures(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  bool expiration_enabled_ = true;
  std::string unexpire_name_;

  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AboutFlagsBrowserTest,
                         ::testing::Values(true, false));

// Goes to chrome://flags page, types text into an ORIGIN_LIST_VALUE field but
// does not enable the feature.
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, PRE_OriginFlagDisabled) {
  NavigateToFlagsPage();

  const base::CommandLine::SwitchMap kInitialSwitches =
      base::CommandLine::ForCurrentProcess()->GetSwitches();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The page should be populated with the sanitized command line value.
  EXPECT_EQ(GetSanitizedCommandLine(), GetOriginListText(contents, kFlagName));

  // Type a value in the experiment's textarea. Since the flag state is
  // "Disabled" by default, command line shouldn't change.
  SimulateTextType(contents, kFlagName, kUnsanitizedInput);
  EXPECT_EQ(kInitialSwitches,
            base::CommandLine::ForCurrentProcess()->GetSwitches());

  // Input should be restored after a page reload.
  NavigateToFlagsPage();
  EXPECT_EQ(GetSanitizedInputAndCommandLine(),
            GetOriginListText(contents, kFlagName));
}

// Flaky. http://crbug.com/1010678
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, DISABLED_OriginFlagDisabled) {
  // Even though the feature is disabled, the switch is set directly via command
  // line.
  EXPECT_EQ(
      GetInitialCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));

  NavigateToFlagsPage();
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(IsDropdownEnabled(contents, kFlagName));
  EXPECT_EQ(GetSanitizedInputAndCommandLine(),
            GetOriginListText(contents, kFlagName));
}

// Goes to chrome://flags page, types text into an ORIGIN_LIST_VALUE field and
// enables the feature.
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, PRE_OriginFlagEnabled) {
  NavigateToFlagsPage();

  const base::CommandLine::SwitchMap kInitialSwitches =
      base::CommandLine::ForCurrentProcess()->GetSwitches();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The page should be populated with the sanitized command line value.
  EXPECT_EQ(GetSanitizedCommandLine(), GetOriginListText(contents, kFlagName));

  // Type a value in the experiment's textarea. Since the flag state is
  // "Disabled" by default, command line shouldn't change.
  SimulateTextType(contents, kFlagName, kUnsanitizedInput);
  EXPECT_EQ(kInitialSwitches,
            base::CommandLine::ForCurrentProcess()->GetSwitches());

  // Enable the experiment. The behavior is different between ChromeOS and
  // non-ChromeOS.
  ToggleEnableDropdown(contents, kFlagName, true);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // On non-ChromeOS, the command line is not modified until restart.
  EXPECT_EQ(kInitialSwitches,
            base::CommandLine::ForCurrentProcess()->GetSwitches());
#else
  // On ChromeOS, the command line is immediately modified.
  EXPECT_NE(kInitialSwitches,
            base::CommandLine::ForCurrentProcess()->GetSwitches());
  EXPECT_EQ(
      GetSanitizedInputAndCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));
#endif

  // Input should be restored after a page reload.
  NavigateToFlagsPage();
  EXPECT_EQ(GetSanitizedInputAndCommandLine(),
            GetOriginListText(contents, kFlagName));
}

// Flaky. http://crbug.com/1010678
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, DISABLED_OriginFlagEnabled) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // On non-ChromeOS, the command line is modified after restart.
  EXPECT_EQ(
      GetSanitizedInputAndCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));
#else
  // On ChromeOS, the command line isn't modified after restart.
  EXPECT_EQ(
      GetInitialCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));
#endif

  NavigateToFlagsPage();
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsDropdownEnabled(contents, kFlagName));
  EXPECT_EQ(GetSanitizedInputAndCommandLine(),
            GetOriginListText(contents, kFlagName));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS doesn't read chrome://flags values on startup so we explicitly
  // need to disable and re-enable the flag here.
  ToggleEnableDropdown(contents, kFlagName, true);
#endif

  EXPECT_EQ(
      GetSanitizedInputAndCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));
}

IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, ExpiryHidesFlag) {
  NavigateToFlagsPage();
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsFlagPresent(contents, kFlagName));
  EXPECT_FALSE(IsFlagPresent(contents, kExpiredFlagName));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, PRE_ExpiredFlagDoesntApply) {
  NavigateToFlagsPage();
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsFlagPresent(contents, kExpiredFlagName));
  EXPECT_FALSE(IsDropdownEnabled(contents, kExpiredFlagName));

  ToggleEnableDropdown(contents, kExpiredFlagName, true);
}

// Flaky everywhere: https://crbug.com/1024028
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, DISABLED_ExpiredFlagDoesntApply) {
  NavigateToFlagsPage();
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(IsFlagPresent(contents, kExpiredFlagName));

  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      kExpiredFlagSwitchName));
}
#endif

// Regression test for https://crbug.com/1101828:
// Test that simply setting a flag (without the backing feature) is sufficient
// to consider a flag unexpired. This test checks that by using a flag with the
// expected unexpire name, but wired to a dummy switch rather than the usual
// feature.
//
// This isn't a perfect regression test - that would require two separate
// browser restarts:
// 1) Enable temporary-unexpire-flags-m$M, restart
// 2) Enable the test flag (which is only visible after the previous restart),
//    restart
// 3) Ensure that the test flag got applied at startup
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, RawFlagUnexpiryWorks) {
  NavigateToFlagsPage();
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(IsFlagPresent(contents, kExpiredFlagName));
  ToggleEnableDropdown(contents, unexpire_name_.c_str(), true);

  NavigateToFlagsPage();
  contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsFlagPresent(contents, kExpiredFlagName));
}

IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, FormRestore) {
  NavigateToFlagsPage();
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Remove the internal_name property from a flag's selector, then synthesize a
  // change event for it. This simulates what happens during form restoration in
  // Blink, when navigating back and then forward to the flags page. This test
  // ensures that that does not crash the browser.
  // See https://crbug.com/1038638 for more details.
  EXPECT_TRUE(content::ExecJs(
      contents,
      base::StringPrintf(
          "var k = "
          "document.querySelector('flags-app').shadowRoot.getElementById('%s');"
          "var s = "
          "k.shadowRoot."
          "querySelector('.experiment-enable-disable');"
          "delete s.internal_name;"
          "const e = document.createEvent('HTMLEvents');"
          "e.initEvent('change', true, true);"
          "s.dispatchEvent(e);",
          kFlagWithOptionSelectorName),
      // Execute script in an isolated world to avoid causing a Trusted Types
      // violation due to eval.
      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

}  // namespace
