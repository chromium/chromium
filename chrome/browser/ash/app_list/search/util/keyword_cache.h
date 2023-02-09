// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_KEYWORD_CACHE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_KEYWORD_CACHE_H_

#include <map>
#include <vector>

#include "chrome/browser/ash/app_list/search/scoring.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// Store the latest results and train the keyword ranker with selected result.
class KeywordCache {
 public:
  static constexpr double kMaxBoostFactor = 0.5;
  static constexpr double kMinBoostFactor = 0.0;

  KeywordCache();
  ~KeywordCache();

  KeywordCache(const KeywordCache&) = delete;
  KeywordCache& operator=(const KeywordCache&) = delete;

  // Clear the last_item_scores_ and last_matched_items_.
  void Clear();

  // Store all the scores of last passed in provider that does have a keyword
  // match. Set the keyword_multiplier and then store it.
  void ScoreAndRecordMatch(ResultsMap& results,
                           ProviderType provider,
                           double keyword_multiplier);

  // Store all the score of last passed in provider that does not have a
  // keyword match.
  void RecordNonMatch(ResultsMap& results, ProviderType provider);

  // Train the boost_factor.
  void Train(const std::string& item);

  // Return the boost factor.
  double boost_factor() const { return boost_factor_; }

 private:
  // Calculate the change to the boosting keyword factor.
  double CalculateBoostChange(const std::string& item) const;

  bool IsBoosted(const std::string& item) const;

  std::map<std::string, double> last_item_scores_;
  std::set<std::string> last_matched_items_;
  double boost_factor_ = 0.25;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_KEYWORD_CACHE_H_
