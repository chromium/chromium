// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_protection_metrics_observer.h"

#include "base/base_switches.h"
#include "base/metrics/statistics_recorder.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/site_protection/site_familiarity_heuristic_name.h"
#include "chrome/browser/site_protection/site_protection_metrics.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/common/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/process_selection_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
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

    SetUpForNewWebContents();

    safe_browsing_database_manager_ =
        base::MakeRefCounted<TestSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(
        safe_browsing_database_manager_.get());

    browser_process_->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
    browser_process_->safe_browsing_service()->Initialize();
  }

  void TearDown() override {
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);

    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory()}};
  }

  void SetIncognito() {
    Profile* const otr_profile =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    EXPECT_TRUE(otr_profile->IsIncognitoProfile());
    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::Create(otr_profile);
    SetContents(content::WebContentsTester::CreateTestWebContents(
        otr_profile, std::move(site_instance)));

    SetUpForNewWebContents();
  }

  void SetUpForNewWebContents() {
    // Create services which observe page navigation.
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        web_contents());
    SiteProtectionMetricsObserver::CreateForWebContents(web_contents());
  }

  history::HistoryService* GetRegularProfileHistoryService() {
    return HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::IMPLICIT_ACCESS);
  }

  void AddPageVisitedYesterdayToRegularProfile(const GURL& url) {
    GetRegularProfileHistoryService()->AddPage(
        url, (base::Time::Now() - base::Hours(25)), history::SOURCE_BROWSED);
  }

  void NavigateAndWaitForHistogram(const GURL& url,
                                   const std::string& histogram_name) {
    base::RunLoop run_loop;
    base::StatisticsRecorder::ScopedHistogramSampleObserver observer(
        histogram_name,
        base::BindLambdaForTesting(
            [&](std::string_view histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample32 sample) { run_loop.Quit(); }));
    NavigateAndCommit(url);
    run_loop.Run();
  }

  void NavigateAndCheckRecordedHeuristicHistograms(
      const GURL& url,
      const std::vector<SiteFamiliarityHeuristicName>& expected_heuristics) {
    const char kFamiliarityHistogramName[] =
        "SafeBrowsing.SiteProtection.FamiliarityHeuristic";

    base::HistogramTester histogram_tester;
    NavigateAndWaitForHistogram(url, kFamiliarityHistogramName);

    histogram_tester.ExpectTotalCount(kFamiliarityHistogramName,
                                      expected_heuristics.size());
    for (SiteFamiliarityHeuristicName expected_heuristic :
         expected_heuristics) {
      histogram_tester.ExpectBucketCount(kFamiliarityHistogramName,
                                         expected_heuristic, 1);
    }
  }

  void NavigateAndCheckCandidate1HeuristicHistogram(const GURL& url,
                                                    bool expected_value) {
    const char kCandidate1HeuristicHistogramName[] =
        "SafeBrowsing.SiteProtection.FamiliarityHeuristic."
        "Engagement15OrVisitedBeforeTodayOrHighConfidence";
    base::HistogramTester histogram_tester;
    NavigateAndWaitForHistogram(url, kCandidate1HeuristicHistogramName);
    histogram_tester.ExpectUniqueSample(kCandidate1HeuristicHistogramName,
                                        expected_value, 1);
  }

  int64_t GetUkmFamiliarityHeuristicValue(ukm::TestUkmRecorder& ukm_recorder,
                                          const std::string& metric_name) {
    std::vector<int64_t> values = ukm_recorder.GetMetricsEntryValues(
        "SiteFamiliarityHeuristicResult", metric_name);
    return values.size() == 1u ? values[0] : -1;
  }

  int64_t GetUkmHistoryFamiliarityHeuristicValue(
      ukm::TestUkmRecorder& ukm_recorder) {
    return GetUkmFamiliarityHeuristicValue(ukm_recorder,
                                           "SiteFamiliarityHistoryHeuristic");
  }

  void NavigateAndCheckRecordedHeuristicUkm(const GURL& url,
                                            const std::string& metric_name,
                                            int64_t expected_value) {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    base::RunLoop run_loop;
    ukm_recorder.SetOnAddEntryCallback(
        ukm::builders::SiteFamiliarityHeuristicResult::kEntryName,
        run_loop.QuitClosure());
    NavigateAndCommit(url);
    run_loop.Run();
    EXPECT_EQ(expected_value,
              GetUkmFamiliarityHeuristicValue(ukm_recorder, metric_name));
  }

  bool AreV8OptimizersEnabled(content::RenderFrameHost* rfh) {
    return !rfh->GetProcess()->AreV8OptimizationsDisabled();
  }

 protected:
  raw_ptr<TestingBrowserProcess> browser_process_;

  scoped_refptr<TestSafeBrowsingDatabaseManager>
      safe_browsing_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

