// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_journey_tracker.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

constexpr SessionID kSourceTab1 = SessionID::FromSerializedValue(1);
constexpr SessionID kSourceTab2 = SessionID::FromSerializedValue(2);
constexpr SessionID kSearchTab1 = SessionID::FromSerializedValue(10);
constexpr SessionID kOtherTab = SessionID::FromSerializedValue(20);
constexpr int kNavEntryId = 42;
constexpr int kOtherNavEntryId = 43;
constexpr char kSourceUrl[] = "https://example.com/doc";

void SetNonGoogleDefaultSearchProvider(
    TemplateURLService* template_url_service) {
  TemplateURLData data;
  data.SetShortName(u"NonGoogle");
  data.SetKeyword(u"nongoogle.com");
  data.SetURL("https://www.nongoogle.com/search?q={searchTerms}");
  TemplateURL* turl =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(turl);
}

AimPromoParams CreatePromoParams(std::string mstk = "token") {
  AimPromoParams params;
  params.mstk = std::move(mstk);
  params.aioh = "3";
  params.csuir = "1";
  params.sxsrf = "xsrf";
  return params;
}

}  // namespace

class SearchJourneyTrackerTest : public testing::Test {
 protected:
  void SetUp() override {
    search_engines_test_environment_.template_url_service()->Load();
  }

  // Simulates the user leaving `source_tab` for `search_tab` and searching for
  // `query` there.
  void StartJourney(SessionID source_tab,
                    SessionID search_tab,
                    std::u16string_view query) {
    tracker_.OnTabDeactivated(source_tab, kNavEntryId, GURL(kSourceUrl),
                              SessionID::InvalidValue());
    tracker_.OnTabActivated(search_tab, /*nav_entry_id=*/0);
    tracker_.OnSearchNavigationCommitted(search_tab, query);
  }

  const JourneyState* GetJourney(SessionID source_tab) const {
    const auto& journeys = tracker_.GetActiveJourneysForTesting();
    auto it = journeys.find(source_tab);
    return it == journeys.end() ? nullptr : &it->second;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_{kABASearchJourneys};
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  SearchJourneyTracker tracker_{
      search_engines_test_environment_.template_url_service()};
};

TEST_F(SearchJourneyTrackerTest, NormalizeForJourneyMatchMultiLineNoSpaces) {
  // Omnibox StripJavascriptSchemas strips raw \r and \n before collapsing
  // whitespace when no space precedes the line break.
  const std::optional<size_t> copied =
      SearchJourneyTracker::NormalizeForJourneyMatch(
          u"apple\nbanana\r\ncherry");
  const std::optional<size_t> omnibox =
      SearchJourneyTracker::NormalizeForJourneyMatch(u"applebananacherry");
  ASSERT_TRUE(copied.has_value());
  ASSERT_TRUE(omnibox.has_value());
  EXPECT_EQ(*copied, *omnibox);
}

TEST_F(SearchJourneyTrackerTest, NormalizeForJourneyMatchMultiLineWithSpaces) {
  const std::optional<size_t> copied =
      SearchJourneyTracker::NormalizeForJourneyMatch(
          u"what is \n photosynthesis");
  const std::optional<size_t> google_web =
      SearchJourneyTracker::NormalizeForJourneyMatch(u"what is photosynthesis");
  ASSERT_TRUE(copied.has_value());
  ASSERT_TRUE(google_web.has_value());
  EXPECT_EQ(*copied, *google_web);
}

TEST_F(SearchJourneyTrackerTest, NormalizeForJourneyMatchWhitespaceAndCase) {
  const std::optional<size_t> padded =
      SearchJourneyTracker::NormalizeForJourneyMatch(
          u"  \t  RÉSUMÉ   TIPS  \n ");
  const std::optional<size_t> canonical =
      SearchJourneyTracker::NormalizeForJourneyMatch(u"résumé tips");
  ASSERT_TRUE(padded.has_value());
  ASSERT_TRUE(canonical.has_value());
  EXPECT_EQ(*padded, *canonical);
}

TEST_F(SearchJourneyTrackerTest, NormalizeForJourneyMatchShortStrings) {
  EXPECT_FALSE(SearchJourneyTracker::NormalizeForJourneyMatch(u"").has_value());
  EXPECT_FALSE(
      SearchJourneyTracker::NormalizeForJourneyMatch(u"abcd").has_value());
  EXPECT_FALSE(
      SearchJourneyTracker::NormalizeForJourneyMatch(u"  a\nb  ").has_value());
  EXPECT_FALSE(
      SearchJourneyTracker::NormalizeForJourneyMatch(u"ab   c").has_value());
  EXPECT_TRUE(
      SearchJourneyTracker::NormalizeForJourneyMatch(u"abcde").has_value());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kABASearchJourneys,
      {{"aba_search_journeys_min_query_match_length", "8"}});
  EXPECT_EQ(GetABASearchJourneysMinQueryMatchLength(), 8u);
  EXPECT_FALSE(
      SearchJourneyTracker::NormalizeForJourneyMatch(u"abcdefg").has_value());
  EXPECT_TRUE(
      SearchJourneyTracker::NormalizeForJourneyMatch(u"abcdefgh").has_value());
}

