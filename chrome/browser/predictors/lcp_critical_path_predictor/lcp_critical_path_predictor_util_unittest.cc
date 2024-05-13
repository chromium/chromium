// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using testing::StrictMock;

namespace predictors {
namespace {
class Updater {
 public:
  Updater(size_t sliding_window_size, size_t max_histogram_buckets)
      : sliding_window_size_(sliding_window_size),
        max_histogram_buckets_(max_histogram_buckets) {}
  ~Updater() = default;

  void Update(const std::string& new_entry,
              std::optional<std::string>& dropped_entry) {
    UpdateLcppStringFrequencyStatData(sliding_window_size_,
                                      max_histogram_buckets_, new_entry,
                                      stat_data_, dropped_entry);
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

template <typename T>
class FakeLoadingPredictorKeyValueTable
    : public sqlite_proto::KeyValueTable<T> {
 public:
  FakeLoadingPredictorKeyValueTable() : sqlite_proto::KeyValueTable<T>("") {}
  void GetAllData(std::map<std::string, T>* data_map,
                  sql::Database* db) const override {
    *data_map = data_;
  }
  void UpdateData(const std::string& key,
                  const T& data,
                  sql::Database* db) override {
    data_[key] = data;
  }
  void DeleteData(const std::vector<std::string>& keys,
                  sql::Database* db) override {
    for (const auto& key : keys) {
      data_.erase(key);
    }
  }
  void DeleteAllData(sql::Database* db) override { data_.clear(); }

  std::map<std::string, T> data_;
};

class MockResourcePrefetchPredictorTables
    : public ResourcePrefetchPredictorTables {
 public:
  using DBTask = base::OnceCallback<void(sql::Database*)>;

  explicit MockResourcePrefetchPredictorTables(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner)
      : ResourcePrefetchPredictorTables(std::move(db_task_runner)) {}

  void ScheduleDBTask(const base::Location& from_here, DBTask task) override {
    ExecuteDBTaskOnDBSequence(std::move(task));
  }

  void ExecuteDBTaskOnDBSequence(DBTask task) override {
    std::move(task).Run(nullptr);
  }

  sqlite_proto::KeyValueTable<LcppData>* lcpp_table() override {
    return &lcpp_table_;
  }

  FakeLoadingPredictorKeyValueTable<LcppData> lcpp_table_;

 protected:
  ~MockResourcePrefetchPredictorTables() override = default;
};

}  // namespace

TEST(UpdateLcppStringFrequencyStatDataTest, Base) {
  Updater updater(/*sliding_window_size=*/5u,
                  /*max_histogram_buckets=*/2u);
  EXPECT_EQ(updater.Data(), MakeData({}, 0)) << updater.Data();

  std::optional<std::string> dropped_entry;
  updater.Update("foo", dropped_entry);
  EXPECT_FALSE(dropped_entry);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1}}, 0)) << updater.Data();

  updater.Update("bar", dropped_entry);
  EXPECT_FALSE(dropped_entry);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1}, {"bar", 1}}, 0))
      << updater.Data();

  updater.Update("foo", dropped_entry);
  EXPECT_FALSE(dropped_entry);
  updater.Update("foo", dropped_entry);
  EXPECT_FALSE(dropped_entry);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 3}, {"bar", 1}}, 0))
      << updater.Data();

  updater.Update("baz", dropped_entry);
  EXPECT_TRUE(dropped_entry);
  EXPECT_EQ("bar", *dropped_entry);
  // If kinds of entry are over 'max_histogram_buckets', the oldest bucket is
  // converted to 'other_bucket_frequency'.
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 3}, {"baz", 1}}, 1))
      << updater.Data();

  updater.Update("foobar", dropped_entry);
  EXPECT_TRUE(dropped_entry);
  EXPECT_EQ("baz", *dropped_entry);
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