// Test that SiteProtectionMetricsObserver logs the correct histogram and UKM if
// history doesn't have any history entries older than 24 hours ago.
TEST_F(SiteProtectionMetricsObserverTest, NoHistoryOlderThanADayAgo) {
  GURL kUrlVisited8HoursAgo("https://bar.com");
  GURL kUrlVisitedToday("https://baz.com");

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GetRegularProfileHistoryService()->AddPage(
      kUrlVisited8HoursAgo, (base::Time::Now() - base::Hours(8)),
      history::SOURCE_BROWSED);

  NavigateAndCheckRecordedHeuristicHistograms(
      kUrlVisitedToday,
      {SiteFamiliarityHeuristicName::kNoVisitsToAnySiteMoreThanADayAgo});
  EXPECT_EQ(static_cast<int>(SiteFamiliarityHistoryHeuristicName::
                                 kNoVisitsToAnySiteMoreThanADayAgo),
            GetUkmHistoryFamiliarityHeuristicValue(ukm_recorder));
}

// Test the histograms and UKM which are logged by SiteProtectionMetricsObserver
// based on how long ago the current page URL was previously visited.
TEST_F(SiteProtectionMetricsObserverTest, VisitInHistoryMoreThanADayAgo) {
  GURL kUrlVisitedYesterday("https://foo.com");
  GURL kUrlVisited8HoursAgo("https://bar.com");
  GURL kUrlVisited1HourAgo("https://baz.com");

  GetRegularProfileHistoryService()->AddPage(
      kUrlVisitedYesterday, (base::Time::Now() - base::Hours(25)),
      history::SOURCE_BROWSED);
  GetRegularProfileHistoryService()->AddPage(
      kUrlVisited8HoursAgo, (base::Time::Now() - base::Hours(8)),
      history::SOURCE_BROWSED);
  GetRegularProfileHistoryService()->AddPage(
      kUrlVisited1HourAgo, base::Time::Now(), history::SOURCE_BROWSED);

  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    NavigateAndCheckRecordedHeuristicHistograms(
        kUrlVisitedYesterday,
        {SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo,
         SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo});
    EXPECT_EQ(static_cast<int>(
                  SiteFamiliarityHistoryHeuristicName::kVisitedMoreThanADayAgo),
              GetUkmHistoryFamiliarityHeuristicValue(ukm_recorder));
  }

  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    NavigateAndCheckRecordedHeuristicHistograms(
        kUrlVisited8HoursAgo,
        {SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo});
    EXPECT_EQ(
        static_cast<int>(
            SiteFamiliarityHistoryHeuristicName::kVisitedMoreThanFourHoursAgo),
        GetUkmHistoryFamiliarityHeuristicValue(ukm_recorder));
  }

  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    NavigateAndCheckRecordedHeuristicHistograms(
        kUrlVisited1HourAgo, {SiteFamiliarityHeuristicName::kNoHeuristicMatch});
    EXPECT_EQ(static_cast<int>(
                  SiteFamiliarityHistoryHeuristicName::kNoHeuristicMatch),
              GetUkmHistoryFamiliarityHeuristicValue(ukm_recorder));
  }
}

// Test the histograms which are logged by SiteProtectionMetricsObserver for
// different levels of site engagement.
TEST_F(SiteProtectionMetricsObserverTest, SiteEngagementScore) {
  AddPageVisitedYesterdayToRegularProfile(GURL("https://bar.com"));

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
  AddPageVisitedYesterdayToRegularProfile(GURL("https://bar.com"));

  GURL kUrl("https://foo.com");
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  base::StatisticsRecorder::ScopedHistogramSampleObserver observer(
      "SafeBrowsing.SiteProtection.FamiliarityHeuristic",
      base::BindLambdaForTesting(
          [&](std::string_view histogram_name, uint64_t name_hash,
              base::HistogramBase::Sample32 sample) { run_loop.Quit(); }));

  NavigateAndCommit(kUrl, ui::PAGE_TRANSITION_TYPED);
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SiteProtection.FamiliarityHeuristic",
      SiteFamiliarityHeuristicName::kNoHeuristicMatch, 1);

  site_engagement::SiteEngagementService* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());
  EXPECT_LT(0, site_engagement_service->GetScore(kUrl));
}

