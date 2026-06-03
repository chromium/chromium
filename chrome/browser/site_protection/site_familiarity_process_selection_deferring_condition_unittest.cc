// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_process_selection_deferring_condition.h"

#include <memory>
#include <queue>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/site_protection/site_familiarity_fetcher.h"
#include "chrome/browser/site_protection/site_familiarity_process_selection_user_data.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/test_history_database.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_protection {
namespace {

// MockSafeBrowsingDatabaseManager which enables adding URL to high confidence
// allowlist.
//
// This mock also supports a "manual callback mode" to simulate asynchronous
// lookups in tests. When manual mode is enabled via
// `SetManualCallbackMode(true)`:
// 1. Incoming Safe Browsing lookups are queued up in `pending_callbacks_`
//    instead of completing immediately.
// 2. You must manually trigger the callbacks by calling `RunNextCallback()`.
// 3. You can inspect how many lookups are pending using
// `GetNumPendingCallbacks()`.
//
// By default, manual mode is disabled and lookups complete synchronously.
class TestSafeBrowsingDatabaseManager : public MockSafeBrowsingDatabaseManager {
 public:
  TestSafeBrowsingDatabaseManager() = default;

  void SetUrlOnHighConfidenceAllowlist(const GURL& url) {
    url_on_high_confidence_allowlist_ = url;
  }

  void CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      CheckUrlForHighConfidenceAllowlistCallback callback) override {
    num_queries_++;
    if (manual_callback_mode_) {
      pending_callbacks_.push(base::BindOnce(
          std::move(callback), url == url_on_high_confidence_allowlist_,
          std::nullopt));
      return;
    }
    std::move(callback).Run(
        /*url_on_high_confidence_allowlist=*/(
            url == url_on_high_confidence_allowlist_),
        /*logging_details=*/std::nullopt);
  }

  void SetManualCallbackMode(bool enabled) { manual_callback_mode_ = enabled; }

  void RunNextCallback() {
    if (pending_callbacks_.empty()) {
      return;
    }
    std::move(pending_callbacks_.front()).Run();
    pending_callbacks_.pop();
  }

  size_t GetNumPendingCallbacks() const { return pending_callbacks_.size(); }

  int num_queries() const { return num_queries_; }
  void ResetQueryCount() { num_queries_ = 0; }

 protected:
  ~TestSafeBrowsingDatabaseManager() override = default;

 private:
  GURL url_on_high_confidence_allowlist_;
  bool manual_callback_mode_ = false;
  std::queue<base::OnceClosure> pending_callbacks_;
  int num_queries_ = 0;
};

std::unique_ptr<KeyedService> BuildTestSiteEngagementService(
    content::BrowserContext* context) {
  return std::make_unique<site_engagement::SiteEngagementService>(context);
}

}  // anonymous namespace

