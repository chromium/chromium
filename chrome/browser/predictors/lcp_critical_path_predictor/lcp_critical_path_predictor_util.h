// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_

#include <optional>

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor.pb.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace predictors {
namespace lcpp {
struct LastVisitTimeCompare {
  template <typename T>
  bool operator()(const T& lhs, const T& rhs) const {
    return lhs.last_visit_time() < rhs.last_visit_time();
  }
};

}  // namespace lcpp

// Converts LcppStat to LCPCriticalPathPredictorNavigationTimeHint
// so that it can be passed to the renderer via the navigation handle.
std::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
ConvertLcppStatToLCPCriticalPathPredictorNavigationTimeHint(
    const LcppStat& data);

// Returns possible fonts from past loads for a given `stat`.
// The returned urls are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictFetchedFontUrls(const LcppStat& stat);

// Returns possible preconnects based on past loads for a given `stat`.
// The returned origins are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictPreconnectableOrigins(const LcppStat& stat);

// Returns possible subresource URLs from past loads for a given `stat`.
// The returned URLs are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictFetchedSubresourceUrls(const LcppStat& stat);

// Returns possible unused preload URLs from past loads for a given `stat`.
// The returned URLs are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictUnusedPreloads(const LcppStat& stat);

// An input to update LcppData.
struct LcppDataInputs {
  LcppDataInputs();
  ~LcppDataInputs();

  // LCPP write path [1]: Staging area of the proto3 serialized element locator
  // of the latest LCP candidate element. [1]
  // https://docs.google.com/document/d/1waakt6bSvedWdaUQ2mC255NF4k8j7LybK2dQ7WptxiE/edit#heading=h.hy4g58pyf548
  // We do not know the final data to be serialized from the beginning.
  // They are updated on each LCP candidate. We record the data we had at the
  // `FinalizeLCP` timing as the representative, since they should have the
  // data of the LCP candidate that won.
  //
  // a locator of the LCP element.
  std::string lcp_element_locator;
  // async script urls of the latest LCP candidate element.
  std::vector<GURL> lcp_influencer_scripts;
  std::vector<GURL> preconnect_origins;

  // Fetched font URLs.
  // Unlike data above, the field will be updated per font fetch.
  // The number of URLs in the vector is up to the size defined by
  // `kLCPPFontURLPredictorMaxUrlCountPerOrigin`.
  std::vector<GURL> font_urls;
  // This field keeps the number of font URLs without omitting due to
  // reaching `kLCPPFontURLPredictorMaxUrlCountPerOrigin` or deduplication.
  size_t font_url_count = 0;
  // This field keeps the number of preloaded font hit. i.e. it is incremented
  // if the fetched font URL is listed in the list of predicted fonts.
  size_t font_url_hit_count = 0;
  // This field keeps the number of same-site font URLs.
  size_t same_site_font_url_count = 0;
  // This field keeps the number of cross-site font URLs.
  size_t cross_site_font_url_count = 0;
  // This field keeps the number of preloaded font that is going to be recorded
  // to the database again.
  size_t font_url_reenter_count = 0;
  // This field keeps the subresource URLs as a key, and the TimeDelta and
  // destination as a value. TimeDelta stores the duration from navigation
  // start to resource loading start time.
  std::map<GURL, std::pair<base::TimeDelta, network::mojom::RequestDestination>>
      subresource_urls;

  // URLs of preloaded but not actually used resources.
  std::vector<GURL> unused_preload_resources;
};

bool UpdateLcppStatWithLcppDataInputs(const LoadingPredictorConfig& config,
                                      const LcppDataInputs& inputs,
                                      LcppStat& stat);

// Update `lcpp_stat_data` adding `new_entry` with `sliding_window_size` and
// `max_histogram_buckets` parameters by the top-k algorithm.
// See lcp_critical_path_predictor_util.cc for detail.
// `dropped_entry` is assigned if this updating dropped an existing entry.
void UpdateLcppStringFrequencyStatData(
    size_t sliding_window_size,
    size_t max_histogram_buckets,
    const std::string& new_entry,
    LcppStringFrequencyStatData& lcpp_stat_data,
    std::optional<std::string>& dropped_entry);

// Update `lcpp_stat_data` adding `new_entry` with `sliding_window_size` and
// `max_histogram_buckets` parameters by the top-k algorithm while
// keeping `map` have same keys in `lcpp_stat_data`.
// See lcp_critical_path_predictor_util.cc for detail.
template <typename T>
T* UpdateFrequencyStatAndTryGetEntry(
    size_t sliding_window_size,
    size_t max_histogram_buckets,
    const std::string& new_entry,
    LcppStringFrequencyStatData& frequency_stat,
    google::protobuf::Map<std::string, T>& map) {
  std::optional<std::string> dropped_entry;
  UpdateLcppStringFrequencyStatData(sliding_window_size, max_histogram_buckets,
                                    new_entry, frequency_stat, dropped_entry);
  // Since UpdateLcppStringFrequencyStatData modifies a part of `data`,
  // caller should update the stored data if the function is called.
  if (dropped_entry) {
    if (*dropped_entry == new_entry) {
      // This means `frequency_stat` is already full of well-used other
      // first-level-path entries.
      // However since the frequency map is updated, we need to update
      // root `data` too via `data_updated` flag.
      return nullptr;
    } else {
      map.erase(*dropped_entry);
    }
  }
  return &(map[new_entry]);
}

// Aligns `frequency_stat` elements and `map` elements.
// Clears both if `frequency_stat` has invalid parameters too.
template <typename T>
bool CanonicalizeFrequencyData(size_t max_histogram_buckets,
                               LcppStringFrequencyStatData& frequency_stat,
                               google::protobuf::Map<std::string, T>& map) {
  bool is_canonicalized = false;
  auto* frequency_main_buckets = frequency_stat.mutable_main_buckets();
  std::vector<std::string> remove_from_map;
  for (const auto& it : map) {
    if (auto pos = frequency_main_buckets->find(it.first);
        pos == frequency_main_buckets->end()) {
      remove_from_map.push_back(it.first);
    }
  }
  for (std::string& str : remove_from_map) {
    map.erase(str);
  }
  is_canonicalized |= !remove_from_map.empty();

  std::vector<std::string> remove_from_frequency_stat;
  for (const auto& it : *frequency_main_buckets) {
    if (auto pos = map.find(it.first); pos == map.end()) {
      remove_from_frequency_stat.push_back(it.first);
    }
  }
  for (std::string& str : remove_from_frequency_stat) {
    frequency_main_buckets->erase(str);
  }
  is_canonicalized |= !remove_from_frequency_stat.empty();
  CHECK_EQ(frequency_main_buckets->size(), map.size());

  if (frequency_stat.other_bucket_frequency() < 0 ||
      frequency_stat.main_buckets().size() > max_histogram_buckets) {
    frequency_stat.Clear();
    map.clear();
    is_canonicalized = true;
  }
  return is_canonicalized;
}

// Returns true if the LcppData is valid. i.e. looks not corrupted.
// Otherwise, data might be corrupted.
bool IsValidLcppStat(const LcppStat& lcpp_stat);

// Returns true if the url is valid for learning.
bool IsURLValidForLcpp(const GURL& url);

// Returns the first level path of the url. The url should be true for
// the above IsURLValidForLcpp(url).
// This function can return empty string if the URL doesn't have
// the first level path or it length exceeds kLCPPMultipleKeyMaxPathLength.
std::string GetFirstLevelPath(const GURL& url);

class LcppDataMap {
 public:
  using DataTable = sqlite_proto::KeyValueTable<LcppData>;
  using DataMap =
      sqlite_proto::KeyValueData<LcppData, lcpp::LastVisitTimeCompare>;
  using OriginTable = sqlite_proto::KeyValueTable<LcppOrigin>;
  using OriginMap =
      sqlite_proto::KeyValueData<LcppOrigin, lcpp::LastVisitTimeCompare>;

  LcppDataMap(scoped_refptr<sqlite_proto::TableManager> manager,
              const LoadingPredictorConfig& config);
  ~LcppDataMap();
  LcppDataMap(const LcppDataMap&) = delete;

  static bool CreateOrClearTablesIfNecessary(sql::Database* db);

  void InitializeOnDBSequence();
  void InitializeAfterDBInitialization();

  // Record LCP element locators after a page has finished loading and LCP has
  // been determined.
  // Returns true if it was updated.
  bool LearnLcpp(const std::optional<url::Origin>& initiator_origin,
                 const GURL& url,
                 const LcppDataInputs& inputs);

  // Returns LcppStat for the `url`, or std::nullopt on failure.
  std::optional<LcppStat> GetLcppStat(
      const std::optional<url::Origin>& initiator_origin,
      const GURL& url) const;

  void DeleteUrls(const std::vector<GURL>& urls);

  void DeleteAllData();

  LcppDataMap(scoped_refptr<sqlite_proto::TableManager> manager,
              const LoadingPredictorConfig& config,
              std::unique_ptr<DataTable> data_table_);
  static std::unique_ptr<LcppDataMap> CreateWithMockTableForTesting(

      scoped_refptr<sqlite_proto::TableManager> manager,
      const LoadingPredictorConfig& config);

 private:
  friend class LcppDataMapTest;
  friend class LcppInitiatorOriginTest;
  const std::map<std::string, LcppData>& GetAllCachedForTesting();
  const std::map<std::string, LcppOrigin>& GetAllCachedOriginForTesting();

  scoped_refptr<sqlite_proto::TableManager> manager_;
  const LoadingPredictorConfig config_;
  std::unique_ptr<DataTable> data_table_;
  std::unique_ptr<DataMap> data_map_;
  std::unique_ptr<OriginTable> origin_table_;
  std::unique_ptr<OriginMap> origin_map_;
  // This member is accessed from both the db thread (InitializeOnDBSequence)
  // and the UI thread (ResourcePrefetchPredictor::CreateCaches ->
  // InitializeAfterDBInitialize).
  // This is accessed from each thread only once and the order is guaranteed.
  // TODO(crbug.com/353548219): Consider more better structure.
  std::map<std::string, LcppOrigin> needs_update_on_initialize_;
  bool initialized_ = false;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_
