// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/search_hijacking_detector.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class SearchHijackingDetectorTest : public testing::Test {
 public:
  SearchHijackingDetectorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing::Test::SetUp();
    // Set up the TemplateURLService.
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_ = profile_builder.Build();
    template_url_service_ =
        TemplateURLServiceFactory::GetForProfile(profile_.get());

    // Set up the detector.
    detector_ = std::make_unique<SearchHijackingDetector>(
        profile_->GetPrefs(), template_url_service_);
    detector_->SetHeuristicCheckInterval(base::Seconds(0));
  }

  void TearDown() override {
    detector_->ClearAllDataFromPrefs();
    testing::Test::TearDown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<SearchHijackingDetector> detector_;

  TemplateURL* SetDefaultSearchProvider(const std::string& search_url) {
    TemplateURLData data;
    data.SetShortName(u"Test");
    data.SetKeyword(u"test");
    data.SetURL(search_url);
    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
    return template_url;
  }
};

TEST_F(SearchHijackingDetectorTest, Defaults) {
  auto detector = std::make_unique<SearchHijackingDetector>(
      profile_->GetPrefs(), template_url_service_);
  EXPECT_EQ(detector->GetHeuristicCheckInterval(),
            SearchHijackingDetector::kDefaultHeuristicCheckInterval);
  EXPECT_EQ(detector->GetHeuristicThreshold(),
            SearchHijackingDetector::kDefaultHeuristicThreshold);
}

TEST_F(SearchHijackingDetectorTest, GettersAndSetters) {
  detector_->SetHeuristicCheckInterval(base::Seconds(123));
  EXPECT_EQ(detector_->GetHeuristicCheckInterval(), base::Seconds(123));

  detector_->SetHeuristicThreshold(456);
  EXPECT_EQ(detector_->GetHeuristicThreshold(), 456);
}

TEST_F(SearchHijackingDetectorTest, OnOmniboxSearch_DefaultSearchProvider) {
  SetDefaultSearchProvider("http://test.com/search?q={searchTerms}");
  AutocompleteMatch match;
  match.destination_url = GURL("http://test.com/search?q=foo");

  detector_->OnOmniboxSearch(match);

  EXPECT_EQ(detector_->GetCurrentEventCountsForTesting().omnibox_searches, 1);
}

TEST_F(SearchHijackingDetectorTest, OnOmniboxSearch_NotDefaultSearchProvider) {
  SetDefaultSearchProvider("http://test.com/search?q={searchTerms}");
  AutocompleteMatch match;
  match.destination_url = GURL("http://other.com/search?q=foo");

  detector_->OnOmniboxSearch(match);

  EXPECT_EQ(detector_->GetCurrentEventCountsForTesting().omnibox_searches, 0);
}

TEST_F(SearchHijackingDetectorTest, OnDseSerpLoaded) {
  detector_->OnDseSerpLoaded();
  EXPECT_EQ(detector_->GetCurrentEventCountsForTesting().serp_landings, 1);
}

TEST_F(SearchHijackingDetectorTest, MaybeCheckForHeuristicMatch_NoSignal) {
  detector_->OnOmniboxSearch(AutocompleteMatch());
  detector_->OnDseSerpLoaded();

  detector_->MaybeCheckForHeuristicMatch();

  EXPECT_EQ(detector_->GetSignalForReport(), nullptr);
}

TEST_F(SearchHijackingDetectorTest,
       MaybeCheckForHeuristicMatch_ThresholdNotMet) {
  // 2 searches, 1 SERP load. Threshold is 2, so no signal.
  detector_->SetHeuristicThreshold(2);
  SetDefaultSearchProvider("http://test.com/search?q={searchTerms}");
  AutocompleteMatch match;
  match.destination_url = GURL("http://test.com/search?q=foo");
  detector_->OnOmniboxSearch(match);
  detector_->OnOmniboxSearch(match);
  detector_->OnDseSerpLoaded();

  detector_->MaybeCheckForHeuristicMatch();

  EXPECT_EQ(detector_->GetSignalForReport(), nullptr);
}

TEST_F(SearchHijackingDetectorTest, MaybeCheckForHeuristicMatch_SignalCreated) {
  // Perform enough searches to meet the threshold.
  SetDefaultSearchProvider("http://test.com/search?q={searchTerms}");
  AutocompleteMatch match;
  match.destination_url = GURL("http://test.com/search?q=foo");
  int threshold = detector_->GetHeuristicThreshold();
  for (int i = 0; i < threshold + 1; ++i) {
    detector_->OnOmniboxSearch(match);
  }
  detector_->OnDseSerpLoaded();

  detector_->MaybeCheckForHeuristicMatch();

  auto signal = detector_->GetSignalForReport();
  ASSERT_NE(signal, nullptr);
  EXPECT_EQ(signal->omnibox_search_count(), (unsigned int)threshold + 1);
  EXPECT_EQ(signal->serp_landing_count(), 1u);
}

