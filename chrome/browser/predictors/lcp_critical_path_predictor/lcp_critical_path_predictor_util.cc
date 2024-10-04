// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace predictors {

namespace {
constexpr std::string_view kLcppTableName = "lcp_critical_path_predictor";
constexpr std::string_view kLcppTableNameInitiatorOrigin =
    "lcp_critical_path_predictor_initiator_origin";
const char kCreateProtoTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, "
    "proto BLOB, "
    "PRIMARY KEY(key))";

// Convert `LcppStringFrequencyStatData` a vector of frequency and std::string.
// The result is sorted with frequency (from high to low).
std::vector<std::pair<double, std::string>> ConvertToFrequencyStringPair(
    const LcppStringFrequencyStatData& data) {
  const auto& buckets = data.main_buckets();
  std::vector<std::pair<double, std::string>> frequency_and_string;
  frequency_and_string.reserve(buckets.size());
  for (const auto& [script_url, frequency] : buckets) {
    frequency_and_string.emplace_back(frequency, script_url);
  }

  // Reverse sort `frequency_and_string`. i.e. higher frequency goes first.
  // That is why `rbegin` and `rend` instead of `begin` and `end`.
  std::sort(frequency_and_string.rbegin(), frequency_and_string.rend());
  return frequency_and_string;
}

// Returns true if the given `url` is a valid URL to be used as a key
// of `LcppStringFrequencyStatData`.
bool IsValidUrlInLcppStringFrequencyStatData(const std::string& url) {
  if (url.empty()) {
    return false;
  }

  if (url.size() > ResourcePrefetchPredictorTables::kMaxStringLength) {
    return false;
  }

  GURL parsed_url(url);
  if (!parsed_url.is_valid() || !parsed_url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  return true;
}

// Returns LCP element locators in the past loads for a given `stat`.  The
// returned LCP element locators are ordered by descending frequency (the
// most frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<std::string> PredictLcpElementLocators(const LcppStat& stat) {
  // We do not use `ConvertToFrequencyStringPair` for the following code
  // because the core part of the code is converting `std::map` to
  // `std::vector<std::pair<double, std::string>>`, which we need the different
  // logic due to the `bytes` protobuf type.
  const auto& buckets =
      stat.lcp_element_locator_stat().lcp_element_locator_buckets();
  std::vector<std::pair<double, std::string>>
      lcp_element_locators_with_frequency;
  lcp_element_locators_with_frequency.reserve(buckets.size());
  for (const auto& bucket : buckets) {
    lcp_element_locators_with_frequency.emplace_back(
        bucket.frequency(), bucket.lcp_element_locator());
  }

  // Makes higher frequency goes first by `rbegin` and `rend`.
  std::sort(lcp_element_locators_with_frequency.rbegin(),
            lcp_element_locators_with_frequency.rend());

  std::vector<std::string> lcp_element_locators;
  lcp_element_locators.reserve(lcp_element_locators_with_frequency.size());
  for (auto& bucket : lcp_element_locators_with_frequency) {
    lcp_element_locators.push_back(std::move(bucket.second));
  }
  return lcp_element_locators;
}

// Returns LCP influencer scripts from past loads for a given `stat`.
// The returned script urls are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictLcpInfluencerScripts(const LcppStat& stat) {
  std::vector<std::pair<double, std::string>> lcp_script_urls_with_frequency =
      ConvertToFrequencyStringPair(stat.lcp_script_url_stat());

  std::vector<GURL> lcp_script_urls;
  lcp_script_urls.reserve(lcp_script_urls_with_frequency.size());
  for (const auto& [frequency, script_url] : lcp_script_urls_with_frequency) {
    GURL parsed_url(script_url);
    if (!parsed_url.is_valid() || !parsed_url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    lcp_script_urls.push_back(std::move(parsed_url));
  }
  return lcp_script_urls;
}

double SumOfFrequency(const std::map<std::string, double>& histogram,
                      double other_bucket_frequency) {
  double sum = other_bucket_frequency;
  for (const auto& it : histogram) {
    sum += it.second;
  }
  return sum;
}

// This class implements the algorithm to update the
// `LcppStringFrequencyStatData` with given entries.
//
// The class is used in three steps.
// 1. Instantiate.
// 2. Update (You can repeat this step if there is multiple inputs).
// 3. Extract.
// e.g.
// ```
// // Instantiate.
// LcppFrequencyStatDataUpdater updater =
// LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
//    sliding_window_size, max_histogram_buckets,
//    *stat.mutable_lcp_script_url_stat());
// // Update.
// updater.Update(url);
// // Extract.
// *stat.mutable_lcp_script_url_stat() =
//     updater.ToLcppStringFrequencyStatData();
// ```
//
// The algorithm takes two parameters:
// - sliding_window_size
// - max_histogram_buckets
//
// `sliding_window_size` is a virtual sliding window size that builds
// histogram.
// `max_histogram_buckets` is a bucket count that actually can be saved
// in the database. If the histogram has more buckets than
// `max_histogram_buckets`, this algorithm sums up the less frequent
// buckets, and stores them in a single "other_bucket" called
// `other_bucket_frequency`.
//
// <Conceptual model of sliding window and histogram>
//
// <------ sliding window ------->
// +-----------------------------+                 <- data feed
// | /#a   /#a   /#a   /#b   /#b | /#b   /#c   /#d   /#c   /#d
// +-----------------------------+
//  => histogram: {/#a: 3, /#b: 2}
//
//       <------ sliding window ------->
//       +-----------------------------+
//   /#a | /#a   /#a   /#b   /#b   /#b | /#c   /#d   /#c   /#d
//       +-----------------------------+
//        => histogram: {/#a: 2, /#b: 3}
//
//             <------ sliding window ------->
//             +-----------------------------+
//   /#a   /#a | /#a   /#b   /#b   /#b   /#c | /#d   /#c   /#d
//             +-----------------------------+
//              => histogram: {/#a: 1, /#b: 3, /#c: 1}
//
// The above sliding window model has the following two problems for us.
//
// - [Problem_1] We need to keep the entire data inside the sliding
//               window to know which item is the first item inside
//               the sliding window. But we don't want to keep such
//               large data.
// - [Problem_2] The histogram can be large if the items don't have
//               overlap. We don't want to keep a large histogram.
//
// To address [Problem_1], we decided not to use the first item
// inside the sliding window. Instead, We decided to reduce the
// weight of the item.
//
// histogram: {/#a: 3, /#b: 2}
// => histogram: {/#a: {1, 1, 1}, /#b: {1, 1}}
//
// To add new item "/#c", Reduce the item weight, and add "/#c".
//
// histogram: {/#a: {4/5, 4/5, 4/5}, /#b: {4/5, 4/5}, /#c: {1}}
//
// To address [Problem_2], we decided to introduce an "others"
// bucket.
//
// histogram: {/#a: 5, /#b: 3, /#c: 2, /#d: 2}
//
// To reduce the bucket count under 3 buckets, we merge /#c and /#d
// buckets into an <other> bucket.
//
// histogram: {/#a: 5, /#b: 3, <other>: 4}
//
// For more information, please see:
// https://docs.google.com/document/d/1T80d4xW8xIEqfo792g1nC1deFqzMraunFJW_5ft4ziQ/edit
class LcppFrequencyStatDataUpdater {
 public:
  ~LcppFrequencyStatDataUpdater() = default;
  LcppFrequencyStatDataUpdater(const LcppFrequencyStatDataUpdater&) = delete;
  const LcppFrequencyStatDataUpdater& operator=(
      const LcppFrequencyStatDataUpdater&) = delete;