// Test for SiteFamiliarityProcessSelectionDeferringCondition.
class SiteFamiliarityProcessSelectionDeferringConditionTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    safe_browsing_database_manager_ =
        base::MakeRefCounted<TestSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(
        safe_browsing_database_manager_.get());

    browser_process_ = TestingBrowserProcess::GetGlobal();
    browser_process_->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
    browser_process_->safe_browsing_service()->Initialize();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{HistoryServiceFactory::GetInstance(),
                                           GetHistoryTestingFactory()},
            TestingProfile::TestingFactory{
                site_engagement::SiteEngagementServiceFactory::GetInstance(),
                base::BindRepeating(&BuildTestSiteEngagementService)}};
  }

  void TearDown() override {
    site_protection::SiteFamiliarityFetcher::ResetFamiliarUrlsForTesting();
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void AddPageVisitedYesterday(const GURL& url) {
    history_service()->AddPage(url, (base::Time::Now() - base::Hours(25)),
                               history::SOURCE_BROWSED);
  }

  void SetSiteEngagementScore(const GURL& url, double score) {
    site_engagement::SiteEngagementService::Get(profile())
        ->ResetBaseScoreForURL(url, score);
  }

  void BuildAndWaitForConditionToRunCallback(
      content::NavigationHandle& navigation_handle) {
    SiteFamiliarityProcessSelectionDeferringCondition condition(
        navigation_handle);
    base::RunLoop run_loop;
    if (condition.OnWillSelectFinalProcess(run_loop.QuitClosure()) ==
        content::ProcessSelectionDeferringCondition::Result::kDefer) {
      run_loop.Run();
    }
  }

  virtual BrowserContextKeyedServiceFactory::TestingFactory
  GetHistoryTestingFactory() const {
    return HistoryServiceFactory::GetDefaultFactory();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  const SiteFamiliarityProcessSelectionUserData* ExtractSiteFamiliarityUserData(
      content::NavigationHandle& navigation_handle) {
    return static_cast<SiteFamiliarityProcessSelectionUserData*>(
        navigation_handle.GetProcessSelectionUserData().GetUserData(
            &SiteFamiliarityProcessSelectionUserData::kUserDataKey));
  }

  void CheckSiteFamiliar(content::NavigationHandle& navigation_handle) {
    auto* user_data = ExtractSiteFamiliarityUserData(navigation_handle);
    ASSERT_TRUE(user_data);
    EXPECT_TRUE(user_data->is_site_familiar());
  }

  void CheckSiteUnfamiliar(content::NavigationHandle& navigation_handle) {
    auto* user_data = ExtractSiteFamiliarityUserData(navigation_handle);
    ASSERT_TRUE(user_data);
    EXPECT_FALSE(user_data->is_site_familiar());
  }

 protected:
  raw_ptr<TestingBrowserProcess> browser_process_;

  scoped_refptr<TestSafeBrowsingDatabaseManager>
      safe_browsing_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

// Test that data URLs are considered unfamiliar.
// Data URLs should stay in their initiator process regardless of the data://
// URL site familiarity verdict. See https://crbug.com/452135534
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_DataUrl) {
  GURL kDataUrl("data:text/html,foo");
  content::MockNavigationHandle navigation_handle(kDataUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(condition.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  CheckSiteUnfamiliar(navigation_handle);
}

// Test that data URLs explicitly marked as familiar for testing are considered
// familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_DataUrl_FamiliarForTesting) {
  // All data: URLs are always marked as unfamiliar by SiteFamiliarityFetcher.
  GURL kDataUrl("data:text/html,foo");
  content::MockNavigationHandle navigation_handle(kDataUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);
  EXPECT_EQ(condition.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  CheckSiteUnfamiliar(navigation_handle);

  // Set data: URL as familiar.
  site_protection::SiteFamiliarityFetcher::SetUrlFamiliarForTesting(kDataUrl);
  SiteFamiliarityProcessSelectionDeferringCondition condition_familiar(
      navigation_handle);
  EXPECT_EQ(condition_familiar.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  // Site is familiar despite being a data: URL.
  CheckSiteFamiliar(navigation_handle);
}

// Test that standard https URLs explicitly marked as unfamiliar for testing are
// considered familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_HttpsUrl_FamiliarForTesting) {
  // A standard URL that is unfamiliar.
  GURL kTestUrl("https://www.example.com");
  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);
  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  BuildAndWaitForConditionToRunCallback(navigation_handle);
  CheckSiteUnfamiliar(navigation_handle);

  // Set URL as familiar.
  site_protection::SiteFamiliarityFetcher::SetUrlFamiliarForTesting(kTestUrl);
  SiteFamiliarityProcessSelectionDeferringCondition condition_familiar(
      navigation_handle);
  EXPECT_EQ(condition_familiar.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  // Site is familiar without meeting history/site engagement/SB list
  // requirements.
  CheckSiteFamiliar(navigation_handle);
}

// Test that web-safe non-http URLs (like blob:) explicitly marked as familiar
// for testing are considered familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_BlobUrl_FamiliarForTesting) {
  // All blob: URLs are marked as unfamiliar.
  GURL kBlobUrl(
      "blob:https://example.org/40a5fb5a-d56d-4a33-b4e2-0acf6a8e5f64");
  content::MockNavigationHandle navigation_handle(kBlobUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);
  EXPECT_EQ(condition.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  CheckSiteUnfamiliar(navigation_handle);

  // Set URL as familiar.
  site_protection::SiteFamiliarityFetcher::SetUrlFamiliarForTesting(kBlobUrl);
  SiteFamiliarityProcessSelectionDeferringCondition condition_familiar(
      navigation_handle);
  EXPECT_EQ(condition_familiar.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  // Site is now considered familiar.
  CheckSiteFamiliar(navigation_handle);
}

// Test that SiteFamiliarityProcessSelectionDeferringCondition does not check
// history or the safe-browsing-high-confidence-allowlist for web-safe non-http,
// non-https URLs. Most non-http,non-https web-safe schemes are not recorded in
// chrome://history so the history fetch would be useless.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_DontFetchWebSafeNonHttp) {
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  ASSERT_TRUE(policy->IsWebSafeScheme("blob"));
  GURL kWebSafeScheme(
      "blob:https://example.org/40a5fb5a-d56d-4a33-b4e2-0acf6a8e5f64");
  content::MockNavigationHandle navigation_handle(kWebSafeScheme, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(condition.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  CheckSiteUnfamiliar(navigation_handle);
}

// Test that SiteFamiliarityProcessSelectionDeferringCondition does not check
// history or the safe-browsing-high-confidence-allowlist for non-web-safe
// schemes. Non-web-safe-schemes have special handling in
// ChromeContentBrowserClient::AreV8OptimizationsDisabledForSite().
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_DontFetchNonWebSafe) {
  ASSERT_FALSE(
      content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
          "chrome"));
  GURL kTestUrl("chrome://settings");
  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(condition.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  // Returned site familiarity will be overwritten by
  // ChromeContentBrowserClient::AreV8OptimizationsDisabledForSite().
}

// Test that URLs on the safe-browsing-high-confidence-allowlist are considered
// familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_OnHighConfidenceAllowlist) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(kTestUrl);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  BuildAndWaitForConditionToRunCallback(navigation_handle);
  CheckSiteFamiliar(navigation_handle);
}

// Test that if chrome://history has an entry for the origin older than a day
// that the origin is considered familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_HistoryOlderThan24h) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  AddPageVisitedYesterday(kTestUrl);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  BuildAndWaitForConditionToRunCallback(navigation_handle);
  CheckSiteFamiliar(navigation_handle);
}

// Test that if a site has an engagement score equal to the threshold, then it
// is considered familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_EngagementScoreEqualToThreshold) {
  GURL kTestUrl("https://www.example.com");

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  BuildAndWaitForConditionToRunCallback(navigation_handle);

  CheckSiteFamiliar(navigation_handle);
}

// Test that if a site has an engagement score above the threshold, then it is
// considered familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_EngagementScoreAboveThreshold) {
  GURL kTestUrl("https://www.example.com");

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity + 1);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  BuildAndWaitForConditionToRunCallback(navigation_handle);

  CheckSiteFamiliar(navigation_handle);
}

