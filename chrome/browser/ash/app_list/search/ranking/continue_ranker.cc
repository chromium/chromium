// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/continue_ranker.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

ContinueRanker::ContinueRanker() = default;
ContinueRanker::~ContinueRanker() = default;

void ContinueRanker::UpdateResultRanks(ResultsMap& results,
                                       ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  // Always rank zero-state Drive files higher than zero-state local files by
  // giving them a higher continue_rank.
  if (provider == ProviderType::kZeroStateFile) {
    for (auto& result : it->second)
      result->scoring().continue_rank = 1;
  } else if (provider == ProviderType::kZeroStateDrive) {
    for (auto& result : it->second)
      result->scoring().continue_rank = 2;
  } else if (provider == ProviderType::kZeroStateHelpApp) {
    for (auto& result : it->second)
      result->scoring().continue_rank = 3;
  }
}

}  // namespace app_list