TEST(UpdateLcppStringFrequencyStatDataTest, AddNewEntryToFullBuckets) {
  Updater updater(/*sliding_window_size=*/4u,
                  /*max_histogram_buckets=*/2u);

  std::optional<std::string> dropped_entry;
  updater.Update("foo", dropped_entry);
  EXPECT_FALSE(dropped_entry.has_value());
  updater.Update("foo", dropped_entry);
  EXPECT_FALSE(dropped_entry.has_value());
  updater.Update("bar", dropped_entry);
  EXPECT_FALSE(dropped_entry.has_value());
  updater.Update("bar", dropped_entry);
  EXPECT_FALSE(dropped_entry.has_value());
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 2}, {"bar", 2}}, 0))
      << updater.Data();

  updater.Update("qux", dropped_entry);
  EXPECT_EQ(*dropped_entry, "qux");
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1.5}, {"bar", 1.5}}, 1))
      << updater.Data();

  updater.Update("qux", dropped_entry);
  EXPECT_EQ(*dropped_entry, "qux");
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1.125}, {"bar", 1.125}}, 1.75))
      << updater.Data();

  updater.Update("qux", dropped_entry);
  EXPECT_EQ(*dropped_entry, "bar");
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 0.84375}, {"qux", 1}}, 2.15625))
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

class LcppDataMapTest : public testing::Test {
 public:
  void InitializeDB(const LoadingPredictorConfig& config) {
    config_ = config;
    db_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    mock_tables_ =
        base::MakeRefCounted<StrictMock<MockResourcePrefetchPredictorTables>>(
            db_task_runner_);
    lcpp_data_map_ = std::make_unique<LcppDataMap>(*mock_tables_, config);
    db_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                lcpp_data_map_->InitializeOnDBSequence();
                              }));
    db_task_runner_->RunUntilIdle();
  }

 protected:
  double SumOfLcppStringFrequencyStatData(
      const LcppStringFrequencyStatData& data) {
    double sum = data.other_bucket_frequency();
    for (const auto& [url, frequency] : data.main_buckets()) {
      sum += frequency;
    }
    return sum;
  }

  void LearnLcpp(const GURL& url, const LcppDataInputs& inputs) {
    lcpp_data_map_->LearnLcpp(url, inputs);
  }

  void LearnElementLocator(
      const GURL& url,
      const std::string& lcp_element_locator,
      const std::vector<GURL>& lcp_influencer_scripts = {}) {
    predictors::LcppDataInputs inputs;
    inputs.lcp_element_locator = lcp_element_locator;
    inputs.lcp_influencer_scripts = lcp_influencer_scripts;
    LearnLcpp(url, inputs);
  }

  void LearnFontUrls(const GURL& url, const std::vector<GURL>& font_urls) {
    LcppDataInputs inputs;
    inputs.font_urls = font_urls;
    LearnLcpp(url, inputs);
  }

  void LearnSubresourceUrls(
      const GURL& url,
      const std::map<GURL, base::TimeDelta>& subresource_urls) {
    LcppDataInputs inputs;
    inputs.subresource_urls = subresource_urls;
    LearnLcpp(url, inputs);
  }

  std::optional<LcppStat> GetLcppStat(const GURL& url) {
    return lcpp_data_map_->GetLcppStat(url);
  }

  void TestLearnLcppURL(
      const std::vector<std::pair<std::string, std::string>>& url_keys,
      const base::Location& location = FROM_HERE) {
    std::map<std::string, int> frequency;
    for (const auto& url_key : url_keys) {
      const std::string& url = url_key.first;
      const std::string& key = url_key.second;
      LearnElementLocator(GURL(url), "/#a", {});
      // Confirm 'url' was learned as 'key'.
      auto stat = lcpp_data_map_->GetLcppStat(GURL("http://" + key));
      EXPECT_TRUE(stat) << location.ToString() << url;
      LcppData expected;
      InitializeLcpElementLocatorBucket(expected, "/#a", ++frequency[key]);
      EXPECT_EQ(expected.lcpp_stat(), *stat)
          << location.ToString() << url << *stat;
    }
  }

  static LcppStat MakeLcppStatWithLCPElementLocator(
      const std::string& lcp_element_locator,
      double frequency = 1) {
    LcppStat stat;
    LcpElementLocatorBucket& bucket = *stat.mutable_lcp_element_locator_stat()
                                           ->add_lcp_element_locator_buckets();
    bucket.set_lcp_element_locator(lcp_element_locator);
    bucket.set_frequency(frequency);
    return stat;
  }

  LoadingPredictorConfig config_;
  scoped_refptr<base::TestSimpleTaskRunner> db_task_runner_;
  scoped_refptr<StrictMock<MockResourcePrefetchPredictorTables>> mock_tables_;
  std::unique_ptr<LcppDataMap> lcpp_data_map_;
};

