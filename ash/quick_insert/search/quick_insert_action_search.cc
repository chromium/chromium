// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_action_search.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/quick_insert_shortcuts.h"
#include "ash/quick_insert/views/quick_insert_strings.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "chromeos/ash/components/string_matching/prefix_matcher.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

using CaseTransformType = QuickInsertCaseTransformResult::Type;

constexpr auto kTransformMessageIds =
    std::to_array<std::pair<int, CaseTransformType>>({
        {IDS_PICKER_UPPER_CASE_MENU_LABEL, CaseTransformType::kUpperCase},
        {IDS_PICKER_LOWER_CASE_MENU_LABEL, CaseTransformType::kLowerCase},
        {IDS_PICKER_TITLE_CASE_MENU_LABEL, CaseTransformType::kTitleCase},
    });

bool IsMatch(const string_matching::TokenizedString& query,
             std::u16string text) {
  // Both arguments are stored as `raw_ref`s in the `PrefixMatcher` below, so
  // they need to outlive the matcher.
  string_matching::TokenizedString tokenized_terms(std::move(text));
  string_matching::PrefixMatcher matcher(query, tokenized_terms);
  // TODO: b/325973235 - Use `matcher.relevance()` to sort these results.
  return matcher.Match();
}

}  // namespace

std::vector<QuickInsertSearchResult> PickerActionSearch(
    base::span<const QuickInsertCategory> available_categories,
    bool caps_lock_state_to_search,
    bool search_case_transforms,
    std::u16string_view query) {
  CHECK(!query.empty());
  string_matching::TokenizedString tokenized_query((std::u16string(query)));

  // TODO: b/349494170 - Speed this up by pretokenizing the search terms.
  std::vector<QuickInsertSearchResult> matches;
  for (const QuickInsertCategory category : available_categories) {
    if (IsMatch(tokenized_query, GetLabelForQuickInsertCategory(category))) {
      matches.push_back(QuickInsertCategoryResult(category));
    }
  }

  if (IsMatch(tokenized_query,
              l10n_util::GetStringUTF16(
                  caps_lock_state_to_search
                      ? IDS_PICKER_CAPS_LOCK_ON_MENU_LABEL
                      : IDS_PICKER_CAPS_LOCK_OFF_MENU_LABEL))) {
    matches.push_back(QuickInsertCapsLockResult(
        caps_lock_state_to_search, GetPickerShortcutForCapsLock()));
  }

  if (search_case_transforms) {
    for (const auto& [message_id, type] : kTransformMessageIds) {
      if (IsMatch(tokenized_query, l10n_util::GetStringUTF16(message_id))) {
        matches.push_back(QuickInsertCaseTransformResult(type));
      }
    }
  }

  return matches;
}

}  // namespace ash
