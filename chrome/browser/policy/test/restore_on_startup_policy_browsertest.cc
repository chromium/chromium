// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <vector>

#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/url_blocking_policy_test_utils.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker_test_support.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr const char* kRestoredURLs[] = {
    "http://aaa.com/empty.html",
    "http://bbb.com/empty.html",
};

bool IsNonSwitchArgument(const base::CommandLine::StringType& s) {
  return s.empty() || s[0] != '-';
}

}  // namespace

// Similar to PolicyTest but allows setting policies before the browser is
// created. Each test parameter is a method that sets up the early policies
// and stores the expected startup URLs in |expected_urls_|.
class RestoreOnStartupPolicyTest : public UrlBlockingPolicyTest,
                                   public testing::WithParamInterface<void (
                                       RestoreOnStartupPolicyTest::*)(void)> {
 public:
  RestoreOnStartupPolicyTest() {
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
  }

  ~RestoreOnStartupPolicyTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    // Set early policies now, before the browser is created.
    (this->*(GetParam()))();

    // Remove the non-switch arguments, so that session restore kicks in for
    // these tests.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringVector argv = command_line->argv();
    std::erase_if(argv, IsNonSwitchArgument);
    command_line->InitFromArgv(argv);
    ASSERT_TRUE(base::ranges::equal(argv, command_line->argv()));
  }

  void ListOfURLs() {
    // Verifies that policy can set the startup pages to a list of URLs.
    base::Value::List urls;
    for (const auto* url : kRestoredURLs) {
      urls.Append(url);
      expected_urls_.push_back(GURL(url));
    }
    PolicyMap policies;
    policies.Set(key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(SessionStartupPref::kPrefValueURLs), nullptr);
    policies.Set(key::kRestoreOnStartupURLs, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(std::move(urls)), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void NTP() {
    // Verifies that policy can set the startup page to the NTP.
    PolicyMap policies;
    policies.Set(key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(SessionStartupPref::kPrefValueNewTab), nullptr);
    provider_.UpdateChromePolicy(policies);
    expected_urls_.push_back(GURL(chrome::kChromeUINewTabURL));
  }

  void Last() {
    // Verifies that policy can set the startup pages to the last session.
    PolicyMap policies;
    policies.Set(key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(SessionStartupPref::kPrefValueLast), nullptr);
    provider_.UpdateChromePolicy(policies);
    // This should restore the tabs opened at PRE_RunTest below.
    for (const auto* url : kRestoredURLs)
      expected_urls_.push_back(GURL(url));
  }

  void LastAndListOfURLs() {
    // Verifies that policy can set the startup pages to "the last session and a
    // list of URLs". |expected_urls_| will be restored from the last session.
    // |expected_urls_in_new_window_| will be opened on a policy-designated new
    // window.
    base::Value::List urls;
    for (const auto* url : kRestoredURLs) {
      urls.Append(url);
      expected_urls_.emplace_back(url);
      expected_urls_in_new_window_.emplace_back(url);
    }
    PolicyMap policies;
    policies.Set(key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(SessionStartupPref::kPrefValueLastAndURLs),
                 nullptr);
    policies.Set(key::kRestoreOnStartupURLs, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(std::move(urls)), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void Blocked() {
    // Verifies that URLs are blocked during session restore.
    PolicyMap policies;
    policies.Set(key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(SessionStartupPref::kPrefValueLast), nullptr);
    base::Value::List urls;
    for (const auto* url_string : kRestoredURLs)
      urls.Append(url_string);
    policies.Set(key::kURLBlocklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(std::move(urls)), nullptr);
    provider_.UpdateChromePolicy(policies);
    // This should restore the tabs opened at PRE_RunTest below, yet all should
    // be blocked.
    blocked_ = true;
    for (const auto* url_string : kRestoredURLs)
      expected_urls_.emplace_back(url_string);
  }

  // Check if |kRestoredURLs| are opened on the current browser.
  bool AreRestoredURLsOpened() const {
    TabStripModel* model = browser()->tab_strip_model();
    if (model->count() != std::size(kRestoredURLs))
      return false;
    for (int i = 0; i < model->count(); ++i) {
      if (model->GetWebContentsAt(i)->GetVisibleURL() != kRestoredURLs[i])
        return false;
    }
    return true;
  }

  base::test::ScopedFeatureList feature_list_;

  // URLs that are expected to be loaded.
  std::vector<GURL> expected_urls_;

  // URLs that are expected to be loaded in a new window.
  std::vector<GURL> expected_urls_in_new_window_;

  // True if the loaded URLs should be blocked by policy.
  bool blocked_ = false;
};

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, PRE_RunTest) {
  // Do not show Welcome Page.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage,
                                               true);

  // If policy urls are set, those might be opened at startup. Because
  // some tabs are already opened, we don't need to navigate or open more tabs
  // for verification of tab restoration.
  if (AreRestoredURLsOpened())
    return;

  // Open some tabs to verify if they are restored after the browser restarts.
  // Most policy settings override this, except kPrefValueLast and
  // kPrefValueLastAndURLs which enforce a restore.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kRestoredURLs[0])));
  for (size_t i = 1; i < std::size(kRestoredURLs); ++i) {
    content::CreateAndLoadWebContentsObserver observer;
    chrome::AddSelectedTabWithURL(browser(), GURL(kRestoredURLs[i]),
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
}

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, RunTest) {
  {
    TabStripModel* model = browser()->tab_strip_model();
    int size = static_cast<int>(expected_urls_.size());
    EXPECT_EQ(size, model->count());
    resource_coordinator::WaitForTransitionToLoaded(model);
    for (int i = 0; i < size && i < model->count(); ++i) {
      content::WebContents* web_contents = model->GetWebContentsAt(i);
      if (blocked_) {
        CheckURLIsBlockedInWebContents(web_contents, expected_urls_[i]);
      } else if (expected_urls_[i] == GURL(chrome::kChromeUINewTabURL)) {
        EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(browser()->profile()),
                  web_contents->GetLastCommittedURL());
      } else {
        EXPECT_EQ(expected_urls_[i], web_contents->GetLastCommittedURL());
      }
    }
  }
  // Policy urls should be opened on a new window if the startup policy is set
  // as kPrefValueLastAndURLs.
  if (!expected_urls_in_new_window_.empty()) {
    ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    Browser* pref_urls_opened_browser =
        chrome::FindLastActiveWithProfile(browser()->profile());
    ASSERT_TRUE(pref_urls_opened_browser);
    TabStripModel* model = pref_urls_opened_browser->tab_strip_model();
    int size = static_cast<int>(expected_urls_in_new_window_.size());
    EXPECT_EQ(size, model->count());
    resource_coordinator::WaitForTransitionToLoaded(model);
    for (int i = 0; i < size && i < model->count(); ++i) {
      content::WebContents* web_contents = model->GetWebContentsAt(i);
      EXPECT_EQ(expected_urls_[i], web_contents->GetURL());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    RestoreOnStartupPolicyTestInstance,
    RestoreOnStartupPolicyTest,
    testing::Values(&RestoreOnStartupPolicyTest::ListOfURLs,
                    &RestoreOnStartupPolicyTest::NTP,
                    &RestoreOnStartupPolicyTest::Last,
                    &RestoreOnStartupPolicyTest::LastAndListOfURLs,
                    &RestoreOnStartupPolicyTest::Blocked));
}  // namespace policy