TEST_F(LcppDataMapTest, LearnLcpp) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  EXPECT_EQ(5U, config.lcpp_histogram_sliding_window_size);
  EXPECT_EQ(2U, config.max_lcpp_histogram_buckets);
  InitializeDB(config);
  EXPECT_TRUE(mock_tables_->lcpp_table_.data_.empty());

  auto SumOfElementLocatorFrequency = [](const LcppData& data) {
    const LcpElementLocatorStat& stat =
        data.lcpp_stat().lcp_element_locator_stat();
    double sum = stat.other_bucket_frequency();
    for (const auto& bucket : stat.lcp_element_locator_buckets()) {
      sum += bucket.frequency();
    }
    return sum;
  };

  auto SumOfInfluencerUrlFrequency = [](const LcppData& data) {
    const LcppStringFrequencyStatData& stat =
        data.lcpp_stat().lcp_script_url_stat();
    double sum = stat.other_bucket_frequency();
    for (const auto& [url, frequency] : stat.main_buckets()) {
      sum += frequency;
    }
    return sum;
  };

  for (int i = 0; i < 3; ++i) {
    LearnElementLocator(GURL("http://a.test"), "/#a", {});
  }
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 3);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
    EXPECT_DOUBLE_EQ(3, SumOfElementLocatorFrequency(data));
  }

  for (int i = 0; i < 2; ++i) {
    LearnElementLocator(GURL("http://a.test"), "/#b", {});
  }
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 3);
    InitializeLcpElementLocatorBucket(data, "/#b", 2);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
    EXPECT_DOUBLE_EQ(5, SumOfElementLocatorFrequency(data));
  }

  LearnElementLocator(GURL("http://a.test"), "/#c", {});
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 2.4);
    InitializeLcpElementLocatorBucket(data, "/#b", 1.6);
    InitializeLcpElementLocatorOtherBucket(data, 1);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
    EXPECT_DOUBLE_EQ(5, SumOfElementLocatorFrequency(data));
  }

  LearnElementLocator(GURL("http://a.test"), "/#d", {});
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 1.92);
    InitializeLcpElementLocatorBucket(data, "/#b", 1.28);
    InitializeLcpElementLocatorOtherBucket(data, 1.8);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
    EXPECT_DOUBLE_EQ(5, SumOfElementLocatorFrequency(data));
  }

  for (int i = 0; i < 2; ++i) {
    LearnElementLocator(GURL("http://a.test"), "/#c", {});
    LearnElementLocator(GURL("http://a.test"), "/#d", {});
  }
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#d", 1);
    InitializeLcpElementLocatorBucket(data, "/#c", 0.8);
    InitializeLcpElementLocatorOtherBucket(data, 3.2);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
    EXPECT_DOUBLE_EQ(5, SumOfElementLocatorFrequency(data));
  }

  // Test that element locators and influencer scripts are independently learnt.
  for (int i = 0; i < 2; ++i) {
    LearnElementLocator(
        GURL("http://a.test"), "",
        {GURL("https://a.test/script1.js"), GURL("https://a.test/script2.js")});
  }
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#d", 1);
    InitializeLcpElementLocatorBucket(data, "/#c", 0.8);
    InitializeLcpElementLocatorOtherBucket(data, 3.2);
    InitializeLcpInfluencerScriptUrlsBucket(
        data,
        {GURL("https://a.test/script1.js"), GURL("https://a.test/script2.js")},
        2);
    InitializeLcpInfluencerScriptUrlsOtherBucket(data, 0);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
    EXPECT_DOUBLE_EQ(5, SumOfElementLocatorFrequency(data));
    EXPECT_DOUBLE_EQ(4, SumOfInfluencerUrlFrequency(data));
  }

  for (int i = 0; i < 3; ++i) {
    LearnElementLocator(
        GURL("http://a.test"), "",
        {GURL("https://a.test/script3.js"), GURL("https://a.test/script4.js")});
  }
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#d", 1);
    InitializeLcpElementLocatorBucket(data, "/#c", 0.8);
    InitializeLcpElementLocatorOtherBucket(data, 3.2);
    InitializeLcpInfluencerScriptUrlsBucket(
        data, {GURL("https://a.test/script3.js")}, 0.8);
    InitializeLcpInfluencerScriptUrlsBucket(
        data, {GURL("https://a.test/script4.js")}, 1);
    InitializeLcpInfluencerScriptUrlsOtherBucket(data, 3.2);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
    EXPECT_DOUBLE_EQ(5, SumOfInfluencerUrlFrequency(data));
  }
}

