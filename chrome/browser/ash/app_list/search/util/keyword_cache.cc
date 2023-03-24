// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/keyword_cache.h"

#include <algorithm>

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

KeywordCache::KeywordCache() = default;
KeywordCache::~KeywordCache() = default;

void KeywordCache::Clear() {
  last_item_scores_.clear();
  last_matched_items_.clear();
}

void KeywordCache::ScoreAndRecordMatch(ResultsMap& results,
                                       ProviderType provider,
                                       double keyword_multiplier) {
  const auto it = results.find(provider);

  if (it == results.end()) {
    return;
  }

  for (auto& result : it->second) {
    result->scoring().set_keyword_multiplier(keyword_multiplier);
    last_item_scores_[result->id()] =
        result->scoring().ftrl_result_score() * keyword_multiplier;
    last_matched_items_.insert(result->id());
  }
}

void KeywordCache::RecordNonMatch(ResultsMap& results, ProviderType provider) {
  const auto it = results.find(provider);

  if (it == results.end()) {
    return;
  }

  for (auto& result : it->second) {
    last_item_scores_[result->id()] = result->scoring().ftrl_result_score();
  }
}

void KeywordCache::Train(const std::string& item) {
  if (last_item_scores_.empty()) {
    return;
  }
  boost_factor_ += CalculateBoostChange(item);
}

// If the change is positive, boost_factor_ increase as (0.5 - original
// boost_factor) * (num_item_above) / rank. If the change is negative,
// boost_factor_ decreases as (original boost_factor) * (num_item_above) / rank.
double KeywordCache::CalculateBoostChange(const std::string& item) const {
  bool is_boosted = IsBoosted(item);

  auto it = last_item_scores_.find(item);
  if (it == last_item_scores_.end()) {
    return 0;
  }
  double selected_item_score = it->second;

  int rank = 0;
  int num_item_above = 0;

  for (const auto& [cached_item, cached_item_score] : last_item_scores_) {
    if (cached_item_score > selected_item_score) {
      rank++;
      if (IsBoosted(cached_item) && !is_boosted) {
        num_item_above--;
      }
      if (!IsBoosted(cached_item) && is_boosted) {
        num_item_above++;
      }
    }
  }
  if (rank == 0) {
    return 0;
  }
  if (num_item_above >= 0) {
    return (kMaxBoostFactor - boost_factor_) *
           static_cast<double>(num_item_above) / static_cast<double>(rank);
  }
  return boost_factor_ * static_cast<double>(num_item_above) /
         static_cast<double>(rank);
}

bool KeywordCache::IsBoosted(const std::string& item) const {
  return find(last_matched_items_.begin(), last_matched_items_.end(), item) !=
         last_matched_items_.end();
}

}  // namespace app_list