  static std::unique_ptr<LcppFrequencyStatDataUpdater>
  FromLcppStringFrequencyStatData(
      size_t sliding_window_size,
      size_t max_histogram_buckets,
      const LcppStringFrequencyStatData& lcpp_stat_data) {
    // Prepare working variables (histogram and other_bucket_frequency) from
    // proto. If the data is corrupted, the previous data will be cleared.
    bool corrupted = false;
    double other_bucket_frequency = lcpp_stat_data.other_bucket_frequency();
    if (other_bucket_frequency < 0 ||
        lcpp_stat_data.main_buckets().size() > max_histogram_buckets) {
      corrupted = true;
    }
    std::map<std::string, double> histogram;
    for (const auto& [entry, frequency] : lcpp_stat_data.main_buckets()) {
      if (corrupted || entry.empty() || frequency < 0.0) {
        corrupted = true;
        break;
      }
      histogram.insert_or_assign(entry, frequency);
    }
    if (corrupted) {
      other_bucket_frequency = 0;
      histogram.clear();
    }
    return base::WrapUnique(new LcppFrequencyStatDataUpdater(
        sliding_window_size, max_histogram_buckets, histogram,
        other_bucket_frequency));
  }

  static std::unique_ptr<LcppFrequencyStatDataUpdater>
  FromLcpElementLocatorStat(
      const LoadingPredictorConfig& config,
      const LcpElementLocatorStat& lcp_element_locator_stat) {
    // Prepare working variables (histogram and other_bucket_frequency) from
    // proto. If the data is corrupted, the previous data will be cleared.
    bool corrupted = false;
    double other_bucket_frequency =
        lcp_element_locator_stat.other_bucket_frequency();
    if (other_bucket_frequency < 0 ||
        lcp_element_locator_stat.lcp_element_locator_buckets_size() >
            static_cast<int>(config.max_lcpp_histogram_buckets)) {
      corrupted = true;
    }
    std::map<std::string, double> histogram;
    for (const auto& it :
         lcp_element_locator_stat.lcp_element_locator_buckets()) {
      if (corrupted || !it.has_lcp_element_locator() || !it.has_frequency() ||
          it.frequency() < 0.0) {
        corrupted = true;
        break;
      }
      histogram.insert_or_assign(it.lcp_element_locator(), it.frequency());
    }
    if (corrupted) {
      other_bucket_frequency = 0;
      histogram.clear();
    }
    return base::WrapUnique(new LcppFrequencyStatDataUpdater(
        config.lcpp_histogram_sliding_window_size,
        config.max_lcpp_histogram_buckets, histogram, other_bucket_frequency));
  }

  void Update(const std::string& new_entry) {
    // If there is no room to add a `new_entry` (the capacity is
    // the same as the sliding window size), create a room by discounting the
    // existing histogram frequency.
    if (1 + SumOfFrequency(histogram_, other_bucket_frequency_) >
        sliding_window_size_) {
      double discount = 1.0 / sliding_window_size_;
      for (auto it = histogram_.begin(); it != histogram_.end();) {
        it->second -= it->second * discount;
        // Remove item that has too small frequency.
        if (it->second < 1e-7) {
          it = histogram_.erase(it);
        } else {
          ++it;
        }
      }
      other_bucket_frequency_ -= other_bucket_frequency_ * discount;
    }

    // Now we have one free space to store a new lcp_script_url.
    // (`SumOfFrequency()` takes time. Hence `DCHECK_LE` is used.)
    // Adds `1e-5` to avoid floating point errors.
    DCHECK_LE(1 + SumOfFrequency(histogram_, other_bucket_frequency_),
              sliding_window_size_ + 1e-5);

    // Store new_entry.
    {
      auto it = histogram_.emplace(new_entry, 1);
      if (!it.second) {
        ++it.first->second;
        ++num_matched_;
      }
    }

    // Before saving histogram, we need to reduce the count of buckets less
    // than `max_histogram_buckets`. If the bucket count is more than
    // `max_histogram_buckets`, we can merge the least frequent bucket into
    // other_bucket.
    if (histogram_.size() > max_histogram_buckets_) {
      const auto& least_frequent_bucket =
          std::min_element(histogram_.begin(), histogram_.end(),
                           [](const auto& lhs, const auto& rhs) {
                             return lhs.second < rhs.second;
                           });
      other_bucket_frequency_ += least_frequent_bucket->second;
      dropped_entries_.push_back(least_frequent_bucket->first);
      histogram_.erase(least_frequent_bucket);
    }
    has_updated_ = true;
  }

  LcppStringFrequencyStatData ToLcppStringFrequencyStatData() {
    LcppStringFrequencyStatData out_data;
    // Copy the results (histogram and other_bucket_frequency) into proto.
    out_data.set_other_bucket_frequency(other_bucket_frequency_);

    for (const auto& [url, frequency] : histogram_) {
      out_data.mutable_main_buckets()->insert({url, frequency});
    }
    return out_data;
  }

