// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_MRFU_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_MRFU_RANKER_H_

#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/search/util/ftrl_optimizer.h"
#include "chrome/browser/ash/app_list/search/util/mrfu_cache.h"

namespace app_list {

// These two classes wrap an MrfuCache to provide most-recently-frequently-used
// (MRFU) scoring of either results or categories.

// A ranker that keeps track of usage of individual results using an MRFU cache
// of launch history.
//
// Can be used to either immutably return results scores with GetResultRanks
// or modify them with UpdateResultRanks.
class MrfuResultRanker : public Ranker {
 public:
  explicit MrfuResultRanker(MrfuCache::Params params, MrfuCache::Proto proto);

  ~MrfuResultRanker() override;

  // Ranker:
  std::vector<double> GetResultRanks(const ResultsMap& results,
                                     ProviderType provider) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
  void Train(const LaunchData& launch) override;

 private:
  std::unique_ptr<MrfuCache> mrfu_;
};

// A ranker that keeps track of category usage with an MRFU cache of launch
// history.
//
// Can be used to either immutably return category scores with GetCategoryRanks
// or modify them with UpdateCategoryRanks.
class MrfuCategoryRanker : public Ranker {
 public:
  MrfuCategoryRanker(MrfuCache::Params params,
                     ash::PersistentProto<MrfuCacheProto> proto);
  ~MrfuCategoryRanker() override;

  // Ranker:
  void Start(const std::u16string& query,
             const CategoriesList& categories) override;
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override;
  std::vector<double> GetCategoryRanks(const ResultsMap& results,
                                       const CategoriesList& categories,
                                       ProviderType provider) override;
  void Train(const LaunchData& launch) override;

 private:
  void SetDefaultCategoryScores();

  std::unique_ptr<MrfuCache> mrfu_;
  std::vector<double> current_category_scores_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_MRFU_RANKER_H_
