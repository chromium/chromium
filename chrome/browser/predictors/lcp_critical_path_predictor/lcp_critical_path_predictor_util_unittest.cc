// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_test_util.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
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

template <typename T>
bool IsCanonicalizedFrequencyData(
    size_t max_histogram_buckets,
    const LcppStringFrequencyStatData& frequency_stat,
    const google::protobuf::Map<std::string, T>& map) {
  const auto& frequency_main_buckets = frequency_stat.main_buckets();
  std::vector<std::string> remove_from_map;
  for (const auto& it : map) {
    if (auto pos = frequency_main_buckets.find(it.first);
        pos == frequency_main_buckets.end()) {
      return false;
    }
  }

  for (const auto& it : frequency_main_buckets) {
    if (auto pos = map.find(it.first); pos == map.end()) {
      return false;
    }
  }
  CHECK_EQ(frequency_main_buckets.size(), map.size());

  if (frequency_stat.other_bucket_frequency() < 0 ||
      frequency_stat.main_buckets().size() > max_histogram_buckets) {
    return false;
  }
  return true;
}

template <typename T>
class MapUpdater {
 public:
  MapUpdater(size_t sliding_window_size, size_t max_histogram_buckets)
      : sliding_window_size_(sliding_window_size),
        max_histogram_buckets_(max_histogram_buckets) {}
  ~MapUpdater() = default;

  T* UpdateAndTryGetEntry(const std::string& new_entry) {
    return UpdateFrequencyStatAndTryGetEntry(sliding_window_size_,
                                             max_histogram_buckets_, new_entry,
                                             stat_data_, map_);
  }

  void InitializeWith(const LcppStringFrequencyStatData& data) {
    for (auto& [key, freq] : data.main_buckets()) {
      for (size_t i = 0; i < freq; i++) {
        UpdateAndTryGetEntry(key);
      }
    }
    stat_data_ = data;
  }

  bool CanonicalizeFrequencyData() {
    return ::predictors::CanonicalizeFrequencyData(max_histogram_buckets_,
                                                   stat_data_, map_);
  }

  bool IsCanonicalFrequencyData() {
    return IsCanonicalizedFrequencyData(max_histogram_buckets_, stat_data_,
                                        map_);
  }

  const LcppStringFrequencyStatData& Data() { return stat_data_; }

  google::protobuf::Map<std::string, T> map_;
  LcppStringFrequencyStatData stat_data_;

 private:
  const size_t sliding_window_size_;
  const size_t max_histogram_buckets_;
};

template <typename T>
std::string ToStr(const google::protobuf::Map<std::string, T>& map) {
  std::ostringstream os;
  os << "[";
  size_t i = 0;
  for (const auto& it : map) {
    os << "{" << it.first << "," << it.second << "}";
    if (i++ != map.size() - 1) {
      os << ", ";
    }
  }
  os << "]";
  return os.str();
}

LcppStringFrequencyStatData MakeData(std::map<std::string, double> main_buckets,
                                     double others) {
  LcppStringFrequencyStatData data;
  for (auto& [key, freq] : main_buckets) {
    data.mutable_main_buckets()->insert({key, freq});
  }
  data.set_other_bucket_frequency(others);
  return data;
}

void InitializeSubresourceUrlDestinationsBucket(
    LcppData& lcpp_data,
    const std::vector<std::pair<std::string, int32_t>>& urls) {
  for (const auto& url : urls) {
    lcpp_data.mutable_lcpp_stat()
        ->mutable_fetched_subresource_url_destination()
        ->insert({url.first, url.second});
  }
}

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

  // Increase existing frequency when the bucket is full.
  updater.Update("foo", dropped_entry);
  EXPECT_FALSE(dropped_entry);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 2.92}, {"foobar", 0.8}}, 1.28))
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

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(UpdateFrequencyStatAndTryGetEntryTest, Base) {
  MapUpdater<int> updater(/*sliding_window_size=*/5u,
                          /*max_histogram_buckets=*/2u);
  EXPECT_EQ(updater.Data(), MakeData({}, 0)) << updater.Data();
  EXPECT_THAT(updater.map_, IsEmpty());

  EXPECT_EQ(*updater.UpdateAndTryGetEntry("foo"), 0);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1}}, 0)) << updater.Data();
  EXPECT_THAT(updater.map_, UnorderedElementsAre(Pair("foo", 0)));

  updater.map_["foo"] = 42;
  EXPECT_EQ(*updater.UpdateAndTryGetEntry("bar"), 0);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1}, {"bar", 1}}, 0))
      << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 42), Pair("bar", 0)));

  EXPECT_EQ(*updater.UpdateAndTryGetEntry("foo"), 42);
  EXPECT_EQ(*updater.UpdateAndTryGetEntry("foo"), 42);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 3}, {"bar", 1}}, 0))
      << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 42), Pair("bar", 0)));

  EXPECT_EQ(*updater.UpdateAndTryGetEntry("qux"), 0);
  // If kinds of entry are over 'max_histogram_buckets', the oldest bucket is
  // converted to 'other_bucket_frequency'.
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 3}, {"qux", 1}}, 1))
      << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 42), Pair("qux", 0)));

  EXPECT_EQ(*updater.UpdateAndTryGetEntry("foobar"), 0);
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
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 42), Pair("foobar", 0)));
}