  LcpElementLocatorStat ToLcpElementLocatorStat() {
    LcpElementLocatorStat lcp_element_locator_stat;
    // Copy the results (histogram and other_bucket_frequency) into proto.
    lcp_element_locator_stat.set_other_bucket_frequency(
        other_bucket_frequency_);
    for (const auto& bucket : histogram_) {
      auto* bucket_to_add =
          lcp_element_locator_stat.add_lcp_element_locator_buckets();
      bucket_to_add->set_lcp_element_locator(bucket.first);
      bucket_to_add->set_frequency(bucket.second);
    }
    return lcp_element_locator_stat;
  }

  bool has_updated() const { return has_updated_; }
  const std::vector<std::string>& dropped_entries() const {
    return dropped_entries_;
  }

  size_t num_matched() const { return num_matched_; }

 private:
  LcppFrequencyStatDataUpdater(size_t sliding_window_size,
                               size_t max_histogram_buckets,
                               std::map<std::string, double> histogram,
                               double other_bucket_frequency)
      : sliding_window_size_(sliding_window_size),
        max_histogram_buckets_(max_histogram_buckets),
        histogram_(histogram),
        other_bucket_frequency_(other_bucket_frequency) {}

  const size_t sliding_window_size_;
  const size_t max_histogram_buckets_;
  std::map<std::string, double> histogram_;
  double other_bucket_frequency_;
  bool has_updated_ = false;
  size_t num_matched_ = 0;
  std::vector<std::string> dropped_entries_;
};

bool RecordLcpElementLocatorHistogram(const LoadingPredictorConfig& config,
                                      const std::string& lcp_element_locator,
                                      LcppStat& stat) {
  if (lcp_element_locator.size() >
          ResourcePrefetchPredictorTables::kMaxStringLength ||
      lcp_element_locator.empty()) {
    return false;
  }
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcpElementLocatorStat(
          config, stat.lcp_element_locator_stat());
  CHECK(updater);
  updater->Update(lcp_element_locator);
  *stat.mutable_lcp_element_locator_stat() = updater->ToLcpElementLocatorStat();
  return true;
}

bool RecordLcpInfluencerScriptUrlsHistogram(
    const LoadingPredictorConfig& config,
    const std::vector<GURL>& lcp_influencer_scripts,
    LcppStat& stat) {
  // Contrasting to LCPP Element locator, there are multiple LCP dependency URLs
  // for an origin. Record each in a separate histogram.
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
          config.lcpp_histogram_sliding_window_size,
          config.max_lcpp_histogram_buckets, stat.lcp_script_url_stat());
  CHECK(updater);
  for (auto& script_url : lcp_influencer_scripts) {
    const auto& lcpp_script = script_url.spec();
    if (!IsValidUrlInLcppStringFrequencyStatData(lcpp_script)) {
      continue;
    }
    updater->Update(lcpp_script);
  }
  *stat.mutable_lcp_script_url_stat() =
      updater->ToLcppStringFrequencyStatData();
  return updater->has_updated();
}

bool RecordPreconnectOriginsHistogram(const LoadingPredictorConfig& config,
                                      const std::vector<GURL>& origins,
                                      LcppStat& stat) {
  // There could be multiple preconnect origins. Record each in a separate
  // histogram.
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
          config.lcpp_histogram_sliding_window_size,
          config.max_lcpp_histogram_buckets, stat.preconnect_origin_stat());
  CHECK(updater);
  for (auto& origin : origins) {
    const auto& origin_spec = origin.spec();
    if (!IsValidUrlInLcppStringFrequencyStatData(origin_spec)) {
      continue;
    }
    updater->Update(origin_spec);
  }
  *stat.mutable_preconnect_origin_stat() =
      updater->ToLcppStringFrequencyStatData();
  return updater->has_updated();
}

bool RecordFetchedFontUrlsHistogram(const LoadingPredictorConfig& config,
                                    const std::vector<GURL>& fetched_font_urls,
                                    LcppStat& stat) {
  // Due to LCPP data structure, histogram is saved per origin.
  // Therefore, it sounds better to have this as a histogram instead of
  // a static data.
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
          config.lcpp_histogram_sliding_window_size,
          config.max_lcpp_histogram_buckets, stat.fetched_font_url_stat());
  std::set<GURL> used_urls;
  size_t max_url_length = 0;
  for (const auto& url : fetched_font_urls) {
    // Deduplicate the font URLs.
    if (!used_urls.insert(url).second) {
      continue;
    }
    const std::string& font_spec = url.spec();
    max_url_length = std::max<size_t>(max_url_length, font_spec.length());
    if (!IsValidUrlInLcppStringFrequencyStatData(font_spec)) {
      continue;
    }
    updater->Update(font_spec);
  }
  *stat.mutable_fetched_font_url_stat() =
      updater->ToLcppStringFrequencyStatData();

  base::UmaHistogramCounts10000(
      "Blink.LCPP.RecordedFontCount",
      base::checked_cast<int>(fetched_font_urls.size()));
  base::UmaHistogramCounts10000("Blink.LCPP.RecordedFontUrlsMaxLength",
                                base::checked_cast<int>(max_url_length));
  base::UmaHistogramCounts10000(
      "Blink.LCPP.RecordedFontUrlMatchCount",
      base::checked_cast<int>(updater->num_matched()));
  if (!fetched_font_urls.empty()) {
    base::UmaHistogramCounts10000(
        "Blink.LCPP.RecordedFontUrlMatchCountForPagesWithFonts",
        base::checked_cast<int>(updater->num_matched()));
    base::UmaHistogramPercentage(
        "Blink.LCPP.RecordedFontUrlPredictionMatchPercent",
        base::checked_cast<int>(100 * updater->num_matched() /
                                fetched_font_urls.size()));
  }

  return updater->has_updated();
}

