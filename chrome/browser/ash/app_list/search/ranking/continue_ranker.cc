// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/continue_ranker.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

ContinueRanker::ContinueRanker()
    : mix_local_and_drive_files_(
          ash::features::UseMixedFileLauncherContinueSection()) {}

ContinueRanker::~ContinueRanker() = default;

void ContinueRanker::UpdateResultRanks(ResultsMap& results,
                                       ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  // Note: Always rank desks admin templates higher than any other type of
  // providers in the continue section view. Always rank zero-state Drive files
  // higher than zero-state local files by giving them a higher continue_rank.
  int continue_rank = -1;
  switch (provider) {
    case ProviderType::kZeroStateFile:
      continue_rank = mix_local_and_drive_files_ ? 2 : 1;
      break;
    case ProviderType::kZeroStateDrive:
      continue_rank = 2;
      break;
    case ProviderType::kZeroStateHelpApp:
      continue_rank = 3;
      break;
    case ProviderType::kDesksAdminTemplate:
      continue_rank = 4;
      break;
    default:
      break;
  }

  for (auto& result : it->second) {
    result->scoring().set_continue_rank(continue_rank);
  }
}

}  // namespace app_list