TEST(UpdateFrequencyStatAndTryGetEntryTest, AddNewEntryToFullBuckets) {
  MapUpdater<int> updater(/*sliding_window_size=*/4u,
                          /*max_histogram_buckets=*/2u);

  EXPECT_EQ(*updater.UpdateAndTryGetEntry("foo"), 0);
  updater.map_["foo"] = 42;
  EXPECT_EQ(*updater.UpdateAndTryGetEntry("foo"), 42);
  EXPECT_EQ(*updater.UpdateAndTryGetEntry("bar"), 0);
  updater.map_["bar"] = 3;
  EXPECT_EQ(*updater.UpdateAndTryGetEntry("bar"), 3);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 2}, {"bar", 2}}, 0))
      << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 42), Pair("bar", 3)));

  // "qux" is not inserted while others frequency are enough high.
  EXPECT_FALSE(updater.UpdateAndTryGetEntry("qux"));
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1.5}, {"bar", 1.5}}, 1))
      << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 42), Pair("bar", 3)));

  EXPECT_FALSE(updater.UpdateAndTryGetEntry("qux"));
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 1.125}, {"bar", 1.125}}, 1.75))
      << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 42), Pair("bar", 3)));

  // "qux" is finally inserted by dropping "bar".
  EXPECT_EQ(*updater.UpdateAndTryGetEntry("qux"), 0);
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 0.84375}, {"qux", 1}}, 2.15625))
      << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 42), Pair("qux", 0)));
}

TEST(LcppCanonicalizeFrequencyDataTest, BadFrequency) {
  MapUpdater<int> updater(/*sliding_window_size=*/5u,
                          /*max_histogram_buckets=*/2u);
  const LcppStringFrequencyStatData data =
      MakeData({{"foo", 2}, {"bar", 1}}, 1.3);
  updater.InitializeWith(data);
  EXPECT_EQ(updater.Data(), data) << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 0), Pair("bar", 0)))
      << ToStr(updater.map_);
  EXPECT_TRUE(updater.IsCanonicalFrequencyData());

  updater.stat_data_.set_other_bucket_frequency(-1.0);
  EXPECT_FALSE(updater.IsCanonicalFrequencyData());

  EXPECT_TRUE(updater.CanonicalizeFrequencyData());
  EXPECT_TRUE(updater.IsCanonicalFrequencyData());
  EXPECT_EQ(updater.Data(), MakeData({}, 0)) << updater.Data();
  EXPECT_THAT(updater.map_, IsEmpty());
}

TEST(LcppCanonicalizeFrequencyDataTest, LessFrequencyData) {
  MapUpdater<int> updater(/*sliding_window_size=*/5u,
                          /*max_histogram_buckets=*/2u);
  const LcppStringFrequencyStatData data =
      MakeData({{"foo", 2}, {"bar", 1}}, 1.3);
  updater.InitializeWith(data);
  EXPECT_EQ(updater.Data(), data) << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 0), Pair("bar", 0)))
      << ToStr(updater.map_);
  EXPECT_TRUE(updater.IsCanonicalFrequencyData());

  updater.stat_data_.mutable_main_buckets()->erase("bar");
  EXPECT_FALSE(updater.IsCanonicalFrequencyData());

  EXPECT_TRUE(updater.CanonicalizeFrequencyData());
  EXPECT_TRUE(updater.IsCanonicalFrequencyData());
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 2}}, 1.3)) << updater.Data();
  EXPECT_THAT(updater.map_, UnorderedElementsAre(Pair("foo", 0)))
      << ToStr(updater.map_);
}