bool RecordFetchedSubresourceUrlsHistogram(
    const LoadingPredictorConfig& config,
    const std::map<
        GURL,
        std::pair<base::TimeDelta, network::mojom::RequestDestination>>&
        fetched_subresource_urls,
    LcppStat& stat) {
  // `time_and_urls` keeps URLs (and its fetch timings) in a reversed
  // event order. The URL count that can be stored in the database is
  // limited. By processing recently fetched URLs first, we can keep the
  // URLs that were fetched in the beginning of navigation.
  std::vector<std::pair<base::TimeDelta, std::string>> time_and_urls;
  time_and_urls.reserve(fetched_subresource_urls.size());
  for (const auto& [subresource_url, time_and_request_destination] :
       fetched_subresource_urls) {
    time_and_urls.emplace_back(time_and_request_destination.first,
                               subresource_url.spec());

    stat.mutable_fetched_subresource_url_destination()->insert(
        {subresource_url.spec(),
         static_cast<int32_t>(time_and_request_destination.second)});
  }
  // Reverse sort `time_and_urls`. That is why `rbegin` and `rend`
  // instead of `begin` and `end`.
  std::sort(time_and_urls.rbegin(), time_and_urls.rend());

  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
          config.lcpp_histogram_sliding_window_size,
          config.max_lcpp_histogram_buckets,
          stat.fetched_subresource_url_stat());
  for (const auto& [resource_load_start, subresource_url] : time_and_urls) {
    if (!IsValidUrlInLcppStringFrequencyStatData(subresource_url)) {
      continue;
    }
    updater->Update(subresource_url);
  }
  *stat.mutable_fetched_subresource_url_stat() =
      updater->ToLcppStringFrequencyStatData();
  for (const auto& dropped_url : updater->dropped_entries()) {
    stat.mutable_fetched_subresource_url_destination()->erase(dropped_url);
  }
  return updater->has_updated();
}

bool IsValidLcpElementLocatorHistogram(
    const LcpElementLocatorStat& lcp_element_locator_stat) {
  if (lcp_element_locator_stat.other_bucket_frequency() < 0.0) {
    return false;
  }
  for (const auto& it :
       lcp_element_locator_stat.lcp_element_locator_buckets()) {
    if (!it.has_lcp_element_locator() || !it.has_frequency() ||
        it.frequency() < 0.0) {
      return false;
    }
  }
  return true;
}

bool RecordUnusedPreloadUrlsHistogram(const LoadingPredictorConfig& config,
                                      const std::vector<GURL>& unused_preloads,
                                      LcppStat& stat) {
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
          config.lcpp_histogram_sliding_window_size,
          config.max_lcpp_histogram_buckets, stat.unused_preload_stat());
  CHECK(updater);
  for (auto& url : unused_preloads) {
    if (!IsValidUrlInLcppStringFrequencyStatData(url.spec())) {
      continue;
    }
    updater->Update(url.spec());
  }
  *stat.mutable_unused_preload_stat() =
      updater->ToLcppStringFrequencyStatData();

  return updater->has_updated();
}

bool IsValidLcpUrlsHistogram(
    const LcppStringFrequencyStatData& lcpp_stat_data) {
  if (lcpp_stat_data.other_bucket_frequency() < 0.0) {
    return false;
  }
  for (const auto& [entry, frequency] : lcpp_stat_data.main_buckets()) {
    if (frequency < 0.0) {
      return false;
    }
    if (!IsValidUrlInLcppStringFrequencyStatData(entry)) {
      return false;
    }
  }
  return true;
}

size_t GetLCPPMultipleKeyMaxPathLength() {
  static const size_t max_length = base::checked_cast<size_t>(
      blink::features::kLCPPMultipleKeyMaxPathLength.Get());
  return max_length;
}

bool IsKeyLengthValidForMultipleKey(const std::string& host,
                                    const std::string& first_level_path) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kLCPPMultipleKey));
  // The key must not be longer than `kMaxStringLength`.
  // Note that we confirmed that url.host() is less than the limit in
  // `IsURLValidForLcpp()`.
  return host.length() + first_level_path.length() <=
         ResourcePrefetchPredictorTables::kMaxStringLength;
}

bool IsLcppMultipleKeyKeyStatEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kLCPPMultipleKey) &&
         (blink::features::kLcppMultipleKeyType.Get() ==
          blink::features::LcppMultipleKeyTypes::kLcppKeyStat);
}

std::string GetLCPPDatabaseKey(const GURL& url) {
  CHECK(IsURLValidForLcpp(url));

  if (!base::FeatureList::IsEnabled(blink::features::kLCPPMultipleKey) ||
      IsLcppMultipleKeyKeyStatEnabled()) {
    return url.host();
  }

  const std::string first_level_path = GetFirstLevelPath(url);
  if (!IsKeyLengthValidForMultipleKey(url.host(), first_level_path)) {
    return url.host();
  }
  return url.host() + first_level_path;
}

// Returns LcppStat from `data` for LcppMultipleKeyKeyStat.
// This function can modify `data` to emplace new LcppStat. `data_updated` is
// true on the case and the caller should update the stored data.
// This can return nullptr based on the FrequencyStatData of `data`.
LcppStat* TryToGetLcppStatForKeyStat(const LoadingPredictorConfig& config,
                                     const GURL& url,
                                     LcppData& data,
                                     bool& data_updated) {
  CHECK(IsLcppMultipleKeyKeyStatEnabled());

  const std::string first_level_path = GetFirstLevelPath(url);
  if (first_level_path.empty() ||
      !IsKeyLengthValidForMultipleKey(url.host(), first_level_path)) {
    return data.mutable_lcpp_stat();
  }

  LcppKeyStat& key_stat = *data.mutable_lcpp_key_stat();
  // Since UpdateLcppStringFrequencyStatData modifies a part of `data`,
  // caller should update the stored data if the function is called.
  data_updated = true;
  return UpdateFrequencyStatAndTryGetEntry(
      config.lcpp_multiple_key_histogram_sliding_window_size,
      config.lcpp_multiple_key_max_histogram_buckets, first_level_path,
      *key_stat.mutable_key_frequency_stat(),
      *key_stat.mutable_lcpp_stat_map());
}

