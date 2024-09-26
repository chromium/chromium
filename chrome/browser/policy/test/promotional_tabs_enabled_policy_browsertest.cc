// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace policy {

// Base class for testing the policy.
class PromotionalTabsEnabledPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<
          std::pair</*PromotionalTabsEnabledPolicy*/ PolicyTest::BooleanPolicy,
                    /*PromotionsEnabledPolicy*/ PolicyTest::BooleanPolicy>> {
 public:
  PromotionalTabsEnabledPolicyTest(const PromotionalTabsEnabledPolicyTest&) =
      delete;
  PromotionalTabsEnabledPolicyTest& operator=(
      const PromotionalTabsEnabledPolicyTest&) = delete;

 protected:
  PromotionalTabsEnabledPolicyTest() {
    const std::vector<base::test::FeatureRef> kEnabledFeatures = {
      whats_new::kForceEnabled,
      welcome::kForceEnabled,
    };
    scoped_feature_list_.InitWithFeatures(kEnabledFeatures, {});
  }
  ~PromotionalTabsEnabledPolicyTest() override = default;

  void SetUp() override {
    // Ordinarily, browser tests include chrome://blank on the command line to
    // suppress any onboarding or promotional tabs. This test, on the other
    // hand, must evaluate startup with nothing on the command line so that a
    // default launch takes place.
    set_open_about_blank_on_browser_launch(false);
    PolicyTest::SetUp();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Set policies before the browser starts up.
    PolicyMap policies;

    // Suppress the first-run dialog by disabling metrics reporting.
    policies.Set(key::kMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(false),
                 nullptr);

    // Apply the policy setting under test.
    if (GetParam().first != BooleanPolicy::kNotConfigured) {
      policies.Set(key::kPromotionalTabsEnabled, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value(GetParam().first == BooleanPolicy::kTrue),
                   nullptr);
    }
    if (GetParam().second != BooleanPolicy::kNotConfigured) {
      policies.Set(key::kPromotionsEnabled, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value(GetParam().second == BooleanPolicy::kTrue),
                   nullptr);
    }

    UpdateProviderPolicy(policies);
    PolicyTest::CreatedBrowserMainParts(browser_main_parts);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the PromotionalTabsEnabled policy properly suppresses the welcome
// page for browser first-runs.
class PromotionalTabsEnabledPolicyWelcomeTest
    : public PromotionalTabsEnabledPolicyTest {
 public:
  PromotionalTabsEnabledPolicyWelcomeTest(
      const PromotionalTabsEnabledPolicyWelcomeTest&) = delete;
  PromotionalTabsEnabledPolicyWelcomeTest& operator=(
      const PromotionalTabsEnabledPolicyWelcomeTest&) = delete;

 protected:
  PromotionalTabsEnabledPolicyWelcomeTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceFirstRun);
  }
};

IN_PROC_BROWSER_TEST_P(PromotionalTabsEnabledPolicyWelcomeTest, RunTest) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_GE(tab_strip->count(), 1);
  const auto& url = tab_strip->GetWebContentsAt(0)->GetLastCommittedURL();

  // Only the NTP should show, regardless of the policy state.
  EXPECT_EQ(tab_strip->count(), 1);
  if (url.possibly_invalid_spec() != chrome::kChromeUINewTabURL) {
    EXPECT_PRED2(search::IsNTPOrRelatedURL, url, browser()->profile());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PromotionalTabsEnabledPolicyWelcomeTest,
    testing::ValuesIn(std::vector<std::pair<PolicyTest::BooleanPolicy,
                                            PolicyTest::BooleanPolicy>>{
        {PolicyTest::BooleanPolicy::kNotConfigured,
         PolicyTest::BooleanPolicy::kNotConfigured},
        {PolicyTest::BooleanPolicy::kFalse, PolicyTest::BooleanPolicy::kFalse},
        {PolicyTest::BooleanPolicy::kTrue, PolicyTest::BooleanPolicy::kTrue},
        {PolicyTest::BooleanPolicy::kFalse,
         PolicyTest::BooleanPolicy::kTrue}}));

// Tests that the PromotionalTabsEnabled policy properly suppresses the What's
// New page.
class PromotionalTabsEnabledPolicyWhatsNewTest
    : public PromotionalTabsEnabledPolicyTest {
 public:
  PromotionalTabsEnabledPolicyWhatsNewTest(
      const PromotionalTabsEnabledPolicyWhatsNewTest&) = delete;
  PromotionalTabsEnabledPolicyWhatsNewTest& operator=(
      const PromotionalTabsEnabledPolicyWhatsNewTest&) = delete;

 protected:
  PromotionalTabsEnabledPolicyWhatsNewTest() = default;
  ~PromotionalTabsEnabledPolicyWhatsNewTest() override = default;

  virtual int WhatsNewVersionForPref() { return CHROME_VERSION_MAJOR - 1; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->RemoveSwitch(switches::kForceFirstRun);
    command_line->AppendSwitch(switches::kForceWhatsNew);
  }

  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    // Suppress the welcome page by setting the pref indicating that it has
    // already been seen. This is necessary because welcome/onboarding takes
    // precedence over What's New.
    std::string json;
    base::Value::Dict prefs;
    prefs.SetByDottedPath(prefs::kHasSeenWelcomePage, true);
    // Set the session startup pref to NewTab. This enables consistent test
    // expectations across platforms - we should always expect to see the NTP.
    // Without this line, on ChromeOS only, the default type is LAST, which
    // tries to restore the last session and suppresses the NTP.
    prefs.SetByDottedPath(prefs::kRestoreOnStartup,
                          SessionStartupPref::kPrefValueNewTab);
    base::JSONWriter::Write(prefs, &json);

    base::FilePath default_dir =
        user_data_dir.AppendASCII(chrome::kInitialProfile);
    if (!base::CreateDirectory(default_dir)) {
      ADD_FAILURE() << "base::CreateDirectory() failed, " << default_dir;
      return false;
    }

    base::FilePath preferences_path =
        default_dir.Append(chrome::kPreferencesFilename);
    if (!base::WriteFile(preferences_path, json)) {
      ADD_FAILURE() << "base::WriteFile() failed, " << preferences_path;
      return false;
    }

    // Also set the version for What's New in the local state.
    base::Value::Dict local_state;
    local_state.SetByDottedPath(prefs::kLastWhatsNewVersion,
                                WhatsNewVersionForPref());
    std::string local_state_string;
    base::JSONWriter::Write(local_state, &local_state_string);
    base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);
    if (!base::WriteFile(local_state_path, local_state_string)) {
      ADD_FAILURE() << "base::WriteFile() failed, " << local_state_path;
      return false;
    }

    return true;
  }
};

