// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "third_party/blink/public/common/features.h"

namespace predictors {

namespace {

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

// Returns LCP element locators in the past loads for a given `data`.  The
// returned LCP element locators are ordered by descending frequency (the
// most frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<std::string> PredictLcpElementLocators(const LcppData& data) {
  // We do not use `ConvertToFrequencyStringPair` for the following code
  // because the core part of the code is converting `std::map` to
  // `std::vector<std::pair<double, std::string>>`, which we need the different
  // logic due to the `bytes` protobuf type.
  const auto& buckets =
      data.lcpp_stat().lcp_element_locator_stat().lcp_element_locator_buckets();
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

// Returns LCP influencer scripts from past loads for a given `data`.
// The returned script urls are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictLcpInfluencerScripts(const LcppData& data) {
  std::vector<std::pair<double, std::string>> lcp_script_urls_with_frequency =
      ConvertToFrequencyStringPair(data.lcpp_stat().lcp_script_url_stat());

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
//     config, *data.mutable_lcpp_stat()->mutable_lcp_script_url_stat());
// // Update.
// updater.Update(url);
// // Extract.
// *data.mutable_lcpp_stat()->mutable_lcp_script_url_stat() =
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
      const LoadingPredictorConfig& config,
      const LcppStringFrequencyStatData& lcpp_stat_data) {
    // Prepare working variables (histogram and other_bucket_frequency) from
    // proto. If the data is corrupted, the previous data will be cleared.
    bool corrupted = false;
    double other_bucket_frequency = lcpp_stat_data.other_bucket_frequency();
    if (other_bucket_frequency < 0 || lcpp_stat_data.main_buckets().size() >
                                          config.max_lcpp_histogram_buckets) {
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
        config, histogram, other_bucket_frequency));
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
        config, histogram, other_bucket_frequency));
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

  size_t num_matched() const { return num_matched_; }

 private:
  LcppFrequencyStatDataUpdater(const LoadingPredictorConfig& config,
                               std::map<std::string, double> histogram,
                               double other_bucket_frequency)
      : sliding_window_size_(config.lcpp_histogram_sliding_window_size),
        max_histogram_buckets_(config.max_lcpp_histogram_buckets),
        histogram_(histogram),
        other_bucket_frequency_(other_bucket_frequency) {}

  const size_t sliding_window_size_;
  const size_t max_histogram_buckets_;
  std::map<std::string, double> histogram_;
  double other_bucket_frequency_;
  bool has_updated_ = false;
  size_t num_matched_ = 0;
};

bool RecordLcpElementLocatorHistogram(const LoadingPredictorConfig& config,
                                      const std::string& lcp_element_locator,
                                      LcppData& data) {
  if (lcp_element_locator.size() >
          ResourcePrefetchPredictorTables::kMaxStringLength ||
      lcp_element_locator.empty()) {
    return false;
  }
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcpElementLocatorStat(
          config, data.mutable_lcpp_stat()->lcp_element_locator_stat());
  CHECK(updater);
  updater->Update(lcp_element_locator);
  *data.mutable_lcpp_stat()->mutable_lcp_element_locator_stat() =
      updater->ToLcpElementLocatorStat();
  return true;
}

bool RecordLcpInfluencerScriptUrlsHistogram(
    const LoadingPredictorConfig& config,
    const std::vector<GURL>& lcp_influencer_scripts,
    LcppData& data) {
  // Contrasting to LCPP Element locator, there are multiple LCP dependency URLs
  // for an origin. Record each in a separate histogram.
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
          config, data.mutable_lcpp_stat()->lcp_script_url_stat());
  CHECK(updater);
  for (auto& script_url : lcp_influencer_scripts) {
    const auto& lcpp_script = script_url.spec();
    if (!IsValidUrlInLcppStringFrequencyStatData(lcpp_script)) {
      continue;
    }
    updater->Update(lcpp_script);
  }
  *data.mutable_lcpp_stat()->mutable_lcp_script_url_stat() =
      updater->ToLcppStringFrequencyStatData();
  return updater->has_updated();
}

bool RecordFetchedFontUrlsHistogram(const LoadingPredictorConfig& config,
                                    const std::vector<GURL>& fetched_font_urls,
                                    LcppData& data) {
  // Due to LCPP data structure, histogram is saved per origin.
  // Therefore, it sounds better to have this as a histogram instead of
  // a static data.
  std::unique_ptr<LcppFrequencyStatDataUpdater> updater =
      LcppFrequencyStatDataUpdater::FromLcppStringFrequencyStatData(
          config, data.mutable_lcpp_stat()->fetched_font_url_stat());
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
  *data.mutable_lcpp_stat()->mutable_fetched_font_url_stat() =
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
    base::UmaHistogramPercentage(
        "Blink.LCPP.RecordedFontUrlPredictionMatchPercent",
        base::checked_cast<int>(100 * updater->num_matched() /
                                fetched_font_urls.size()));
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

}  // namespace

absl::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
ConvertLcppDataToLCPCriticalPathPredictorNavigationTimeHint(
    const LcppData& lcpp_data) {
  std::vector<std::string> lcp_element_locators =
      PredictLcpElementLocators(lcpp_data);
  std::vector<GURL> lcp_influencer_scripts =
      PredictLcpInfluencerScripts(lcpp_data);
  std::vector<GURL> fetched_fonts = PredictFetchedFontUrls(lcpp_data);

  if (!lcp_element_locators.empty() || !lcp_influencer_scripts.empty() ||
      !fetched_fonts.empty()) {
    return blink::mojom::LCPCriticalPathPredictorNavigationTimeHint(
        std::move(lcp_element_locators), std::move(lcp_influencer_scripts),
        std::move(fetched_fonts));
  }
  return absl::nullopt;
}

std::vector<GURL> PredictFetchedFontUrls(const LcppData& data) {
  std::vector<std::pair<double, std::string>> font_urls_with_frequency =
      ConvertToFrequencyStringPair(data.lcpp_stat().fetched_font_url_stat());

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
  return font_urls;
}

LcppDataInputs::LcppDataInputs() = default;
LcppDataInputs::~LcppDataInputs() = default;

bool UpdateLcppDataWithLcppDataInputs(const LoadingPredictorConfig& config,
                                      const LcppDataInputs& inputs,
                                      LcppData& data) {
  bool data_updated = false;
  data_updated |= RecordLcpElementLocatorHistogram(
      config, inputs.lcp_element_locator, data);
  data_updated |= RecordLcpInfluencerScriptUrlsHistogram(
      config, inputs.lcp_influencer_scripts, data);
  data_updated |=
      RecordFetchedFontUrlsHistogram(config, inputs.font_urls, data);
  base::UmaHistogramCounts10000("Blink.LCPP.ReportedFontCount",
                                base::checked_cast<int>(inputs.font_url_count));
  return data_updated;
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
  return true;
}

}  // namespace predictors
