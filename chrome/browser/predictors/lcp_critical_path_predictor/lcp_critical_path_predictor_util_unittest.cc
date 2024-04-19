// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace predictors {
namespace {
class Updater {
 public:
  Updater(size_t sliding_window_size, size_t max_histogram_buckets)
      : sliding_window_size_(sliding_window_size),
        max_histogram_buckets_(max_histogram_buckets) {}
  ~Updater() = default;

  void Update(const std::string& new_entry) {
    UpdateLcppStringFrequencyStatData(
        sliding_window_size_, max_histogram_buckets_, new_entry, stat_data_);
  }

  const LcppStringFrequencyStatData& Data() { return stat_data_; }

 private:
  LcppStringFrequencyStatData stat_data_;
  const size_t sliding_window_size_;
  const size_t max_histogram_buckets_;
};

LcppStringFrequencyStatData MakeData(std::map<std::string, double> main_buckets,
                                     double others) {
  LcppStringFrequencyStatData data;
  for (auto& [key, freq] : main_buckets) {
    data.mutable_main_buckets()->insert({key, freq});
  }
  data.set_other_bucket_frequency(others);
  return data;
}

}  // namespace

TEST(UpdateLcppStringFrequencyStatDataTest, Base) {
  Updater updater(/*sliding_window_size=*/5u,
                  /*max_histogram_buckets=*/2u);
  EXPECT_EQ(updater.Data(), MakeData({}, 0)) << updater.Data();

  updater.Update("foo");
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1}}, 0)) << updater.Data();

  updater.Update("bar");
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1}, {"bar", 1}}, 0))
      << updater.Data();

  updater.Update("foo");
  updater.Update("foo");
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 3}, {"bar", 1}}, 0))
      << updater.Data();

  updater.Update("baz");
  // If kinds of entry are over 'max_histogram_buckets', the oldest bucket is
  // converted to 'other_bucket_frequency'.
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 3}, {"baz", 1}}, 1))
      << updater.Data();

  updater.Update("foobar");
  // When an entry is dropped out of 'sliding_window_size', existing frequencies
  // are recalculated as:
  // next_freq = current_freq/sliding_window_size * (sliding_window_size - 1)
  // next_others_frequency = (current_others_freq + dropped_entry_freq)
  //                         /sliding_window_size * (sliding_window_size - 1)
  // new_freq = 1
  // then
  // "foo" = 3/5 * 4
  // "others" = (1 + 1)/5 * 4
  // See lcp_critical_path_predictor_util.cc for detail.
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 2.4}, {"foobar", 1}}, 1.6))
      << updater.Data();
}

TEST(IsValidLcppStatTest, Empty) {
  LcppStat lcpp_stat;
  EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
}

