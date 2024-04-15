// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_

#include <optional>

#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.pb.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"

namespace predictors {

// Converts LcppData to LCPCriticalPathPredictorNavigationTimeHint
// so that it can be passed to the renderer via the navigation handle.
std::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
ConvertLcppDataToLCPCriticalPathPredictorNavigationTimeHint(
    const LcppData& data);

// Returns possible fonts from past loads for a given `data`.
// The returned urls are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictFetchedFontUrls(const LcppData& data);

// Returns possible preconnects based on past loads for a given `data`.
// The returned origins are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictPreconnectableOrigins(const LcppData& data);

// Returns possible subresource URLs from past loads for a given `data`.
// The returned URLs are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictFetchedSubresourceUrls(const LcppData& data);

// Returns possible unused preload URLs from past loads for a given `data`.
// The returned URLs are ordered by descending frequency (the most
// frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<GURL> PredictUnusedPreloads(const LcppData& data);

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
  // This field keeps the number of preloaded font that is going to be recorded
  // to the database again.
  size_t font_url_reenter_count = 0;
  // This field keeps the subresource URLs as a key, and the TimeDelta as a
  // value. TimeDelta stores the duration from navigation start to resource
  // loading start time.
  std::map<GURL, base::TimeDelta> subresource_urls;

  // URLs of preloaded but not actually used resources.
  std::vector<GURL> unused_preload_resources;
};

bool UpdateLcppDataWithLcppDataInputs(const LoadingPredictorConfig& config,
                                      const LcppDataInputs& inputs,
                                      LcppData& data);

// Returns true if the LcppData is valid. i.e. looks not corrupted.
// Otherwise, data might be corrupted.
bool IsValidLcppStat(const LcppStat& lcpp_stat);

// Returns true if the url is valid for learning.
bool IsURLValidForLcpp(const GURL& url);

// Returns the key string for the url. The url should be true for
// the above IsURLValidForLcpp(url).
std::string GetLCPPDatabaseKey(const GURL& url);

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_UTIL_H_
