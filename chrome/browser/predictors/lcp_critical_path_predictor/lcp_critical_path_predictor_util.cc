// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"

namespace predictors {

namespace {

// Returns LCP element locators in the past loads for a given `data`.  The
// returned LCP element locators are ordered by descending frequency (the
// most frequent one comes first). If there is no data, it returns an empty
// vector.
std::vector<std::string> PredictLcpElementLocators(const LcppData& data) {
  const auto& buckets =
      data.lcpp_stat().lcp_element_locator_stat().lcp_element_locator_buckets();
  std::vector<std::pair<double, std::string>>
      lcp_element_locators_with_frequency;
  lcp_element_locators_with_frequency.reserve(buckets.size());
  for (const auto& bucket : buckets) {
    lcp_element_locators_with_frequency.emplace_back(
        bucket.frequency(), bucket.lcp_element_locator());
  }

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
  const auto& buckets = data.lcpp_stat().lcp_script_url_stat().main_buckets();
  std::vector<std::pair<double, std::string>> lcp_script_urls_with_frequency;
  lcp_script_urls_with_frequency.reserve(buckets.size());
  for (const auto& [script_url, frequency] : buckets) {
    lcp_script_urls_with_frequency.emplace_back(frequency, script_url);
  }

  std::sort(lcp_script_urls_with_frequency.rbegin(),
            lcp_script_urls_with_frequency.rend());

  std::vector<GURL> lcp_script_urls;
  lcp_script_urls.reserve(lcp_script_urls_with_frequency.size());
  for (const auto& [frequency, script_url] : lcp_script_urls_with_frequency) {
    GURL parsed_url(script_url);
    if (parsed_url.is_empty() || !parsed_url.is_valid() ||
        !parsed_url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    lcp_script_urls.push_back(std::move(parsed_url));
  }
  return lcp_script_urls;
}

}  // namespace

absl::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
ConvertLcppDataToLCPCriticalPathPredictorNavigationTimeHint(
    const LcppData& lcpp_data) {
  std::vector<std::string> lcp_element_locators =
      PredictLcpElementLocators(lcpp_data);
  std::vector<GURL> lcp_influencer_scripts =
      PredictLcpInfluencerScripts(lcpp_data);

  if (!lcp_element_locators.empty() || !lcp_influencer_scripts.empty()) {
    return blink::mojom::LCPCriticalPathPredictorNavigationTimeHint(
        std::move(lcp_element_locators), std::move(lcp_influencer_scripts));
  }
  return absl::nullopt;
}

}  // namespace predictors
