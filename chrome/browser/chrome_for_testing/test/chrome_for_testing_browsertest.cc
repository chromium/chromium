// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_for_testing/test/chrome_for_testing_browsertest.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_for_testing/config.h"
#include "chrome/browser/chrome_for_testing/switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/headless/clipboard/headless_clipboard.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engines_switches.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_non_backed.h"

namespace chrome_for_testing {

ChromeForTestingBrowserTest::ChromeForTestingBrowserTest() {
  CHECK(temp_dir_.CreateUniqueTempDir());
}

void ChromeForTestingBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  AppendConfigSwitch(command_line);
}

std::string ChromeForTestingBrowserTest::GetConfigJson() {
  return std::string();
}

std::string ChromeForTestingBrowserTest::FormatConfigJsonBoolean(
    std::string_view option,
    bool value) {
  static constexpr char kJson[] = R"(
    {
      "%s": %s
    }
  )";
  return base::StringPrintf(kJson, option, value ? "true" : "false");
}

void ChromeForTestingBrowserTest::AppendConfigSwitch(
    base::CommandLine* command_line) {
  std::string config_json = GetConfigJson();
  if (config_json.empty()) {
    return;
  }

  base::FilePath config_path = temp_dir_.GetPath().AppendASCII("config.json");
  CHECK(base::WriteFile(config_path, config_json));

  command_line->AppendSwitchPath(switches::kChromeForTestingConfig,
                                 config_path);
}

namespace {

// Provides enabled/disabled test name suffix for the boolean option tests.
auto BooleanParamStringProvider = [](const testing::TestParamInfo<bool>& info) {
  return info.param ? "enabled" : "disabled";
};

// Miscellaneous tests --------------------------------------------------------

class ChromeForTestingNoOptionsBrowserTest
    : public ChromeForTestingBrowserTest {
 protected:
  std::string GetConfigJson() override { return "{}"; }
};

IN_PROC_BROWSER_TEST_F(ChromeForTestingNoOptionsBrowserTest, DefaultState) {
  EXPECT_FALSE(IsEnableUserEducationUI());
  EXPECT_FALSE(IsEnableSearchEngineChoiceDialog());
  EXPECT_FALSE(IsEnableVirtualClipboard());
}

// Search engine choice dialog tests ------------------------------------------

class ChromeForTestingSearchEngineChoiceDialogBrowserTest
    : public ChromeForTestingBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeForTestingBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry,
                                    switches::kEeaListCountryOverride);
  }

  bool EnableSearchEngineChoiceDialog() { return GetParam(); }

  std::string GetConfigJson() override {
    return FormatConfigJsonBoolean("enableSearchEngineChoiceDialog",
                                   EnableSearchEngineChoiceDialog());
  }

  bool IsSearchEngineChoiceDialogEnabled() {
    Profile* profile = browser()->profile();
    return !!SearchEngineChoiceDialogServiceFactory::GetForProfile(profile);
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ChromeForTestingSearchEngineChoiceDialogBrowserTest,
    testing::Bool(),
    BooleanParamStringProvider);

IN_PROC_BROWSER_TEST_P(ChromeForTestingSearchEngineChoiceDialogBrowserTest,
                       SearchEngineChoiceDialogOption) {
  EXPECT_EQ(EnableSearchEngineChoiceDialog(),
            IsEnableSearchEngineChoiceDialog());

  EXPECT_EQ(EnableSearchEngineChoiceDialog(),
            IsSearchEngineChoiceDialogEnabled());
}

// User education UI tests ----------------------------------------------------

class ChromeForTestingUserEducationUIBrowserTest
    : public ChromeForTestingBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  bool EnableUserEducationUI() { return GetParam(); }

  std::string GetConfigJson() override {
    return FormatConfigJsonBoolean("enableUserEducationUI",
                                   EnableUserEducationUI());
  }

  bool IsUserEducationUIEnabled(Profile* profile) {
    return UserEducationServiceFactory::ProfileAllowsUserEducation(profile);
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ChromeForTestingUserEducationUIBrowserTest,
    testing::Bool(),
    BooleanParamStringProvider);

IN_PROC_BROWSER_TEST_P(ChromeForTestingUserEducationUIBrowserTest,
                       UserEducationUIOption) {
  EXPECT_EQ(EnableUserEducationUI(), IsEnableUserEducationUI());

  Profile* profile = browser()->profile();
  ASSERT_FALSE(profile->IsIncognitoProfile());
  ASSERT_FALSE(profile->IsGuestSession());
  ASSERT_FALSE(profiles::IsDemoSession());
  ASSERT_FALSE(profiles::IsChromeAppKioskSession());

  EXPECT_EQ(EnableUserEducationUI(), IsUserEducationUIEnabled(profile));
}

// Virtual clipboard tests ----------------------------------------------------

class ChromeForTestingVirtualClipboardBrowserTest
    : public ChromeForTestingBrowserTest {
 protected:
  std::string GetConfigJson() override {
    return FormatConfigJsonBoolean("enableVirtualClipboard", true);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeForTestingVirtualClipboardBrowserTest,
                       VirtualClipboardOption) {
  ui::Clipboard* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ASSERT_TRUE(clipboard);

  ui::ClipboardBuffer buffer = ui::ClipboardBuffer::kCopyPaste;
  ASSERT_TRUE(ui::Clipboard::IsSupportedClipboardBuffer(buffer));

  int initial_request_counter =
      headless::GetSequenceNumberRequestCounterForTesting();

  clipboard->GetSequenceNumber(buffer);

  int current_request_counter =
      headless::GetSequenceNumberRequestCounterForTesting();

  // Expect sequence number to be incremented. This confirms that the headless
  // clipboard implementation is being used.
  EXPECT_GT(current_request_counter, initial_request_counter);
}

}  // namespace
}  // namespace chrome_for_testing
