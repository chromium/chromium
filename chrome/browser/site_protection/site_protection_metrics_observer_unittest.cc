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
#include "components/ukm/test_ukm_recorder.h"
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

  void SetIncognito() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto* global_browser_process = TestingBrowserProcess::GetGlobal();
    global_browser_process->SetLocalState(nullptr);
#endif

    Profile* const otr_profile =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    EXPECT_TRUE(otr_profile->IsIncognitoProfile());
    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::Create(otr_profile);
    SetContents(content::WebContentsTester::CreateTestWebContents(
        otr_profile, std::move(site_instance)));

    SetUpForNewWebContents();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    global_browser_process->SetLocalState(profile()->GetPrefs());
#endif
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
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
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

}  // namespace site_protection