// Test that an origin is considered unfamiliar if it is not on the
// safe-browsing-high-confidence-allowlist, chrome://history does not have an
// entry for the origin older than a day, and the site's engagement score is
// below the familiarity threshold.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_Unfamiliar) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);
  history_service()->AddPage(kTestUrl, (base::Time::Now() - base::Hours(1)),
                             history::SOURCE_BROWSED);
  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);
  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  BuildAndWaitForConditionToRunCallback(navigation_handle);
  CheckSiteUnfamiliar(navigation_handle);
}

// Similar to test above but test with an empty history service.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_Unfamiliar_EmptyHistoryService) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  BuildAndWaitForConditionToRunCallback(navigation_handle);
  CheckSiteUnfamiliar(navigation_handle);
}

namespace {

// Similar to base::MockOnceClosure but easier to repeatedly query whether the
// callback was run.
class MockConditionCallback {
 public:
  base::OnceClosure Get() {
    return base::BindOnce(&MockConditionCallback::Run, base::Unretained(this));
  }

  bool was_run() const { return was_run_; }

 private:
  void Run() { was_run_ = true; }

  bool was_run_ = false;
};

// history::HistoryService subclass which enables controlling when
// the GetLastVisitToOrigin() callback is called.
class ManualCallbackEmptyHistoryService : public history::HistoryService {
 public:
  ManualCallbackEmptyHistoryService() = default;
  ~ManualCallbackEmptyHistoryService() override = default;

  base::CancelableTaskTracker::TaskId GetLastVisitToOrigin(
      const url::Origin& origin,
      base::Time begin_time,
      base::Time end_time,
      history::VisitQuery404sPolicy policy_for_404_visits,
      GetLastVisitCallback callback,
      base::CancelableTaskTracker* tracker) override {
    callbacks_.push(std::move(callback));
    return 1;
  }

  void RunNextCallback(history::HistoryLastVisitResult result =
                           history::HistoryLastVisitResult()) {
    if (callbacks_.empty()) {
      return;
    }
    std::move(callbacks_.front()).Run(result);
    callbacks_.pop();
  }

  size_t GetNumQueuedCallbacks() const { return callbacks_.size(); }