TEST(LcppCanonicalizeFrequencyDataTest, LessMap) {
  MapUpdater<int> updater(/*sliding_window_size=*/5u,
                          /*max_histogram_buckets=*/2u);
  const LcppStringFrequencyStatData data =
      MakeData({{"foo", 2}, {"bar", 1}}, 1.3);
  updater.InitializeWith(data);
  EXPECT_EQ(updater.Data(), data) << updater.Data();
  EXPECT_THAT(updater.map_,
              UnorderedElementsAre(Pair("foo", 0), Pair("bar", 0)))
      << ToStr(updater.map_);
  EXPECT_TRUE(updater.IsCanonicalFrequencyData());

  updater.map_.erase("bar");
  EXPECT_FALSE(updater.IsCanonicalFrequencyData());

  EXPECT_TRUE(updater.CanonicalizeFrequencyData());
  EXPECT_TRUE(updater.IsCanonicalFrequencyData());
  EXPECT_EQ(updater.Data(), MakeData({{"foo", 2}}, 1.3)) << updater.Data();
  EXPECT_THAT(updater.map_, UnorderedElementsAre(Pair("foo", 0)))
      << ToStr(updater.map_);
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
    scoped_refptr<base::SequencedTaskRunner> db_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    predictor_database_ =
        std::make_unique<PredictorDatabase>(&profile_, db_task_runner);
    lcpp_data_map_ = std::make_unique<LcppDataMap>(
        predictor_database_->resource_prefetch_tables(), config);
    db_task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&LcppDataMap::InitializeOnDBSequence,
                       base::Unretained(lcpp_data_map_.get())),
        base::BindOnce(&LcppDataMap::InitializeAfterDBInitialization,
                       base::Unretained(lcpp_data_map_.get())));
    content::RunAllTasksUntilIdle();
  }

  void TearDownDB() {
    content::RunAllTasksUntilIdle();
    lcpp_data_map_.reset();
    predictor_database_.reset();
    content::RunAllTasksUntilIdle();
  }

 protected:
  void TearDown() override {
    if (lcpp_data_map_) {
      lcpp_data_map_->DeleteAllData();
    }
  }

  const std::map<std::string, LcppData>& GetDataMap() {
    return lcpp_data_map_->GetAllCachedForTesting();
  }

  const std::map<std::string, LcppOrigin>& GetOriginMap() {
    return lcpp_data_map_->GetAllCachedOriginForTesting();
  }

  void UpdateKeyValueDataDirectly(const std::string& key,
                                  const LcppData& data) {
    lcpp_data_map_->data_map_->UpdateData(key, data);
  }

  double SumOfLcppStringFrequencyStatData(
      const LcppStringFrequencyStatData& data) {
    double sum = data.other_bucket_frequency();
    for (const auto& [url, frequency] : data.main_buckets()) {
      sum += frequency;
    }
    return sum;
  }

  bool LearnLcpp(const std::optional<url::Origin>& initiator_origin,
                 const GURL& url,
                 const LcppDataInputs& inputs) {
    return lcpp_data_map_->LearnLcpp(initiator_origin, url, inputs);
  }
  bool LearnLcpp(const GURL& url, const LcppDataInputs& inputs) {
    return lcpp_data_map_->LearnLcpp(/*initiator_origin=*/std::nullopt, url,
                                     inputs);
  }

  bool LearnElementLocator(
      const std::optional<url::Origin>& initiator_origin,
      const GURL& url,
      const std::string& lcp_element_locator,
      const std::vector<GURL>& lcp_influencer_scripts = {}) {
    predictors::LcppDataInputs inputs;
    inputs.lcp_element_locator = lcp_element_locator;
    inputs.lcp_influencer_scripts = lcp_influencer_scripts;
    return LearnLcpp(initiator_origin, url, inputs);
  }

  void LearnElementLocator(
      const GURL& url,
      const std::string& lcp_element_locator,
      const std::vector<GURL>& lcp_influencer_scripts = {}) {
    LearnElementLocator(/*initiator_origin=*/std::nullopt, url,
                        lcp_element_locator, lcp_influencer_scripts);
  }

  void LearnFontUrls(const GURL& url, const std::vector<GURL>& font_urls) {
    LcppDataInputs inputs;
    inputs.font_urls = font_urls;
    LearnLcpp(url, inputs);
  }

  void LearnSubresourceUrls(
      const GURL& url,
      const std::map<
          GURL,
          std::pair<base::TimeDelta, network::mojom::RequestDestination>>&
          subresource_urls) {
    LcppDataInputs inputs;
    inputs.subresource_urls = subresource_urls;
    LearnLcpp(url, inputs);
  }

  std::optional<LcppStat> GetLcppStat(
      const std::optional<url::Origin>& initiator_origin,
      const GURL& url) {
    return lcpp_data_map_->GetLcppStat(initiator_origin, url);
  }
  std::optional<LcppStat> GetLcppStat(const GURL& url) {
    return GetLcppStat(/*initiator_origin=*/std::nullopt, url);
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
      auto stat = lcpp_data_map_->GetLcppStat(/*initiator_origin=*/std::nullopt,
                                              GURL("http://" + key));
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

  static url::Origin CreateOrigin(const std::string& host_name) {
    const url::Origin origin = url::Origin::Create(GURL("http://" + host_name));
    CHECK_EQ(origin.host(), host_name);
    return origin;
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  std::unique_ptr<PredictorDatabase> predictor_database_;

  LoadingPredictorConfig config_;
  std::unique_ptr<LcppDataMap> lcpp_data_map_;
};

class LcppDataMapFeatures
    : public LcppDataMapTest,
      public testing::WithParamInterface<std::vector<base::test::FeatureRef>> {
 public:
  LcppDataMapFeatures() {
    scoped_feature_list_.InitWithFeatures(GetParam(),
                                          /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

auto& kLCPPInitiatorOrigin = blink::features::kLCPPInitiatorOrigin;
auto& kLCPPMultipleKey = blink::features::kLCPPMultipleKey;
const std::vector<base::test::FeatureRef> featureset1[] = {
    {},
    {kLCPPInitiatorOrigin},
    {kLCPPMultipleKey},
    {kLCPPInitiatorOrigin, kLCPPMultipleKey}};
inline std::string CustomParamNameFunction(
    const testing::TestParamInfo<LcppDataMapFeatures::ParamType>& info) {
  const auto& features = info.param;
  if (features.empty()) {
    return std::string("Default");
  }
  std::string name;
  for (size_t i = 0; i < features.size(); i++) {
    if (features[i] == kLCPPInitiatorOrigin) {
      name += "InitiatorOrigin";
    } else {
      name += "MultipleKey";
    }
    if (i < features.size() - 1) {
      name += "_";
    }
  }
  return name;
}
INSTANTIATE_TEST_SUITE_P(LcppFeatureSet1,
                         LcppDataMapFeatures,
                         testing::ValuesIn(featureset1),
                         &CustomParamNameFunction);

TEST_P(LcppDataMapFeatures, Base) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  InitializeDB(config);
  EXPECT_TRUE(GetDataMap().empty());

  predictors::LcppDataInputs inputs;
  inputs.lcp_element_locator = "/#foo";
  GURL url = GURL("https://a.test");
  lcpp_data_map_->LearnLcpp(/*initiator_origin=*/std::nullopt, url, inputs);

  auto stat =
      lcpp_data_map_->GetLcppStat(/*initiator_origin=*/std::nullopt, url);
  EXPECT_TRUE(stat);
  LcppData expected;
  InitializeLcpElementLocatorBucket(expected, "/#foo", 1);
  EXPECT_EQ(expected.lcpp_stat(), *stat) << *stat;
}

TEST_P(LcppDataMapFeatures, LearnLcpp) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  EXPECT_EQ(5U, config.lcpp_histogram_sliding_window_size);
  EXPECT_EQ(2U, config.max_lcpp_histogram_buckets);
  InitializeDB(config);
  EXPECT_TRUE(GetDataMap().empty());

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
    EXPECT_EQ(data, GetDataMap().at("a.test"));
    EXPECT_DOUBLE_EQ(3, SumOfElementLocatorFrequency(data));
  }

  for (int i = 0; i < 2; ++i) {
    LearnElementLocator(GURL("http://a.test"), "/#b", {});
  }
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 3);
    InitializeLcpElementLocatorBucket(data, "/#b", 2);
    EXPECT_EQ(data, GetDataMap().at("a.test"));
    EXPECT_DOUBLE_EQ(5, SumOfElementLocatorFrequency(data));
  }

  LearnElementLocator(GURL("http://a.test"), "/#c", {});
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 2.4);
    InitializeLcpElementLocatorBucket(data, "/#b", 1.6);
    InitializeLcpElementLocatorOtherBucket(data, 1);
    EXPECT_EQ(data, GetDataMap().at("a.test"));
    EXPECT_DOUBLE_EQ(5, SumOfElementLocatorFrequency(data));
  }

  LearnElementLocator(GURL("http://a.test"), "/#d", {});
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 1.92);
    InitializeLcpElementLocatorBucket(data, "/#b", 1.28);
    InitializeLcpElementLocatorOtherBucket(data, 1.8);
    EXPECT_EQ(data, GetDataMap().at("a.test"));
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
    EXPECT_EQ(data, GetDataMap().at("a.test"));
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
    EXPECT_EQ(data, GetDataMap().at("a.test"));
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
    EXPECT_EQ(data, GetDataMap().at("a.test"));
    EXPECT_DOUBLE_EQ(5, SumOfInfluencerUrlFrequency(data));
  }
}