TEST(IsValidLcppStatTest, LcpElementLocatorStat) {
  {
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_lcp_element_locator("fake");
    bucket->set_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Without the repeated field.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative other frequency is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Nothing in an entry is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    locator_stat->add_lcp_element_locator_buckets();  // allocate a bucket.
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_lcp_element_locator("fake");
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No element locator in an entry is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_frequency(0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_lcp_element_locator("fake");
    bucket->set_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
}

TEST(IsValidLcppStatTest, LcpScriptUrlStat) {
  {
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/script.js", 0.1});
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Without the map field.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative other frequency is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/script.js", -0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Empty URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Invalid URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"invalid url", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No HTTP/HTTPS URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"wss://example.com/", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Too long URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/" +
             std::string(ResourcePrefetchPredictorTables::kMaxStringLength,
                         'a'),
         0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
}

TEST(IsValidLcppStatTest, FetchedFontUrlStat) {
  {
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com/a.woff", 0.1});
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Without the map field.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative other frequency is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com/a.woff", -0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Empty URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Invalid URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"invalid url", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No HTTP/HTTPS URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"wss://example.com/", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Too long URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/" +
             std::string(ResourcePrefetchPredictorTables::kMaxStringLength,
                         'a'),
         0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
}

TEST(IsValidLcppStatTest, PreconnectOriginsStat) {
  {
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com", 0.1});
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Without the map field.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative other frequency is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com", -0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Empty URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Invalid URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"invalid url", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No HTTP/HTTPS URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"wss://example.com/", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Too long URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/" +
             std::string(ResourcePrefetchPredictorTables::kMaxStringLength,
                         'a'),
         0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
}

TEST(IsValidLcppStatTest, DeferUnusedPreloads) {
  {
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/unused.png", 0.1});
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Without the map field.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative other frequency is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/unused.png", -0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Empty URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Invalid URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"invalid url", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No HTTP/HTTPS URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"wss://example.com/", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Too long URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/" +
             std::string(ResourcePrefetchPredictorTables::kMaxStringLength,
                         'a'),
         0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
}

TEST(IsValidLcppStatTest, MixedPattern) {
  LcppStat lcpp_stat;
  auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
  locator_stat->set_other_bucket_frequency(0.1);
  {
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_lcp_element_locator("fake");
    bucket->set_frequency(0.1);
  }
  {
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/script.js", 0.1});
  }
  {
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com/a.woff", 0.1});
  }
  {
    auto* stat = lcpp_stat.mutable_preconnect_origin_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com", 0.1});
  }
  {
    auto* stat = lcpp_stat.mutable_unused_preload_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/unused.png", 0.1});
  }
  EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
}

TEST(PredictFetchedFontUrls, Empty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPFontURLPredictor, {}}}, {});
  LcppStat lcpp_stat;
  EXPECT_EQ(std::vector<GURL>(), PredictFetchedFontUrls(lcpp_stat));
}

TEST(PredictFetchedFontUrls, Simple) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPFontURLPredictor,
        {{blink::features::kLCPPFontURLPredictorFrequencyThreshold.name, "0.5"},
         {blink::features::kLCPPFontURLPredictorMaxPreloadCount.name, "10"}}}},
      {});
  LcppStat lcpp_stat;
  lcpp_stat.mutable_fetched_font_url_stat()->mutable_main_buckets()->insert(
      {"https://example.com/a.woff", 0.9});
  std::vector<GURL> expected;
  expected.emplace_back("https://example.com/a.woff");
  EXPECT_EQ(expected, PredictFetchedFontUrls(lcpp_stat));
}

TEST(PredictFetchedFontUrls, BrokenFontNames) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPFontURLPredictor,
        {{blink::features::kLCPPFontURLPredictorFrequencyThreshold.name, "0.5"},
         {blink::features::kLCPPFontURLPredictorMaxPreloadCount.name, "10"}}}},
      {});
  LcppStat lcpp_stat;
  auto* main_buckets =
      lcpp_stat.mutable_fetched_font_url_stat()->mutable_main_buckets();
  // Duplicated.
  main_buckets->insert({"https://example.com/a.woff", 0.9});
  main_buckets->insert({"https://example.com/a.woff", 0.8});
  main_buckets->insert({"https://example.com/a.woff", 0.7});
  main_buckets->insert({"https://example.com/a.woff", 0.6});
  // Not an HTTP/HTTPS.
  main_buckets->insert({"wss://example.com/", 0.9});
  std::vector<GURL> expected;
  expected.emplace_back("https://example.com/a.woff");
  EXPECT_EQ(expected, PredictFetchedFontUrls(lcpp_stat));
}