// Test that SiteProtectionMetricsObserver logs the site engagement to UKM.
TEST_F(SiteProtectionMetricsObserverTest, SiteEngagementScoreUkm) {
  GURL kUrl("https://foo.com");
  const int kSiteEngagement = 15;
  // Site engagement should be rounded down to multiple of 10 in UKM.
  const int kExpectedUkmSiteEngagement = 10;

  site_engagement::SiteEngagementService* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());
  site_engagement_service->ResetBaseScoreForURL(kUrl, kSiteEngagement);
  GetRegularProfileHistoryService()->AddPage(
      kUrl, (base::Time::Now() - base::Hours(1)), history::SOURCE_BROWSED);

  NavigateAndCheckRecordedHeuristicUkm(kUrl, "SiteEngagementScore",
                                       kExpectedUkmSiteEngagement);
}

// Test that SiteProtectionMetricsObserver logs the correct histograms and UKM
// if the site is on the safe browsing global allowlist.
TEST_F(SiteProtectionMetricsObserverTest, GlobalAllowlistMatch) {
  AddPageVisitedYesterdayToRegularProfile(GURL("https://baz.com"));

  GURL kUrlOnHighConfidenceAllowlist("https://foo.com");
  GURL kRegularUrl("https://bar.com");
  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(
      kUrlOnHighConfidenceAllowlist);

  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    NavigateAndCheckRecordedHeuristicHistograms(
        kUrlOnHighConfidenceAllowlist,
        {SiteFamiliarityHeuristicName::kGlobalAllowlistMatch});
    EXPECT_EQ(true, GetUkmFamiliarityHeuristicValue(
                        ukm_recorder, "OnHighConfidenceAllowlist"));
  }

  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    NavigateAndCheckRecordedHeuristicHistograms(
        kRegularUrl, {SiteFamiliarityHeuristicName::kNoHeuristicMatch});
    EXPECT_EQ(false, GetUkmFamiliarityHeuristicValue(
                         ukm_recorder, "OnHighConfidenceAllowlist"));
  }
}

// Test that SiteProtectionMetricsObserver logs the correct histograms and UKM
// if the SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo heuristic
// applies for the current visit to the site but the heuristic did not apply
// to the previous visit to the site.
TEST_F(SiteProtectionMetricsObserverTest, SiteFamiliarWasPreviouslyUnfamiliar) {
  GURL kPageUrl("https://bar.com");
  GURL kOtherUrl("https://baz.com");

  base::Time now = base::Time::Now();
  base::Time kPageVisitTime1 = now - base::Hours(25);
  base::Time kPageVisitTime2 = kPageVisitTime1 - base::Hours(25);
  base::Time kOtherVisitTime = kPageVisitTime2 - base::Hours(25);

  GetRegularProfileHistoryService()->AddPage(kPageUrl, kPageVisitTime1,
                                             history::SOURCE_BROWSED);
  GetRegularProfileHistoryService()->AddPage(kOtherUrl, kOtherVisitTime,
                                             history::SOURCE_BROWSED);
  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    NavigateAndCheckRecordedHeuristicHistograms(
        kPageUrl,
        {SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo,
         SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo,
         SiteFamiliarityHeuristicName::kFamiliarLikelyPreviouslyUnfamiliar});
    EXPECT_EQ(static_cast<int>(SiteFamiliarityHistoryHeuristicName::
                                   kVisitedMoreThanADayAgoPreviouslyUnfamiliar),
              GetUkmHistoryFamiliarityHeuristicValue(ukm_recorder));
  }

  GetRegularProfileHistoryService()->AddPage(kPageUrl, kPageVisitTime2,
                                             history::SOURCE_BROWSED);

  {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    NavigateAndCheckRecordedHeuristicHistograms(
        kPageUrl, {SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo,
                   SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo});
    EXPECT_EQ(static_cast<int>(
                  SiteFamiliarityHistoryHeuristicName::kVisitedMoreThanADayAgo),
              GetUkmHistoryFamiliarityHeuristicValue(ukm_recorder));
  }
}