TEST_F(SearchJourneyTrackerTest, ArmsJourneyForCrossTabSearch) {
  StartJourney(kSourceTab1, kSearchTab1, u"how do plants make food");

  ASSERT_EQ(tracker_.GetActiveJourneysForTesting().size(), 1u);
  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  EXPECT_EQ(journey->source_nav_entry_id, kNavEntryId);
  EXPECT_EQ(journey->search_tab_id, kSearchTab1);
  EXPECT_EQ(journey->search_terms, u"how do plants make food");
  EXPECT_EQ(journey->search_committed_time, base::TimeTicks::Now());
  EXPECT_FALSE(journey->matched_copy);
  EXPECT_FALSE(journey->search_tab_closed);
}

TEST_F(SearchJourneyTrackerTest, ShortQueryArmsJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"tax");

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  EXPECT_EQ(journey->search_terms, u"tax");
  EXPECT_FALSE(journey->matched_copy);
}

TEST_F(SearchJourneyTrackerTest, CopyFromSourceTabLabelsJourney) {
  const std::u16string kQuery = u"quantum computing basics";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 1u);

  StartJourney(kSourceTab1, kSearchTab1, kQuery);

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  EXPECT_TRUE(journey->matched_copy);
}

TEST_F(SearchJourneyTrackerTest, CopyFromOtherTabDoesNotLabelJourney) {
  const std::u16string kQuery = u"quantum computing basics";
  tracker_.OnCopyRecorded(kSourceTab2, kNavEntryId, kQuery);

  StartJourney(kSourceTab1, kSearchTab1, kQuery);

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  EXPECT_FALSE(journey->matched_copy);
}

TEST_F(SearchJourneyTrackerTest, SameTabSearchIgnored) {
  tracker_.OnTabDeactivated(kSourceTab1, kNavEntryId, GURL(kSourceUrl),
                            SessionID::InvalidValue());
  tracker_.OnTabActivated(kSourceTab1, kNavEntryId);
  tracker_.OnSearchNavigationCommitted(kSourceTab1, u"quantum computing");

  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, SearchInBackgroundTabIgnored) {
  tracker_.OnTabDeactivated(kSourceTab1, kNavEntryId, GURL(kSourceUrl),
                            SessionID::InvalidValue());
  tracker_.OnTabActivated(kOtherTab, kNavEntryId);
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"quantum computing");

  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, IneligibleSourceTabIgnored) {
  for (const char* url :
       {"http://example.com/doc", "https://www.google.com/search?q=quantum",
        "chrome://newtab/"}) {
    tracker_.OnTabDeactivated(kSourceTab1, kNavEntryId, GURL(url),
                              SessionID::InvalidValue());
    tracker_.OnTabActivated(kSearchTab1, /*nav_entry_id=*/0);
    tracker_.OnSearchNavigationCommitted(kSearchTab1, u"quantum computing");
    EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty()) << url;
  }
}