TEST(PredictFetchedFontUrls, Threshold) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPFontURLPredictor,
        {{blink::features::kLCPPFontURLPredictorFrequencyThreshold.name, "0.5"},
         {blink::features::kLCPPFontURLPredictorMaxPreloadCount.name, "10"}}}},
      {});
  LcppStat lcpp_stat;
  auto* main_buckets =
      lcpp_stat.mutable_fetched_font_url_stat()->mutable_main_buckets();
  main_buckets->insert({"https://example.com/a.woff", 0.9});
  main_buckets->insert({"https://example.com/b.woff", 0.1});
  std::vector<GURL> expected;
  expected.emplace_back("https://example.com/a.woff");
  EXPECT_EQ(expected, PredictFetchedFontUrls(lcpp_stat));
}

TEST(PredictFetchedFontUrls, MaxUrls) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{blink::features::kLCPPFontURLPredictor,
          {{blink::features::kLCPPFontURLPredictorFrequencyThreshold.name,
            "0.5"},
           {blink::features::kLCPPFontURLPredictorMaxPreloadCount.name, "1"}}}},
        {});
    LcppStat lcpp_stat;
    auto* main_buckets =
        lcpp_stat.mutable_fetched_font_url_stat()->mutable_main_buckets();
    main_buckets->insert({"https://example.com/a.woff", 0.9});
    main_buckets->insert({"https://example.com/b.woff", 0.8});
    std::vector<GURL> expected;
    expected.emplace_back("https://example.com/a.woff");
    EXPECT_EQ(expected, PredictFetchedFontUrls(lcpp_stat));
  }
  {  // Use MaxUrls as a kill switch.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{blink::features::kLCPPFontURLPredictor,
          {{blink::features::kLCPPFontURLPredictorFrequencyThreshold.name,
            "0.5"},
           {blink::features::kLCPPFontURLPredictorMaxPreloadCount.name, "0"}}}},
        {});
    LcppStat lcpp_stat;
    auto* main_buckets =
        lcpp_stat.mutable_fetched_font_url_stat()->mutable_main_buckets();
    main_buckets->insert({"https://example.com/a.woff", 0.9});
    main_buckets->insert({"https://example.com/b.woff", 0.8});
    std::vector<GURL> expected;
    EXPECT_EQ(expected, PredictFetchedFontUrls(lcpp_stat));
  }
}

TEST(PredictFetchedSubresourceUrls, Empty) {
  EXPECT_EQ(std::vector<GURL>(), PredictFetchedSubresourceUrls({}));
}

TEST(PredictFetchedSubresourceUrls, SingleEntry) {
  LcppStat lcpp_stat;
  lcpp_stat.mutable_fetched_subresource_url_stat()
      ->mutable_main_buckets()
      ->insert({"https://example.com/a.jpeg", 0.9});
  EXPECT_EQ(std::vector<GURL>({GURL("https://example.com/a.jpeg")}),
            PredictFetchedSubresourceUrls(lcpp_stat));
}

TEST(PredictFetchedSubresourceUrls, SortedByFrequencyInDescendingOrder) {
  LcppStat lcpp_stat;
  auto* buckets =
      lcpp_stat.mutable_fetched_subresource_url_stat()->mutable_main_buckets();
  buckets->insert({"https://example.com/c.jpeg", 0.1});
  buckets->insert({"https://example.com/a.jpeg", 0.3});
  buckets->insert({"https://example.com/b.jpeg", 0.2});
  EXPECT_EQ(std::vector<GURL>({GURL("https://example.com/a.jpeg"),
                               GURL("https://example.com/b.jpeg"),
                               GURL("https://example.com/c.jpeg")}),
            PredictFetchedSubresourceUrls(lcpp_stat));
}

TEST(PredictFetchedSubresourceUrls, FilterUrls) {
  LcppStat lcpp_stat;
  auto* buckets =
      lcpp_stat.mutable_fetched_subresource_url_stat()->mutable_main_buckets();
  buckets->insert({"https://example.com/a.jpeg", 0.1});
  buckets->insert({"https://example.com/b.jpeg", 0.2});
  // Not an HTTP/HTTPS.
  buckets->insert({"file://example.com/c.jpeg", 0.7});
  // Not an URL.
  buckets->insert({"d.jpeg", 0.8});
  EXPECT_EQ(4U, buckets->size());
  EXPECT_EQ(std::vector<GURL>({GURL("https://example.com/b.jpeg"),
                               GURL("https://example.com/a.jpeg")}),
            PredictFetchedSubresourceUrls(lcpp_stat));
}

