// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/keyboard_shortcut_item.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
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
}

KeyboardShortcutProvider::~KeyboardShortcutProvider() = default;

void KeyboardShortcutProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel all previous searches.
  weak_factory_.InvalidateWeakPtrs();

  if (query.size() < kMinQueryLength)
    return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&Search, shortcut_data_, query),
      base::BindOnce(&KeyboardShortcutProvider::OnSearchComplete,
                     weak_factory_.GetWeakPtr()));
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

}  // namespace app_list