bool IsLCPPFontPrefetchExcludedHost(const GURL& url) {
  static const base::NoDestructor<base::flat_set<std::string>> excluded_hosts(
      base::SplitString(
          blink::features::kLCPPFontURLPredictorExcludedHosts.Get(), ",",
          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
  return base::Contains(*excluded_hosts, url.host());
}

class FakeLoadingPredictorKeyValueTable
    : public sqlite_proto::KeyValueTable<LcppData> {
 public:
  FakeLoadingPredictorKeyValueTable()
      : sqlite_proto::KeyValueTable<LcppData>("") {}
  void GetAllData(std::map<std::string, LcppData>* data_map,
                  sql::Database* db) const override {
    *data_map = data_;
  }
  void UpdateData(const std::string& key,
                  const LcppData& data,
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

  std::map<std::string, LcppData> data_;
};

bool EnsureTable(sql::Database* db, const std::string_view& table_name) {
  return (db->DoesTableExist(table_name) ||
          db->Execute(base::StringPrintf(kCreateProtoTableStatementTemplate,
                                         std::string(table_name).c_str())));
}

bool IsInitiatorOriginEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kLCPPInitiatorOrigin);
}

void DeleteTables(std::unique_ptr<LcppDataMap::DataTable> data_table,
                  std::unique_ptr<LcppDataMap::OriginTable> origin_table) {
  if (IsInitiatorOriginEnabled()) {
    origin_table.reset();
  }
  data_table.reset();
}

}  // namespace

std::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
ConvertLcppStatToLCPCriticalPathPredictorNavigationTimeHint(
    const LcppStat& lcpp_stat) {
  std::vector<std::string> lcp_element_locators =
      PredictLcpElementLocators(lcpp_stat);
  std::vector<GURL> lcp_influencer_scripts =
      PredictLcpInfluencerScripts(lcpp_stat);
  std::vector<GURL> fetched_fonts = PredictFetchedFontUrls(lcpp_stat);
  std::vector<GURL> preconnect_origins =
      PredictPreconnectableOrigins(lcpp_stat);
  std::vector<GURL> unused_preloads = PredictUnusedPreloads(lcpp_stat);

  if (!lcp_element_locators.empty() || !lcp_influencer_scripts.empty() ||
      !fetched_fonts.empty() || !preconnect_origins.empty() ||
      !unused_preloads.empty()) {
    return blink::mojom::LCPCriticalPathPredictorNavigationTimeHint(
        std::move(lcp_element_locators), std::move(lcp_influencer_scripts),
        std::move(fetched_fonts), std::move(preconnect_origins),
        std::move(unused_preloads));
  }
  return std::nullopt;
}