TEST_F(LcppDataMapTest, LearnFontUrls) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  EXPECT_EQ(5U, config.lcpp_histogram_sliding_window_size);
  EXPECT_EQ(2U, config.max_lcpp_histogram_buckets);
  InitializeDB(config);
  EXPECT_TRUE(mock_tables_->lcpp_table_.data_.empty());

  auto SumOfFontUrlFrequency = [this](const LcppData& data) {
    return SumOfLcppStringFrequencyStatData(
        data.lcpp_stat().fetched_font_url_stat());
  };
  for (int i = 0; i < 2; ++i) {
    LearnFontUrls(GURL("http://example.test"),
                  {
                      GURL("https://example.test/test.woff"),
                      GURL("https://example.test/test.ttf"),
                  });
  }
  {
    LcppData data = CreateLcppData("example.test", 10);
    InitializeFontUrlsBucket(data,
                             {GURL("https://example.test/test.woff"),
                              GURL("https://example.test/test.ttf")},
                             2);
    InitializeFontUrlsOtherBucket(data, 0);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["example.test"]);
    EXPECT_DOUBLE_EQ(4, SumOfFontUrlFrequency(data));
  }
  for (int i = 0; i < 3; ++i) {
    LearnFontUrls(GURL("http://example.test"),
                  {
                      GURL("https://example.org/test.otf"),
                      GURL("https://example.net/test.svg"),
                  });
  }
  {
    LcppData data = CreateLcppData("example.test", 10);
    InitializeFontUrlsBucket(data, {GURL("https://example.org/test.otf")}, 0.8);
    InitializeFontUrlsBucket(data, {GURL("https://example.net/test.svg")}, 1);
    InitializeFontUrlsOtherBucket(data, 3.2);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["example.test"]);
    EXPECT_DOUBLE_EQ(5, SumOfFontUrlFrequency(data));
  }
}

TEST_F(LcppDataMapTest, LearnSubresourceUrls) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  EXPECT_EQ(5U, config.lcpp_histogram_sliding_window_size);
  EXPECT_EQ(2U, config.max_lcpp_histogram_buckets);
  InitializeDB(config);
  EXPECT_TRUE(mock_tables_->lcpp_table_.data_.empty());

  auto SumOfFontUrlFrequency = [this](const LcppData& data) {
    return SumOfLcppStringFrequencyStatData(
        data.lcpp_stat().fetched_subresource_url_stat());
  };
  for (int i = 0; i < 2; ++i) {
    LearnSubresourceUrls(GURL("http://example.test"),
                         {
                             {GURL("https://a.test/a.jpeg"), base::Seconds(1)},
                             {GURL("https://b.test/b.jpeg"), base::Seconds(2)},
                         });
  }
  {
    LcppData data = CreateLcppData("example.test", 10);
    InitializeSubresourceUrlsBucket(
        data, {GURL("https://a.test/a.jpeg"), GURL("https://b.test/b.jpeg")},
        2);
    InitializeSubresourceUrlsOtherBucket(data, 0);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["example.test"]);
    EXPECT_DOUBLE_EQ(4, SumOfFontUrlFrequency(data));
  }
  for (int i = 0; i < 3; ++i) {
    LearnSubresourceUrls(GURL("http://example.test"),
                         {
                             {GURL("https://c.test/a.jpeg"), base::Seconds(1)},
                             {GURL("https://d.test/b.jpeg"), base::Seconds(2)},
                         });
  }
  {
    LcppData data = CreateLcppData("example.test", 10);
    InitializeSubresourceUrlsBucket(data, {GURL("https://c.test/a.jpeg")}, 1);
    InitializeSubresourceUrlsBucket(data, {GURL("https://d.test/b.jpeg")}, 0.8);
    InitializeSubresourceUrlsOtherBucket(data, 3.2);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["example.test"]);
    EXPECT_DOUBLE_EQ(5, SumOfFontUrlFrequency(data));
  }
}

