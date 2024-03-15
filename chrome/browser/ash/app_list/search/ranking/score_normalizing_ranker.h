// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZING_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZING_RANKER_H_

#include "ash/utility/persistent_proto.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/util/score_normalizer.h"

namespace app_list {

class ScoreNormalizerProto;

// A ranker that transforms the result scores of search providers into something
// close to a uniform distribution. This is done:
//
// - Per-provider. An independent transformation is learned for each provider.
// - In aggregate. The overall long-running transformed score distribution for a
//   provider will be close to uniform, but any given batch of search results -
//   eg. for one query - may not be.
//
// Some providers don't have any transformation applied, see
// ShouldIgnoreProvider in the implementation for details.
class ScoreNormalizingRanker : public Ranker {
 public:
  ScoreNormalizingRanker(ScoreNormalizer::Params params,
                         ash::PersistentProto<ScoreNormalizerProto> proto);
  ~ScoreNormalizingRanker() override;

  ScoreNormalizingRanker(const ScoreNormalizingRanker&) = delete;
  ScoreNormalizingRanker& operator=(const ScoreNormalizingRanker&) = delete;

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;

 private:
  ScoreNormalizer normalizer_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_SCORE_NORMALIZING_RANKER_H_