TEST_F(SearchJourneyTrackerTest, IneligibleTabKeepsEarlierSourceTab) {
  tracker_.OnTabDeactivated(kSourceTab1, kNavEntryId, GURL(kSourceUrl),
                            SessionID::InvalidValue());
  tracker_.OnTabActivated(kOtherTab, kNavEntryId);
  tracker_.OnTabDeactivated(kOtherTab, kNavEntryId, GURL("chrome://newtab/"),
                            SessionID::InvalidValue());
  tracker_.OnTabActivated(kSearchTab1, /*nav_entry_id=*/0);
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"quantum computing");

  EXPECT_TRUE(GetJourney(kSourceTab1));
}

TEST_F(SearchJourneyTrackerTest, LeavingSourceTabFromIneligiblePageClearsIt) {
  tracker_.OnTabDeactivated(kSourceTab1, kNavEntryId, GURL(kSourceUrl),
                            SessionID::InvalidValue());
  tracker_.OnTabActivated(kSourceTab1, kNavEntryId);
  tracker_.OnTabDeactivated(kSourceTab1, kOtherNavEntryId,
                            GURL("chrome://settings/"),
                            SessionID::InvalidValue());
  tracker_.OnTabActivated(kSearchTab1, /*nav_entry_id=*/0);
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"quantum computing");

  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, TabOpenedFromSearchTabIsNeverSourceTab) {
  tracker_.OnTabDeactivated(kSourceTab1, kNavEntryId,
                            GURL("https://example.com/result"),
                            /*opener_tab_id=*/kSearchTab1);
  tracker_.OnTabActivated(kSearchTab1, /*nav_entry_id=*/0);
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"quantum computing");

  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, NewSearchInSearchTabReplacesJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"first query");
  task_environment_.AdvanceClock(base::Minutes(1));
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"second query");

  ASSERT_EQ(tracker_.GetActiveJourneysForTesting().size(), 1u);
  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  EXPECT_EQ(journey->search_terms, u"second query");
  EXPECT_EQ(journey->search_committed_time, base::TimeTicks::Now());
}

TEST_F(SearchJourneyTrackerTest, ReturnToSourceTab) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  tracker_.OnTabDeactivated(kSearchTab1, /*nav_entry_id=*/0,
                            GURL("https://www.google.com/search?q=quantum"),
                            SessionID::InvalidValue());

  EXPECT_TRUE(tracker_.OnTabActivated(kSourceTab1, kNavEntryId));
  EXPECT_TRUE(GetJourney(kSourceTab1));
}

TEST_F(SearchJourneyTrackerTest, ReturnToSourceTabOnOtherEntryDropsJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");

  EXPECT_FALSE(tracker_.OnTabActivated(kSourceTab1, kOtherNavEntryId));
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, ActivatingSearchTabKeepsJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");

  EXPECT_FALSE(tracker_.OnTabActivated(kSearchTab1, /*nav_entry_id=*/0));
  EXPECT_TRUE(GetJourney(kSourceTab1));
}

TEST_F(SearchJourneyTrackerTest, IntermediateTabDropsJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");

  EXPECT_FALSE(tracker_.OnTabActivated(kOtherTab, kNavEntryId));
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
  EXPECT_FALSE(tracker_.OnTabActivated(kSourceTab1, kNavEntryId));
}

TEST_F(SearchJourneyTrackerTest, IntermediateTabKeepsJourneyWhenAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kABASearchJourneys,
      {{"aba_search_journeys_allow_intermediate_tabs", "true"}});
  EXPECT_TRUE(ShouldAllowABASearchJourneysIntermediateTabs());
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");

  EXPECT_FALSE(tracker_.OnTabActivated(kOtherTab, kNavEntryId));
  EXPECT_TRUE(tracker_.OnTabActivated(kSourceTab1, kNavEntryId));
}

