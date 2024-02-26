// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_category_search.h"

#include <string>
#include <string_view>
#include <vector>

#include "ash/picker/views/picker_strings.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/check.h"
#include "chromeos/ash/components/string_matching/prefix_matcher.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash {

std::vector<PickerSearchResult> PickerCategorySearch(
    base::span<const PickerCategory> categories,
    std::u16string_view query) {
  CHECK(!query.empty());
  string_matching::TokenizedString tokenized_query((std::u16string(query)));

  std::vector<PickerSearchResult> matches;
  for (const PickerCategory category : categories) {
    string_matching::TokenizedString tokenized_category(
        GetLabelForPickerCategory(category));
    // Both arguments are stored as `raw_ref`s in the `PrefixMatcher` below, so
    // they need to outlive the matcher.
    string_matching::PrefixMatcher matcher(tokenized_query, tokenized_category);
    // TODO: b/325973235 - Use `matcher.relevance()` to sort these results.
    if (matcher.Match()) {
      matches.push_back(PickerSearchResult::Category(category));
    }
  }
  return matches;
}

}  // namespace ash
