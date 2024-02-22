// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_category_search.h"

#include <string>
#include <vector>

#include "ash/picker/views/picker_strings.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"

namespace ash {

std::vector<PickerSearchResult> PickerCategorySearch(
    base::span<const PickerCategory> categories,
    std::u16string_view query) {
  // TODO: b/325973235 - Search using //chromeos/ash/components/string_matching
  std::vector<PickerSearchResult> matches;
  for (const PickerCategory category : categories) {
    if (query == GetLabelForPickerCategory(category)) {
      matches.push_back(PickerSearchResult::Category(category));
    }
  }
  return matches;
}

}  // namespace ash