TEST(PredictPreconnectableOrigins, Empty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPAutoPreconnectLcpOrigin, {}}}, {});
  LcppStat lcpp_stat;
  EXPECT_EQ(std::vector<GURL>(), PredictPreconnectableOrigins(lcpp_stat));
}

TEST(PredictPreconnectableOrigins, Simple) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPAutoPreconnectLcpOrigin,
        {{blink::features::kLCPPAutoPreconnectFrequencyThreshold.name, "0.5"},
         {blink::features::kkLCPPAutoPreconnectMaxPreconnectOriginsCount.name,
          "10"}}}},
      {});
  LcppStat lcpp_stat;
  lcpp_stat.mutable_preconnect_origin_stat()->mutable_main_buckets()->insert(
      {"https://example.com", 0.9});
  std::vector<GURL> expected;
  expected.emplace_back("https://example.com");
  EXPECT_EQ(expected, PredictPreconnectableOrigins(lcpp_stat));
}

TEST(PredictPreconnectableOrigins, SortedByFrequencyInDescendingOrder) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPAutoPreconnectLcpOrigin,
        {{blink::features::kLCPPAutoPreconnectFrequencyThreshold.name, "0.1"},
         {blink::features::kkLCPPAutoPreconnectMaxPreconnectOriginsCount.name,
          "10"}}}},
      {});
  LcppStat lcpp_stat;
  auto* buckets =
      lcpp_stat.mutable_preconnect_origin_stat()->mutable_main_buckets();
  buckets->insert({"https://example.com", 0.1});
  buckets->insert({"https://example2.com", 0.3});
  buckets->insert({"https://example3.com", 0.2});
  EXPECT_EQ(std::vector<GURL>({GURL("https://example2.com"),
                               GURL("https://example3.com"),
                               GURL("https://example.com")}),
            PredictPreconnectableOrigins(lcpp_stat));
}

TEST(PredictPreconnectableOrigins, Threshold) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPAutoPreconnectLcpOrigin,
        {{blink::features::kLCPPAutoPreconnectFrequencyThreshold.name, "0.5"},
         {blink::features::kkLCPPAutoPreconnectMaxPreconnectOriginsCount.name,
          "10"}}}},
      {});
  LcppStat lcpp_stat;
  auto* main_buckets =
      lcpp_stat.mutable_preconnect_origin_stat()->mutable_main_buckets();
  main_buckets->insert({"https://example1.com", 0.9});
  main_buckets->insert({"https://example2.com", 0.1});
  std::vector<GURL> expected;
  expected.emplace_back("https://example1.com");
  EXPECT_EQ(expected, PredictPreconnectableOrigins(lcpp_stat));
}

TEST(PredictPreconnectableOrigins, MaxUrls) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{blink::features::kLCPPAutoPreconnectLcpOrigin,
          {{blink::features::kLCPPAutoPreconnectFrequencyThreshold.name, "0.5"},
           {blink::features::kkLCPPAutoPreconnectMaxPreconnectOriginsCount.name,
            "1"}}}},
        {});
    LcppStat lcpp_stat;
    auto* main_buckets =
        lcpp_stat.mutable_preconnect_origin_stat()->mutable_main_buckets();
    main_buckets->insert({"https://example.com", 0.9});
    main_buckets->insert({"https://example1.com", 0.8});
    std::vector<GURL> expected;
    expected.emplace_back("https://example.com");
    EXPECT_EQ(expected, PredictPreconnectableOrigins(lcpp_stat));
  }
  {  // Use MaxUrls as a kill switch.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{blink::features::kLCPPAutoPreconnectLcpOrigin,
          {{blink::features::kLCPPAutoPreconnectFrequencyThreshold.name, "0.5"},
           {blink::features::kkLCPPAutoPreconnectMaxPreconnectOriginsCount.name,
            "0"}}}},
        {});
    LcppStat lcpp_stat;
    auto* main_buckets =
        lcpp_stat.mutable_preconnect_origin_stat()->mutable_main_buckets();
    main_buckets->insert({"https://example1.com", 0.9});
    main_buckets->insert({"https://example2.com", 0.8});
    std::vector<GURL> expected;
    EXPECT_EQ(expected, PredictPreconnectableOrigins(lcpp_stat));
  }
}

