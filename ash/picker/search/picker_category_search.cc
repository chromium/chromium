// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_category_search.h"

#include <string_view>
#include <vector>

#include "ash/picker/views/picker_strings.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"

namespace ash {
namespace {

bool ContainsCaseInsensitive(std::u16string_view haystack,
                             std::u16string_view needle) {
  return base::ranges::search(haystack, needle,
                              base::CaseInsensitiveCompareASCII<char>()) !=
         haystack.end();
}

}  // namespace

std::vector<PickerSearchResult> PickerCategorySearch(
    base::span<const PickerCategory> categories,
    std::u16string_view query) {
  CHECK(!query.empty());

  std::vector<PickerSearchResult> matches;
  for (const PickerCategory category : categories) {
    if (ContainsCaseInsensitive(GetLabelForPickerCategory(category), query)) {
      matches.push_back(PickerSearchResult::Category(category));
    }
  }
  return matches;
}

}  // namespace ash