 private:
  std::queue<GetLastVisitCallback> callbacks_;
};

std::unique_ptr<KeyedService> BuildManualCallbackEmptyHistoryService(
    content::BrowserContext* browser_context) {
  TestingProfile* profile = static_cast<TestingProfile*>(browser_context);
  auto service = std::make_unique<ManualCallbackEmptyHistoryService>();
  service->Init(history::TestHistoryDatabaseParamsForPath(profile->GetPath()));
  return service;
}

// This test fixture allows simulating asynchronous behavior of Safe Browsing
// and History lookups.
//
// By default, both lookups are asynchronous in the real implementation, but
// in tests, we can control them manually:
//
// 1. History Lookup:
//    The `ManualCallbackEmptyHistoryService` is used by default in this
//    fixture. Lookups are ALWAYS queued. They will NOT execute their callbacks
//    until `RunNextCallback()` is explicitly called on the history service.
//
// 2. Safe Browsing Lookup:
//    In these tests, Safe Browsing lookups complete synchronously by default.
//    To simulate a pending/asynchronous Safe Browsing check, you must
//    explicitly call:
//      `safe_browsing_database_manager_->SetManualCallbackMode(true);`
//    This queues up the Safe Browsing callbacks. You must then manually trigger
//    their execution by calling:
//      `safe_browsing_database_manager_->RunNextCallback();`
//
// Example pattern to test a pending state where both are waiting:
//   safe_browsing_database_manager_->SetManualCallbackMode(true);
//   ... start navigation ...
//   // Both are now pending.
//   ... resume/complete history ...
//   history_service->RunNextCallback();
//   ... resume/complete safe browsing ...
//   safe_browsing_database_manager_->RunNextCallback();
class SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest
    : public SiteFamiliarityProcessSelectionDeferringConditionTest {
 public:
  BrowserContextKeyedServiceFactory::TestingFactory GetHistoryTestingFactory()
      const override {
    return base::BindRepeating(&BuildManualCallbackEmptyHistoryService);
  }
};

}  // anonymous namespace

TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       Defer_LowEngagement_PendingSB_PendingHistory) {
  GURL kTestUrl("https://www.example.com");
  base::HistogramTester histogram_tester;

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  // 1. Low engagement.
  // 2. SB lookup pending (enable manual mode).
  safe_browsing_database_manager_->SetManualCallbackMode(true);
  // 3. History lookup pending (mock history service is pending by default).

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  // Should defer because we don't know yet.
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  // Verify tasks are queued.
  EXPECT_EQ(1u, safe_browsing_database_manager_->GetNumPendingCallbacks());
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  EXPECT_EQ(1u, mock_history_service->GetNumQueuedCallbacks());
}

TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       Defer_LowEngagement_PendingSB_UnfamiliarHistory) {
  GURL kTestUrl("https://www.example.com");
  base::HistogramTester histogram_tester;

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  // 1. Low engagement.
  // 2. SB lookup pending (enable manual mode).
  safe_browsing_database_manager_->SetManualCallbackMode(true);
  // 3. History lookup done but unfamiliar.

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  // Complete history as unfamiliar.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  mock_history_service->RunNextCallback();

  // Should still defer because SB is still pending.
  EXPECT_EQ(1u, safe_browsing_database_manager_->GetNumPendingCallbacks());
}

// Test that chrome-extension:// URLs are considered familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       FamiliarityHeuristic_ChromeExtensionUrl) {
  GURL kChromeExtensionUrl("chrome-extension://123/popup.html");
  content::MockNavigationHandle navigation_handle(kChromeExtensionUrl,
                                                  main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(condition.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
  CheckSiteFamiliar(navigation_handle);

  // chrome-extension:// URLs are known to be familiar, so we want to ensure
  // that no unnecessary history queries are made.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  EXPECT_EQ(0u, mock_history_service->GetNumQueuedCallbacks());
}

// Test that
// SiteFamiliarityProcessSelectionDeferringCondition::OnWillSelectFinalProcess()
// returns Result::kDefer if all the data has not yet been fetched when
// OnWillSelectFinalProcess() is called.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       Proceed_LowEngagement_FamiliarSB_PendingHistory) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  base::HistogramTester histogram_tester;

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(kTestUrl);

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kProceed,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  CheckSiteFamiliar(navigation_handle);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.DeferNavigationToComputeSiteFamiliarity", false,
      1);
}

TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       Defer_LowEngagement_UnfamiliarSB_PendingHistory) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  base::HistogramTester histogram_tester;

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  // SB is unfamiliar by default (not set on allowlist).
  // History is pending.

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run());

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  // Should defer because SB is unfamiliar and History is pending.
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  // Complete the history fetch (still unfamiliar).
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  mock_history_service->RunNextCallback();

  CheckSiteUnfamiliar(navigation_handle);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.DeferNavigationToComputeSiteFamiliarity", true,
      1);
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationDurationHistogram, 1);
}

TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       Proceed_HighEngagement_NoTasksQueued) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  base::HistogramTester histogram_tester;

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity);

  safe_browsing_database_manager_->SetManualCallbackMode(true);
  safe_browsing_database_manager_->ResetQueryCount();

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  // Should proceed immediately because Site Engagement is familiar.
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kProceed,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  CheckSiteFamiliar(navigation_handle);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.DeferNavigationToComputeSiteFamiliarity", false,
      1);

  // Verify no history query was made.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  EXPECT_EQ(0u, mock_history_service->GetNumQueuedCallbacks());

  // Verify no Safe Browsing query was made.
  EXPECT_EQ(0, safe_browsing_database_manager_->num_queries());
  EXPECT_EQ(0u, safe_browsing_database_manager_->GetNumPendingCallbacks());
}

TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       Proceed_LowEngagement_PendingSB_FamiliarHistory) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  base::HistogramTester histogram_tester;

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  // 1. Low engagement.
  // 2. SB lookup pending (enable manual mode).
  safe_browsing_database_manager_->SetManualCallbackMode(true);
  // 3. History lookup pending initially.

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run());  // Should be run when history completes.

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  // Should defer initially because both are pending.
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  // Complete history as familiar.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());

  history::HistoryLastVisitResult familiar_result;
  familiar_result.success = true;
  familiar_result.last_visit = base::Time::Now() - base::Hours(25);

  mock_history_service->RunNextCallback(familiar_result);

  // Should proceed now because History is familiar, even though SB is still
  // pending.
  CheckSiteFamiliar(navigation_handle);

  // It should have deferred.
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.DeferNavigationToComputeSiteFamiliarity", true,
      1);

  // Verify SB is still pending (we haven't run it, but the fetcher should have
  // cancelled it).
  EXPECT_EQ(1u, safe_browsing_database_manager_->GetNumPendingCallbacks());
}

// Test that
// SiteFamiliarityProcessSelectionDeferringCondition::OnWillSelectFinalProcess()
// returns Result::kProceed if all the data has been fetched when
// OnWillSelectFinalProcess() is called.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       DoNotDefer) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  base::HistogramTester histogram_tester;

  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(kTestUrl);

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  // Complete the history fetch.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  mock_history_service->RunNextCallback();

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kProceed,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));
  CheckSiteFamiliar(navigation_handle);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.DeferNavigationToComputeSiteFamiliarity", false,
      1);
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationDurationHistogram, 0);
}

// Test that the safe-browsing-high-confidence-allowlist is re-queried when the
// navigation is redirected.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       RequestRedirected) {
  GURL kTestUrl1("https://www.example.com");
  GURL kTestUrl2("https://www.bar.com");
  url::Origin kTestOrigin1 = url::Origin::Create(kTestUrl1);
  url::Origin kTestOrigin2 = url::Origin::Create(kTestUrl2);

  base::HistogramTester histogram_tester;
  SetSiteEngagementScore(kTestUrl1, kMinSiteEngagementScoreForFamiliarity - 1);
  SetSiteEngagementScore(kTestUrl2, kMinSiteEngagementScoreForFamiliarity - 1);
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());

  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(kTestUrl1);

  content::MockNavigationHandle navigation_handle(kTestUrl1, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);
  // Complete the history fetch.
  mock_history_service->RunNextCallback();

  navigation_handle.set_url(kTestUrl2);

  condition.OnRequestRedirected();
  mock_history_service->RunNextCallback();
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kProceed,
            condition.OnWillSelectFinalProcess(base::OnceClosure()));

  CheckSiteUnfamiliar(navigation_handle);
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationDurationHistogram, 0);
}

// Test that the safe-browsing-high-confidence-allowlist and history are
// re-queried when the navigation is redirected. This test differs from the
// RequestRedirected test because the redirect occurs while the history request
// is still pending.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest,
       RequestRedirectedDuringQuery) {
  GURL kTestUrl1("https://www.example.com");
  GURL kTestUrl2("https://www.bar.com");
  url::Origin kTestOrigin1 = url::Origin::Create(kTestUrl1);
  url::Origin kTestOrigin2 = url::Origin::Create(kTestUrl2);

  SetSiteEngagementScore(kTestUrl1, kMinSiteEngagementScoreForFamiliarity - 1);
  SetSiteEngagementScore(kTestUrl2, kMinSiteEngagementScoreForFamiliarity - 1);

  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());

  content::MockNavigationHandle navigation_handle(kTestUrl1, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  MockConditionCallback callback;
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(callback.Get()));

  navigation_handle.set_url(kTestUrl2);
  condition.OnRequestRedirected();
  EXPECT_FALSE(callback.was_run());
  EXPECT_EQ(2u, mock_history_service->GetNumQueuedCallbacks());

  // Run history request which was started prior to redirect.
  mock_history_service->RunNextCallback();
  EXPECT_FALSE(callback.was_run());

  // Run history request which was started after redirect.
  mock_history_service->RunNextCallback();
  EXPECT_TRUE(callback.was_run());

  CheckSiteUnfamiliar(navigation_handle);
}

