// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/filtering_ranker.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/ash/app_list/search/ranking/constants.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"

namespace app_list {
namespace {

using CrosApiSearchResult = ::crosapi::mojom::SearchResult;

constexpr auto kRestrictedAnswerTypes =
    base::MakeFixedFlatSet<CrosApiSearchResult::AnswerType>({
        CrosApiSearchResult::AnswerType::kDefaultAnswer,
        CrosApiSearchResult::AnswerType::kDictionary,
        CrosApiSearchResult::AnswerType::kTranslation,
    });

// Given `higher_priority` and `lower_priority` result types, deduplicate
// results between the two result types in `results` based on their id,
// preserving the ones in `higher_priority`.
//
// Note this only deduplicates results whose ids are present in both result
// types; if two results of one result type have the same id, they will not be
// deduplicated.
void DeduplicateResults(ResultsMap& results,
                        ResultType higher_priority,
                        ResultType lower_priority) {
  const auto first_it = results.find(higher_priority);
  const auto second_it = results.find(lower_priority);
  if (first_it == results.end() || second_it == results.end()) {
    return;
  }
  const auto& first_results = first_it->second;
  const auto& second_results = second_it->second;

  base::flat_set<std::string> first_ids;
  for (const auto& result : first_results) {
    if (result->result_type() == higher_priority) {
      first_ids.insert(result->id());
    }
  }

  for (auto& result : second_results) {
    if (first_ids.contains(result->id())) {
      result->scoring().set_filtered(true);
    }
  }
}

void DeduplicateDriveFilesAndTabs(ResultsMap& results) {
  const auto omnibox_it = results.find(ProviderType::kOmnibox);
  const auto drive_it = results.find(ProviderType::kDriveSearch);
  if (omnibox_it == results.end() || drive_it == results.end()) {
    return;
  }
  const auto& omnibox_results = omnibox_it->second;
  const auto& drive_results = drive_it->second;

  base::flat_set<std::string> drive_tab_ids;
  for (const auto& result : omnibox_results) {
    if (result->result_type() == ResultType::kOpenTab && result->DriveId()) {
      drive_tab_ids.insert(result->DriveId().value());
    }
  }

  for (auto& result : drive_results) {
    const auto& drive_id = result->DriveId();
    if (drive_id && drive_tab_ids.contains(drive_id.value())) {
      result->scoring().set_filtered(true);
    }
  }
}

void FilterOmniboxResults(ResultsMap& results, const std::u16string& query) {
  // We currently only filter omnibox results. So if we don't have any yet,
  // early exit.
  const auto it = results.find(ProviderType::kOmnibox);
  if (it == results.end()) {
    return;
  }

  auto& omnibox_results = results[ProviderType::kOmnibox];

  // Some answer result types overtrigger on short queries, so these will be
  // filtered out be default.
  if (query.size() <= kMinQueryLengthForCommonAnswers) {
    for (auto& omnibox_result : omnibox_results) {
      if (omnibox_result->display_type() == DisplayType::kAnswerCard &&
          kRestrictedAnswerTypes.contains(omnibox_result->answer_type())) {
        omnibox_result->scoring().set_filtered(true);
      }
    }
  }

  // Compute the total number of results. If we have fewer than can fit in the
  // UI, early exit.
  static const int max_search_results =
      ash::SharedAppListConfig::instance().max_search_results();
  int total_results = 0;
  for (const auto& type_results : results) {
    total_results += type_results.second.size();
  }
  if (total_results <= max_search_results) {
    return;
  }

  // Sort the list of omnibox results best-to-worst.
  std::sort(omnibox_results.begin(), omnibox_results.end(),
            [](const auto& a, const auto& b) {
              return a->relevance() > b->relevance();
            });

  // Filter all results after the |kMaxOmniboxResults|th one out of the UI,
  // but never remove best matches  or answer cards.
  for (size_t i = kMaxOmniboxResults; i < omnibox_results.size(); ++i) {
    auto& scoring = omnibox_results[i]->scoring();
    if (scoring.best_match_rank() == -1 &&
        omnibox_results[i]->display_type() != DisplayType::kAnswerCard) {
      scoring.set_filtered(true);
    }
  }
}

}  //  namespace

FilteringRanker::FilteringRanker() = default;

FilteringRanker::~FilteringRanker() = default;

void FilteringRanker::Start(const std::u16string& query,
                            const CategoriesList& categories) {
  last_query_ = query;
}

void FilteringRanker::UpdateResultRanks(ResultsMap& results,
                                        ProviderType provider) {
  // Do not filter for zero-state.
  if (last_query_.empty()) {
    return;
  }
  FilterOmniboxResults(results, last_query_);
  DeduplicateDriveFilesAndTabs(results);
  // TODO(crbug.com/40218201): Verify that game URLs match the omnibox stripped
  // URL once game URLs are finalized.
  DeduplicateResults(results, ResultType::kGames, ResultType::kOmnibox);
  DeduplicateResults(results, ResultType::kImageSearch,
                     ResultType::kFileSearch);
}

}  // namespace app_list