TEST_P(LcppDataMapFeatures, LearnFontUrls) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  EXPECT_EQ(5U, config.lcpp_histogram_sliding_window_size);
  EXPECT_EQ(2U, config.max_lcpp_histogram_buckets);
  InitializeDB(config);
  EXPECT_TRUE(GetDataMap().empty());

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
    EXPECT_EQ(data, GetDataMap().at("example.test"));
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
    EXPECT_EQ(data, GetDataMap().at("example.test"));
    EXPECT_DOUBLE_EQ(5, SumOfFontUrlFrequency(data));
  }
}

TEST_P(LcppDataMapFeatures, LearnSubresourceUrls) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  EXPECT_EQ(5U, config.lcpp_histogram_sliding_window_size);
  EXPECT_EQ(2U, config.max_lcpp_histogram_buckets);
  InitializeDB(config);
  EXPECT_TRUE(GetDataMap().empty());
  const network::mojom::RequestDestination kEmpty =
      network::mojom::RequestDestination::kEmpty;
  const int32_t kEmptyValue = static_cast<int32_t>(kEmpty);
  const network::mojom::RequestDestination kImage =
      network::mojom::RequestDestination::kImage;
  const int32_t kImageValue = static_cast<int32_t>(kImage);
  const std::string kUrl = "example.test";
  const GURL kGURL = GURL("http://" + kUrl);
  const std::string kJpegA = "https://" + kUrl + "/a.jpeg";
  const std::string kJpegB = "https://" + kUrl + "/b.jpeg";

  auto SumOfFontUrlFrequency = [this](const LcppData& data) {
    return SumOfLcppStringFrequencyStatData(
        data.lcpp_stat().fetched_subresource_url_stat());
  };
  for (int i = 0; i < 2; ++i) {
    LearnSubresourceUrls(
        kGURL, {
                   {GURL(kJpegA), std::make_pair(base::Seconds(1), kEmpty)},
                   {GURL(kJpegB), std::make_pair(base::Seconds(2), kImage)},
               });
  }
  {
    LcppData expected = CreateLcppData(kUrl, 10);
    InitializeSubresourceUrlsBucket(expected, {GURL(kJpegA), GURL(kJpegB)}, 2);
    InitializeSubresourceUrlsOtherBucket(expected, 0);
    InitializeSubresourceUrlDestinationsBucket(
        expected, {std::make_pair(kJpegA, kEmptyValue),
                   std::make_pair(kJpegB, kImageValue)});
    const LcppData& result = GetDataMap().at(kUrl);
    EXPECT_EQ(expected, result) << expected << result;
    EXPECT_DOUBLE_EQ(4, SumOfFontUrlFrequency(result));
  }

  const std::string kJpegC = "https://" + kUrl + "/c.jpeg";
  const std::string kJpegD = "https://" + kUrl + "/d.jpeg";

  LearnSubresourceUrls(
      kGURL, {
                 {GURL(kJpegC), std::make_pair(base::Seconds(1), kEmpty)},
                 {GURL(kJpegD), std::make_pair(base::Seconds(2), kImage)},
             });
  {
    // New URLs are not recorded due to less frequency.
    LcppData expected = CreateLcppData(kUrl, 10);
    InitializeSubresourceUrlsBucket(expected, {GURL(kJpegA), GURL(kJpegB)},
                                    1.6);
    InitializeSubresourceUrlsOtherBucket(expected, 1.8);
    InitializeSubresourceUrlDestinationsBucket(
        expected, {std::make_pair(kJpegA, kEmptyValue),
                   std::make_pair(kJpegB, kImageValue)});
    const LcppData& result = GetDataMap().at(kUrl);
    EXPECT_EQ(expected, result) << expected << result;
    EXPECT_DOUBLE_EQ(5, SumOfFontUrlFrequency(result));
  }

  for (int i = 0; i < 2; ++i) {
    LearnSubresourceUrls(
        kGURL, {
                   {GURL(kJpegC), std::make_pair(base::Seconds(1), kEmpty)},
                   {GURL(kJpegD), std::make_pair(base::Seconds(2), kImage)},
               });
  }
  {
    // Now they are recorded.
    LcppData expected = CreateLcppData(kUrl, 10);
    InitializeSubresourceUrlsBucket(expected, {GURL(kJpegC)}, 1);
    InitializeSubresourceUrlsBucket(expected, {GURL(kJpegD)}, 0.8);
    InitializeSubresourceUrlsOtherBucket(expected, 3.2);
    InitializeSubresourceUrlDestinationsBucket(
        expected, {std::make_pair(kJpegC, kEmptyValue),
                   std::make_pair(kJpegD, kImageValue)});
    const LcppData& result = GetDataMap().at("example.test");
    EXPECT_EQ(expected, result) << expected << result;
    EXPECT_DOUBLE_EQ(5, SumOfFontUrlFrequency(result));
  }
}

