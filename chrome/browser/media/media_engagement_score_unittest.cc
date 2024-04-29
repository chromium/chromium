// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_score.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::testing::Optional;

base::Time GetReferenceTime() {
  static constexpr base::Time::Exploded kReferenceTime = {.year = 2015,
                                                          .month = 1,
                                                          .day_of_week = 5,
                                                          .day_of_month = 30,
                                                          .hour = 11};
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kReferenceTime, &out_time));
  return out_time;
}

}  // namespace

class MediaEngagementScoreTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_clock.SetNow(GetReferenceTime());
    score_ = std::make_unique<MediaEngagementScore>(&test_clock, url::Origin(),
                                                    nullptr);
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  base::SimpleTestClock test_clock;

 protected:
  std::unique_ptr<MediaEngagementScore> score_;

  void VerifyScore(const MediaEngagementScore& score,
                   int expected_visits,
                   int expected_media_playbacks,
                   base::Time expected_last_media_playback_time,
                   bool has_high_score) {
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
    EXPECT_EQ(expected_last_media_playback_time,
              score.last_media_playback_time());
    EXPECT_EQ(has_high_score, score.high_score());
  }

  void UpdateScore(MediaEngagementScore& score) {
    test_clock.SetNow(test_clock.Now() + base::Hours(1));

    score.IncrementVisits();
    score.IncrementMediaPlaybacks();
  }

  void TestScoreInitializesAndUpdates(
      base::Value::Dict score_dict,
      int expected_visits,
      int expected_media_playbacks,
      base::Time expected_last_media_playback_time,
      bool has_high_score,
      bool update_score_expectation) {
    MediaEngagementScore initial_score(&test_clock, url::Origin(),
                                       std::move(score_dict),
                                       nullptr /* settings */);
    VerifyScore(initial_score, expected_visits, expected_media_playbacks,
                expected_last_media_playback_time, has_high_score);

    // Updating the score dict should return false, as the score shouldn't
    // have changed at this point.
    EXPECT_FALSE(initial_score.UpdateScoreDict());

    // Increment the scores and check that the values were stored correctly.
    UpdateScore(initial_score);
    EXPECT_EQ(update_score_expectation, initial_score.UpdateScoreDict());
  }

  static void SetScore(MediaEngagementScore& score,
                       int visits,
                       int media_playbacks) {
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
  }

  void SetScore(int visits, int media_playbacks) {
    SetScore(*score_, visits, media_playbacks);
  }

  void VerifyGetScoreDetails(const MediaEngagementScore& score) {
    media::mojom::MediaEngagementScoreDetailsPtr details =
        score.GetScoreDetails();
    EXPECT_EQ(details->origin, score.origin_);
    EXPECT_EQ(details->total_score, score.actual_score());
    EXPECT_EQ(details->visits, score.visits());
    EXPECT_EQ(details->media_playbacks, score.media_playbacks());
    EXPECT_EQ(details->last_media_playback_time,
              score.last_media_playback_time().InMillisecondsFSinceUnixEpoch());
  }
};

