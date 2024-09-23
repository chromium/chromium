// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager_factory.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_match.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "content/public/browser/storage_partition.h"

namespace app_list {

namespace {

using ::ash::string_matching::TokenizedString;

constexpr size_t kMinQueryLength = 3u;
constexpr size_t kMaxResults = 3u;
constexpr double kResultRelevanceThreshold = 0.79;
// The threshold is used to filter the results from the search handler of the
// new shortcuts app.
constexpr double kRelevanceScoreThreshold = 0.52;

// Remove disabled shortcuts and leave enabled ones only.
void RemoveDisabledShortcuts(
    ash::shortcut_customization::mojom::SearchResultPtr& search_result) {
  std::erase_if(search_result->accelerator_infos, [](const auto& x) {
    return x->state != ash::mojom::AcceleratorState::kEnabled;
  });
}

}  // namespace

KeyboardShortcutProvider::KeyboardShortcutProvider(Profile* profile)
    : SearchProvider(SearchCategory::kHelp), profile_(profile) {
  CHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager->IsUserSessionStartUpTaskCompleted()) {
    // If user session start up task has completed, the initialization can
    // start.
    MaybeInitialize();
  } else {
    // Wait for the user session start up task completion to prioritize
    // resources for them.
    session_manager_observation_.Observe(session_manager);
  }
}

KeyboardShortcutProvider::~KeyboardShortcutProvider() = default;

void KeyboardShortcutProvider::MaybeInitialize(
    ash::shortcut_ui::SearchHandler* fake_search_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ensures that the provider can be initialized once only.
  if (has_initialized) {
    return;
  }
  has_initialized = true;

  // Initialization is happening, so we no longer need to wait for user session
  // start up task completion.
  session_manager_observation_.Reset();

  // Use fake search handler if provided in tests, or get it from
  // `shortcuts_app_manager`.
  if (fake_search_handler) {
    search_handler_ = fake_search_handler;
    return;
  }

  auto* shortcuts_app_manager =
      ash::shortcut_ui::ShortcutsAppManagerFactory::GetForBrowserContext(
          profile_);
  CHECK(shortcuts_app_manager);
  search_handler_ = shortcuts_app_manager->search_handler();
}

void KeyboardShortcutProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel all previous searches.
  weak_factory_.InvalidateWeakPtrs();

  if (query.size() < kMinQueryLength) {
    return;
  }

  if (ash::features::IsSearchCustomizableShortcutsInLauncherEnabled()) {
    if (!search_handler_) {
      // If user has started to user launcher search before the user session
      // startup tasks completed, we should honor this user action and
      // initialize the provider. It makes the key shortcut search available
      // earlier.
      MaybeInitialize();
      return;
    }

    search_handler_->Search(
        query, UINT32_MAX,
        base::BindOnce(&KeyboardShortcutProvider::OnShortcutsSearchComplete,
                       weak_factory_.GetWeakPtr(), query));
  }
}

void KeyboardShortcutProvider::StopQuery() {
  // Cancel all previous searches.
  weak_factory_.InvalidateWeakPtrs();
}

ash::AppListSearchResultType KeyboardShortcutProvider::ResultType() const {
  return ash::AppListSearchResultType::kKeyboardShortcut;
}

void KeyboardShortcutProvider::OnUserSessionStartUpTaskCompleted() {
  MaybeInitialize();
}

void KeyboardShortcutProvider::OnShortcutsSearchComplete(
    const std::u16string& query,
    std::vector<ash::shortcut_customization::mojom::SearchResultPtr>
        search_results) {
  CHECK(ash::features::IsSearchCustomizableShortcutsInLauncherEnabled());

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TokenizedString tokenized_query(query, TokenizedString::Mode::kWords);

  ash::string_matching::TokenizedStringMatch match;

  // Find all shortcuts which meet the relevance threshold.
  std::vector<ash::shortcut_customization::mojom::SearchResultPtr> candidates;
  for (auto& search_result : search_results) {
    // Only enabled shortcuts should be displayed.
    RemoveDisabledShortcuts(search_result);
    // The search results are sorted by relevance score in descending order
    // already.
    if (search_result->relevance_score < kRelevanceScoreThreshold) {
      break;
    }

    TokenizedString tokenized_target(
        search_result->accelerator_layout_info->description,
        TokenizedString::Mode::kWords);

    // Excludes the result if either the query or the description is empty.
    if (tokenized_query.text().empty() || tokenized_target.text().empty()) {
      continue;
    }

    // Re-calculate the relevance score using tokenized string match.
    double relevance = match.Calculate(tokenized_query, tokenized_target);
    if (relevance > kResultRelevanceThreshold) {
      search_result->relevance_score = relevance;
      candidates.emplace_back(std::move(search_result));
    }
  }

  // Sort candidates by descending relevance score. The sort is required as the
  // relevance score has been re-calculated.
  std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) {
    return a->relevance_score > b->relevance_score;
  });

  // Convert final candidates into correct type, and publish.
  SearchProvider::Results results;
  for (size_t i = 0; i < std::min(candidates.size(), kMaxResults); ++i) {
    results.push_back(
        std::make_unique<KeyboardShortcutResult>(profile_, candidates[i]));
  }

  SwapResults(&results);
}

}  // namespace app_list
