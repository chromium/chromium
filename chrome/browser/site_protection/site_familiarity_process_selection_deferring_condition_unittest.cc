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
#include "testing/gtest/include/gtest/gtest.h"

namespace site_protection {
namespace {

// MockSafeBrowsingDatabaseManager which enables adding URL to high confidence
// allowlist.
class TestSafeBrowsingDatabaseManager : public MockSafeBrowsingDatabaseManager {
 public:
  TestSafeBrowsingDatabaseManager() = default;

  void SetUrlOnHighConfidenceAllowlist(const GURL& url) {
    url_on_high_confidence_allowlist_ = url;
  }

  void CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      CheckUrlForHighConfidenceAllowlistCallback callback) override {
    std::move(callback).Run(
        /*url_on_high_confidence_allowlist=*/(
            url == url_on_high_confidence_allowlist_),
        /*logging_details=*/std::nullopt);
  }

 protected:
  ~TestSafeBrowsingDatabaseManager() override = default;

 private:
  GURL url_on_high_confidence_allowlist_;
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
    condition.OnWillSelectFinalProcess(run_loop.QuitClosure());
    run_loop.Run();
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

  void RunNextCallback() {
    if (callbacks_.empty()) {
      return;
    }
    std::move(callbacks_.front()).Run(history::HistoryLastVisitResult());
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

// SiteFamiliarityProcessSelectionDeferringConditionTest subclass which uses a
// mock history service.
class SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest
    : public SiteFamiliarityProcessSelectionDeferringConditionTest {
 public:
  BrowserContextKeyedServiceFactory::TestingFactory GetHistoryTestingFactory()
      const override {
    return base::BindRepeating(&BuildManualCallbackEmptyHistoryService);
  }
};

}  // anonymous namespace

// Test that chrome-extension:// URLs are considered familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest,
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
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest,
       Defer) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  base::HistogramTester histogram_tester;

  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(kTestUrl);

  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run());

  content::MockNavigationHandle navigation_handle(kTestUrl, main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(content::ProcessSelectionDeferringCondition::Result::kDefer,
            condition.OnWillSelectFinalProcess(mock_callback.Get()));

  // Complete the history fetch.
  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());
  mock_history_service->RunNextCallback();

  CheckSiteFamiliar(navigation_handle);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.DeferNavigationToComputeSiteFamiliarity", true,
      1);
}

// Test that
// SiteFamiliarityProcessSelectionDeferringCondition::OnWillSelectFinalProcess()
// returns Result::kProceed if all the data has been fetched when
// OnWillSelectFinalProcess() is called.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest,
       DoNotDefer) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);

  base::HistogramTester histogram_tester;

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
}

// Test that the safe-browsing-high-confidence-allowlist is re-queried when the
// navigation is redirected.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest,
       RequestRedirected) {
  GURL kTestUrl1("https://www.example.com");
  GURL kTestUrl2("https://www.bar.com");
  url::Origin kTestOrigin1 = url::Origin::Create(kTestUrl1);
  url::Origin kTestOrigin2 = url::Origin::Create(kTestUrl2);

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
}

// Test that the safe-browsing-high-confidence-allowlist and history are
// re-queried when the navigation is redirected. This test differs from the
// RequestRedirected test because the redirect occurs while the history request
// is still pending.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest,
       RequestRedirectedDuringQuery) {
  GURL kTestUrl1("https://www.example.com");
  GURL kTestUrl2("https://www.bar.com");
  url::Origin kTestOrigin1 = url::Origin::Create(kTestUrl1);
  url::Origin kTestOrigin2 = url::Origin::Create(kTestUrl2);

  raw_ptr<ManualCallbackEmptyHistoryService> mock_history_service =
      static_cast<ManualCallbackEmptyHistoryService*>(history_service());

  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(kTestUrl1);

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
    : public SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest {
 public:
  void SetUp() override {
    SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest::SetUp();
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
    SiteFamiliarityProcessSelectionDeferringConditionMockHistoryTest::
        TearDown();
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
  CheckSiteFamiliar(navigation_handle);
}

// Test that a navigation to a non-search page on the DSE's origin is deferred
// and is unfamiliar.
TEST_F(SiteFamiliarityDefaultSearchEngineSkipFamiliarityCheckTest,
       NonSearchUrl_DseOrigin) {
  GURL kHomepageUrl("https://www.example.com/");
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
}

// Test that a navigation to a site unrelated to the DSE can be deferred and is
// unfamiliar.
TEST_F(SiteFamiliarityDefaultSearchEngineSkipFamiliarityCheckTest,
       NonSearchUrl_NonDseOrigin) {
  GURL kUnrelatedUrl("https://www.unrelated.com/");
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
}

// Test that a DSE search URL with history query is not deferred,
// histogram is not logged, and site is marked unfamiliar.
TEST_F(SiteFamiliarityDefaultSearchEngineRunFamiliarityCheckTest,
       SearchUrl_HistoryQuery) {
  GURL kSearchUrl("https://www.example.com/search?q=test2");

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

  CheckSiteUnfamiliar(navigation_handle);
}

}  // anonymous namespace

}  // namespace site_protection