namespace {
// Test subclass for Default Search Engine (DSE) specific tests.
class SiteFamiliarityDefaultSearchEngineTestBase
    : public SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest {
 public:
  void SetUp() override {
    SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest::SetUp();
    factory_util_ =
        std::make_unique<TemplateURLServiceFactoryTestUtil>(profile());
    factory_util_->VerifyLoad();

    TemplateURLData data;
    data.SetShortName(u"example.com");
    data.SetKeyword(u"example.com");
    data.SetURL("https://www.example.com/search?q={searchTerms}");
    TemplateURL* template_url =
        factory_util_->model()->Add(std::make_unique<TemplateURL>(data));
    factory_util_->model()->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void TearDown() override {
    factory_util_.reset();
    SiteFamiliarityProcessSelectionDeferringConditionMockLookupTest::TearDown();
  }

 protected:
  std::unique_ptr<TemplateURLServiceFactoryTestUtil> factory_util_;
};

// SiteFamiliarityDefaultSearchEngineTestBase subclass for tests that skip
// site familiarity calculations for DSE navigations.
class SiteFamiliarityDefaultSearchEngineSkipFamiliarityCheckTest
    : public SiteFamiliarityDefaultSearchEngineTestBase {
 public:
  void SetUp() override {
    SiteFamiliarityDefaultSearchEngineTestBase::SetUp();
    feature_list_.InitAndEnableFeature(
        site_protection::kSkipSiteFamiliarityDeferralForDefaultSearchEngine);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Test that a navigation to the DSE search page does not defer and is familiar.
TEST_F(SiteFamiliarityDefaultSearchEngineSkipFamiliarityCheckTest, SearchUrl) {
  GURL kSearchUrl("https://www.example.com/search?q=test2");
  SetSiteEngagementScore(kSearchUrl, kMinSiteEngagementScoreForFamiliarity - 1);
  content::MockNavigationHandle navigation_handle(kSearchUrl, main_rfh());
  base::HistogramTester histogram_tester;
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  // Proceed with process selection without deferral.
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kProceed,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  histogram_tester.ExpectUniqueSample(
      kSiteFamiliarityDeferNavigationForDefaultSearchEngineHistogram, false, 1);
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationDurationHistogram, 0);
  CheckSiteFamiliar(navigation_handle);
}

// Test that a navigation to a non-search page on the DSE's origin is deferred
// and is unfamiliar.
TEST_F(SiteFamiliarityDefaultSearchEngineSkipFamiliarityCheckTest,
       NonSearchUrl_DseOrigin) {
  GURL kHomepageUrl("https://www.example.com/");
  SetSiteEngagementScore(kHomepageUrl,
                         kMinSiteEngagementScoreForFamiliarity - 1);
  content::MockNavigationHandle navigation_handle(kHomepageUrl, main_rfh());
  base::HistogramTester histogram_tester;
  MockConditionCallback callback;

  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(callback.Get()));
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationForDefaultSearchEngineHistogram, 0);

  // Complete history fetch.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  mock_history_service->RunNextCallback();
  CheckSiteUnfamiliar(navigation_handle);
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationDurationHistogram, 1);
}

// Test that a navigation to a site unrelated to the DSE can be deferred and is
// unfamiliar.
TEST_F(SiteFamiliarityDefaultSearchEngineSkipFamiliarityCheckTest,
       NonSearchUrl_NonDseOrigin) {
  GURL kUnrelatedUrl("https://www.unrelated.com/");
  SetSiteEngagementScore(kUnrelatedUrl,
                         kMinSiteEngagementScoreForFamiliarity - 1);
  content::MockNavigationHandle navigation_handle(kUnrelatedUrl, main_rfh());
  base::HistogramTester histogram_tester;
  MockConditionCallback callback;
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(callback.Get()));
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationForDefaultSearchEngineHistogram, 0);

  // Complete history fetch.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  mock_history_service->RunNextCallback();
  CheckSiteUnfamiliar(navigation_handle);
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationDurationHistogram, 1);
}

