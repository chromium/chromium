// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"

#include <algorithm>
#include <cstdint>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/keyboard_shortcut_item.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager_factory.h"
#include "base/feature_list.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace app_list {

namespace {

using ::ash::string_matching::TokenizedString;

constexpr size_t kMinQueryLength = 3u;
constexpr size_t kMaxResults = 3u;
constexpr double kResultRelevanceThreshold = 0.79;
// The threshold is used to filter the results from the search handler of the
// new shortcuts app.
constexpr double kRelevanceScoreThreshold = 0.52;

std::vector<std::pair<KeyboardShortcutData, double>> Search(
    const std::vector<KeyboardShortcutData>& shortcut_data,
    std::u16string query) {
  TokenizedString tokenized_query(query, TokenizedString::Mode::kWords);

  // Find all shortcuts which meet the relevance threshold.
  std::vector<std::pair<KeyboardShortcutData, double>> candidates;
  for (const auto& shortcut : shortcut_data) {
    double relevance = KeyboardShortcutResult::CalculateRelevance(
        tokenized_query, shortcut.description);
    if (relevance > kResultRelevanceThreshold) {
      candidates.push_back(std::make_pair(shortcut, relevance));
    }
  }
  return candidates;
}

}  // namespace

KeyboardShortcutProvider::KeyboardShortcutProvider(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProcessShortcutList();

  auto* shortcuts_app_manager_factory =
      ash::shortcut_ui::ShortcutsAppManagerFactory::GetForBrowserContext(
          profile_);
  // The factory is null in unit tests.
  if (shortcuts_app_manager_factory) {
    search_handler_ = shortcuts_app_manager_factory->search_handler();
  }
}

KeyboardShortcutProvider::~KeyboardShortcutProvider() = default;

void KeyboardShortcutProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel all previous searches.
  weak_factory_.InvalidateWeakPtrs();

  if (query.size() < kMinQueryLength) {
    return;
  }

  if (ash::features::isSearchCustomizableShortcutsInLauncherEnabled()) {
    if (!search_handler_) {
      return;
    }
    search_handler_->Search(
        query, UINT32_MAX,
        base::BindOnce(&KeyboardShortcutProvider::OnShortcutsSearchComplete,
                       weak_factory_.GetWeakPtr()));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&Search, shortcut_data_, query),
        base::BindOnce(&KeyboardShortcutProvider::OnSearchComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void KeyboardShortcutProvider::StopQuery() {
  // Cancel all previous searches.
  weak_factory_.InvalidateWeakPtrs();
}

ash::AppListSearchResultType KeyboardShortcutProvider::ResultType() const {
  return ash::AppListSearchResultType::kKeyboardShortcut;
}

void KeyboardShortcutProvider::ProcessShortcutList() {
  DCHECK(shortcut_data_.empty());
  for (const auto& item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    shortcut_data_.push_back(KeyboardShortcutData(item));
  }
}

void KeyboardShortcutProvider::OnSearchComplete(
    KeyboardShortcutProvider::ShortcutDataAndScores candidates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Sort candidates by descending relevance score.
  std::sort(candidates.begin(), candidates.end(),
            [](auto& a, auto& b) { return a.second > b.second; });

  // Convert final candidates into correct type, and publish.
  SearchProvider::Results results;
  for (size_t i = 0; i < std::min(candidates.size(), kMaxResults); ++i) {
    results.push_back(std::make_unique<KeyboardShortcutResult>(
        profile_, candidates[i].first, candidates[i].second));
  }
  SwapResults(&results);
}

void KeyboardShortcutProvider::OnShortcutsSearchComplete(
    std::vector<ash::shortcut_customization::mojom::SearchResultPtr>
        search_results) {
  CHECK(ash::features::isSearchCustomizableShortcutsInLauncherEnabled());

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert final candidates into correct type, and publish.
  SearchProvider::Results results;
  for (const auto& search_result : search_results) {
    // The search results are sorted by relevance score in descending order
    // already.
    if (search_result->relevance_score < kRelevanceScoreThreshold) {
      break;
    }
    results.push_back(
        std::make_unique<KeyboardShortcutResult>(profile_, search_result));
    if (results.size() >= kMaxResults) {
      break;
    }
  }

  SwapResults(&results);
}

}  // namespace app_list