TEST_F(LcppDataMapTest, WhenLcppDataIsCorrupted_ResetData) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  InitializeDB(config);
  EXPECT_TRUE(mock_tables_->lcpp_table_.data_.empty());

  // Prepare a corrupted data.
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 1.92);
    InitializeLcpElementLocatorBucket(data, "/#b", 1.28);
    InitializeLcpElementLocatorBucket(data, "/#c", -1);
    InitializeLcpElementLocatorOtherBucket(data, -1);
    mock_tables_->lcpp_table_.data_["a.test"] = data;
  }

  // Confirm that new learning process reset the corrupted data.
  LearnElementLocator(GURL("http://a.test"), "/#a", {});
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 1);
    EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
  }
}

TEST_F(LcppDataMapTest, LcppMaxHosts) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 3u;
  InitializeDB(config);
  EXPECT_TRUE(mock_tables_->lcpp_table_.data_.empty());

  const GURL url_a("http://a.test");
  EXPECT_FALSE(GetLcppStat(url_a));

  LearnElementLocator(url_a, "/#a");
  EXPECT_TRUE(GetLcppStat(url_a));

  const GURL url_b("http://b.test");
  LearnElementLocator(url_b, "/#a");
  const GURL url_c("http://c.test");
  LearnElementLocator(url_c, "/#a");
  EXPECT_TRUE(GetLcppStat(url_a));
  EXPECT_TRUE(GetLcppStat(url_b));
  EXPECT_TRUE(GetLcppStat(url_c));

  const GURL url_d("http://d.test");
  LearnElementLocator(url_d, "/#a");
  EXPECT_TRUE(GetLcppStat(url_d));
  // Confirm first host is dropped.
  EXPECT_FALSE(GetLcppStat(url_a));
}

TEST_F(LcppDataMapTest, LcppLearnURL) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  InitializeDB(config);

  const std::vector<std::pair<std::string, std::string>> url_keys = {
      {"http://a.test", "a.test"},
      {"http://a.test/", "a.test"},
      {"http://a.test/foo", "a.test"},
      {"http://a.test/bar?q=c", "a.test"},
      {"http://user:pass@a.test:99/foo;bar?q=a#ref", "a.test"},
  };

  TestLearnLcppURL(url_keys);
}

TEST_F(LcppDataMapTest, DeleteUrls) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 10u;
  InitializeDB(config);

  const GURL url_a("http://a.test");
  const GURL url_b("http://b.test");
  const GURL url_c("http://c.test");

  LearnElementLocator(url_a, "/#a");
  LearnElementLocator(url_b, "/#a");
  LearnElementLocator(url_c, "/#a");
  EXPECT_TRUE(GetLcppStat(url_a));
  EXPECT_TRUE(GetLcppStat(url_b));
  EXPECT_TRUE(GetLcppStat(url_c));

  lcpp_data_map_->DeleteUrls({url_a, url_b});
  EXPECT_FALSE(GetLcppStat(url_a));
  EXPECT_FALSE(GetLcppStat(url_b));
  EXPECT_TRUE(GetLcppStat(url_c));
}

class LcppMultipleKeyTest : public LcppDataMapTest,
                            public testing::WithParamInterface<
                                blink::features::LcppMultipleKeyTypes> {
 public:
  void SetUp() override {
    auto& kLcppMultipleKeyType = blink::features::kLcppMultipleKeyType;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kLCPPMultipleKey,
        {{kLcppMultipleKeyType.name,
          kLcppMultipleKeyType.GetName(GetParam())}});

    LoadingPredictorConfig config;
    PopulateTestConfig(&config);
    config.max_hosts_to_track_for_lcpp = 100u;
    config.lcpp_histogram_sliding_window_size = 10u;
    config.lcpp_multiple_key_histogram_sliding_window_size = 100u;
    config.lcpp_multiple_key_max_histogram_buckets = 100u;
    InitializeDB(config);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    LcppMultipleKeyTypes,
    LcppMultipleKeyTest,
    testing::Values(blink::features::LcppMultipleKeyTypes::kDefault,
                    blink::features::LcppMultipleKeyTypes::kLcppKeyStat));

