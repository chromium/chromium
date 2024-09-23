// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is in maintenance mode, please do NOT add new tests into this file.
//
// policy_browsertests.cc contains lots of tests for multiple policies. However,
// it became huge with hundreds of policies. Instead of adding even more tests
// here, please put new ones with the policy implementation. For example, a
// network policy test can be moved to chrome/browser/net.
//
// Policy component dependency is not necessary for policy test. Most of
// policy values are copied into local state or Profile prefs. They can be used
// to enable policy during test.
//
// Simple policy to prefs mapping can be tested with
// chrome/test/data/policy/pref_mapping/[PolicyName].json. If the conversion is
// complicated and requires custom policy handler, we recommend to test the
// handler separately.

#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/platform/platform_event_source.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_MAC)
#include "base/compiler_specific.h"
#endif

using testing::_;
using testing::Mock;

namespace policy {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const int kOneHourInMs = 60 * 60 * 1000;
const int kThreeHoursInMs = 180 * 60 * 1000;
#endif

// Checks if WebGL is enabled in the given WebContents.
bool IsWebGLEnabled(content::WebContents* contents) {
  return content::EvalJs(contents,
                         "var canvas = document.createElement('canvas');"
                         "var context = canvas.getContext('webgl');"
                         "context != null;")
      .ExtractBool();
}

}  // namespace

// TODO(crbug.com/40684098): Deflake this test.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_Disable3DAPIs DISABLED_Disable3DAPIs
#else
#define MAYBE_Disable3DAPIs Disable3DAPIs
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_Disable3DAPIs) {
  // This test assumes Gpu access.
  if (!content::GpuDataManager::GetInstance()->HardwareAccelerationEnabled())
    return;

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  // WebGL is enabled by default.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsWebGLEnabled(contents));
  // Disable with a policy.
  PolicyMap policies;
  policies.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  UpdateProviderPolicy(policies);
  // Crash and reload the tab to get a new renderer.
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  if (content::ShouldSkipEarlyCommitPendingForCrashedFrame())
    EXPECT_TRUE(content::WaitForLoadStop(contents));
  EXPECT_FALSE(IsWebGLEnabled(contents));
  // Enable with a policy.
  policies.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  if (content::ShouldSkipEarlyCommitPendingForCrashedFrame())
    EXPECT_TRUE(content::WaitForLoadStop(contents));
  EXPECT_TRUE(IsWebGLEnabled(contents));
}

// TODO(crbug.com/40243891): Re-enable this flaky test on Linux
// and lacros asan builder.
#if BUILDFLAG(IS_LINUX) || \
    (BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER))
#define MAYBE_HomepageLocation DISABLED_HomepageLocation
#else
#define MAYBE_HomepageLocation HomepageLocation
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_HomepageLocation) {
  // Verifies that the homepage can be configured with policies.
  // Set a default, and check that the home button navigates there.
  browser()->profile()->GetPrefs()->SetString(prefs::kHomePage,
                                              chrome::kChromeUIPolicyURL);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage,
                                               false);
  EXPECT_EQ(GURL(chrome::kChromeUIPolicyURL),
            browser()->profile()->GetHomePage());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(GURL(url::kAboutBlankURL), contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  EXPECT_EQ(GURL(chrome::kChromeUIPolicyURL), contents->GetVisibleURL());

  // Now override with policy.
  PolicyMap policies;
  policies.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(chrome::kChromeUICreditsURL), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  EXPECT_TRUE(content::WaitForLoadStop(contents));
  EXPECT_EQ(GURL(chrome::kChromeUICreditsURL), contents->GetLastCommittedURL());

  policies.Set(key::kHomepageIsNewTabPage, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  EXPECT_TRUE(content::WaitForLoadStop(contents));
  EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(browser()->profile()),
            contents->GetLastCommittedURL());
}

#if BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER)
// Flaky on ASAN on Mac. See https://crbug.com/674497.
#define MAYBE_IncognitoEnabled DISABLED_IncognitoEnabled
#else
#define MAYBE_IncognitoEnabled IncognitoEnabled
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_IncognitoEnabled) {
  // Verifies that incognito windows can't be opened when disabled by policy.

  const BrowserList* active_browser_list = BrowserList::GetInstance();

  // Disable incognito via policy and verify that incognito windows can't be
  // opened.
  EXPECT_EQ(1u, active_browser_list->size());
  EXPECT_FALSE(BrowserList::IsOffTheRecordBrowserActive());
  PolicyMap policies;
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(1u, active_browser_list->size());
  EXPECT_FALSE(BrowserList::IsOffTheRecordBrowserActive());

  // Enable via policy and verify that incognito windows can be opened.
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(2u, active_browser_list->size());
  EXPECT_TRUE(BrowserList::IsOffTheRecordBrowserActive());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// We need to block mouse events in |WaitForInitialUserActivityUnsatisfied| test
// to avoid flakiness due to unexpected mouse input.
class BlockMouseEventPolicyTest : public PolicyTest {
 public:
  void SetUp() override {
    // Backup previous IgnoreNativePlatformEvents value to restore after test.
    old_ignore_native_platform_events_ =
        ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents();
    ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);

    PolicyTest::SetUp();
  }
  void TearDown() override {
    ui::PlatformEventSource::SetIgnoreNativePlatformEvents(
        old_ignore_native_platform_events_);

    PolicyTest::TearDown();
  }

 private:
  bool old_ignore_native_platform_events_;
};

IN_PROC_BROWSER_TEST_F(BlockMouseEventPolicyTest,
                       PRE_WaitForInitialUserActivityUnsatisfied) {
  // Indicate that the session started 2 hours ago and no user activity has
  // occurred yet.
  g_browser_process->local_state()->SetInt64(
      prefs::kSessionStartTime,
      (base::Time::Now() - base::Hours(2)).ToInternalValue());
}

IN_PROC_BROWSER_TEST_F(BlockMouseEventPolicyTest,
                       WaitForInitialUserActivityUnsatisfied) {
  PolicyTestAppTerminationObserver observer;

  // Require initial user activity.
  PolicyMap policies;
  policies.Set(key::kWaitForInitialUserActivity, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();

  // Set the session length limit to 1 hour. Verify that the session is not
  // terminated.
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(kOneHourInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer.WasAppTerminated());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_WaitForInitialUserActivitySatisfied) {
  // Indicate that initial user activity in this session occurred 2 hours ago.
  g_browser_process->local_state()->SetInt64(
      prefs::kSessionStartTime,
      (base::Time::Now() - base::Hours(2)).ToInternalValue());
  g_browser_process->local_state()->SetBoolean(prefs::kSessionUserActivitySeen,
                                               true);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, WaitForInitialUserActivitySatisfied) {
  PolicyTestAppTerminationObserver observer;

  // Require initial user activity and set the session length limit to 3 hours.
  // Verify that the session is not terminated.
  PolicyMap policies;
  policies.Set(key::kWaitForInitialUserActivity, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(kThreeHoursInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer.WasAppTerminated());

  // Decrease the session length limit to 1 hour. Verify that the session is
  // terminated immediately.
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(kOneHourInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.WasAppTerminated());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace policy
