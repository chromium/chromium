// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_process_selection_deferring_condition.h"

#include <queue>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/site_protection/site_familiarity_process_selection_user_data.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
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
                                           GetHistoryTestingFactory()}};
  }

  void TearDown() override {
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void AddPageVisitedYesterday(const GURL& url) {
    history_service()->AddPage(url, (base::Time::Now() - base::Hours(25)),
                               history::SOURCE_BROWSED);
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

// Test that chrome-extension:// URLs are considered familiar.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_ChromeExtensionUrl) {
  GURL kChromeExtensionUrl("chrome-extension://123/popup.html");
  content::MockNavigationHandle navigation_handle(kChromeExtensionUrl,
                                                  main_rfh());
  SiteFamiliarityProcessSelectionDeferringCondition condition(
      navigation_handle);

  EXPECT_EQ(condition.OnWillSelectFinalProcess(base::OnceClosure()),
            content::ProcessSelectionDeferringCondition::Result::kProceed);
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

// Test that an origin is considered unfamiliar if it is neither on the
// safe-browsing-high-confidence-allowlist nor chrome://history has an entry
// for the origin older than a day.
TEST_F(SiteFamiliarityProcessSelectionDeferringConditionTest,
       FamiliarityHeuristic_Unfamiliar) {
  GURL kTestUrl("https://www.example.com");
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);
  history_service()->AddPage(kTestUrl, (base::Time::Now() - base::Hours(1)),
                             history::SOURCE_BROWSED);

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
  return std::make_unique<ManualCallbackEmptyHistoryService>();
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

}  // namespace site_protection
