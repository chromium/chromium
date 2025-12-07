// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_ANSWER_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_ANSWER_RANKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// A ranker that selects at most one result to be an Answer card by settings its
// DisplayType to kAnswerCard.
// This ranker also hides any Omnibox answers that are not selected.
class AnswerRanker : public Ranker {
 public:
  AnswerRanker();
  ~AnswerRanker() override;

  AnswerRanker(const AnswerRanker&) = delete;
  AnswerRanker& operator=(const AnswerRanker&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             const CategoriesList& categories) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
  void OnBurnInPeriodElapsed() override;

 private:
  // Officially promotes the current answer candidate if there is one.
  void PromoteChosenAnswer();

  // The currently selected answer. A nullptr value indicates that no answer
  // card has been chosen.
  base::WeakPtr<ChromeSearchResult> chosen_answer_;

  // All current Omnibox answer candidates.
  std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
      omnibox_candidates_;

  bool burn_in_elapsed_ = false;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_ANSWER_RANKER_H_
