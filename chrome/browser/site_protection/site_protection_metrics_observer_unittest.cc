// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_protection_metrics_observer.h"

#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/site_protection/site_familiarity_heuristic_name.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  std::optional<HighConfidenceAllowlistCheckLoggingDetails>
  CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run((url == url_on_high_confidence_allowlist_));
    return std::nullopt;
  }

 protected:
  ~TestSafeBrowsingDatabaseManager() override = default;

 private:
  GURL url_on_high_confidence_allowlist_;
};

}  // anonymous namespace

// Test for SiteProtectionMetricsObserver.
class SiteProtectionMetricsObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SiteProtectionMetricsObserverTest() = default;
  ~SiteProtectionMetricsObserverTest() override = default;

  SiteProtectionMetricsObserverTest(const SiteProtectionMetricsObserverTest&) =
      delete;
  SiteProtectionMetricsObserverTest& operator=(
      const SiteProtectionMetricsObserverTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    browser_process_ = TestingBrowserProcess::GetGlobal();

    // Create services which observe page navigation.
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        web_contents());
    SiteProtectionMetricsObserver::CreateForWebContents(web_contents());

    safe_browsing_database_manager_ =
        base::MakeRefCounted<TestSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(
        safe_browsing_database_manager_.get());

    auto* global_browser_process = TestingBrowserProcess::GetGlobal();
    global_browser_process->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
    global_browser_process->safe_browsing_service()->Initialize();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Local state is needed to construct ProxyConfigService, which is a
    // dependency of PingManager on ChromeOS.
    global_browser_process->SetLocalState(profile()->GetPrefs());