class MediaEngagementScoreWithOverrideFieldTrialsTest
    : public MediaEngagementScoreTest {
 public:
  void SetUp() override {
    MediaEngagementScoreTest::SetUp();
    SetScore(20, 16);
    // Raise the upper threshold. Since the score was already considered high
    // it should still be considered high.
    OverrideFieldTrial(5, 0.7, 0.9);
  }

  void OverrideFieldTrial(int min_visits,
                          double lower_threshold,
                          double upper_threshold) {
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();

    std::map<std::string, std::string> params;
    params[MediaEngagementScore::kScoreMinVisitsParamName] =
        base::NumberToString(min_visits);
    params[MediaEngagementScore::kHighScoreLowerThresholdParamName] =
        base::NumberToString(lower_threshold);
    params[MediaEngagementScore::kHighScoreUpperThresholdParamName] =
        base::NumberToString(upper_threshold);

    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeatureWithParameters(
        media::kMediaEngagementBypassAutoplayPolicies, params);

    std::map<std::string, std::string> actual_params;
    EXPECT_TRUE(base::GetFieldTrialParamsByFeature(
        media::kMediaEngagementBypassAutoplayPolicies, &actual_params));
    EXPECT_EQ(params, actual_params);

    score_->Recalculate();
  }

 private:
  // Has to be initialized at the test harness level, not at the level of
  // individual tests.
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Test Mojo serialization.
TEST_F(MediaEngagementScoreTest, MojoSerialization) {
  VerifyGetScoreDetails(*score_);
  UpdateScore(*score_);
  VerifyGetScoreDetails(*score_);
}

// Test that scores are read / written correctly from / to empty score
// dictionaries.
TEST_F(MediaEngagementScoreTest, EmptyDictionary) {
  TestScoreInitializesAndUpdates(base::Value::Dict(), 0, 0, base::Time(), false,
                                 true);
}

// Test that scores are read / written correctly from / to partially empty
// score dictionaries.
TEST_F(MediaEngagementScoreTest, PartiallyEmptyDictionary) {
  base::Value::Dict dict;
  dict.Set(MediaEngagementScore::kVisitsKey, int(2));

  TestScoreInitializesAndUpdates(std::move(dict), 2, 0, base::Time(), false,
                                 true);
}

// Test that scores are read / written correctly from / to populated score
// dictionaries.
TEST_F(MediaEngagementScoreTest, PopulatedDictionary) {
  base::Value::Dict dict;
  dict.Set(MediaEngagementScore::kVisitsKey, int(20));
  dict.Set(MediaEngagementScore::kMediaPlaybacksKey, int(12));
  dict.Set(MediaEngagementScore::kLastMediaPlaybackTimeKey,
           double(test_clock.Now().ToInternalValue()));
  dict.Set(MediaEngagementScore::kHasHighScoreKey, true);

  TestScoreInitializesAndUpdates(std::move(dict), 20, 12, test_clock.Now(),
                                 true, true);
}

// Test getting and commiting the score works correctly with different
// origins.
TEST_F(MediaEngagementScoreTest, ContentSettingsMultiOrigin) {
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));

  // Replace |score_| with one with an actual URL, and with a settings map.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  MediaEngagementScore score(&test_clock, origin, settings_map);

  // Verify the score is originally zero, try incrementing and storing
  // the score.
  VerifyScore(score, 0, 0, base::Time(), false);
  score.IncrementVisits();
  UpdateScore(score);
  score.Commit();

  // Now confirm the correct score is present on the same origin,
  // but zero for a different origin.
  url::Origin same_origin = url::Origin::Create(GURL("https://www.google.com"));
  url::Origin different_origin =
      url::Origin::Create(GURL("https://www.google.co.uk"));
  MediaEngagementScore new_score(&test_clock, origin, settings_map);
  MediaEngagementScore same_origin_score(&test_clock, same_origin,
                                         settings_map);
  MediaEngagementScore different_origin_score(&test_clock, different_origin,
                                              settings_map);
  VerifyScore(new_score, 2, 1, test_clock.Now(), false);
  VerifyScore(same_origin_score, 2, 1, test_clock.Now(), false);
  VerifyScore(different_origin_score, 0, 0, base::Time(), false);
}

