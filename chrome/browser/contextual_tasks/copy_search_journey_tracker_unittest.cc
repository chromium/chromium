// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/copy_search_journey_tracker.h"

#include <memory>
#include <optional>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/copy_search_journey_tracker_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

namespace {

constexpr SessionID kSourceTab1 = SessionID::FromSerializedValue(1);
constexpr SessionID kSourceTab2 = SessionID::FromSerializedValue(2);
constexpr SessionID kSearchTab1 = SessionID::FromSerializedValue(10);
constexpr SessionID kSearchTab2 = SessionID::FromSerializedValue(11);
constexpr int kNavEntryId = 42;

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

}  // namespace

class CopySearchJourneyTrackerTest : public testing::Test {
 protected:
  void SetUp() override {
    search_engines_test_environment_.template_url_service()->Load();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_{kCopyTextJourneys};
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  CopySearchJourneyTracker tracker_{
      search_engines_test_environment_.template_url_service()};
};

TEST_F(CopySearchJourneyTrackerTest,
       NormalizeForJourneyMatchMultiLineNoSpaces) {
  // Omnibox StripJavascriptSchemas strips raw \r and \n before collapsing
  // whitespace when no space precedes the line break.
  const std::optional<size_t> copied =
      CopySearchJourneyTracker::NormalizeForJourneyMatch(
          u"apple\nbanana\r\ncherry");
  const std::optional<size_t> omnibox =
      CopySearchJourneyTracker::NormalizeForJourneyMatch(u"applebananacherry");
  ASSERT_TRUE(copied.has_value());
  ASSERT_TRUE(omnibox.has_value());
  EXPECT_EQ(*copied, *omnibox);
}

TEST_F(CopySearchJourneyTrackerTest,
       NormalizeForJourneyMatchMultiLineWithSpaces) {
  const std::optional<size_t> copied =
      CopySearchJourneyTracker::NormalizeForJourneyMatch(
          u"what is \n photosynthesis");
  const std::optional<size_t> google_web =
      CopySearchJourneyTracker::NormalizeForJourneyMatch(
          u"what is photosynthesis");
  ASSERT_TRUE(copied.has_value());
  ASSERT_TRUE(google_web.has_value());
  EXPECT_EQ(*copied, *google_web);
}

TEST_F(CopySearchJourneyTrackerTest,
       NormalizeForJourneyMatchWhitespaceAndCase) {
  const std::optional<size_t> padded =
      CopySearchJourneyTracker::NormalizeForJourneyMatch(
          u"  \t  RÉSUMÉ   TIPS  \n ");
  const std::optional<size_t> canonical =
      CopySearchJourneyTracker::NormalizeForJourneyMatch(u"résumé tips");
  ASSERT_TRUE(padded.has_value());
  ASSERT_TRUE(canonical.has_value());
  EXPECT_EQ(*padded, *canonical);
}

TEST_F(CopySearchJourneyTrackerTest, NormalizeForJourneyMatchShortStrings) {
  EXPECT_FALSE(
      CopySearchJourneyTracker::NormalizeForJourneyMatch(u"").has_value());
  EXPECT_FALSE(
      CopySearchJourneyTracker::NormalizeForJourneyMatch(u"abcd").has_value());
  EXPECT_FALSE(CopySearchJourneyTracker::NormalizeForJourneyMatch(u"  a\nb  ")
                   .has_value());
  EXPECT_FALSE(CopySearchJourneyTracker::NormalizeForJourneyMatch(u"ab   c")
                   .has_value());
  EXPECT_TRUE(
      CopySearchJourneyTracker::NormalizeForJourneyMatch(u"abcde").has_value());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCopyTextJourneys, {{"copy_text_journeys_min_query_match_length", "8"}});
  EXPECT_EQ(GetCopyTextJourneysMinQueryMatchLength(), 8u);
  EXPECT_FALSE(CopySearchJourneyTracker::NormalizeForJourneyMatch(u"abcdefg")
                   .has_value());
  EXPECT_TRUE(CopySearchJourneyTracker::NormalizeForJourneyMatch(u"abcdefgh")
                  .has_value());
}

TEST_F(CopySearchJourneyTrackerTest, StandardCrossTabCorrelation) {
  const std::u16string kQuery = u"quantum computing basics";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 1u);

  tracker_.OnSearchNavigationCommitted(kSearchTab1, kQuery);

  const auto& journeys = tracker_.GetActiveJourneysForTesting();
  auto it = journeys.find(kSourceTab1);
  ASSERT_NE(it, journeys.end());
  EXPECT_EQ(it->second.copy_record.source_tab_id, kSourceTab1);
  EXPECT_EQ(it->second.copy_record.source_nav_entry_id, kNavEntryId);
  EXPECT_EQ(it->second.search_tab_id, kSearchTab1);
  EXPECT_EQ(it->second.copy_record.normalized_query_hash,
            *CopySearchJourneyTracker::NormalizeForJourneyMatch(kQuery));
}