TEST(PredictPreconnectableOrigins, FilterUrls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPAutoPreconnectLcpOrigin,
        {{blink::features::kLCPPAutoPreconnectFrequencyThreshold.name, "0.5"},
         {blink::features::kkLCPPAutoPreconnectMaxPreconnectOriginsCount.name,
          "10"}}}},
      {});
  LcppStat lcpp_stat;
  auto* buckets =
      lcpp_stat.mutable_preconnect_origin_stat()->mutable_main_buckets();
  buckets->insert({"https://example1.com", 0.9});
  buckets->insert({"https://example2.com", 0.8});
  // Not an HTTP/HTTPS.
  buckets->insert({"file://example.com", 0.7});
  // Not an URL.
  buckets->insert({"d.jpeg", 0.8});
  EXPECT_EQ(4U, buckets->size());
  EXPECT_EQ(std::vector<GURL>(
                {GURL("https://example1.com"), GURL("https://example2.com")}),
            PredictPreconnectableOrigins(lcpp_stat));
}

TEST(PredictUnusedPreloads, Empty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPDeferUnusedPreload,
        {{blink::features::kLCPPDeferUnusedPreloadFrequencyThreshold.name,
          "0.5"}}}},
      {});

  EXPECT_EQ(std::vector<GURL>(), PredictUnusedPreloads({}));
}

TEST(PredictUnusedPreloads, SingleEntry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPDeferUnusedPreload,
        {{blink::features::kLCPPDeferUnusedPreloadFrequencyThreshold.name,
          "0.5"}}}},
      {});

  LcppStat lcpp_stat;
  lcpp_stat.mutable_unused_preload_stat()->mutable_main_buckets()->insert(
      {"https://example.com/a.jpeg", 0.9});
  EXPECT_EQ(std::vector<GURL>({GURL("https://example.com/a.jpeg")}),
            PredictUnusedPreloads(lcpp_stat));
}

TEST(PredictUnusedPreloads, SortedByFrequencyInDescendingOrder) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPDeferUnusedPreload,
        {{blink::features::kLCPPDeferUnusedPreloadFrequencyThreshold.name,
          "0"}}}},
      {});

  LcppStat lcpp_stat;
  auto* buckets =
      lcpp_stat.mutable_unused_preload_stat()->mutable_main_buckets();
  buckets->insert({"https://example.com/c.jpeg", 0.1});
  buckets->insert({"https://example.com/a.jpeg", 0.3});
  buckets->insert({"https://example.com/b.jpeg", 0.2});
  EXPECT_EQ(std::vector<GURL>({GURL("https://example.com/a.jpeg"),
                               GURL("https://example.com/b.jpeg"),
                               GURL("https://example.com/c.jpeg")}),
            PredictUnusedPreloads(lcpp_stat));
}