TEST_P(LcppDataMapFeatures, WhenLcppDataIsCorrupted_ResetData) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  InitializeDB(config);
  EXPECT_TRUE(GetDataMap().empty());

  // Prepare a corrupted data.
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 1.92);
    InitializeLcpElementLocatorBucket(data, "/#b", 1.28);
    InitializeLcpElementLocatorBucket(data, "/#c", -1);
    InitializeLcpElementLocatorOtherBucket(data, -1);
    UpdateKeyValueDataDirectly("a.test", data);
  }

  // Confirm that new learning process reset the corrupted data.
  LearnElementLocator(GURL("http://a.test"), "/#a", {});
  {
    LcppData data = CreateLcppData("a.test", 10);
    InitializeLcpElementLocatorBucket(data, "/#a", 1);
    EXPECT_EQ(data, GetDataMap().at("a.test"));
  }
}

TEST_P(LcppDataMapFeatures, LcppMaxHosts) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 3u;
  InitializeDB(config);
  EXPECT_TRUE(GetDataMap().empty());

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

class LcppDataMapFeatures2 : public LcppDataMapFeatures {};

const std::vector<base::test::FeatureRef> featureset2[] = {
    {},
    {kLCPPInitiatorOrigin}};
INSTANTIATE_TEST_SUITE_P(LcppFeatureSet2,
                         LcppDataMapFeatures2,
                         testing::ValuesIn(featureset2),
                         &CustomParamNameFunction);

TEST_P(LcppDataMapFeatures2, LcppLearnURL) {
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

TEST_P(LcppDataMapFeatures, DeleteUrls) {
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

class LcppMultipleKeyTest
    : public LcppDataMapTest,
      public testing::WithParamInterface<
          std::tuple<bool, blink::features::LcppMultipleKeyTypes>> {
 public:
  void SetUp() override {
    auto& kLcppMultipleKeyType = blink::features::kLcppMultipleKeyType;
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kLCPPMultipleKey,
         {{kLcppMultipleKeyType.name,
           kLcppMultipleKeyType.GetName(std::get<1>(GetParam()))}}}};
    if (std::get<0>(GetParam())) {
      base::test::FeatureRefAndParams params = {kLCPPInitiatorOrigin, {}};
      enabled_features.push_back(params);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});

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
    testing::Combine(
        testing::Bool(),
        testing::Values(blink::features::LcppMultipleKeyTypes::kDefault,
                        blink::features::LcppMultipleKeyTypes::kLcppKeyStat)),
    [](const testing::TestParamInfo<LcppMultipleKeyTest::ParamType>& info) {
      std::string name;
      name += std::get<0>(info.param) ? "InitiatorOrigin_" : "Default_";
      name += (std::get<1>(info.param) ==
               blink::features::LcppMultipleKeyTypes::kDefault)
                  ? "Default"
                  : "KeyStat";
      return name;
    });

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
  const bool kIsDefault = std::get<1>(GetParam()) ==
                          blink::features::LcppMultipleKeyTypes::kDefault;
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