TEST_F(SearchHijackingDetectorTest,
       MaybeCheckForHeuristicMatch_IntervalNotPassed) {
  detector_->SetHeuristicCheckInterval(base::Hours(1));
  base::Time start_time = base::Time::Now();
  detector_->MaybeCheckForHeuristicMatch();  // This sets the last check time.
  EXPECT_EQ(detector_->GetLastHeuristicCheckTimeForTesting(), start_time);

  // Perform enough searches to meet the threshold.
  SetDefaultSearchProvider("http://test.com/search?q={searchTerms}");
  AutocompleteMatch match;
  match.destination_url = GURL("http://test.com/search?q=foo");
  int threshold = detector_->GetHeuristicThreshold();
  for (int i = 0; i < threshold + 1; ++i) {
    detector_->OnOmniboxSearch(match);
  }
  detector_->OnDseSerpLoaded();

  // This check should not trigger the heuristic.
  detector_->MaybeCheckForHeuristicMatch();
  EXPECT_EQ(detector_->GetSignalForReport(), nullptr);

  // Advance time.
  task_environment_.FastForwardBy(base::Hours(1));
  base::Time end_time = base::Time::Now();
  detector_->MaybeCheckForHeuristicMatch();  // Should run now.
  EXPECT_EQ(detector_->GetLastHeuristicCheckTimeForTesting(), end_time);

  EXPECT_NE(detector_->GetSignalForReport(), nullptr);
}

TEST_F(SearchHijackingDetectorTest, ClearsAllDataFromPrefs) {
  SetDefaultSearchProvider("http://test.com/search?q={searchTerms}");
  AutocompleteMatch match;
  match.destination_url = GURL("http://test.com/search?q=foo");
  int threshold = detector_->GetHeuristicThreshold();
  for (int i = 0; i < threshold + 1; ++i) {
    detector_->OnOmniboxSearch(match);
  }
  detector_->OnDseSerpLoaded();
  // Manually trigger a heuristic check.
  detector_->MaybeCheckForHeuristicMatch();

  // Add a couple of events again, since the heuristic check clears out the
  // individual counts as well.
  detector_->OnOmniboxSearch(match);
  detector_->OnDseSerpLoaded();

  // Verify that there is data to clear first.
  ASSERT_NE(detector_->GetSignalForReport(), nullptr);
  ASSERT_EQ(detector_->GetCurrentEventCountsForTesting().omnibox_searches, 1);
  ASSERT_EQ(detector_->GetCurrentEventCountsForTesting().serp_landings, 1);
  ASSERT_NE(detector_->GetLastHeuristicCheckTimeForTesting(), base::Time());

  detector_->ClearAllDataFromPrefs();

  EXPECT_EQ(detector_->GetCurrentEventCountsForTesting().omnibox_searches, 0);
  EXPECT_EQ(detector_->GetCurrentEventCountsForTesting().serp_landings, 0);
  EXPECT_EQ(detector_->GetSignalForReport(), nullptr);
  EXPECT_EQ(detector_->GetLastHeuristicCheckTimeForTesting(), base::Time());
}

TEST_F(SearchHijackingDetectorTest, RecordsHistograms) {
  base::HistogramTester histogram_tester;
  SetDefaultSearchProvider("http://test.com/search?q={searchTerms}");
  AutocompleteMatch match;
  match.destination_url = GURL("http://test.com/search?q=foo");

  // search_count < serp_count
  detector_->OnOmniboxSearch(match);
  detector_->OnDseSerpLoaded();
  detector_->OnDseSerpLoaded();
  detector_->MaybeCheckForHeuristicMatch();
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector.HeuristicMatch",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector."
      "SearchVsSerpCount",
      /*kSearchCountIsLessThanSerpCount*/ 0, 1);

  // search_count == serp_count
  detector_->OnOmniboxSearch(match);
  detector_->OnOmniboxSearch(match);
  detector_->OnDseSerpLoaded();
  detector_->OnDseSerpLoaded();
  detector_->MaybeCheckForHeuristicMatch();
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector.HeuristicMatch",
      false, 2);
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector."
      "SearchVsSerpCount",
      /*kSearchCountEqualsSerpCount*/ 1, 1);

  // search_count > serp_count, no heuristic match
  detector_->SetHeuristicThreshold(2);
  detector_->OnOmniboxSearch(match);
  detector_->OnOmniboxSearch(match);
  detector_->OnDseSerpLoaded();
  detector_->MaybeCheckForHeuristicMatch();
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector.HeuristicMatch",
      false, 3);
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector."
      "SearchVsSerpCount",
      /*kSearchCountIsGreaterThanSerpCount*/ 2, 1);

  // search_count > serp_count, with heuristic match
  detector_->OnOmniboxSearch(match);
  detector_->OnOmniboxSearch(match);
  detector_->OnOmniboxSearch(match);
  detector_->OnDseSerpLoaded();
  detector_->MaybeCheckForHeuristicMatch();
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector.HeuristicMatch",
      true, 1);
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector."
      "SearchVsSerpCount",
      /*kSearchCountIsGreaterThanSerpCount*/ 2, 2);

  // search_count == 0, serp_count == 0
  detector_->MaybeCheckForHeuristicMatch();
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector.HeuristicMatch",
      false, 4);
  // SearchVsSerpCount should not be incremented. Total count is 1+1+1+1=4
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector."
      "SearchVsSerpCount",
      4);
}

}  // namespace safe_browsing