// Tests content settings read/write.
TEST_F(MediaEngagementScoreTest, ContentSettings) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  int example_num_visits = 20;
  int example_media_playbacks = 5;

  // Store some example data in content settings.
  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  base::Value::Dict score_dict;
  score_dict.Set(MediaEngagementScore::kVisitsKey, example_num_visits);
  score_dict.Set(MediaEngagementScore::kMediaPlaybacksKey,
                 example_media_playbacks);
  score_dict.Set(MediaEngagementScore::kLastMediaPlaybackTimeKey,
                 double(test_clock.Now().ToInternalValue()));
  score_dict.Set(MediaEngagementScore::kHasHighScoreKey, false);
  settings_map->SetWebsiteSettingDefaultScope(
      origin.GetURL(), GURL(), ContentSettingsType::MEDIA_ENGAGEMENT,
      base::Value(std::move(score_dict)));

  // Make sure we read that data back correctly.
  MediaEngagementScore score(&test_clock, origin, settings_map);
  EXPECT_EQ(score.visits(), example_num_visits);
  EXPECT_EQ(score.media_playbacks(), example_media_playbacks);
  EXPECT_EQ(score.last_media_playback_time(), test_clock.Now());
  EXPECT_FALSE(score.high_score());

  UpdateScore(score);
  score.IncrementMediaPlaybacks();
  EXPECT_TRUE(score.high_score());
  score.Commit();

  // Now read back content settings and make sure we have the right values.
  base::Value::Dict values =
      settings_map
          ->GetWebsiteSetting(origin.GetURL(), GURL(),
                              ContentSettingsType::MEDIA_ENGAGEMENT, nullptr)
          .TakeDict();
  std::optional<int> stored_visits =
      values.FindInt(MediaEngagementScore::kVisitsKey);
  std::optional<int> stored_media_playbacks =
      values.FindInt(MediaEngagementScore::kMediaPlaybacksKey);
  std::optional<double> stored_last_media_playback_time =
      values.FindDouble(MediaEngagementScore::kLastMediaPlaybackTimeKey);
  EXPECT_TRUE(stored_visits);
  EXPECT_TRUE(stored_media_playbacks);
  EXPECT_TRUE(stored_last_media_playback_time);
  EXPECT_THAT(values.FindBool(MediaEngagementScore::kHasHighScoreKey),
              Optional(true));
  EXPECT_EQ(*stored_visits, example_num_visits + 1);
  EXPECT_EQ(*stored_media_playbacks, example_media_playbacks + 2);
  EXPECT_EQ(*stored_last_media_playback_time,
            test_clock.Now().ToInternalValue());
}

// Test that the engagement score is calculated correctly.
TEST_F(MediaEngagementScoreTest, EngagementScoreCalculation) {
  EXPECT_EQ(0, score_->actual_score());
  UpdateScore(*score_);

  // Check that the score increases when there is one visit.
  EXPECT_EQ(0.05, score_->actual_score());

  SetScore(20, 8);
  EXPECT_EQ(0.4, score_->actual_score());

  UpdateScore(*score_);
  EXPECT_EQ(9.0 / 21.0, score_->actual_score());
}

// Test that a score without the high_score bit uses the correct bounds.
TEST_F(MediaEngagementScoreTest, HighScoreLegacy_High) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://www.example.com"));
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  {
    base::Value::Dict dict;
    dict.Set(MediaEngagementScore::kVisitsKey, 20);
    dict.Set(MediaEngagementScore::kMediaPlaybacksKey, 6);
    settings_map->SetWebsiteSettingDefaultScope(
        origin.GetURL(), GURL(), ContentSettingsType::MEDIA_ENGAGEMENT,
        base::Value(std::move(dict)));
  }

  {
    MediaEngagementScore score(&test_clock, origin, settings_map);
    VerifyScore(score, 20, 6, base::Time(), true);
  }
}

// Test that a score without the high_score bit uses the correct bounds.
TEST_F(MediaEngagementScoreTest, HighScoreLegacy_Low) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://www.example.com"));
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  {
    base::Value::Dict dict;
    dict.Set(MediaEngagementScore::kVisitsKey, 20);
    dict.Set(MediaEngagementScore::kMediaPlaybacksKey, 4);
    settings_map->SetWebsiteSettingDefaultScope(
        origin.GetURL(), GURL(), ContentSettingsType::MEDIA_ENGAGEMENT,
        base::Value(std::move(dict)));
  }

  {
    MediaEngagementScore score(&test_clock, origin, settings_map);
    VerifyScore(score, 20, 4, base::Time(), false);
  }
}