class LcppMultipleKeyTestDefault : public LcppDataMapTest,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    auto& kLcppMultipleKeyType = blink::features::kLcppMultipleKeyType;
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kLCPPMultipleKey,
         {{kLcppMultipleKeyType.name,
           kLcppMultipleKeyType.GetName(
               blink::features::LcppMultipleKeyTypes::kDefault)}}}};
    if (GetParam()) {
      base::test::FeatureRefAndParams params = {kLCPPInitiatorOrigin, {}};
      enabled_features.push_back(params);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
    LcppDataMapTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    LcppMultipleKeyTestDefaultP,
    LcppMultipleKeyTestDefault,
    testing::Bool(),
    [](const testing::TestParamInfo<LcppMultipleKeyTestDefault::ParamType>&
           info) { return (info.param) ? "InitiatorOrigin" : "Default"; });

TEST_P(LcppMultipleKeyTestDefault, MaxHosts) {
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
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{GetParam()},
        /*disabled_features=*/{});
  }

  static base::test::FeatureRefAndParams GetParam() {
    auto& kLcppMultipleKeyType = blink::features::kLcppMultipleKeyType;
    return {blink::features::kLCPPMultipleKey,
            {{kLcppMultipleKeyType.name,
              kLcppMultipleKeyType.GetName(
                  blink::features::LcppMultipleKeyTypes::kLcppKeyStat)}}};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LcppMultipleKeyTestKeyStat : public LcppDataMapTest,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        ScopedLcppKeyStatFeature::GetParam()};
    if (GetParam()) {
      base::test::FeatureRefAndParams params = {kLCPPInitiatorOrigin, {}};
      enabled_features.push_back(params);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
    LcppDataMapTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    LcppMultipleKeyTestKeyStatP,
    LcppMultipleKeyTestKeyStat,
    testing::Bool(),
    [](const testing::TestParamInfo<LcppMultipleKeyTestKeyStat::ParamType>&
           info) { return (info.param) ? "InitiatorOrigin" : "Default"; });

TEST_P(LcppMultipleKeyTestKeyStat, MaxHostsAndKeys) {
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
  EXPECT_EQ(data, GetDataMap().at("a.test"));
}

TEST_P(LcppMultipleKeyTestKeyStat, AddNewEntryToFullBucketKeyStat) {
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

class ScopedInitiatorOriginFeature {
 public:
  ScopedInitiatorOriginFeature() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kLCPPInitiatorOrigin);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LcppInitiatorOriginTest : public LcppDataMapTest {
 public:
  void SetUp() override {
    scoped_feature_ = std::make_unique<ScopedInitiatorOriginFeature>();
    LcppDataMapTest::SetUp();
  }

 protected:
  void TearDown() override {
    if (lcpp_data_map_) {
      for (const auto& it : GetOriginMap()) {
        const LcppOrigin& lcpp_origin = it.second;
        EXPECT_TRUE(IsCanonicalFrequencyData(lcpp_origin)) << it.first;
      }
      lcpp_data_map_->origin_map_->DeleteAllData();
    }
    LcppDataMapTest::TearDown();
  }

  LcppDataMap::OriginMap* OriginMap() {
    return lcpp_data_map_ ? lcpp_data_map_->origin_map_.get() : nullptr;
  }

  bool IsCanonicalFrequencyData(const LcppOrigin& lcpp_origin) {
    return IsCanonicalizedFrequencyData(
        config_.lcpp_initiator_origin_max_histogram_buckets,
        lcpp_origin.key_frequency_stat(), lcpp_origin.origin_data_map());
  }

 private:
  std::unique_ptr<ScopedInitiatorOriginFeature> scoped_feature_;
};

TEST_F(LcppInitiatorOriginTest, Base) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 1u;
  config.lcpp_initiator_origin_histogram_sliding_window_size = 4u;
  config.lcpp_initiator_origin_max_histogram_buckets = 3u;
  InitializeDB(config);
  EXPECT_TRUE(GetDataMap().empty());
  EXPECT_TRUE(GetOriginMap().empty());

  const GURL url("http://a.test");
  LearnElementLocator(url, "/#lcp0");
  EXPECT_EQ(*GetLcppStat(std::nullopt, url),
            MakeLcppStatWithLCPElementLocator("/#lcp0"));

  const url::Origin origin1 = CreateOrigin("origin1.test");
  LearnElementLocator(origin1, url, "/#lcp1");
  EXPECT_EQ(*GetLcppStat(origin1, url),
            MakeLcppStatWithLCPElementLocator("/#lcp1"));

  const url::Origin origin2 = CreateOrigin("origin2.test");
  LearnElementLocator(origin2, url, "/#lcp2");
  EXPECT_EQ(*GetLcppStat(origin2, url),
            MakeLcppStatWithLCPElementLocator("/#lcp2"));

  const url::Origin origin3 = CreateOrigin("origin3.test");
  LearnElementLocator(origin3, url, "/#lcp3");
  EXPECT_EQ(*GetLcppStat(origin3, url),
            MakeLcppStatWithLCPElementLocator("/#lcp3"));

  const url::Origin origin4 = CreateOrigin("origin4.test");
  LearnElementLocator(origin4, url, "/#lcp4");
  EXPECT_EQ(*GetLcppStat(origin4, url),
            MakeLcppStatWithLCPElementLocator("/#lcp4"));
  // Confirm the first origin over `lcpp_multiple_key_max_histogram_buckets` is
  // dropped.
  EXPECT_FALSE(GetLcppStat(origin1, url));
  // Non origin entry is still alive.
  EXPECT_EQ(*GetLcppStat(std::nullopt, url),
            MakeLcppStatWithLCPElementLocator("/#lcp0"));

  // Confirm adding other host urls over `max_hosts_to_track_for_lcpp` lets all
  // the origin-associated entries be dropped.
  const GURL url_b("http://b.test");
  LearnElementLocator(origin1, url_b, "/#b");
  EXPECT_EQ(*GetLcppStat(origin1, url_b),
            MakeLcppStatWithLCPElementLocator("/#b"));
  EXPECT_FALSE(GetLcppStat(origin2, url));
  EXPECT_FALSE(GetLcppStat(origin3, url));
  EXPECT_FALSE(GetLcppStat(origin4, url));
  // Non origin entry is still alive.
  EXPECT_EQ(*GetLcppStat(std::nullopt, url),
            MakeLcppStatWithLCPElementLocator("/#lcp0"));
}

