// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "extensions/buildflags/buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace policy {

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(416755147): To enable this test for Android,
// ui_test_utils::HistoryEnumerator would need to get moved to a non Browser
// dependency code location and the navigation functions have to be special
// cased for a browserless invocation.
IN_PROC_BROWSER_TEST_F(PolicyTest, SavingBrowserHistoryDisabled) {
  // Verifies that browsing history is not saved.
  PolicyMap policies;
  policies.Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  GURL url = chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("empty.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Verify that the navigation wasn't saved in the history.
  ui_test_utils::HistoryEnumerator enumerator1(browser()->profile());
  EXPECT_EQ(0u, enumerator1.urls().size());

  // Now flip the policy and try again.
  policies.Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Verify that the navigation was saved in the history.
  ui_test_utils::HistoryEnumerator enumerator2(browser()->profile());
  ASSERT_EQ(1u, enumerator2.urls().size());
  EXPECT_EQ(url, enumerator2.urls()[0]);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class DeletingBrowsingHistoryPolicyTest : public PolicyTest {
 public:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  history::QueryResults GetHistory() {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            chrome_test_utils::GetProfile(this),
            ServiceAccessType::EXPLICIT_ACCESS);
    history::QueryResults history_query_results;
    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    history_service->QueryHistory(
        std::u16string(), history::QueryOptions(),
        base::BindLambdaForTesting([&](history::QueryResults results) {
          history_query_results = std::move(results);
          run_loop.Quit();
        }),
        &tracker);
    run_loop.Run();
    return history_query_results;
  }

  void ClearHistory(uint64_t remove_mask) {
    content::BrowsingDataRemover* remover =
        chrome_test_utils::GetProfile(this)->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(
        /*delete_begin=*/base::Time(), /*delete_end=*/base::Time::Max(),
        remove_mask, content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }
};

IN_PROC_BROWSER_TEST_F(DeletingBrowsingHistoryPolicyTest,
                       DeletingBrowsingHistoryDisabled) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  const GURL test_url = embedded_test_server()->GetURL("/empty.html");

  // 1. Add a history entry.
  ASSERT_TRUE(NavigateToUrl(test_url, this));
  history::QueryResults history = GetHistory();
  ASSERT_EQ(1u, history.size());
  EXPECT_EQ(test_url, history[0].url());

  // 2. Set policy to false to PREVENT history deletion.
  PolicyMap policies;
  SetPolicy(&policies, key::kAllowDeletingBrowserHistory, base::Value(false));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));

  // 3. Attempt to clear history using DATA_TYPE_NO_CHECKS to bypass the
  // DCHECK.
  ClearHistory(chrome_browsing_data_remover::DATA_TYPE_HISTORY |
               content::BrowsingDataRemover::DATA_TYPE_NO_CHECKS);

  // 4. Verify history was NOT cleared (because may_delete_history is false).
  history = GetHistory();
  ASSERT_EQ(1u, history.size());
  EXPECT_EQ(test_url, history[0].url());

  // 5. Set policy to true to ALLOW history deletion.
  policies.Clear();  // Clear previous policy
  SetPolicy(&policies, key::kAllowDeletingBrowserHistory, base::Value(true));
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));

  // 6. Clear history (this time it should work).
  ClearHistory(chrome_browsing_data_remover::DATA_TYPE_HISTORY);

  // 7. Verify history WAS cleared.
  history = GetHistory();
  EXPECT_EQ(0u, history.size());
}

}  // namespace policy
