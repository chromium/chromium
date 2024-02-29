// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_CONTINUE_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_CONTINUE_RANKER_H_

#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// A ranker for handling result ordering in the Continue section.
class ContinueRanker : public Ranker {
 public:
  ContinueRanker();
  ~ContinueRanker() override;

  ContinueRanker(const ContinueRanker&) = delete;
  ContinueRanker& operator=(const ContinueRanker&) = delete;

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;

 private:
  const bool mix_local_and_drive_files_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_CONTINUE_RANKER_H_