// Test that SiteProtectionMetricsObserver logs the correct histograms
// if:
// - SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo heuristic
//   applies for the current visit.
// AND
// -  SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo heuristic
//   does not apply to the previous visit.
// AND
// - SiteFamiliarityHeuristicName::kNoVisitsToAnySiteMoreThanADayAgo
//   applies to the previous visit.
TEST_F(SiteProtectionMetricsObserverTest,
       SiteFamiliarWasPreviouslyUnfamiliarDueToNoOldHistory) {
  GURL kPageUrl("https://bar.com");
  GURL kOtherUrl("https://baz.com");

  base::Time now = base::Time::Now();
  base::Time kPageVisitTime = now - base::Hours(25);
  base::Time kOtherVisitTime1 = kPageVisitTime - base::Hours(1);
  base::Time kOtherVisitTime2 = kOtherVisitTime1 - base::Hours(100);

  GetRegularProfileHistoryService()->AddPage(kPageUrl, kPageVisitTime,
                                             history::SOURCE_BROWSED);
  GetRegularProfileHistoryService()->AddPage(kOtherUrl, kOtherVisitTime1,
                                             history::SOURCE_BROWSED);

  NavigateAndCheckRecordedHeuristicHistograms(
      kPageUrl, {SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo,
                 SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo});

  GetRegularProfileHistoryService()->AddPage(kOtherUrl, kOtherVisitTime2,
                                             history::SOURCE_BROWSED);
  NavigateAndCheckRecordedHeuristicHistograms(
      kPageUrl,
      {SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo,
       SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo,
       SiteFamiliarityHeuristicName::kFamiliarLikelyPreviouslyUnfamiliar});
}

// Test that SiteProtectionMetricsObserver logs whether any heuristics matched
// to UKM.
TEST_F(SiteProtectionMetricsObserverTest, AnyHeuristicsMatchUkm) {
  GURL kUrlVisitedYesterday("https://foo.com");
  GURL kUrlVisitedNever("https://bar.com");

  AddPageVisitedYesterdayToRegularProfile(kUrlVisitedYesterday);

  NavigateAndCheckRecordedHeuristicUkm(kUrlVisitedYesterday,
                                       "AnyHeuristicsMatch", true);
  NavigateAndCheckRecordedHeuristicUkm(kUrlVisitedNever, "AnyHeuristicsMatch",
                                       false);
}

// Test that the SiteProtectionMetricsObserver does not log any metrics for
// file:// URLs.
TEST_F(SiteProtectionMetricsObserverTest, FileUrlNoMetricsLogged) {
  GURL kFileUrlVisitedNever("file:///usr/");
  GURL kUrlVisitedNever("https://bar.com");

  SiteProtectionMetricsObserver* site_protection_observer =
      SiteProtectionMetricsObserver::FromWebContents(web_contents());

  base::HistogramTester histogram_tester;

  // Check that there are no pending asynchronous tasks which would perhaps log
  // UMA when run.
  NavigateAndCommit(kFileUrlVisitedNever);
  EXPECT_FALSE(site_protection_observer->HasPendingTasksForTesting());

  // Check that there are pending asynchronous tasks which would log UMA when
  // run.
  NavigateAndCommit(kUrlVisitedNever);
  EXPECT_TRUE(site_protection_observer->HasPendingTasksForTesting());

  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.SiteProtection.FamiliarityHeuristic", 0u);
}

// Test that SiteProtectionMetricsObserver logs to
// SafeBrowsing.SiteProtection.FamiliarityHeuristic.OffTheRecord when in
// incognito.
TEST_F(SiteProtectionMetricsObserverTest, Incognito) {
  const char kRegularProfileHistogramName[] =
      "SafeBrowsing.SiteProtection.FamiliarityHeuristic";
  const char kOffTheRecordHistogramName[] =
      "SafeBrowsing.SiteProtection.FamiliarityHeuristic.OffTheRecord";
  GURL kUrlVisitedNever("https://bar.com");

  SetIncognito();

  base::HistogramTester histogram_tester;
  NavigateAndWaitForHistogram(kUrlVisitedNever, kOffTheRecordHistogramName);
  histogram_tester.ExpectUniqueSample(
      kOffTheRecordHistogramName,
      SiteFamiliarityHeuristicName::kNoVisitsToAnySiteMoreThanADayAgo, 1);
  histogram_tester.ExpectTotalCount(kRegularProfileHistogramName, 0u);
}