TEST_F(SearchJourneyTrackerTest, ClosingSearchTabCountsAsReturn) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  tracker_.OnTabDestroyed(kSearchTab1);

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  EXPECT_TRUE(journey->search_tab_closed);
  EXPECT_TRUE(tracker_.OnTabActivated(kSourceTab1, kNavEntryId));
}

TEST_F(SearchJourneyTrackerTest, ClosingSourceTabClearsJourney) {
  const std::u16string kQuery = u"quantum computing basics";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  StartJourney(kSourceTab1, kSearchTab1, kQuery);
  ASSERT_TRUE(GetJourney(kSourceTab1));

  tracker_.OnTabDestroyed(kSourceTab1);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);

  // The closed tab is no longer a candidate Tab A for a new search.
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"another query");
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, StoresPromoParamsForJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");

  EXPECT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(),
      {AimPromoQueryHint::kEduHomework, AimPromoQueryHint::kHowToWebApp}));

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  ASSERT_TRUE(journey->promo_params.has_value());
  EXPECT_EQ(journey->promo_params->mstk, "token");
  EXPECT_EQ(journey->promo_params->aioh, "3");
  EXPECT_EQ(journey->promo_params->csuir, "1");
  EXPECT_EQ(journey->promo_params->sxsrf, "xsrf");
  EXPECT_EQ(journey->query_hints,
            AimPromoQueryHints({AimPromoQueryHint::kEduHomework,
                                AimPromoQueryHint::kHowToWebApp}));
}

TEST_F(SearchJourneyTrackerTest, NewPromoParamsReplaceEarlierOnes) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  ASSERT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams("first"),
      {AimPromoQueryHint::kEduHomework}));

  EXPECT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams("second"), {}));

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  ASSERT_TRUE(journey->promo_params.has_value());
  EXPECT_EQ(journey->promo_params->mstk, "second");
  EXPECT_TRUE(journey->query_hints.empty());
}

TEST_F(SearchJourneyTrackerTest, PromoParamsForOtherQueryIgnored) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");

  EXPECT_FALSE(tracker_.OnAimPromoParamsReceived(kSearchTab1, u"another query",
                                                 CreatePromoParams(), {}));
  ASSERT_TRUE(GetJourney(kSourceTab1));
  EXPECT_FALSE(GetJourney(kSourceTab1)->promo_params.has_value());
}

TEST_F(SearchJourneyTrackerTest, PromoParamsWithoutJourneyIgnored) {
  EXPECT_FALSE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(), {}));

  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  EXPECT_FALSE(tracker_.OnAimPromoParamsReceived(
      kOtherTab, u"quantum computing", CreatePromoParams(), {}));
  EXPECT_FALSE(GetJourney(kSourceTab1)->promo_params.has_value());
}

TEST_F(SearchJourneyTrackerTest, PromoParamsWithoutMstkIgnored) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");

  EXPECT_FALSE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(/*mstk=*/""), {}));
  EXPECT_FALSE(GetJourney(kSourceTab1)->promo_params.has_value());
}

TEST_F(SearchJourneyTrackerTest, NewSearchInSearchTabDropsPromoParams) {
  StartJourney(kSourceTab1, kSearchTab1, u"first query");
  ASSERT_TRUE(tracker_.OnAimPromoParamsReceived(kSearchTab1, u"first query",
                                                CreatePromoParams(), {}));

  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"second query");

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  EXPECT_EQ(journey->search_terms, u"second query");
  EXPECT_FALSE(journey->promo_params.has_value());

  // A late registration from the first results page is never paired with the
  // new query.
  EXPECT_FALSE(tracker_.OnAimPromoParamsReceived(kSearchTab1, u"first query",
                                                 CreatePromoParams(), {}));
  EXPECT_FALSE(GetJourney(kSourceTab1)->promo_params.has_value());
}