#endif
  }

  void TearDown() override {
    auto* global_browser_process = TestingBrowserProcess::GetGlobal();
    global_browser_process->SetLocalState(nullptr);
    global_browser_process->safe_browsing_service()->ShutDown();
    global_browser_process->SetSafeBrowsingService(nullptr);

    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory()}};
  }

  history::HistoryService* GetHistoryService() {
    return HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::IMPLICIT_ACCESS);
  }

  void AddPageVisitedYesterday(const GURL& url) {
    GetHistoryService()->AddPage(url, (base::Time::Now() - base::Hours(25)),
                                 history::SOURCE_BROWSED);
  }

  void NavigateAndCheckRecordedHeuristicHistograms(
      const GURL& url,
      const std::vector<SiteFamiliarityHeuristicName>& expected_heuristics) {
    const char kFamiliarityHistogramName[] =
        "SafeBrowsing.SiteProtection.FamiliarityHeuristic";

    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    base::StatisticsRecorder::ScopedHistogramSampleObserver observer(
        kFamiliarityHistogramName,
        base::BindLambdaForTesting(
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
    NavigateAndCommit(url);
    run_loop.Run();

    histogram_tester.ExpectTotalCount(kFamiliarityHistogramName,
                                      expected_heuristics.size());
    for (SiteFamiliarityHeuristicName expected_heuristic :
         expected_heuristics) {
      histogram_tester.ExpectBucketCount(kFamiliarityHistogramName,
                                         expected_heuristic, 1);
    }
  }

 protected:
  raw_ptr<TestingBrowserProcess> browser_process_;
  scoped_refptr<TestSafeBrowsingDatabaseManager>
      safe_browsing_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

// Test that SiteProtectionMetricsObserver logs the
// SiteFamiliarityHeuristicName::kNoVisitsToAnySiteMoreThanADayAgo histogram if
// history doesn't have any history entries older than 24 hours ago.
TEST_F(SiteProtectionMetricsObserverTest, NoHistoryOlderThanADayAgo) {
  GURL kUrlVisited8HoursAgo("https://bar.com");
  GURL kUrlVisitedToday("https://baz.com");

  GetHistoryService()->AddPage(kUrlVisited8HoursAgo,
                               (base::Time::Now() - base::Hours(8)),
                               history::SOURCE_BROWSED);

  NavigateAndCheckRecordedHeuristicHistograms(
      kUrlVisitedToday,
      {SiteFamiliarityHeuristicName::kNoVisitsToAnySiteMoreThanADayAgo});
}

// Test the histograms which are logged by SiteProtectionMetricsObserver based
// on how long ago the current page URL was previously visited.
TEST_F(SiteProtectionMetricsObserverTest, VisitInHistoryMoreThanADayAgo) {
  GURL kUrlVisitedYesterday("https://foo.com");
  GURL kUrlVisited8HoursAgo("https://bar.com");
  GURL kUrlVisited1HourAgo("https://baz.com");

  GetHistoryService()->AddPage(kUrlVisitedYesterday,
                               (base::Time::Now() - base::Hours(25)),
                               history::SOURCE_BROWSED);
  GetHistoryService()->AddPage(kUrlVisited8HoursAgo,
                               (base::Time::Now() - base::Hours(8)),
                               history::SOURCE_BROWSED);
  GetHistoryService()->AddPage(kUrlVisited1HourAgo, base::Time::Now(),
                               history::SOURCE_BROWSED);

  NavigateAndCheckRecordedHeuristicHistograms(
      kUrlVisitedYesterday,
      {SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo,
       SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo});
  NavigateAndCheckRecordedHeuristicHistograms(
      kUrlVisited8HoursAgo,
      {SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo});
  NavigateAndCheckRecordedHeuristicHistograms(
      kUrlVisited1HourAgo, {SiteFamiliarityHeuristicName::kNoHeuristicMatch});
}

// Test the histograms which are logged by SiteProtectionMetricsObserver for
// different levels of site engagement.
TEST_F(SiteProtectionMetricsObserverTest, SiteEngagementScore) {
  AddPageVisitedYesterday(GURL("https://bar.com"));

  GURL kUrl("https://foo.com");
  site_engagement::SiteEngagementService* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());

  site_engagement_service->ResetBaseScoreForURL(kUrl, 1);
  NavigateAndCheckRecordedHeuristicHistograms(
      kUrl, {SiteFamiliarityHeuristicName::kSiteEngagementScoreExists});

  site_engagement_service->ResetBaseScoreForURL(kUrl, 10);
  NavigateAndCheckRecordedHeuristicHistograms(
      kUrl, {SiteFamiliarityHeuristicName::kSiteEngagementScoreExists,
             SiteFamiliarityHeuristicName::kSiteEngagementScoreGte10});

  site_engagement_service->ResetBaseScoreForURL(kUrl, 25);
  NavigateAndCheckRecordedHeuristicHistograms(
      kUrl, {SiteFamiliarityHeuristicName::kSiteEngagementScoreExists,
             SiteFamiliarityHeuristicName::kSiteEngagementScoreGte10,
             SiteFamiliarityHeuristicName::kSiteEngagementScoreGte25});

  site_engagement_service->ResetBaseScoreForURL(kUrl, 50);
  NavigateAndCheckRecordedHeuristicHistograms(
      kUrl, {
                SiteFamiliarityHeuristicName::kSiteEngagementScoreExists,
                SiteFamiliarityHeuristicName::kSiteEngagementScoreGte10,
                SiteFamiliarityHeuristicName::kSiteEngagementScoreGte25,
                SiteFamiliarityHeuristicName::kSiteEngagementScoreGte50,
            });
}

// That that SiteProtectionMetricsObserver ignores engagement due to the
// in-progress navigation.
TEST_F(SiteProtectionMetricsObserverTest, IgnoreCurrentNavigationEngagement) {
  AddPageVisitedYesterday(GURL("https://bar.com"));

  GURL kUrl("https://foo.com");
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  base::StatisticsRecorder::ScopedHistogramSampleObserver observer(
      "SafeBrowsing.SiteProtection.FamiliarityHeuristic",
      base::BindLambdaForTesting(
          [&](const char* histogram_name, uint64_t name_hash,
              base::HistogramBase::Sample sample) { run_loop.Quit(); }));

  NavigateAndCommit(kUrl, ui::PAGE_TRANSITION_TYPED);
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SiteProtection.FamiliarityHeuristic",
      SiteFamiliarityHeuristicName::kNoHeuristicMatch, 1);

  site_engagement::SiteEngagementService* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());
  EXPECT_LT(0, site_engagement_service->GetScore(kUrl));
}

// Test that SiteProtectionMetricsObserver logs
// SiteFamiliarityHeuristicName::kUrlOnHighConfidenceAllowlist histogram if the
// site is on the safe browsing global allowlist.
TEST_F(SiteProtectionMetricsObserverTest, GlobalAllowlistMatch) {
  AddPageVisitedYesterday(GURL("https://baz.com"));

  GURL kUrlOnHighConfidenceAllowlist("https://foo.com");
  GURL kRegularUrl("https://bar.com");
  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(
      kUrlOnHighConfidenceAllowlist);

  NavigateAndCheckRecordedHeuristicHistograms(
      kUrlOnHighConfidenceAllowlist,
      {SiteFamiliarityHeuristicName::kGlobalAllowlistMatch});
  NavigateAndCheckRecordedHeuristicHistograms(
      kRegularUrl, {SiteFamiliarityHeuristicName::kNoHeuristicMatch});
}

}  // namespace site_protection