// Test that
// SafeBrowsing.SiteProtection.FamiliarityHeuristic
//     .Engagement15OrVisitedBeforeTodayOrHighConfidence
// is properly recorded.
TEST_F(SiteProtectionMetricsObserverTest, MatchesHeuristicCandidate) {
  history::HistoryService* history_service = GetRegularProfileHistoryService();
  site_engagement::SiteEngagementService* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());
  const base::Time kTimeNow = base::Time::Now();
  const base::Time kTimeHourAgo = kTimeNow - base::Hours(1);
  const base::Time kTimeYesterday = kTimeNow - base::Days(1);

  GURL kUrlVisitedTodayEngagement5("https://bar.com");
  history_service->AddPage(kUrlVisitedTodayEngagement5, kTimeHourAgo,
                           history::SOURCE_BROWSED);
  site_engagement_service->ResetBaseScoreForURL(kUrlVisitedTodayEngagement5, 5);
  NavigateAndCheckCandidate1HeuristicHistogram(kUrlVisitedTodayEngagement5,
                                               /*expected_value=*/false);

  GURL kUrlVisitedTodayEngagement15("https://baz.com");
  history_service->AddPage(kUrlVisitedTodayEngagement15, kTimeHourAgo,
                           history::SOURCE_BROWSED);
  site_engagement_service->ResetBaseScoreForURL(kUrlVisitedTodayEngagement15,
                                                15);
  NavigateAndCheckCandidate1HeuristicHistogram(kUrlVisitedTodayEngagement15,
                                               /*expected_value=*/true);

  GURL kUrlVisitedYesterday("https://foo.com");
  history_service->AddPage(kUrlVisitedYesterday, kTimeYesterday,
                           history::SOURCE_BROWSED);
  NavigateAndCheckCandidate1HeuristicHistogram(kUrlVisitedYesterday,
                                               /*expected_value=*/true);

  GURL kUrlVisitedNeverHighConfidenceAllowlist("https://hi.com");
  safe_browsing_database_manager_->SetUrlOnHighConfidenceAllowlist(
      kUrlVisitedNeverHighConfidenceAllowlist);
  NavigateAndCheckCandidate1HeuristicHistogram(
      kUrlVisitedNeverHighConfidenceAllowlist,
      /*expected_value=*/true);
}

// MockRenderProcessHost subclass with custom v8-optimizer state.
class TestRenderProcessHost : public content::MockRenderProcessHost {
 public:
  explicit TestRenderProcessHost(content::BrowserContext* browser_context,
                                 bool are_v8_optimizations_enabled)
      : content::MockRenderProcessHost(browser_context,
                                       /*is_for_guests_only=*/false),
        are_v8_optimizations_enabled_(are_v8_optimizations_enabled) {}

  ~TestRenderProcessHost() override = default;

  bool AreV8OptimizationsDisabled() override {
    return !are_v8_optimizations_enabled_;
  }

 private:
  bool are_v8_optimizations_enabled_ = false;
};

// Factory for TestRenderProcessHost.
class TestRenderProcessHostFactory
    : public content::MockRenderProcessHostFactory {
 public:
  explicit TestRenderProcessHostFactory(
      content::ContentBrowserClient* browser_client)
      : browser_client_(browser_client) {}
  ~TestRenderProcessHostFactory() override = default;

 protected:
  std::unique_ptr<content::MockRenderProcessHost> BuildRenderProcessHost(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) override {
    bool should_enable_v8_optimizers =
        browser_client_->AreV8OptimizationsEnabledForSite(browser_context,
                                                          std::nullopt, GURL());
    return std::make_unique<TestRenderProcessHost>(browser_context,
                                                   should_enable_v8_optimizers);
  }

 private:
  raw_ptr<content::ContentBrowserClient> browser_client_ = nullptr;
};

// Test ContentBrowserClient with custom v8-optimizer and origin-isolation
// state.
class TestContentBrowserClient : public content::ContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;

  bool AreV8OptimizationsEnabledForSite(
      content::BrowserContext* browser_context,
      const std::optional<base::SafeRef<content::ProcessSelectionUserData>>&
          process_selection_user_data,
      const GURL& site_url) override {
    return are_v8_optimizations_enabled_;
  }

  bool ShouldDisableOriginIsolation() override {
    return !should_use_origin_isolation_;
  }

  void SetAreV8OptimizationsEnabled(bool are_v8_optimizations_enabled) {
    are_v8_optimizations_enabled_ = are_v8_optimizations_enabled;
  }

  void SetUseOriginIsolation(bool should_use_origin_isolation) {
    should_use_origin_isolation_ = should_use_origin_isolation;
  }

 private:
  bool are_v8_optimizations_enabled_ = false;
  bool should_use_origin_isolation_ = false;
};