TEST_F(LcppInitiatorOriginTest, AddNewEntryToFullBuckets) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 1u;
  config.lcpp_initiator_origin_histogram_sliding_window_size = 4u;
  config.lcpp_initiator_origin_max_histogram_buckets = 2u;
  InitializeDB(config);

  const GURL url("http://a.test");
  const url::Origin origin1 = CreateOrigin("origin1.test");
  LearnElementLocator(origin1, url, "/#lcp1");
  LearnElementLocator(origin1, url, "/#lcp1");
  const url::Origin origin2 = CreateOrigin("origin2.test");
  LearnElementLocator(origin2, url, "/#lcp2");
  LearnElementLocator(origin2, url, "/#lcp2");
  EXPECT_EQ(*GetLcppStat(origin1, url),
            MakeLcppStatWithLCPElementLocator("/#lcp1", /*frequency=*/2));
  EXPECT_EQ(*GetLcppStat(origin2, url),
            MakeLcppStatWithLCPElementLocator("/#lcp2", /*frequency=*/2));

  // New entry is not recorded until its frequency is over existing ones.
  const url::Origin origin3 = CreateOrigin("origin3.test");
  EXPECT_FALSE(LearnElementLocator(origin3, url, "/#lcp3"));
  EXPECT_FALSE(GetLcppStat(origin3, url));

  EXPECT_FALSE(LearnElementLocator(origin3, url, "/#lcp3"));
  EXPECT_FALSE(GetLcppStat(origin3, url));

  EXPECT_TRUE(LearnElementLocator(origin3, url, "/#lcp3"));
  EXPECT_EQ(*GetLcppStat(origin3, url),
            MakeLcppStatWithLCPElementLocator("/#lcp3"));
}

TEST_F(LcppInitiatorOriginTest, TooLongHostName) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  InitializeDB(config);

  const GURL url("http://a.test");
  const url::Origin origin1 = CreateOrigin("origin1.test");
  LearnElementLocator(origin1, url, "/#lcp1");
  CHECK_EQ(*GetLcppStat(origin1, url),
           MakeLcppStatWithLCPElementLocator("/#lcp1"));

  const url::Origin too_long_origin = CreateOrigin(
      std::string(ResourcePrefetchPredictorTables::kMaxStringLength + 1, 'c'));
  EXPECT_FALSE(LearnElementLocator(too_long_origin, url, "/#lcp1"));
  EXPECT_FALSE(GetLcppStat(too_long_origin, url));
}