TEST_P(LcppMultipleKeyTest, LearnURL) {
  const std::string long_host =
      std::string(ResourcePrefetchPredictorTables::kMaxStringLength - 10, 'a') +
      ".test";
  const size_t max_path_length = base::checked_cast<size_t>(
      blink::features::kLCPPMultipleKeyMaxPathLength.Get());
  const std::string long_path = "/" + std::string(max_path_length - 1, 'b');
  const std::string too_long_path =
      "/" + std::string(max_path_length + 1, 'c') + "/bar";
  const std::vector<std::pair<std::string, std::string>> url_keys = {
      {"http://a.test", "a.test"},
      {"http://user:pass@a.test:99/foo;bar?q=a#ref", "a.test/foo;bar"},
      {"http://a.test/", "a.test"},
      {"http://a.test/foo.html", "a.test"},
      {"http://a.test/foo", "a.test/foo"},
      {"http://a.test/foo/", "a.test/foo"},
      {"http://a.test/foo/bar", "a.test/foo"},
      {"http://a.test/foo/bar/", "a.test/foo"},
      {"http://a.test/foo/bar/baz.com", "a.test/foo"},
      {"http://a.test/bar?q=c", "a.test/bar"},
      {"http://a.test/foo/bar?q=c", "a.test/foo"},
      {"http://a.test" + long_path, "a.test" + long_path},
      {"http://a.test" + long_path + "/bar", "a.test" + long_path},
      {"http://a.test" + long_path + "bar", "a.test"},
      {"http://" + long_host + "/bar", long_host + "/bar"},
      // Both valid but if the concated key is too long, take only host.
      {"http://" + long_host + long_path, long_host},
      // Too long path is ignored.
      {"http://a.test" + too_long_path, "a.test"},
      // Invalid length path in subdirectory is also ignored.
      {"http://a.test/bar" + too_long_path, "a.test/bar"}};

  TestLearnLcppURL(url_keys);
}

TEST_P(LcppMultipleKeyTest, ShouldNotLearnTooLongLocators) {
  const GURL url("http://a.test/foo1");
  LearnElementLocator(url, "/#lcp");
  const LcppStat expected = MakeLcppStatWithLCPElementLocator("/#lcp");
  EXPECT_EQ(*GetLcppStat(url), expected);

  LearnElementLocator(
      url, "/#" + std::string(ResourcePrefetchPredictorTables::kMaxStringLength,
                              'a'));
  EXPECT_EQ(*GetLcppStat(url), expected);
}

TEST_P(LcppMultipleKeyTest, DeleteUrls) {
  const bool kIsDefault =
      GetParam() == blink::features::LcppMultipleKeyTypes::kDefault;
  const GURL url_a_1("http://a.test");
  const GURL url_a_2("http://a.test/foo");
  const GURL url_a_3("http://a.test/bar");
  const GURL url_b("http://b.test/baz");
  const GURL url_c("http://c.test");

  const std::vector<GURL> urls = {url_a_1, url_a_2, url_a_3, url_b, url_c};
  for (const GURL& url : urls) {
    LearnElementLocator(url, "/#a");
  }
  for (const GURL& url : urls) {
    EXPECT_TRUE(GetLcppStat(url));
  }

  lcpp_data_map_->DeleteUrls({url_a_2, url_b});
  // In kDefault, only exact match entry is removed.
  // In kLcppKeyStat, all entries having same host are removed.
  EXPECT_EQ(!!GetLcppStat(url_a_1), kIsDefault);
  EXPECT_FALSE(GetLcppStat(url_a_2));
  EXPECT_EQ(!!GetLcppStat(url_a_3), kIsDefault);
  EXPECT_FALSE(GetLcppStat(url_b));
  EXPECT_TRUE(GetLcppStat(url_c));
}