TEST(PredictUnusedPreloads, FilterUrls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPDeferUnusedPreload,
        {{blink::features::kLCPPDeferUnusedPreloadFrequencyThreshold.name,
          "0"}}}},
      {});

  LcppStat lcpp_stat;
  auto* buckets =
      lcpp_stat.mutable_unused_preload_stat()->mutable_main_buckets();
  buckets->insert({"https://example.com/a.jpeg", 0.1});
  buckets->insert({"https://example.com/b.jpeg", 0.2});
  // Not an HTTP/HTTPS.
  buckets->insert({"file://example.com/c.jpeg", 0.7});
  // Not an URL.
  buckets->insert({"d.jpeg", 0.8});
  EXPECT_EQ(4U, buckets->size());
  EXPECT_EQ(std::vector<GURL>({GURL("https://example.com/b.jpeg"),
                               GURL("https://example.com/a.jpeg")}),
            PredictUnusedPreloads(lcpp_stat));
}

TEST(PredictUnusedPreloads, Threshold) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kLCPPDeferUnusedPreload,
        {{blink::features::kLCPPDeferUnusedPreloadFrequencyThreshold.name,
          "0.5"}}}},
      {});

  LcppStat lcpp_stat;
  auto* buckets =
      lcpp_stat.mutable_unused_preload_stat()->mutable_main_buckets();
  buckets->insert({"https://example.com/a.jpeg", 0.9});
  buckets->insert({"https://example.com/b.jpeg", 0.1});
  EXPECT_EQ(std::vector<GURL>({GURL("https://example.com/a.jpeg")}),
            PredictUnusedPreloads(lcpp_stat));
}

TEST(LcppKeyTest, InvalidURLs) {
  const std::string invalid_urls[] = {
      // Invalid urls
      "http://?k=v",
      "http:://google.com",
      "http://google.com:12three45",
      "://google.com",
      "path",
      "",                  // Empty
      "file://server:0",   // File
      "ftp://server",      // Ftp
      "http://localhost",  // Localhost
      "http://127.0.0.1",  // Localhost
      "https://example" +
          std::string(ResourcePrefetchPredictorTables::kMaxStringLength, 'a') +
          ".test/",  // Too long
  };

  for (auto& invalid_url : invalid_urls) {
    const GURL url(invalid_url);
    EXPECT_FALSE(IsURLValidForLcpp(url)) << invalid_url;
  }
}

size_t GetLCPPMultipleKeyMaxPathLength() {
  static const size_t max_length = base::checked_cast<size_t>(
      blink::features::kLCPPMultipleKeyMaxPathLength.Get());
  return max_length;
}

TEST(LcppMultipleKeyTest, GetFirstLevelPath) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(blink::features::kLCPPMultipleKey);

  const size_t max_path_length = GetLCPPMultipleKeyMaxPathLength();
  const std::string long_path = "/" + std::string(max_path_length - 1, 'b');
  const std::string too_long_path =
      "/" + std::string(max_path_length + 1, 'c') + "/bar";
  const std::vector<std::pair<std::string, std::string>> url_keys = {
      {"http://a.test", ""},
      {"http://user:pass@a.test:99/foo;bar?q=a#ref", "/foo;bar"},
      {"http://a.test/", ""},
      {"http://a.test/foo.html", ""},
      {"http://a.test/foo", "/foo"},
      {"http://a.test/foo/", "/foo"},
      {"http://a.test/foo/bar", "/foo"},
      {"http://a.test/foo/bar/", "/foo"},
      {"http://a.test/foo/bar/baz.com", "/foo"},
      {"http://a.test/bar?q=c", "/bar"},
      {"http://a.test/foo/bar?q=c", "/foo"},
      {"http://a.test" + long_path, long_path},
      {"http://a.test" + long_path + "/bar", long_path},
      {"http://a.test" + long_path + "bar", ""},
      // Too long path is ignored.
      {"http://a.test" + too_long_path, ""},
      // Invalid length path in subdirectory is also ignored.
      {"http://a.test/bar" + too_long_path, "/bar"}};

  for (const auto& url_key : url_keys) {
    const GURL url(url_key.first);
    EXPECT_TRUE(IsURLValidForLcpp(url)) << url_key.first;
    EXPECT_EQ(GetFirstLevelPath(url), url_key.second) << url_key.first;
  }
}

}  // namespace predictors