// SiteFamiliarityDefaultSearchEngineTestBase subclass for tests that run site
// familiarity calculations for DSE navigations.
class SiteFamiliarityDefaultSearchEngineRunFamiliarityCheckTest
    : public SiteFamiliarityDefaultSearchEngineTestBase {
 public:
  void SetUp() override {
    SiteFamiliarityDefaultSearchEngineTestBase::SetUp();
    feature_list_.InitAndDisableFeature(
        site_protection::kSkipSiteFamiliarityDeferralForDefaultSearchEngine);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Test that a DSE search URL navigation with no history query is deferred,
// histogram is logged, and site is unfamiliar.
TEST_F(SiteFamiliarityDefaultSearchEngineRunFamiliarityCheckTest,
       SearchUrl_NoHistoryQuery) {
  GURL kSearchUrl("https://www.example.com/search?q=test");
  SetSiteEngagementScore(kSearchUrl, kMinSiteEngagementScoreForFamiliarity - 1);
  content::MockNavigationHandle navigation_handle(kSearchUrl, main_rfh());
  base::HistogramTester histogram_tester;
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  MockConditionCallback callback;
  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(callback.Get()));

  histogram_tester.ExpectUniqueSample(
      kSiteFamiliarityDeferNavigationForDefaultSearchEngineHistogram, true, 1);

  // Complete history fetch.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  mock_history_service->RunNextCallback();

  CheckSiteUnfamiliar(navigation_handle);
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationDurationHistogram, 1);
}

// Test that a DSE search URL with history query is not deferred,
// histogram is not logged, and site is marked unfamiliar.
TEST_F(SiteFamiliarityDefaultSearchEngineRunFamiliarityCheckTest,
       SearchUrl_HistoryQuery) {
  GURL kSearchUrl("https://www.example.com/search?q=test2");

  SetSiteEngagementScore(kSearchUrl, kMinSiteEngagementScoreForFamiliarity - 1);

  content::MockNavigationHandle navigation_handle(kSearchUrl, main_rfh());
  base::HistogramTester histogram_tester;
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  // Complete history fetch so it proceeds immediately.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  mock_history_service->RunNextCallback();

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);

  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kProceed,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  histogram_tester.ExpectUniqueSample(
      kSiteFamiliarityDeferNavigationForDefaultSearchEngineHistogram, false, 1);
  histogram_tester.ExpectTotalCount(
      kSiteFamiliarityDeferNavigationDurationHistogram, 0);

  CheckSiteUnfamiliar(navigation_handle);
}

// Test that top-frame navigations log TopFrame and overall histograms.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       VerdictLogging_TopFrame) {
  GURL kFamiliarUrl("https://familiar.test");
  GURL kUnfamiliarUrl("https://unfamiliar.test");

  SetSiteEngagementScore(kFamiliarUrl, kMinSiteEngagementScoreForFamiliarity);
  SetSiteEngagementScore(kUnfamiliarUrl,
                         kMinSiteEngagementScoreForFamiliarity - 1);

  {
    base::HistogramTester histogram_tester;
    content::MockNavigationHandle navigation_handle(kFamiliarUrl, main_rfh());
    BuildAndWaitForConditionToRunCallback(navigation_handle);

    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.SiteFamiliarity.Verdict.TopFrame",
        SiteFamiliarityFetcher::Verdict::kFamiliar, 1);
    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.SiteFamiliarity.Verdict",
        SiteFamiliarityFetcher::Verdict::kFamiliar, 1);
    histogram_tester.ExpectTotalCount(
        "SafeBrowsing.SiteFamiliarity.Verdict.Subframe", 0);
  }

  {
    base::HistogramTester histogram_tester;
    content::MockNavigationHandle navigation_handle(kUnfamiliarUrl, main_rfh());
    BuildAndWaitForConditionToRunCallback(navigation_handle);

    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.SiteFamiliarity.Verdict.TopFrame",
        SiteFamiliarityFetcher::Verdict::kUnfamiliar, 1);
    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.SiteFamiliarity.Verdict",
        SiteFamiliarityFetcher::Verdict::kUnfamiliar, 1);
    histogram_tester.ExpectTotalCount(
        "SafeBrowsing.SiteFamiliarity.Verdict.Subframe", 0);
  }
}