// Test that if we changed the boundaries the high_score bit is updated
// when the score is loaded.
TEST_F(MediaEngagementScoreTest, HighScoreUpdated) {
  const url::Origin origin =
      url::Origin::Create(GURL("https://www.example.com"));
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  {
    base::Value::Dict dict;
    dict.Set(MediaEngagementScore::kVisitsKey, 10);
    dict.Set(MediaEngagementScore::kMediaPlaybacksKey, 1);
    dict.Set(MediaEngagementScore::kLastMediaPlaybackTimeKey,
             static_cast<double>(test_clock.Now().ToInternalValue()));
    dict.Set(MediaEngagementScore::kHasHighScoreKey, true);

    settings_map->SetWebsiteSettingDefaultScope(
        origin.GetURL(), GURL(), ContentSettingsType::MEDIA_ENGAGEMENT,
        base::Value(std::move(dict)));
  }

  {
    MediaEngagementScore score(&test_clock, origin, settings_map);
    EXPECT_FALSE(score.high_score());
    base::RunLoop().RunUntilIdle();
  }

  {
    base::Value::Dict dict =
        settings_map
            ->GetWebsiteSetting(origin.GetURL(), GURL(),
                                ContentSettingsType::MEDIA_ENGAGEMENT, nullptr)
            .TakeDict();

    EXPECT_THAT(
        dict.FindBoolByDottedPath(MediaEngagementScore::kHasHighScoreKey),
        Optional(false));
  }
}

// Test that the has high score upper and lower thresholds work.
TEST_F(MediaEngagementScoreTest, HighScoreThreshold) {
  EXPECT_FALSE(score_->high_score());

  // Test that a total score of 0.1 is not high.
  SetScore(20, 2);
  EXPECT_FALSE(score_->high_score());

  // Test that a total score of 0.25 is not high but above zero.
  SetScore(20, 5);
  EXPECT_FALSE(score_->high_score());

  // Test that a total score of 0.3 is high.
  SetScore(20, 6);
  EXPECT_TRUE(score_->high_score());

  // Test that a total score of 0.25 is high because of the lower boundary.
  SetScore(20, 5);
  EXPECT_TRUE(score_->high_score());

  // Test that a total score of 0.1 is not high.
  SetScore(20, 2);
  EXPECT_FALSE(score_->high_score());
}

TEST_F(MediaEngagementScoreTest, DefaultValues) {
  EXPECT_EQ(20, MediaEngagementScore::GetScoreMinVisits());
  EXPECT_EQ(0.2, MediaEngagementScore::GetHighScoreLowerThreshold());
  EXPECT_EQ(0.3, MediaEngagementScore::GetHighScoreUpperThreshold());

  SetScore(20, 16);
  EXPECT_EQ(0.8, score_->actual_score());
  EXPECT_TRUE(score_->high_score());
}

TEST_F(MediaEngagementScoreWithOverrideFieldTrialsTest, OverrideFieldTrial) {
  EXPECT_TRUE(score_->high_score());
  EXPECT_EQ(0.7, MediaEngagementScore::GetHighScoreLowerThreshold());
  EXPECT_EQ(0.9, MediaEngagementScore::GetHighScoreUpperThreshold());

  // Raise the lower threshold, the score will no longer be high.
  OverrideFieldTrial(5, 0.85, 0.9);
  EXPECT_FALSE(score_->high_score());
  EXPECT_EQ(0.85, MediaEngagementScore::GetHighScoreLowerThreshold());

  // Raise the minimum visits, the score will now be relative to this new
  // visits requirements.
  OverrideFieldTrial(25, 0.85, 0.9);
  EXPECT_EQ(0.64, score_->actual_score());
  EXPECT_EQ(25, MediaEngagementScore::GetScoreMinVisits());
}

class MediaEngagementScoreWithHTTPSOnlyTest : public MediaEngagementScoreTest {
 private:
  // Has to be initialized at the test harness level, not at the level of
  // individual tests.
  base::test::ScopedFeatureList scoped_feature_list_{
      /*enable_feature=*/media::kMediaEngagementHTTPSOnly};
};

