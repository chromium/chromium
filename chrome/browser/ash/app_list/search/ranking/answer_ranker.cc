// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/answer_ranker.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {
namespace {

// Returns the provider type's priority. A higher value indicates higher
// priority. Providers that are never used for answers will have 0 priority.
int GetPriority(ProviderType type) {
  switch (type) {
    case ProviderType::kOmnibox:
      return 3;
    case ProviderType::kKeyboardShortcut:
      return 2;
      // TODO(b/263994165): Check if this is the correct priority.
    case ProviderType::kSystemInfo:
      return 1;
    default:
      return 0;
  }
}

// If there are any Omnibox answers, returns the highest scoring one. If not,
// returns nullptr.
ChromeSearchResult* GetOmniboxCandidate(Results& results) {
  ChromeSearchResult* top_answer = nullptr;
  double top_score = 0.0;
  for (const auto& result : results) {
    if (result->display_type() != DisplayType::kAnswerCard) {
      continue;
    }

    const double score = result->relevance();
    if (!top_answer || score > top_score) {
      top_answer = result.get();
      top_score = score;
    }
  }
  return top_answer;
}

// If there are any best match SystemInfo answers, returns the highest scoring
// one. If not, returns nullptr.
ChromeSearchResult* GetSystemInfoCandidate(Results& results) {
  ChromeSearchResult* top_answer = nullptr;
  double top_score = 0.0;
  for (const auto& result : results) {
    if (result->display_type() != DisplayType::kAnswerCard ||
        !result->best_match()) {
      continue;
    }

    const double score = result->relevance();
    if (!top_answer || score > top_score) {
      top_answer = result.get();
      top_score = score;
    }
  }
  return top_answer;
}

// Returns the Shortcut best match as long as there is only one. Otherwise,
// returns nullptr.
ChromeSearchResult* GetShortcutCandidate(Results& results) {
  ChromeSearchResult* best_shortcut = nullptr;
  for (auto& result : results) {
    if (!result->best_match()) {
      continue;
    }

    if (best_shortcut) {
      // A best match shortcut has already been found, so there are at least
      // two and neither should be promoted to Answer Card.
      return nullptr;
    }
    best_shortcut = result.get();
  }
  return best_shortcut;
}

}  // namespace

AnswerRanker::AnswerRanker() = default;

AnswerRanker::~AnswerRanker() = default;

void AnswerRanker::Start(const std::u16string& query,
                         const CategoriesList& categories) {
  burn_in_elapsed_ = false;
  chosen_answer_ = nullptr;
  omnibox_candidates_.clear();
}

void AnswerRanker::UpdateResultRanks(ResultsMap& results,
                                     ProviderType provider) {
  if (GetPriority(provider) == 0) {
    return;
  }

  const auto it = results.find(provider);
  DCHECK(it != results.end());
  auto& new_results = it->second;

  // Keep track of Omnibox candidates. Any candidates that are not selected
  // should be filtered out later.
  if (provider == ProviderType::kOmnibox) {
    for (const auto& result : new_results) {
      if (result->display_type() == DisplayType::kAnswerCard) {
        omnibox_candidates_.push_back(result.get());
      }
    }
  }

  // Don't change a selected answer after the burn-in period has elapsed. This
  // includes ensuring that the answer is re-selected.
  if (burn_in_elapsed_ && chosen_answer_) {
    PromoteChosenAnswer();
    return;
  }

  // Don't make any changes if the chosen answer has higher priority than the
  // current provider.
  if (chosen_answer_ &&
      GetPriority(provider) < GetPriority(chosen_answer_->result_type())) {
    return;
  }

  // Finally, choose a new candidate from the current provider if one exists.
  ChromeSearchResult* new_answer = nullptr;
  switch (provider) {
    case ProviderType::kOmnibox:
      new_answer = GetOmniboxCandidate(new_results);
      break;
    case ProviderType::kKeyboardShortcut:
      new_answer = GetShortcutCandidate(new_results);
      break;
    case ProviderType::kSystemInfo:
      new_answer = GetSystemInfoCandidate(new_results);
      break;
    default:
      return;
  }
  if (new_answer) {
    chosen_answer_ = new_answer->GetWeakPtr();
  }

  if (burn_in_elapsed_) {
    PromoteChosenAnswer();
  }
}

void AnswerRanker::OnBurnInPeriodElapsed() {
  burn_in_elapsed_ = true;
  PromoteChosenAnswer();
}

void AnswerRanker::PromoteChosenAnswer() {
  if (!chosen_answer_) {
    return;
  }

  // Filter out unsuccessful Omnibox candidates.
  for (ChromeSearchResult* result : omnibox_candidates_) {
    if (result && result->id() != chosen_answer_->id()) {
      result->scoring().set_filtered(true);
    }
  }

  chosen_answer_->SetDisplayType(DisplayType::kAnswerCard);
  chosen_answer_->SetMultilineTitle(true);
  if (chosen_answer_->result_type() == ResultType::kSystemInfo) {
    chosen_answer_->SetIconDimension(kSystemAnswerCardIconDimension);
  } else {
    chosen_answer_->SetIconDimension(kAnswerCardIconDimension);
  }
}

}  // namespace app_list