// Test fixture for SiteProtectionMetricsObserver::LogV8OptimizerUma() tests.
class SiteProtectionMetricsObserverV8OptTest
    : public SiteProtectionMetricsObserverTest {
 public:
  SiteProtectionMetricsObserverV8OptTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kProcessSelectionDeferringConditions,
                              content_settings::features::
                                  kBlockV8OptimizerOnUnfamiliarSitesSetting,
                              features::kOriginKeyedProcessesByDefault},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);
    SiteProtectionMetricsObserverTest::SetUp();

    profile()->GetTestingPrefService()->SetBoolean(
        prefs::kJavascriptOptimizerBlockedForUnfamiliarSites, true);

    content::SpareRenderProcessHostManager::Get().CleanupSparesForTesting();

    test_render_process_host_factory_ =
        std::make_unique<TestRenderProcessHostFactory>(&browser_client_);
    SetRenderProcessHostFactory(test_render_process_host_factory_.get());

    // Navigate to URL so that tests don't reuse the current render process.
    NavigateAndCommit(GURL("https://baz.com"));
  }

  void TearDown() override {
    DeleteContents();
    SetRenderProcessHostFactory(nullptr);
    test_render_process_host_factory_.reset();
    SetBrowserClientForTesting(old_browser_client_);
    SiteProtectionMetricsObserverTest::TearDown();
  }

 protected:
  content::RenderFrameHost* CreateAndNavigateIframe(
      content::RenderFrameHost* parent,
      const GURL& iframe_url) {
    content::RenderFrameHost* iframe_rfh =
        content::RenderFrameHostTester::For(parent)->AppendChild("child");
    return content::NavigationSimulator::NavigateAndCommitFromDocument(
        iframe_url, iframe_rfh);
  }

  void SetV8OptimizerStateForFutureRenderProcesses(
      bool are_v8_optimizations_enabled) {
    browser_client_.SetAreV8OptimizationsEnabled(are_v8_optimizations_enabled);
  }

  void NavigateToPageWithIframeAndV8OptState(
      const GURL& main_url,
      bool topmost_are_v8_optimizations_enabled,
      const GURL& iframe_url,
      bool iframe_are_v8_optimizations_enabled) {
    SetV8OptimizerStateForFutureRenderProcesses(
        topmost_are_v8_optimizations_enabled);
    NavigateAndCommit(main_url);
    content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
    ASSERT_EQ(topmost_are_v8_optimizations_enabled,
              AreV8OptimizersEnabled(main_rfh));

    SetV8OptimizerStateForFutureRenderProcesses(
        iframe_are_v8_optimizations_enabled);
    content::RenderFrameHost* iframe_rfh =
        CreateAndNavigateIframe(main_rfh, iframe_url);
    ASSERT_EQ(iframe_are_v8_optimizations_enabled,
              AreV8OptimizersEnabled(iframe_rfh));
  }

  void SetUseOriginIsolation(bool should_use_origin_isolation) {
    browser_client_.SetUseOriginIsolation(should_use_origin_isolation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestRenderProcessHostFactory>
      test_render_process_host_factory_;
  TestContentBrowserClient browser_client_;
  raw_ptr<content::ContentBrowserClient> old_browser_client_ = nullptr;
};

TEST_F(SiteProtectionMetricsObserverV8OptTest, NoIframe) {
  base::HistogramTester histogram_tester;

  GURL kMainUrl("https://foo.com");
  NavigateAndCommit(kMainUrl);

  EXPECT_EQ(
      0u, histogram_tester.GetAllSamples("SafeBrowsing.V8Optimizer.IframeState")
              .size());
}

TEST_F(SiteProtectionMetricsObserverV8OptTest,
       Iframe_EnabledForChildAndTopmost) {
  base::HistogramTester histogram_tester;
  GURL kMainUrl("https://foo.com");
  GURL kIframeUrl("https://bar.com");
  NavigateToPageWithIframeAndV8OptState(
      kMainUrl,
      /*topmost_are_v8_optimizations_enabled=*/true, kIframeUrl,
      /*iframe_are_v8_optimizations_enabled=*/true);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.IframeState",
      IframeV8OptimizerState::kEnabledForChildAndTopmost, 1);
}