TEST_F(SearchJourneyTrackerTest, CommitForSameQueryKeepsJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  const base::TimeTicks committed_time = base::TimeTicks::Now();
  ASSERT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(), {}));

  task_environment_.AdvanceClock(base::Minutes(1));
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"quantum computing");

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  EXPECT_TRUE(journey->promo_params.has_value());
  EXPECT_EQ(journey->search_committed_time, committed_time);
}

TEST_F(SearchJourneyTrackerTest, ClosingSearchTabKeepsPromoParams) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  ASSERT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(), {}));

  tracker_.OnTabDestroyed(kSearchTab1);
  EXPECT_FALSE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams("late"), {}));

  const JourneyState* journey = GetJourney(kSourceTab1);
  ASSERT_TRUE(journey);
  ASSERT_TRUE(journey->promo_params.has_value());
  EXPECT_EQ(journey->promo_params->mstk, "token");
}

TEST_F(SearchJourneyTrackerTest, GetRestorableJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  ASSERT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(), {}));

  const JourneyState* journey =
      tracker_.GetRestorableJourney(kSourceTab1, kNavEntryId);
  ASSERT_TRUE(journey);
  EXPECT_EQ(journey, GetJourney(kSourceTab1));
}

TEST_F(SearchJourneyTrackerTest, GetRestorableJourneyWithoutJourney) {
  EXPECT_FALSE(tracker_.GetRestorableJourney(kSourceTab1, kNavEntryId));
}

TEST_F(SearchJourneyTrackerTest, GetRestorableJourneyWithoutPromoParams) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");

  EXPECT_FALSE(tracker_.GetRestorableJourney(kSourceTab1, kNavEntryId));
  EXPECT_TRUE(GetJourney(kSourceTab1));
}

TEST_F(SearchJourneyTrackerTest, GetRestorableJourneyAfterSourceTabChanged) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  ASSERT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(), {}));

  EXPECT_FALSE(tracker_.GetRestorableJourney(kSourceTab1, kOtherNavEntryId));
  // The journey is kept in case the user goes back to the page they left.
  EXPECT_TRUE(tracker_.GetRestorableJourney(kSourceTab1, kNavEntryId));
}

TEST_F(SearchJourneyTrackerTest, GetRestorableJourneyAfterExpiry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kABASearchJourneys, {{"aba_search_journeys_ttl", "5m"}});
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  ASSERT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(), {}));

  task_environment_.AdvanceClock(base::Minutes(6));
  EXPECT_FALSE(tracker_.GetRestorableJourney(kSourceTab1, kNavEntryId));
}

TEST_F(SearchJourneyTrackerTest, OnJourneyRestoredDropsJourney) {
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  ASSERT_TRUE(tracker_.OnAimPromoParamsReceived(
      kSearchTab1, u"quantum computing", CreatePromoParams(), {}));

  tracker_.OnJourneyRestored(kSourceTab1);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
  EXPECT_FALSE(tracker_.GetRestorableJourney(kSourceTab1, kNavEntryId));
}

TEST_F(SearchJourneyTrackerTest, JourneyTtlExpiry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kABASearchJourneys, {{"aba_search_journeys_ttl", "5m"}});
  StartJourney(kSourceTab1, kSearchTab1, u"quantum computing");
  ASSERT_TRUE(GetJourney(kSourceTab1));

  task_environment_.AdvanceClock(base::Minutes(6));
  EXPECT_FALSE(tracker_.OnTabActivated(kSourceTab1, kNavEntryId));
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, SourceTabTtlExpiry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kABASearchJourneys, {{"aba_search_journeys_ttl", "5m"}});
  tracker_.OnTabDeactivated(kSourceTab1, kNavEntryId, GURL(kSourceUrl),
                            SessionID::InvalidValue());
  tracker_.OnTabActivated(kSearchTab1, /*nav_entry_id=*/0);

  task_environment_.AdvanceClock(base::Minutes(6));
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"quantum computing");
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, RingBufferBoundedCapacity) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kABASearchJourneys, {{"aba_search_journeys_max_ring_buffer_size", "3"}});
  EXPECT_EQ(GetABASearchJourneysMaxRingBufferSize(), 3u);

  for (int i = 0; i < 4; ++i) {
    tracker_.OnCopyRecorded(
        kSourceTab1, kNavEntryId,
        u"copied query number " + base::NumberToString16(i));
  }

  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(),
            GetABASearchJourneysMaxRingBufferSize());

  // Entry 0 was evicted; entry 3 is still present.
  StartJourney(kSourceTab1, kSearchTab1, u"copied query number 0");
  ASSERT_TRUE(GetJourney(kSourceTab1));
  EXPECT_FALSE(GetJourney(kSourceTab1)->matched_copy);

  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"copied query number 3");
  ASSERT_TRUE(GetJourney(kSourceTab1));
  EXPECT_TRUE(GetJourney(kSourceTab1)->matched_copy);
}