// Test that scores are read / written correctly from / to populated score
// dictionaries.
TEST_F(MediaEngagementScoreWithHTTPSOnlyTest, PopulatedDictionary_HTTPSOnly) {
  base::Value::Dict dict;
  dict.Set(MediaEngagementScore::kVisitsKey, int(20));
  dict.Set(MediaEngagementScore::kMediaPlaybacksKey, int(12));
  dict.Set(
      MediaEngagementScore::kLastMediaPlaybackTimeKey,
      double(test_clock.Now().ToDeltaSinceWindowsEpoch().InMicroseconds()));
  dict.Set(MediaEngagementScore::kHasHighScoreKey, true);

  TestScoreInitializesAndUpdates(std::move(dict), 0, 0, base::Time(), false,
                                 false);
}

TEST_F(MediaEngagementScoreTest, DoNotStoreDeprecatedFields) {
  constexpr char kVisitsWithMediaTag[] = "visitsWithMediaTag";
  constexpr char kAudiblePlaybacks[] = "audiblePlaybacks";
  constexpr char kSignificantPlaybacks[] = "significantPlaybacks";
  constexpr char kHighScoreChanges[] = "highScoreChanges";
  constexpr char kMediaElementPlaybacks[] = "mediaElementPlaybacks";
  constexpr char kAudioContextPlaybacks[] = "audioContextPlaybacks";

  // This field is not deprecated by it is not used by media engagement so it
  // should not be deleted.
  constexpr char kNotDeprectedUnknown[] = "unknown";

  url::Origin origin = url::Origin::Create(GURL("https://www.google.com"));
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  base::Value::Dict score_dict;

  // Store data with deprecated fields in content settings.
  score_dict.Set(kVisitsWithMediaTag, 10);
  score_dict.Set(kAudiblePlaybacks, 10);
  score_dict.Set(kSignificantPlaybacks, 10);
  score_dict.Set(kHighScoreChanges, 10);
  score_dict.Set(kMediaElementPlaybacks, 10);
  score_dict.Set(kAudioContextPlaybacks, 10);

  // These fields are not deprecated and should not be removed.
  score_dict.Set(MediaEngagementScore::kVisitsKey, 20);
  score_dict.Set(MediaEngagementScore::kMediaPlaybacksKey, 12);
  score_dict.Set(MediaEngagementScore::kLastMediaPlaybackTimeKey,
                 static_cast<double>(test_clock.Now().ToInternalValue()));
  score_dict.Set(MediaEngagementScore::kHasHighScoreKey, true);
  score_dict.Set(kNotDeprectedUnknown, 10);
  settings_map->SetWebsiteSettingDefaultScope(
      origin.GetURL(), GURL(), ContentSettingsType::MEDIA_ENGAGEMENT,
      base::Value(std::move(score_dict)));

  // Run the data through media engagement score.
  MediaEngagementScore score(&test_clock, origin, settings_map);
  UpdateScore(score);
  score.Commit();

  // Check the deprecated fields have been dropped.
  base::Value::Dict values =
      settings_map
          ->GetWebsiteSetting(origin.GetURL(), GURL(),
                              ContentSettingsType::MEDIA_ENGAGEMENT, nullptr)
          .TakeDict();
  EXPECT_FALSE(values.contains(kVisitsWithMediaTag));
  EXPECT_FALSE(values.contains(kAudiblePlaybacks));
  EXPECT_FALSE(values.contains(kSignificantPlaybacks));
  EXPECT_FALSE(values.contains(kHighScoreChanges));
  EXPECT_FALSE(values.contains(kMediaElementPlaybacks));
  EXPECT_FALSE(values.contains(kAudioContextPlaybacks));

  // Check the non-deprecated fields are still present.
  EXPECT_TRUE(values.contains(MediaEngagementScore::kVisitsKey));
  EXPECT_TRUE(values.contains(MediaEngagementScore::kMediaPlaybacksKey));
  EXPECT_TRUE(values.contains(MediaEngagementScore::kLastMediaPlaybackTimeKey));
  EXPECT_TRUE(values.contains(MediaEngagementScore::kHasHighScoreKey));
  EXPECT_TRUE(values.contains(kNotDeprectedUnknown));
}
