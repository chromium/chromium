// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_action_search.h"

#include <string>
#include <string_view>
#include <vector>

#include "ash/picker/views/picker_strings.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "chromeos/ash/components/string_matching/prefix_matcher.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

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

std::vector<PickerSearchResult> PickerActionSearch(
    const PickerActionSearchOptions& options,
    std::u16string_view query) {
  CHECK(!query.empty());
  string_matching::TokenizedString tokenized_query((std::u16string(query)));

  // TODO: b/349494170 - Speed this up by pretokenizing the search terms.
  std::vector<PickerSearchResult> matches;
  for (const PickerCategory category : options.available_categories) {
    if (IsMatch(tokenized_query, GetLabelForPickerCategory(category))) {
      matches.push_back(PickerSearchResult::Category(category));
    }
  }

  if (IsMatch(tokenized_query, l10n_util::GetStringUTF16(
                                   options.caps_lock_state_to_search
                                       ? IDS_PICKER_CAPS_ON_CATEGORY_LABEL
                                       : IDS_PICKER_CAPS_OFF_CATEGORY_LABEL))) {
    matches.push_back(
        PickerSearchResult::CapsLock(options.caps_lock_state_to_search));
  }
  return matches;
}

}  // namespace ash