TEST_F(SearchJourneyTrackerTest, RingBufferTtlExpiry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kABASearchJourneys, {{"aba_search_journeys_ttl", "5m"}});
  EXPECT_EQ(GetABASearchJourneysTtl(), base::Minutes(5));

  const std::u16string kQuery = u"expiring query text";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);

  task_environment_.AdvanceClock(base::Minutes(6));
  StartJourney(kSourceTab1, kSearchTab1, kQuery);
  ASSERT_TRUE(GetJourney(kSourceTab1));
  EXPECT_FALSE(GetJourney(kSourceTab1)->matched_copy);
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);
}

TEST_F(SearchJourneyTrackerTest, FeatureParamsZeroWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kABASearchJourneys);
  EXPECT_EQ(GetABASearchJourneysTtl(), base::TimeDelta());
  EXPECT_EQ(GetABASearchJourneysMaxRingBufferSize(), 0u);
  EXPECT_EQ(GetABASearchJourneysMinQueryMatchLength(), 0u);
  EXPECT_FALSE(ShouldAllowABASearchJourneysIntermediateTabs());
  EXPECT_FALSE(SearchJourneyTracker::NormalizeForJourneyMatch(u"valid query")
                   .has_value());
}

TEST_F(SearchJourneyTrackerTest, NonGoogleDseDoesNotRecordOrArm) {
  SetNonGoogleDefaultSearchProvider(
      search_engines_test_environment_.template_url_service());

  const std::u16string kQuery = u"query with non-google dse";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);

  StartJourney(kSourceTab1, kSearchTab1, kQuery);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, SwitchingToNonGoogleDseClearsTrackedState) {
  const std::u16string kQuery = u"query before dse switch";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  StartJourney(kSourceTab1, kSearchTab1, kQuery);
  ASSERT_EQ(tracker_.GetRingBufferSizeForTesting(), 1u);
  ASSERT_EQ(tracker_.GetActiveJourneysForTesting().size(), 1u);

  SetNonGoogleDefaultSearchProvider(
      search_engines_test_environment_.template_url_service());

  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(SearchJourneyTrackerTest, ShutdownClearsAllState) {
  const std::u16string kQuery = u"query before shutdown";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  StartJourney(kSourceTab1, kSearchTab1, kQuery);
  ASSERT_FALSE(tracker_.GetActiveJourneysForTesting().empty());

  tracker_.Shutdown();
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST(SearchJourneyTrackerFactoryTest, FeatureDisabled) {
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kABASearchJourneys);

  TestingProfile profile;
  EXPECT_EQ(SearchJourneyTrackerFactory::GetForProfile(&profile), nullptr);
}

TEST(SearchJourneyTrackerFactoryTest, EnabledForRegularProfileNotIncognito) {
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(kABASearchJourneys);

  TestingProfile profile;
  EXPECT_NE(SearchJourneyTrackerFactory::GetForProfile(&profile), nullptr);

  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(SearchJourneyTrackerFactory::GetForProfile(otr_profile), nullptr);
}

}  // namespace contextual_tasks
