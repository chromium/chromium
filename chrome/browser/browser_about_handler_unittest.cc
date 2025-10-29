// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/policy/chrome_policy_blocklist_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::Referrer;

struct AboutURLTestCase {
  GURL test_url;
  GURL expected_url;
};

}  // namespace

class BrowserAboutHandlerTest : public testing::Test {
 protected:
  void TestHandleChromeAboutAndChromeSyncRewrite(
      const std::vector<AboutURLTestCase>& test_cases) {

    for (const auto& test_case : test_cases) {
      GURL url(test_case.test_url);
      HandleChromeAboutAndChromeSyncRewrite(&url, profile());
      EXPECT_EQ(test_case.expected_url, url);
    }
  }

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    ChromePolicyBlocklistServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindLambdaForTesting([&](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<PolicyBlocklistService>(
                  std::make_unique<policy::URLBlocklistManager>(
                      profile_->GetPrefs(), policy::policy_prefs::kUrlBlocklist,
                      policy::policy_prefs::kUrlAllowlist),
                  profile_->GetPrefs()));
        }));
  }

  TestingProfile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  // Reverts the effects of a browser quit or restart attempt,
  // specifically for testing environments to prevent test failures.
  void ResetBrowserExitState() {
    browser_shutdown::SetTryingToQuit(false);
#if BUILDFLAG(IS_CHROMEOS)
    chrome::SetSendStopRequestToSessionManager(false);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void SetBlockList(base::Value::List blocklist) {
    profile_->GetPrefs()->SetList(policy::policy_prefs::kUrlBlocklist,
                                  std::move(blocklist));
    task_environment_.RunUntilIdle();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(BrowserAboutHandlerTest, HandleChromeAboutAndChromeSyncRewrite) {
  std::string chrome_prefix(content::kChromeUIScheme);
  chrome_prefix.append(url::kStandardSchemeSeparator);
  std::vector<AboutURLTestCase> test_cases(
      {{GURL("http://google.com"), GURL("http://google.com")},
       {GURL(url::kAboutBlankURL), GURL(url::kAboutBlankURL)},
       {GURL(chrome_prefix + chrome::kChromeUIDefaultHost),
        GURL(chrome_prefix + chrome::kChromeUIVersionHost)},
       {GURL(chrome_prefix + chrome::kChromeUIAboutHost),
        GURL(chrome_prefix + chrome::kChromeUIChromeURLsHost)},
       {GURL(chrome_prefix + chrome::kChromeUISignInInternalsHost),
        GURL(chrome_prefix + chrome::kChromeUISignInInternalsHost)},
       {
           GURL(chrome_prefix + "host/path?query#ref"),
           GURL(chrome_prefix + "host/path?query#ref"),
       }});
  TestHandleChromeAboutAndChromeSyncRewrite(test_cases);
}

TEST_F(BrowserAboutHandlerTest,
       HandleChromeAboutAndChromeSyncRewriteForMDSettings) {
  std::string chrome_prefix(content::kChromeUIScheme);
  chrome_prefix.append(url::kStandardSchemeSeparator);
  std::vector<AboutURLTestCase> test_cases(
      {{GURL(chrome_prefix + chrome::kChromeUISettingsHost),
        GURL(chrome_prefix + chrome::kChromeUISettingsHost)}});
  TestHandleChromeAboutAndChromeSyncRewrite(test_cases);
}

TEST_F(BrowserAboutHandlerTest,
       HandleChromeAboutAndChromeSyncRewriteForHistory) {
  GURL::Replacements replace_foo_query;
  replace_foo_query.SetQueryStr("foo");
  GURL history_foo_url(
      GURL(chrome::kChromeUIHistoryURL).ReplaceComponents(replace_foo_query));
  TestHandleChromeAboutAndChromeSyncRewrite(std::vector<AboutURLTestCase>({
      {GURL("chrome:history"), GURL(chrome::kChromeUIHistoryURL)},
      {GURL(chrome::kChromeUIHistoryURL), GURL(chrome::kChromeUIHistoryURL)},
      {history_foo_url, history_foo_url},
  }));
}

// Ensure that minor BrowserAboutHandler fixup to a URL does not cause us to
// keep a separate virtual URL, which would not be updated on redirects.
// See https://crbug.com/449829.
TEST_F(BrowserAboutHandlerTest, NoVirtualURLForFixup) {
  GURL url("view-source:http://.foo");

  // No "fixing" of the URL is expected at the content::NavigationEntry layer.
  // We should only "fix" strings from the user (e.g. URLs from the Omnibox).
  //
  // Rewriters will remove the view-source prefix and expect it to stay in the
  // virtual URL.
  GURL expected_virtual_url = url;
  GURL expected_url("http://.foo/");

  std::unique_ptr<NavigationEntry> entry(
      NavigationController::CreateNavigationEntry(
          url, Referrer(), /* initiator_origin= */ std::nullopt,
          /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_RELOAD,
          false, std::string(), profile(),
          nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(expected_virtual_url, entry->GetVirtualURL());
  EXPECT_EQ(expected_url, entry->GetURL());
}

TEST_F(BrowserAboutHandlerTest, HandleNonNavigationAboutURL_Invalid) {
  GURL invalid_url("https:");
  ASSERT_FALSE(invalid_url.is_valid());
  EXPECT_FALSE(HandleNonNavigationAboutURL(invalid_url, profile()));
}

TEST_F(BrowserAboutHandlerTest,
       HandleNonNavigationAboutURL_QuitDebugUrlIsBlocked) {
  GURL url(chrome::kChromeUIQuitURL);
  SetBlockList(base::Value::List().Append(chrome::kChromeUIQuitURL));

  // Blocked URL should be handled and should not attempt to quit.
  EXPECT_TRUE(HandleNonNavigationAboutURL(url, profile()));
  task_environment()->RunUntilIdle();

#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
#else
  EXPECT_FALSE(chrome::IsSendingStopRequestToSessionManager());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

TEST_F(BrowserAboutHandlerTest,
       HandleNonNavigationAboutURL_QuitDebugUrlIsNotBlocked) {
  GURL url(chrome::kChromeUIQuitURL);
  SetBlockList(base::Value::List());

  // URL is not blocked, expect a quit attempt.
  EXPECT_TRUE(HandleNonNavigationAboutURL(url, profile()));
  task_environment()->RunUntilIdle();

#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
#else
  EXPECT_TRUE(chrome::IsSendingStopRequestToSessionManager());
#endif  // !BUILDFLAG(IS_CHROMEOS)
  ResetBrowserExitState();
}

TEST_F(BrowserAboutHandlerTest,
       HandleNonNavigationAboutURL_RestartDebugUrlIsBlocked) {
  GURL url(chrome::kChromeUIRestartURL);
  SetBlockList(base::Value::List().Append(chrome::kChromeUIRestartURL));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kWasRestarted));
#endif  // !BUILDFLAG(IS_ANDROID)

  // Blocked URL should be handled and should not attempt to restart.
  EXPECT_TRUE(HandleNonNavigationAboutURL(url, profile()));
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kWasRestarted));
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_F(BrowserAboutHandlerTest,
       HandleNonNavigationAboutURL_RestartDebugUrlIsNotBlocked) {
  GURL url(chrome::kChromeUIRestartURL);
  SetBlockList(base::Value::List());
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kWasRestarted));
#endif  // !BUILDFLAG(IS_ANDROID)

  // URL is not blocked, expect a restart attempt.
  EXPECT_TRUE(HandleNonNavigationAboutURL(url, profile()));
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kWasRestarted));
#endif  // !BUILDFLAG(IS_ANDROID)
  ResetBrowserExitState();
}