// This is disabled due to flakiness: https://crbug.com/1362518
#define MAYBE_RunTest DISABLED_RunTest
IN_PROC_BROWSER_TEST_P(PromotionalTabsEnabledPolicyWhatsNewTest,
                       MAYBE_RunTest) {
  // Delay to allow the network request simulation to finish.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
  run_loop.Run();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_GE(tab_strip->count(), 1);
  const auto& url = tab_strip->GetWebContentsAt(0)->GetLastCommittedURL();
  bool promotions_disabled = (GetParam().first == BooleanPolicy::kFalse) ||
                             (GetParam().first == BooleanPolicy::kFalse);
  if (promotions_disabled) {
    // Only the NTP should show.
    EXPECT_EQ(tab_strip->count(), 1);
    if (url.possibly_invalid_spec() != chrome::kChromeUINewTabURL) {
      EXPECT_PRED2(search::IsNTPOrRelatedURL, url, browser()->profile());
    }
  } else {
    EXPECT_EQ(tab_strip->count(), 2);
    // Whats's New should show and be the active tab.
    EXPECT_EQ(url.possibly_invalid_spec(), chrome::kChromeUIWhatsNewURL);
    EXPECT_EQ(0, tab_strip->active_index());
    // The second tab should be the NTP.
    const auto& url_tab1 =
        tab_strip->GetWebContentsAt(1)->GetLastCommittedURL();
    EXPECT_EQ(url_tab1.possibly_invalid_spec(), chrome::kChromeUINewTabURL);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PromotionalTabsEnabledPolicyWhatsNewTest,
    testing::ValuesIn(std::vector<std::pair<PolicyTest::BooleanPolicy,
                                            PolicyTest::BooleanPolicy>>{
        {PolicyTest::BooleanPolicy::kNotConfigured,
         PolicyTest::BooleanPolicy::kNotConfigured},
        {PolicyTest::BooleanPolicy::kFalse, PolicyTest::BooleanPolicy::kFalse},
        {PolicyTest::BooleanPolicy::kTrue, PolicyTest::BooleanPolicy::kTrue},
        {PolicyTest::BooleanPolicy::kFalse,
         PolicyTest::BooleanPolicy::kTrue}}));

// Tests that What's New doesn't show up regardless of the policy if the version
// is not greater than the one in |prefs::kLastWhatsNewVersion|.
class PromotionalTabsEnabledPolicyWhatsNewInvalidTest
    : public PromotionalTabsEnabledPolicyWhatsNewTest {
 public:
  PromotionalTabsEnabledPolicyWhatsNewInvalidTest(
      const PromotionalTabsEnabledPolicyWhatsNewInvalidTest&) = delete;
  PromotionalTabsEnabledPolicyWhatsNewTest& operator=(
      const PromotionalTabsEnabledPolicyWhatsNewInvalidTest&) = delete;

 protected:
  PromotionalTabsEnabledPolicyWhatsNewInvalidTest() = default;
  ~PromotionalTabsEnabledPolicyWhatsNewInvalidTest() override = default;

  int WhatsNewVersionForPref() override { return CHROME_VERSION_MAJOR; }
};

IN_PROC_BROWSER_TEST_P(PromotionalTabsEnabledPolicyWhatsNewInvalidTest,
                       RunTest) {
  // Delay to allow the network request simulation to finish.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
  run_loop.Run();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_GE(tab_strip->count(), 1);
  const auto& url = tab_strip->GetWebContentsAt(0)->GetLastCommittedURL();

  // Only the NTP should show. There are no other relevant tabs since
  // welcome and What's New have both already been shown or promotional tabs
  // are disabled.
  EXPECT_EQ(tab_strip->count(), 1);
  if (url.possibly_invalid_spec() != chrome::kChromeUINewTabURL) {
    EXPECT_PRED2(search::IsNTPOrRelatedURL, url, browser()->profile());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PromotionalTabsEnabledPolicyWhatsNewInvalidTest,
    testing::ValuesIn(std::vector<std::pair<PolicyTest::BooleanPolicy,
                                            PolicyTest::BooleanPolicy>>{
        {PolicyTest::BooleanPolicy::kNotConfigured,
         PolicyTest::BooleanPolicy::kNotConfigured},
        {PolicyTest::BooleanPolicy::kFalse, PolicyTest::BooleanPolicy::kFalse},
        {PolicyTest::BooleanPolicy::kTrue, PolicyTest::BooleanPolicy::kTrue},
        {PolicyTest::BooleanPolicy::kFalse,
         PolicyTest::BooleanPolicy::kTrue}}));
}  // namespace policy