TEST_F(SiteProtectionMetricsObserverV8OptTest,
       Iframe_EnabledForChildDisabledForTopmost) {
  base::HistogramTester histogram_tester;
  GURL kMainUrl("https://foo.com");
  GURL kIframeUrl("https://bar.com");
  NavigateToPageWithIframeAndV8OptState(
      kMainUrl,
      /*topmost_are_v8_optimizations_enabled=*/false, kIframeUrl,
      /*iframe_are_v8_optimizations_enabled=*/true);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.IframeState",
      IframeV8OptimizerState::kEnabledForChildDisabledForTopmost, 1);
}

TEST_F(SiteProtectionMetricsObserverV8OptTest,
       Iframe_DisabledForChildEnabledForTopmost) {
  base::HistogramTester histogram_tester;
  GURL kMainUrl("https://foo.com");
  GURL kIframeUrl("https://bar.com");
  NavigateToPageWithIframeAndV8OptState(
      kMainUrl,
      /*topmost_are_v8_optimizations_enabled=*/true, kIframeUrl,
      /*iframe_are_v8_optimizations_enabled=*/false);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.IframeState",
      IframeV8OptimizerState::kDisabledForChildEnabledForTopmost, 1);
}

TEST_F(SiteProtectionMetricsObserverV8OptTest,
       Iframe_DisabledForChildAndTopmost) {
  base::HistogramTester histogram_tester;
  GURL kMainUrl("https://foo.com");
  GURL kIframeUrl("https://bar.com");
  NavigateToPageWithIframeAndV8OptState(
      kMainUrl,
      /*topmost_are_v8_optimizations_enabled=*/false, kIframeUrl,
      /*iframe_are_v8_optimizations_enabled=*/false);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.IframeState",
      IframeV8OptimizerState::kDisabledForChildAndTopmost, 1);
}

TEST_F(SiteProtectionMetricsObserverV8OptTest,
       Iframe_DifferentRenderProcessSameSiteMetric_SameProcess) {
  SetUseOriginIsolation(false);

  base::HistogramTester histogram_tester;
  GURL kMainUrl("https://foo.com");
  GURL kIframeUrl("https://sub.foo.com");
  NavigateToPageWithIframeAndV8OptState(
      kMainUrl,
      /*topmost_are_v8_optimizations_enabled=*/false, kIframeUrl,
      /*iframe_are_v8_optimizations_enabled=*/false);
  EXPECT_EQ(0u,
            histogram_tester
                .GetAllSamples("SafeBrowsing.V8Optimizer."
                               "DifferentRenderProcess.SameSite.IframeState")
                .size());
}

TEST_F(SiteProtectionMetricsObserverV8OptTest,
       Iframe_DifferentRenderProcessSameSiteMetric_DifferentProcess) {
  SetUseOriginIsolation(true);

  base::HistogramTester histogram_tester;
  GURL kMainUrl("https://foo.com");
  GURL kIframeUrl("https://sub.foo.com");
  NavigateToPageWithIframeAndV8OptState(
      kMainUrl,
      /*topmost_are_v8_optimizations_enabled=*/false, kIframeUrl,
      /*iframe_are_v8_optimizations_enabled=*/false);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V8Optimizer.DifferentRendererProcess.SameSite.IframeState",
      IframeV8OptimizerState::kDisabledForChildAndTopmost, 1);
}

TEST_F(SiteProtectionMetricsObserverV8OptTest,
       Iframe_DifferentRenderProcessSameSiteMetric_DifferentSite) {
  SetUseOriginIsolation(false);

  base::HistogramTester histogram_tester;
  GURL kMainUrl("https://foo.com");
  GURL kIframeUrl("https://bar.com");
  NavigateToPageWithIframeAndV8OptState(
      kMainUrl,
      /*topmost_are_v8_optimizations_enabled=*/false, kIframeUrl,
      /*iframe_are_v8_optimizations_enabled=*/false);

  EXPECT_EQ(0u,
            histogram_tester
                .GetAllSamples("SafeBrowsing.V8Optimizer."
                               "DifferentRendererProcess.SameSite.IframeState")
                .size());
}

}  // namespace site_protection