class LcppMultipleKeyTestDefault : public LcppDataMapTest {
 public:
  void SetUp() override {
    auto& kLcppMultipleKeyType = blink::features::kLcppMultipleKeyType;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kLCPPMultipleKey,
        {{kLcppMultipleKeyType.name,
          kLcppMultipleKeyType.GetName(
              blink::features::LcppMultipleKeyTypes::kDefault)}});
    LcppDataMapTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LcppMultipleKeyTestDefault, MaxHosts) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 2u;
  InitializeDB(config);

  const std::string host = "http://a.test";
  const GURL url_1(host + "/foo1");
  EXPECT_FALSE(GetLcppStat(url_1));

  LearnElementLocator(url_1, "/#lcp1");
  EXPECT_EQ(*GetLcppStat(url_1), MakeLcppStatWithLCPElementLocator("/#lcp1"));

  const GURL url_2(host + "/foo2");
  LearnElementLocator(url_2, "/#lcp2");
  EXPECT_EQ(*GetLcppStat(url_1), MakeLcppStatWithLCPElementLocator("/#lcp1"));
  EXPECT_EQ(*GetLcppStat(url_2), MakeLcppStatWithLCPElementLocator("/#lcp2"));

  const GURL url_3(host + "/foo3");
  LearnElementLocator(url_3, "/#lcp3");
  EXPECT_EQ(*GetLcppStat(url_3), MakeLcppStatWithLCPElementLocator("/#lcp3"));
  // Confirm the first url over `max_hosts_to_track_for_lcpp` is dropped.
  EXPECT_FALSE(GetLcppStat(url_1));
}