// Test that cross-site subframe navigations log Subframe and overall
// histograms.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       VerdictLogging_CrossSiteSubframe) {
  GURL kTopFrameUrl("https://example.test");
  GURL kFamiliarSubframeUrl("https://familiar.test");
  GURL kUnfamiliarSubframeUrl("https://unfamiliar.test");

  SetSiteEngagementScore(kFamiliarSubframeUrl,
                         kMinSiteEngagementScoreForFamiliarity);
  SetSiteEngagementScore(kUnfamiliarSubframeUrl,
                         kMinSiteEngagementScoreForFamiliarity - 1);

  // Set top frame URL in the test harness so that IsCrossSiteSubframe can
  // compare against it.
  NavigateAndCommit(kTopFrameUrl);

  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("child_frame");

  {
    base::HistogramTester histogram_tester;
    content::MockNavigationHandle navigation_handle(kFamiliarSubframeUrl,
                                                    child_rfh);
    ON_CALL(navigation_handle, GetOriginToCommit())
        .WillByDefault(
            testing::Return(url::Origin::Create(kFamiliarSubframeUrl)));
    BuildAndWaitForConditionToRunCallback(navigation_handle);

    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.SiteFamiliarity.Verdict.Subframe",
        SiteFamiliarityFetcher::Verdict::kFamiliar, 1);
    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.SiteFamiliarity.Verdict",
        SiteFamiliarityFetcher::Verdict::kFamiliar, 1);
    histogram_tester.ExpectTotalCount(
        "SafeBrowsing.SiteFamiliarity.Verdict.TopFrame", 0);
  }

  {
    base::HistogramTester histogram_tester;
    content::MockNavigationHandle navigation_handle(kUnfamiliarSubframeUrl,
                                                    child_rfh);
    ON_CALL(navigation_handle, GetOriginToCommit())
        .WillByDefault(
            testing::Return(url::Origin::Create(kUnfamiliarSubframeUrl)));
    BuildAndWaitForConditionToRunCallback(navigation_handle);

    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.SiteFamiliarity.Verdict.Subframe",
        SiteFamiliarityFetcher::Verdict::kUnfamiliar, 1);
    histogram_tester.ExpectUniqueSample(
        "SafeBrowsing.SiteFamiliarity.Verdict",
        SiteFamiliarityFetcher::Verdict::kUnfamiliar, 1);
    histogram_tester.ExpectTotalCount(
        "SafeBrowsing.SiteFamiliarity.Verdict.TopFrame", 0);
  }
}

// Test that same-site subframe navigations do NOT log any familiarity
// histograms.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       VerdictLogging_SameSiteSubframe) {
  GURL kTopFrameUrl("https://example.test");
  GURL kSameSiteSubframeUrl("https://sub.example.test/page.html");

  // Set top frame URL.
  NavigateAndCommit(kTopFrameUrl);

  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("child_frame");

  base::HistogramTester histogram_tester;
  content::MockNavigationHandle navigation_handle(kSameSiteSubframeUrl,
                                                  child_rfh);
  ON_CALL(navigation_handle, GetOriginToCommit())
      .WillByDefault(
          testing::Return(url::Origin::Create(kSameSiteSubframeUrl)));
  BuildAndWaitForConditionToRunCallback(navigation_handle);

  // Should NOT log to any Verdict histograms because it is same-site.
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.SiteFamiliarity.Verdict.TopFrame", 0);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.SiteFamiliarity.Verdict.Subframe", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SiteFamiliarity.Verdict", 0);
}

// Test that incognito navigations do NOT log any familiarity histograms.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       VerdictLogging_Incognito) {
  GURL kTestUrl("https://example.test");
  SetSiteEngagementScore(kTestUrl, kMinSiteEngagementScoreForFamiliarity);

  content::BrowserContext* otr_context =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<content::WebContents> otr_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(otr_context));

  content::MockNavigationHandle navigation_handle(
      kTestUrl, otr_web_contents->GetPrimaryMainFrame());

  // Make test site familiar.
  site_engagement::SiteEngagementService::Get(
      TestingProfile::FromBrowserContext(otr_context))
      ->ResetBaseScoreForURL(kTestUrl, kMinSiteEngagementScoreForFamiliarity);

  base::HistogramTester histogram_tester;
  BuildAndWaitForConditionToRunCallback(navigation_handle);

  // Should NOT log to any Verdict histograms because it is OTR.
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.SiteFamiliarity.Verdict.TopFrame", 0);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.SiteFamiliarity.Verdict.Subframe", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SiteFamiliarity.Verdict", 0);
}

}  // namespace

}  // namespace site_protection