std::vector<GURL> PredictFetchedFontUrls(const LcppStat& stat) {
  if (!base::FeatureList::IsEnabled(blink::features::kLCPPFontURLPredictor)) {
    return std::vector<GURL>();
  }
  std::vector<std::pair<double, std::string>> font_urls_with_frequency =
      ConvertToFrequencyStringPair(stat.fetched_font_url_stat());

  const double threshold =
      blink::features::kLCPPFontURLPredictorFrequencyThreshold.Get();
  int num_open_spots =
      blink::features::kLCPPFontURLPredictorMaxPreloadCount.Get();
  if (num_open_spots <= 0) {
    return std::vector<GURL>();
  }

  std::vector<GURL> font_urls;
  for (const auto& [frequency, font_url] : font_urls_with_frequency) {
    // The frequencies are reverse sorted by `ConvertToFrequencyStringPair`.
    // No need to see later frequencies if the frequency is smaller than the
    // threshold.
    if (frequency < threshold) {
      break;
    }
    GURL parsed_url(font_url);
    if (!parsed_url.is_valid() || !parsed_url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    font_urls.emplace_back(std::move(parsed_url));
    if (--num_open_spots <= 0) {
      break;
    }
  }
  if (font_urls.empty()) {
    return font_urls;
  }

  // No need to record metrics for pages without web fonts to be prefetched
  // or preloaded.
  double max_bandwidth_mbps;
  net::NetworkChangeNotifier::ConnectionType connection_type;
  net::NetworkChangeNotifier::GetMaxBandwidthAndConnectionType(
      &max_bandwidth_mbps, &connection_type);
  if (blink::features::kLCPPFontURLPredictorThresholdInMbps.Get() > 0 &&
      (connection_type ==
           net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN ||
       max_bandwidth_mbps <
           blink::features::kLCPPFontURLPredictorThresholdInMbps.Get())) {
    base::UmaHistogramEnumeration(
        "Blink.LCPP.FontFetch.Disabled.ConnectionType", connection_type,
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_LAST);
    return std::vector<GURL>();
  }
  // Workaround: we cannot use UmaHistogramEnumeration because
  // connection_type is defined with old C enum, and setting kValue causes
  // namespace conflict.
  base::UmaHistogramEnumeration(
      "Blink.LCPP.FontFetch.Enabled.ConnectionType", connection_type,
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_LAST);
  return font_urls;
}

std::vector<GURL> PredictPreconnectableOrigins(const LcppStat& stat) {
  std::vector<std::pair<double, std::string>>
      preconnect_origins_with_frequency =
          ConvertToFrequencyStringPair(stat.preconnect_origin_stat());

  const double frequency_threshold =
      blink::features::kLCPPAutoPreconnectFrequencyThreshold.Get();
  int preconnects_allowed =
      blink::features::kkLCPPAutoPreconnectMaxPreconnectOriginsCount.Get();
  if (preconnects_allowed <= 0) {
    return std::vector<GURL>();
  }

  std::vector<GURL> preconnect_origins;
  for (const auto& [frequency, preconnect_url] :
       preconnect_origins_with_frequency) {
    // The frequencies are reverse sorted by `ConvertToFrequencyStringPair`.
    // No need to see later frequencies if the frequency is smaller than the
    // frequency_threshold.
    if (frequency < frequency_threshold) {
      break;
    }
    GURL parsed_url(preconnect_url);
    if (!parsed_url.is_valid() || !parsed_url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    preconnect_origins.emplace_back(std::move(parsed_url));
    if (--preconnects_allowed <= 0) {
      break;
    }
  }
  return preconnect_origins;
}

std::vector<GURL> PredictFetchedSubresourceUrls(const LcppStat& stat) {
  std::vector<GURL> subresource_urls;
  for (const auto& [frequency, subresource_url] :
       ConvertToFrequencyStringPair(stat.fetched_subresource_url_stat())) {
    GURL parsed_url(subresource_url);
    if (!parsed_url.is_valid() || !parsed_url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    subresource_urls.push_back(std::move(parsed_url));
  }
  return subresource_urls;
}

std::vector<GURL> PredictUnusedPreloads(const LcppStat& stat) {
  const double frequency_threshold =
      blink::features::kLCPPDeferUnusedPreloadFrequencyThreshold.Get();
  std::vector<GURL> unused_preloads;
  if (!base::FeatureList::IsEnabled(blink::features::kLCPPDeferUnusedPreload)) {
    return unused_preloads;
  }

  for (const auto& [frequency, url] :
       ConvertToFrequencyStringPair(stat.unused_preload_stat())) {
    // The frequencies are reverse sorted by `ConvertToFrequencyStringPair`.
    if (frequency < frequency_threshold) {
      break;
    }
    GURL parsed_url(url);
    if (!parsed_url.is_valid() || !parsed_url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    unused_preloads.push_back(std::move(parsed_url));
  }
  return unused_preloads;
}

LcppDataInputs::LcppDataInputs() = default;
LcppDataInputs::~LcppDataInputs() = default;

bool UpdateLcppStatWithLcppDataInputs(const LoadingPredictorConfig& config,
                                      const LcppDataInputs& inputs,
                                      LcppStat& stat) {
  bool data_updated = false;
  data_updated |= RecordLcpElementLocatorHistogram(
      config, inputs.lcp_element_locator, stat);
  data_updated |= RecordLcpInfluencerScriptUrlsHistogram(
      config, inputs.lcp_influencer_scripts, stat);
  data_updated |=
      RecordFetchedFontUrlsHistogram(config, inputs.font_urls, stat);
  data_updated |= RecordFetchedSubresourceUrlsHistogram(
      config, inputs.subresource_urls, stat);
  data_updated |=
      RecordPreconnectOriginsHistogram(config, inputs.preconnect_origins, stat);
  data_updated |= RecordUnusedPreloadUrlsHistogram(
      config, inputs.unused_preload_resources, stat);
  base::UmaHistogramCounts10000("Blink.LCPP.ReportedFontCount",
                                base::checked_cast<int>(inputs.font_url_count));
  if (inputs.font_url_count > 0 && inputs.font_urls.size() > 0) {
    base::UmaHistogramCounts10000(
        "Blink.LCPP.RecordedFontUrlHitCountForPagesWithFonts",
        base::checked_cast<int>(inputs.font_url_hit_count));
    base::UmaHistogramPercentage(
        "Blink.LCPP.RecordedFontUrlPredictionHitPercent",
        base::checked_cast<int>(100 * inputs.font_url_hit_count /
                                inputs.font_url_count));
    base::UmaHistogramPercentage(
        "Blink.LCPP.RecordedFontUrlPredictionHitPercentInRecordedFonts",
        base::checked_cast<int>(100 * inputs.font_url_hit_count /
                                inputs.font_urls.size()));
    base::UmaHistogramCounts10000(
        "Blink.LCPP.RecordedFontUrlReenterCountForPagesWithFonts",
        base::checked_cast<int>(inputs.font_url_reenter_count));
    base::UmaHistogramPercentage(
        "Blink.LCPP.RecordedFontUrlReenterPercentInRecordedFonts",
        base::checked_cast<int>(100 * inputs.font_url_reenter_count /
                                inputs.font_urls.size()));
    base::UmaHistogramCounts10000(
        "Blink.LCPP.CrossSiteFontUrls",
        base::checked_cast<int>(inputs.cross_site_font_url_count));
    base::UmaHistogramCounts10000(
        "Blink.LCPP.SameSiteFontUrls",
        base::checked_cast<int>(inputs.same_site_font_url_count));
    CHECK_GT(inputs.same_site_font_url_count + inputs.cross_site_font_url_count,
             0UL);
    base::UmaHistogramPercentage(
        "Blink.LCPP.SameSiteFontUrlRatio",
        base::checked_cast<int>(100 * inputs.same_site_font_url_count /
                                (inputs.same_site_font_url_count +
                                 inputs.cross_site_font_url_count)));
  }
  return data_updated;
}

void UpdateLcppStringFrequencyStatData(
    size_t sliding_window_size,
    size_t max_histogram_buckets,
    const std::string& new_entry,
    LcppStringFrequencyStatData& lcpp_stat_data,
    std::optional<std::string>& dropped_entry) {
  dropped_entry = std::nullopt;
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
          sliding_window_size, max_histogram_buckets, lcpp_stat_data);
  updater->Update(new_entry);
  lcpp_stat_data = updater->ToLcppStringFrequencyStatData();
  if (auto dropped_entries = updater->dropped_entries();
      !dropped_entries.empty()) {
    CHECK_EQ(dropped_entries.size(), 1U);
    dropped_entry = dropped_entries.back();
  }
}

bool IsValidLcppStat(const LcppStat& lcpp_stat) {
  if (lcpp_stat.has_lcp_element_locator_stat() &&
      !IsValidLcpElementLocatorHistogram(
          lcpp_stat.lcp_element_locator_stat())) {
    return false;
  }
  if (lcpp_stat.has_lcp_script_url_stat() &&
      !IsValidLcpUrlsHistogram(lcpp_stat.lcp_script_url_stat())) {
    return false;
  }
  if (lcpp_stat.has_fetched_font_url_stat() &&
      !IsValidLcpUrlsHistogram(lcpp_stat.fetched_font_url_stat())) {
    return false;
  }
  if (lcpp_stat.has_fetched_subresource_url_stat() &&
      !IsValidLcpUrlsHistogram(lcpp_stat.fetched_subresource_url_stat())) {
    return false;
  }
  if (lcpp_stat.has_preconnect_origin_stat() &&
      !IsValidLcpUrlsHistogram(lcpp_stat.preconnect_origin_stat())) {
    return false;
  }
  if (lcpp_stat.has_unused_preload_stat() &&
      !IsValidLcpUrlsHistogram(lcpp_stat.unused_preload_stat())) {
    return false;
  }
  return true;
}

bool IsURLValidForLcpp(const GURL& url) {
  return url.is_valid() && !url.host().empty() && !net::IsLocalhost(url) &&
         url.SchemeIsHTTPOrHTTPS() &&
         url.host().size() <= ResourcePrefetchPredictorTables::kMaxStringLength;
}

std::string GetFirstLevelPath(const GURL& url) {
  CHECK(IsURLValidForLcpp(url));

  const std::string path = url.path();
  if (path.length() < 2) {  // path == "/"
    return std::string();
  }
  // Say path is "/foo/baz", find second '/' to cut out the first level path
  // "/foo".
  const size_t max_path_length = GetLCPPMultipleKeyMaxPathLength();
  // Say `max_path_length` is 6, create a string view "/abcdef". If 'f' is
  // slash, "/abcde" is the first level path but if 'f' is not, the path is
  // longer than `max_path_length`.
  std::string_view path_view(path.data(),
                             std::min(path.length(), max_path_length + 1));
  const size_t second_slash_pos = path_view.find('/', 1);
  size_t first_level_path_length;
  if (second_slash_pos == std::string::npos) {
    if (url.ExtractFileName().find('.') != std::string::npos ||
        path_view.length() == max_path_length + 1) {
      // Assume having a file extension is a file and
      // path should not be a file name nor exceed the length limit
      return std::string();
    }
    first_level_path_length = path_view.length();
  } else {
    first_level_path_length = second_slash_pos;
  }
  return url.path().substr(0, first_level_path_length);
}

LcppDataMap::LcppDataMap(scoped_refptr<sqlite_proto::TableManager> manager,
                         const LoadingPredictorConfig& config)
    : LcppDataMap(std::move(manager),
                  config,
                  std::make_unique<DataTable>(std::string(kLcppTableName))) {}

LcppDataMap::LcppDataMap(scoped_refptr<sqlite_proto::TableManager> manager,
                         const LoadingPredictorConfig& config,
                         std::unique_ptr<DataTable> data_table)
    : manager_(manager),
      config_(config),
      data_table_(std::move(data_table)),
      data_map_(std::make_unique<DataMap>(
          manager,
          data_table_.get(),
          config.max_hosts_to_track_for_lcpp,
          base::Seconds(config.flush_data_to_disk_delay_seconds))) {
  if (IsInitiatorOriginEnabled()) {
    origin_table_ = std::make_unique<OriginTable>(
        std::string(kLcppTableNameInitiatorOrigin));
    origin_map_ = std::make_unique<OriginMap>(
        manager, origin_table_.get(), config.max_hosts_to_track_for_lcpp,
        base::Seconds(config.flush_data_to_disk_delay_seconds));
  }
}

std::unique_ptr<LcppDataMap> LcppDataMap::CreateWithMockTableForTesting(
    scoped_refptr<sqlite_proto::TableManager> manager,
    const LoadingPredictorConfig& config) {
  return base::WrapUnique(new LcppDataMap(
      manager, config, std::make_unique<FakeLoadingPredictorKeyValueTable>()));
}

LcppDataMap::~LcppDataMap() {
  // sqlite_proto::KeyValueTable<LcppData> should be deleted on DB thread.
  // See components/sqlite_proto/key_value_data.h for detail.
  manager_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&DeleteTables, std::move(data_table_),
                                std::move(origin_table_)));
}

void LcppDataMap::InitializeOnDBSequence() {
  data_map_->InitializeOnDBSequence();
  if (IsInitiatorOriginEnabled()) {
    origin_map_->InitializeOnDBSequence();
    for (const auto& it : origin_map_->GetAllCached()) {
      const std::string& key = it.first;
      LcppOrigin lcpp_origin = it.second;
      const bool is_canonicalized = CanonicalizeFrequencyData(
          config_.lcpp_initiator_origin_max_histogram_buckets,
          *lcpp_origin.mutable_key_frequency_stat(),
          *lcpp_origin.mutable_origin_data_map());
      if (is_canonicalized) {
        needs_update_on_initialize_[key] = std::move(lcpp_origin);
      }
    }
  }
}

void LcppDataMap::InitializeAfterDBInitialization() {
  if (IsInitiatorOriginEnabled()) {
    for (const auto& it : needs_update_on_initialize_) {
      origin_map_->UpdateData(it.first, it.second);
    }
  }
  initialized_ = true;
}

// Record LCP element locators after a page has finished loading and LCP has
// been determined.
bool LcppDataMap::LearnLcpp(const std::optional<url::Origin>& initiator_origin,
                            const GURL& url,
                            const LcppDataInputs& inputs) {
  CHECK(initialized_);
  if (!IsURLValidForLcpp(url)) {
    return false;
  }
  const std::string key = GetLCPPDatabaseKey(url);
  LcppData* lcpp_data;
  LcppData lcpp_data_body;
  LcppOrigin lcpp_origin;
  const bool use_origin_map = IsInitiatorOriginEnabled() && initiator_origin;
  if (use_origin_map) {
    if (initiator_origin->host().size() >
        ResourcePrefetchPredictorTables::kMaxStringLength) {
      return false;
    }
    origin_map_->TryGetData(key, &lcpp_origin);
    lcpp_origin.set_last_visit_time(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
    lcpp_data = UpdateFrequencyStatAndTryGetEntry(
        config_.lcpp_initiator_origin_histogram_sliding_window_size,
        config_.lcpp_initiator_origin_max_histogram_buckets,
        initiator_origin->host(), *lcpp_origin.mutable_key_frequency_stat(),
        *lcpp_origin.mutable_origin_data_map());
    if (!lcpp_data) {
      origin_map_->UpdateData(key, lcpp_origin);
      return false;
    }
  } else {
    bool exists = data_map_->TryGetData(key, &lcpp_data_body);
    lcpp_data_body.set_last_visit_time(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

    if (!exists) {
      lcpp_data_body.set_host(key);
    }
    lcpp_data = &lcpp_data_body;
  }
  CHECK(lcpp_data);

  if (!IsLcppMultipleKeyKeyStatEnabled()) {
    lcpp_data->mutable_lcpp_key_stat()->Clear();
  }

  bool data_updated = false;
  LcppStat* lcpp_stat =
      IsLcppMultipleKeyKeyStatEnabled()
          ? TryToGetLcppStatForKeyStat(config_, url, *lcpp_data, data_updated)
          : lcpp_data->mutable_lcpp_stat();
  if (lcpp_stat) {
    if (!IsValidLcppStat(*lcpp_stat)) {
      lcpp_stat->Clear();
      base::UmaHistogramBoolean("LoadingPredictor.LcppStatCorruptedAtLearnTime",
                                true);
    }
    data_updated |=
        UpdateLcppStatWithLcppDataInputs(config_, inputs, *lcpp_stat);
    if (IsLCPPFontPrefetchExcludedHost(url) &&
        lcpp_stat->has_fetched_font_url_stat()) {
      lcpp_stat->clear_fetched_font_url_stat();
      data_updated = true;
    }
    DCHECK(IsValidLcppStat(*lcpp_stat));
  }
  if (use_origin_map) {
    // `origin_map` needs always update due to updating the frequency stat.
    origin_map_->UpdateData(key, lcpp_origin);
  } else {
    if (data_updated) {
      data_map_->UpdateData(key, *lcpp_data);
    }
  }

  return data_updated;
}

// Returns LcppStat for the `url`, or std::nullopt on failure.
std::optional<LcppStat> LcppDataMap::GetLcppStat(
    const std::optional<url::Origin>& initiator_origin,
    const GURL& url) const {
  CHECK(initialized_);
  if (!IsURLValidForLcpp(url)) {
    return std::nullopt;
  }
  const std::string key = GetLCPPDatabaseKey(url);

  const LcppData* lcpp_data;
  LcppData lcpp_data_body;
  LcppOrigin lcpp_origin;
  const bool use_origin_map = IsInitiatorOriginEnabled() && initiator_origin;
  if (use_origin_map) {
    if (initiator_origin->host().size() >
        ResourcePrefetchPredictorTables::kMaxStringLength) {
      return std::nullopt;
    }
    if (!origin_map_->TryGetData(key, &lcpp_origin)) {
      return std::nullopt;
    }
    const auto& origin_data_map = lcpp_origin.origin_data_map();
    auto it = origin_data_map.find(initiator_origin->host());
    if (it == origin_data_map.end()) {
      return std::nullopt;
    }
    lcpp_data = &it->second;
  } else {
    if (!data_map_->TryGetData(key, &lcpp_data_body)) {
      return std::nullopt;
    }
    lcpp_data = &lcpp_data_body;
  }
  CHECK(lcpp_data);

  if (IsLcppMultipleKeyKeyStatEnabled()) {
    const std::string first_level_path = GetFirstLevelPath(url);
    if (first_level_path.empty() ||
        !IsKeyLengthValidForMultipleKey(url.host(), first_level_path)) {
      return lcpp_data->lcpp_stat();
    }
    const auto& lcpp_stat_map = lcpp_data->lcpp_key_stat().lcpp_stat_map();
    if (auto flp_stat = lcpp_stat_map.find(first_level_path);
        flp_stat != lcpp_stat_map.end()) {
      return flp_stat->second;
    }
    return std::nullopt;
  }
  return lcpp_data->lcpp_stat();
}

void LcppDataMap::DeleteUrls(const std::vector<GURL>& urls) {
  std::vector<std::string> keys_to_delete;
  std::vector<std::string> hosts_to_delete;
  for (const GURL& url : urls) {
    if (!IsURLValidForLcpp(url)) {
      continue;
    }

    const std::string key = GetLCPPDatabaseKey(url);
    keys_to_delete.emplace_back(key);
    if (IsInitiatorOriginEnabled()) {
      hosts_to_delete.push_back(url::Origin::Create(url).host());
    }
  }
  data_map_->DeleteData(keys_to_delete);
  if (IsInitiatorOriginEnabled()) {
    origin_map_->DeleteData(keys_to_delete);
    // Delete LcppOrigin which `origin_data_map` has hosts in `host_to_delete`.
    std::map<std::string, LcppOrigin> needs_update;
    for (auto& key_value : origin_map_->GetAllCached()) {
      LcppOrigin lcpp_origin = key_value.second;
      bool updated = false;
      for (const auto& host : hosts_to_delete) {
        auto& origin_data_map = *lcpp_origin.mutable_origin_data_map();
        bool origin_found = false;
        if (auto it = origin_data_map.find(host); it != origin_data_map.end()) {
          origin_data_map.erase(it);
          updated = true;
          origin_found = true;
        }
        auto& main_buckets =
            *lcpp_origin.mutable_key_frequency_stat()->mutable_main_buckets();
        bool bucket_found = false;
        if (auto it = main_buckets.find(host); it != main_buckets.end()) {
          main_buckets.erase(it);
          bucket_found = true;
        }
        if (origin_found != bucket_found) {
          LOG(ERROR) << "LcppOrigin for " << key_value.first << " is corrupted";
        }
      }
      if (updated) {
        needs_update[key_value.first] = lcpp_origin;
      }
    }
    for (auto it : needs_update) {
      origin_map_->UpdateData(it.first, it.second);
    }
  }
}

void LcppDataMap::DeleteAllData() {
  data_map_->DeleteAllData();
  if (IsInitiatorOriginEnabled()) {
    origin_map_->DeleteAllData();
  }
}

const std::map<std::string, LcppData>& LcppDataMap::GetAllCachedForTesting() {
  return data_map_->GetAllCached();
}
const std::map<std::string, LcppOrigin>&
LcppDataMap::GetAllCachedOriginForTesting() {
  CHECK(IsInitiatorOriginEnabled());
  return origin_map_->GetAllCached();
}

bool LcppDataMap::CreateOrClearTablesIfNecessary(sql::Database* db) {
  const bool result = EnsureTable(db, kLcppTableName);
  if (IsInitiatorOriginEnabled()) {
    return result && EnsureTable(db, kLcppTableNameInitiatorOrigin);
  }
  return result && db->Execute(base::StringPrintf(
                       "DROP TABLE IF EXISTS %s",
                       std::string(kLcppTableNameInitiatorOrigin).c_str()));
}

}  // namespace predictors