class ScopedLcppKeyStatFeature {
 public:
  ScopedLcppKeyStatFeature() {
    auto& kLcppMultipleKeyType = blink::features::kLcppMultipleKeyType;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kLCPPMultipleKey,
        {{kLcppMultipleKeyType.name,
          kLcppMultipleKeyType.GetName(
              blink::features::LcppMultipleKeyTypes::kLcppKeyStat)}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LcppMultipleKeyTestKeyStat : public LcppDataMapTest {
 public:
  void SetUp() override {
    scoped_lcpp_key_stat_feature_ =
        std::make_unique<ScopedLcppKeyStatFeature>();
    LcppDataMapTest::SetUp();
  }

 private:
  std::unique_ptr<ScopedLcppKeyStatFeature> scoped_lcpp_key_stat_feature_;
};

TEST_F(LcppMultipleKeyTestKeyStat, MaxHostsAndKeys) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 2u;
  config.lcpp_multiple_key_histogram_sliding_window_size = 4u;
  config.lcpp_multiple_key_max_histogram_buckets = 3u;
  InitializeDB(config);

  const std::string host = "http://a.test";
  const GURL url_base(host);
  const GURL url_1(host + "/foo1");
  EXPECT_FALSE(GetLcppStat(url_base));
  EXPECT_FALSE(GetLcppStat(url_1));

  LearnElementLocator(url_base, "/#base");
  EXPECT_EQ(*GetLcppStat(url_base),
            MakeLcppStatWithLCPElementLocator("/#base"));
  LearnElementLocator(url_1, "/#lcp1");
  EXPECT_EQ(*GetLcppStat(url_1), MakeLcppStatWithLCPElementLocator("/#lcp1"));

  const GURL url_2(host + "/foo2");
  LearnElementLocator(url_2, "/#lcp2");
  EXPECT_EQ(*GetLcppStat(url_1), MakeLcppStatWithLCPElementLocator("/#lcp1"));
  EXPECT_EQ(*GetLcppStat(url_2), MakeLcppStatWithLCPElementLocator("/#lcp2"));

  const GURL url_3(host + "/foo3");
  LearnElementLocator(url_3, "/#lcp3");
  EXPECT_EQ(*GetLcppStat(url_3), MakeLcppStatWithLCPElementLocator("/#lcp3"));
  // Confirm the first url over `max_hosts_to_track_for_lcpp` is NOT dropped.
  EXPECT_EQ(*GetLcppStat(url_1), MakeLcppStatWithLCPElementLocator("/#lcp1"));

  const GURL url_4(host + "/foo4");
  LearnElementLocator(url_4, "/#lcp4");
  EXPECT_EQ(*GetLcppStat(url_4), MakeLcppStatWithLCPElementLocator("/#lcp4"));
  // Confirm the first url over `lcpp_multiple_key_max_histogram_buckets` is
  // dropped.
  EXPECT_FALSE(GetLcppStat(url_1));
  // Confirm host-only url still be alive.
  EXPECT_EQ(*GetLcppStat(url_base),
            MakeLcppStatWithLCPElementLocator("/#base"));

  // Confirm adding other host urls over `max_hosts_to_track_for_lcpp` lets all
  // the first url entries be dropped.
  const GURL url_b("http://b.test");
  LearnElementLocator(url_b, "/#b");
  const GURL url_c("http://c.test");
  LearnElementLocator(url_c, "/#c");
  EXPECT_EQ(*GetLcppStat(url_b), MakeLcppStatWithLCPElementLocator("/#b"));
  EXPECT_EQ(*GetLcppStat(url_c), MakeLcppStatWithLCPElementLocator("/#c"));
  EXPECT_FALSE(GetLcppStat(url_base));
  EXPECT_FALSE(GetLcppStat(url_1));
  EXPECT_FALSE(GetLcppStat(url_2));
  EXPECT_FALSE(GetLcppStat(url_3));
  EXPECT_FALSE(GetLcppStat(url_4));
}

TEST_F(LcppDataMapTest, LcppStatShouldBeClearedOverFlagReset) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  InitializeDB(config);

  const std::string host = "http://a.test";
  const GURL url_base(host);
  const GURL url_1(host + "/foo1");

  {
    ScopedLcppKeyStatFeature feature;

    LearnElementLocator(url_base, "/#base");
    EXPECT_EQ(*GetLcppStat(url_base),
              MakeLcppStatWithLCPElementLocator("/#base"));
    LearnElementLocator(url_1, "/#lcp1");
    EXPECT_EQ(*GetLcppStat(url_1), MakeLcppStatWithLCPElementLocator("/#lcp1"));
  }

  LearnElementLocator(url_base, "/#base");
  LcppData data = CreateLcppData("a.test", 10);
  InitializeLcpElementLocatorBucket(data, "/#base", 2);
  EXPECT_EQ(data, mock_tables_->lcpp_table_.data_["a.test"]);
}

TEST_F(LcppMultipleKeyTestKeyStat, AddNewEntryToFullBucketKeyStat) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 2u;
  config.lcpp_multiple_key_histogram_sliding_window_size = 4u;
  config.lcpp_multiple_key_max_histogram_buckets = 2u;
  InitializeDB(config);

  const std::string host = "http://a.test";
  const GURL url_1(host + "/foo1");
  const GURL url_2(host + "/foo2");
  const GURL url_3(host + "/foo3");
  LearnElementLocator(url_1, "/#lcp1");
  LearnElementLocator(url_1, "/#lcp1");
  LearnElementLocator(url_2, "/#lcp2");
  LearnElementLocator(url_2, "/#lcp2");
  EXPECT_EQ(*GetLcppStat(url_1),
            MakeLcppStatWithLCPElementLocator("/#lcp1", 2));
  EXPECT_EQ(*GetLcppStat(url_2),
            MakeLcppStatWithLCPElementLocator("/#lcp2", 2));

  LearnElementLocator(url_3, "/#lcp3");
  EXPECT_FALSE(GetLcppStat(url_3));

  LearnElementLocator(url_3, "/#lcp3");
  EXPECT_FALSE(GetLcppStat(url_3));

  LearnElementLocator(url_3, "/#lcp3");
  EXPECT_EQ(*GetLcppStat(url_3),
            MakeLcppStatWithLCPElementLocator("/#lcp3", 1));
  EXPECT_FALSE(GetLcppStat(url_1));
  EXPECT_EQ(*GetLcppStat(url_2),
            MakeLcppStatWithLCPElementLocator("/#lcp2", 2));
}

}  // namespace predictors