TEST_F(LcppInitiatorOriginTest, DeleteURL) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  config.max_hosts_to_track_for_lcpp = 3u;
  config.lcpp_initiator_origin_histogram_sliding_window_size = 4u;
  config.lcpp_initiator_origin_max_histogram_buckets = 3u;
  InitializeDB(config);

  const GURL url1("http://a.test");
  LearnElementLocator(url1, "/#lcp0");
  EXPECT_EQ(*GetLcppStat(std::nullopt, url1),
            MakeLcppStatWithLCPElementLocator("/#lcp0"));
  const GURL url2("http://b.test");
  const url::Origin origin2 = url::Origin::Create(url2);
  LearnElementLocator(origin2, url1, "/#lcp2");
  EXPECT_EQ(*GetLcppStat(origin2, url1),
            MakeLcppStatWithLCPElementLocator("/#lcp2"));
  const GURL url3("http://c.test");
  const url::Origin origin3 = url::Origin::Create(url3);
  LearnElementLocator(origin3, url1, "/#lcp3");
  EXPECT_EQ(*GetLcppStat(origin3, url1),
            MakeLcppStatWithLCPElementLocator("/#lcp3"));

  LearnElementLocator(url2, "/#lcp4");
  EXPECT_EQ(*GetLcppStat(std::nullopt, url2),
            MakeLcppStatWithLCPElementLocator("/#lcp4"));

  lcpp_data_map_->DeleteUrls({url2, GURL("http://d.test")});
  // Confirm all `url2` associated entries was removed.
  EXPECT_EQ(*GetLcppStat(std::nullopt, url1),
            MakeLcppStatWithLCPElementLocator("/#lcp0"));
  EXPECT_FALSE(GetLcppStat(origin2, url1));
  EXPECT_EQ(*GetLcppStat(origin3, url1),
            MakeLcppStatWithLCPElementLocator("/#lcp3"));
  EXPECT_FALSE(GetLcppStat(std::nullopt, url2));

  lcpp_data_map_->DeleteAllData();
  // Confirm all data was removed.
  EXPECT_FALSE(GetLcppStat(std::nullopt, url1));
  EXPECT_FALSE(GetLcppStat(origin2, url1));
  EXPECT_FALSE(GetLcppStat(origin3, url1));
  EXPECT_FALSE(GetLcppStat(std::nullopt, url2));
}

TEST_F(LcppInitiatorOriginTest, CanonicalizeBrokenDataOnStartUp) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  const GURL url("http://a.test");
  const url::Origin origin1 = CreateOrigin("origin1.test");
  const url::Origin origin2 = CreateOrigin("origin2.test");
  {
    InitializeDB(config);

    LearnElementLocator(origin1, url, "/#lcp1");
    EXPECT_EQ(*GetLcppStat(origin1, url),
              MakeLcppStatWithLCPElementLocator("/#lcp1"));
    LearnElementLocator(origin2, url, "/#lcp2");
    EXPECT_EQ(*GetLcppStat(origin2, url),
              MakeLcppStatWithLCPElementLocator("/#lcp2"));

    LcppOrigin lcpp_origin;
    OriginMap()->TryGetData(url.host(), &lcpp_origin);
    EXPECT_TRUE(!lcpp_origin.origin_data_map().empty());
    EXPECT_TRUE(IsCanonicalFrequencyData(lcpp_origin));
    lcpp_origin.mutable_origin_data_map()->erase(origin2.host());
    EXPECT_FALSE(IsCanonicalFrequencyData(lcpp_origin));
    OriginMap()->UpdateData(url.host(), lcpp_origin);
    TearDownDB();
  }

  {
    InitializeDB(config);
    for (const auto& it : GetOriginMap()) {
      const LcppOrigin& lcpp_origin = it.second;
      EXPECT_TRUE(IsCanonicalFrequencyData(lcpp_origin)) << it.first;
    }

    EXPECT_EQ(*GetLcppStat(origin1, url),
              MakeLcppStatWithLCPElementLocator("/#lcp1"));
    EXPECT_FALSE(GetLcppStat(origin2, url));
    TearDownDB();
  }
}

TEST_F(LcppDataMapTest, ResetInitiatorOriginDBOverFlag) {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  const GURL url("http://a.test");
  const GURL url2("http://b.test");
  const url::Origin origin = CreateOrigin("origin1.test");
  {
    [[maybe_unused]] ScopedInitiatorOriginFeature scoped_feature;
    InitializeDB(config);

    LearnElementLocator(url, "/#lcp0");
    EXPECT_EQ(*GetLcppStat(std::nullopt, url),
              MakeLcppStatWithLCPElementLocator("/#lcp0"));

    LearnElementLocator(origin, url2, "/#lcp1");
    EXPECT_EQ(*GetLcppStat(origin, url2),
              MakeLcppStatWithLCPElementLocator("/#lcp1"));

    TearDownDB();
  }

  {
    [[maybe_unused]] ScopedInitiatorOriginFeature scoped_feature;
    InitializeDB(config);

    EXPECT_EQ(*GetLcppStat(std::nullopt, url),
              MakeLcppStatWithLCPElementLocator("/#lcp0"));
    EXPECT_EQ(*GetLcppStat(origin, url2),
              MakeLcppStatWithLCPElementLocator("/#lcp1"));
    TearDownDB();
  }

  {
    InitializeDB(config);

    EXPECT_EQ(*GetLcppStat(std::nullopt, url),
              MakeLcppStatWithLCPElementLocator("/#lcp0"));
    // LcppStat associated to an initiator origin is removed.
    EXPECT_FALSE(GetLcppStat(origin, url2));
    TearDownDB();
  }

  {
    [[maybe_unused]] ScopedInitiatorOriginFeature scoped_feature;
    InitializeDB(config);

    EXPECT_EQ(*GetLcppStat(std::nullopt, url),
              MakeLcppStatWithLCPElementLocator("/#lcp0"));
    // LcppStat associated to an initiator origin is removed.
    EXPECT_FALSE(GetLcppStat(origin, url2));
    TearDownDB();
  }
}

}  // namespace predictors
