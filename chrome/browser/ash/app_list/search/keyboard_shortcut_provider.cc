// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/keyboard_shortcut_item.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager_factory.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_data.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_result.h"
#include "chrome/browser/ash/app_list/search/manatee/manatee_cache.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/search/util/manatee.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
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
constexpr double kResultRelevanceManateeThreshold = 0.75;

std::vector<std::pair<KeyboardShortcutData, double>> Search(
    const std::vector<KeyboardShortcutData>& shortcut_data,
    std::u16string query) {
  TokenizedString tokenized_query(query, TokenizedString::Mode::kWords);

  // Find all shortcuts which meet the relevance threshold.
  std::vector<std::pair<KeyboardShortcutData, double>> candidates;
  for (const auto& shortcut : shortcut_data) {
    double relevance = KeyboardShortcutResult::CalculateRelevance(
        tokenized_query, shortcut.description());
    if (relevance > kResultRelevanceThreshold) {
      candidates.push_back(std::make_pair(shortcut, relevance));
    }
  }
  return candidates;
}

// Remove disabled shortcuts and leave enabled ones only.
void RemoveDisabledShortcuts(
    ash::shortcut_customization::mojom::SearchResultPtr& search_result) {
  std::erase_if(search_result->accelerator_infos, [](const auto& x) {
    return x->state != ash::mojom::AcceleratorState::kEnabled;
  });
}

}  // namespace

KeyboardShortcutProvider::KeyboardShortcutProvider(
    Profile* profile,
    std::unique_ptr<ManateeCache> manatee_cache)
    : SearchProvider(SearchCategory::kHelp),
      profile_(profile),
      manatee_cache_(std::move(manatee_cache)) {
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
  query_ = query;

  // Cancel all previous searches.
  weak_factory_.InvalidateWeakPtrs();

  if (query.size() < kMinQueryLength) {
    return;
  }

  if (ash::features::IsSearchCustomizableShortcutsInLauncherEnabled()) {
    if (!search_handler_) {
      return;
    }
    search_handler_->Search(
        query, UINT32_MAX,
        base::BindOnce(&KeyboardShortcutProvider::OnShortcutsSearchComplete,
                       weak_factory_.GetWeakPtr()));
  } else {
    if (search_features::isLauncherManateeForKeyboardShortcutsEnabled()) {
      if (!is_embeddings_set_) {
        InitializeManateeCacheForShortcuts();

        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
            base::BindOnce(&Search, shortcut_data_, query),
            base::BindOnce(&KeyboardShortcutProvider::OnSearchComplete,
                           weak_factory_.GetWeakPtr()));
      } else {
        InitializeManateeCacheForQuery(base::UTF16ToUTF8(query));
      }
    } else {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
          base::BindOnce(&Search, shortcut_data_, query),
          base::BindOnce(&KeyboardShortcutProvider::OnSearchComplete,
                         weak_factory_.GetWeakPtr()));
    }
  }
}

void KeyboardShortcutProvider::InitializeManateeCacheForQuery(
    const std::string query) {
  manatee_cache_->RegisterCallback(
      base::BindOnce(&KeyboardShortcutProvider::OnManateeQueryResponseCallback,
                     weak_factory_.GetWeakPtr()));
  manatee_cache_->UrlLoader({query});
}

void KeyboardShortcutProvider::InitializeManateeCacheForShortcuts() {
  std::vector<std::string> descriptions;
  for (const auto& shortcut : shortcut_data_) {
    descriptions.push_back(base::UTF16ToUTF8(shortcut.description()));
  }
  manatee_cache_->RegisterCallback(base::BindOnce(
      &KeyboardShortcutProvider::OnManateeShortcutsResponseCallback,
      base::Unretained(this)));
  manatee_cache_->UrlLoader(descriptions);
}

void KeyboardShortcutProvider::StopQuery() {
  // Cancel all previous searches.
  weak_factory_.InvalidateWeakPtrs();
}

ash::AppListSearchResultType KeyboardShortcutProvider::ResultType() const {
  return ash::AppListSearchResultType::kKeyboardShortcut;
}

// Assumes that |shortcut_data_| has not changed since
// initialisation.
void KeyboardShortcutProvider::OnManateeShortcutsResponseCallback(
    std::vector<std::vector<double>>& reply) {
  CHECK_EQ(shortcut_data_.size(), reply.size());
  for (size_t i = 0; i < reply.size(); ++i) {
    shortcut_data_[i].SetEmbedding(reply[i]);
  }
  is_embeddings_set_ = true;
}

void KeyboardShortcutProvider::OnManateeQueryResponseCallback(
    std::vector<std::vector<double>>& reply) {
  if (reply.size() != 1) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&Search, shortcut_data_, query_),
        base::BindOnce(&KeyboardShortcutProvider::OnSearchComplete,
                       weak_factory_.GetWeakPtr()));
  } else {
    ShortcutDataAndScores candidates;
    for (const auto& data_item : shortcut_data_) {
      std::optional<double> similarity_score =
          (GetEmbeddingSimilarity(data_item.embedding(), reply[0]));
      if (similarity_score.has_value() &&
          similarity_score.value() > kResultRelevanceManateeThreshold) {
        candidates.push_back(
            std::make_pair(data_item, similarity_score.value()));
      }
    }
    OnSearchComplete(candidates);
  }
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
  CHECK(ash::features::IsSearchCustomizableShortcutsInLauncherEnabled());

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert final candidates into correct type, and publish.
  SearchProvider::Results results;
  for (auto& search_result : search_results) {
    // Only enabled shortcuts should be displayed.
    RemoveDisabledShortcuts(search_result);
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