TEST_F(CopySearchJourneyTrackerTest, SameTabSearchIgnored) {
  const std::u16string kQuery = u"quantum computing basics";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  tracker_.OnSearchNavigationCommitted(kSourceTab1, kQuery);

  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(CopySearchJourneyTrackerTest, NewestMatchingCopyWins) {
  const std::u16string kQuery = u"shared query text";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  task_environment_.AdvanceClock(base::Seconds(5));
  tracker_.OnCopyRecorded(kSourceTab2, kNavEntryId, kQuery);

  tracker_.OnSearchNavigationCommitted(kSearchTab1, kQuery);

  const auto& journeys = tracker_.GetActiveJourneysForTesting();
  EXPECT_FALSE(journeys.contains(kSourceTab1));
  auto it = journeys.find(kSourceTab2);
  ASSERT_NE(it, journeys.end());
  EXPECT_EQ(it->second.copy_record.source_tab_id, kSourceTab2);
  EXPECT_EQ(it->second.search_tab_id, kSearchTab1);
}

TEST_F(CopySearchJourneyTrackerTest, RingBufferBoundedCapacity) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCopyTextJourneys, {{"copy_text_journeys_max_ring_buffer_size", "3"}});
  EXPECT_EQ(GetCopyTextJourneysMaxRingBufferSize(), 3u);

  for (int i = 0; i < 4; ++i) {
    tracker_.OnCopyRecorded(
        kSourceTab1, kNavEntryId,
        u"copied query number " + base::NumberToString16(i));
  }

  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(),
            GetCopyTextJourneysMaxRingBufferSize());

  // Entry 0 was evicted; entry 3 is still present.
  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"copied query number 0");
  EXPECT_FALSE(tracker_.GetActiveJourneysForTesting().contains(kSourceTab1));

  tracker_.OnSearchNavigationCommitted(kSearchTab1, u"copied query number 3");
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().contains(kSourceTab1));
}

TEST_F(CopySearchJourneyTrackerTest, RingBufferTtlExpiry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCopyTextJourneys, {{"copy_text_journeys_ttl", "5m"}});
  EXPECT_EQ(GetCopyTextJourneysTtl(), base::Minutes(5));

  const std::u16string kQuery = u"expiring query text";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);

  task_environment_.AdvanceClock(base::Minutes(6));
  tracker_.OnSearchNavigationCommitted(kSearchTab1, kQuery);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);
}

TEST_F(CopySearchJourneyTrackerTest, FeatureParamsZeroWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kCopyTextJourneys);
  EXPECT_EQ(GetCopyTextJourneysTtl(), base::TimeDelta());
  EXPECT_EQ(GetCopyTextJourneysMaxRingBufferSize(), 0u);
  EXPECT_EQ(GetCopyTextJourneysMinQueryMatchLength(), 0u);
  EXPECT_FALSE(
      CopySearchJourneyTracker::NormalizeForJourneyMatch(u"valid query")
          .has_value());
}

TEST_F(CopySearchJourneyTrackerTest, NonGoogleDseDoesNotRecordOrCorrelate) {
  SetNonGoogleDefaultSearchProvider(
      search_engines_test_environment_.template_url_service());

  const std::u16string kQuery = u"query with non-google dse";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);

  tracker_.OnSearchNavigationCommitted(kSearchTab1, kQuery);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(CopySearchJourneyTrackerTest,
       SwitchingToNonGoogleDseClearsTrackedState) {
  const std::u16string kQuery = u"query before dse switch";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  tracker_.OnSearchNavigationCommitted(kSearchTab1, kQuery);
  ASSERT_EQ(tracker_.GetRingBufferSizeForTesting(), 1u);
  ASSERT_EQ(tracker_.GetActiveJourneysForTesting().size(), 1u);

  SetNonGoogleDefaultSearchProvider(
      search_engines_test_environment_.template_url_service());

  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST_F(CopySearchJourneyTrackerTest, TabDestructionCleansUpSourceAndSearchTab) {
  const std::u16string kQuery1 = u"first copied query";
  const std::u16string kQuery2 = u"second copied query";

  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery1);
  tracker_.OnCopyRecorded(kSourceTab2, kNavEntryId, kQuery2);
  tracker_.OnSearchNavigationCommitted(kSearchTab1, kQuery1);
  tracker_.OnSearchNavigationCommitted(kSearchTab2, kQuery2);

  ASSERT_EQ(tracker_.GetActiveJourneysForTesting().size(), 2u);

  // Destroying the source tab removes both its armed journey and its ring
  // buffer entries.
  tracker_.OnTabDestroyed(kSourceTab1);
  EXPECT_FALSE(tracker_.GetActiveJourneysForTesting().contains(kSourceTab1));
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 1u);

  // Destroying the search tab removes the journey associated with it.
  tracker_.OnTabDestroyed(kSearchTab2);
  EXPECT_FALSE(tracker_.GetActiveJourneysForTesting().contains(kSourceTab2));
}

TEST_F(CopySearchJourneyTrackerTest, ShutdownClearsAllState) {
  const std::u16string kQuery = u"query before shutdown";
  tracker_.OnCopyRecorded(kSourceTab1, kNavEntryId, kQuery);
  tracker_.OnSearchNavigationCommitted(kSearchTab1, kQuery);
  ASSERT_FALSE(tracker_.GetActiveJourneysForTesting().empty());

  tracker_.Shutdown();
  EXPECT_EQ(tracker_.GetRingBufferSizeForTesting(), 0u);
  EXPECT_TRUE(tracker_.GetActiveJourneysForTesting().empty());
}

TEST(CopySearchJourneyTrackerFactoryTest, FeatureDisabled) {
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kCopyTextJourneys);

  TestingProfile profile;
  EXPECT_EQ(CopySearchJourneyTrackerFactory::GetForProfile(&profile), nullptr);
}

TEST(CopySearchJourneyTrackerFactoryTest,
     EnabledForRegularProfileNotIncognito) {
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list(kCopyTextJourneys);

  TestingProfile profile;
  EXPECT_NE(CopySearchJourneyTrackerFactory::GetForProfile(&profile), nullptr);

  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(CopySearchJourneyTrackerFactory::GetForProfile(otr_profile),
            nullptr);
}

}  // namespace contextual_tasks
