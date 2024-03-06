// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_math_search.h"

#include <optional>
#include <string_view>

#include "ash/public/cpp/picker/picker_search_result.h"

namespace ash {

std::optional<PickerSearchResult> PickerMathSearch(std::u16string_view query) {
  if (query == u"1 + 1") {
    return PickerSearchResult::Text(u"2");
  }
  return std::nullopt;
}

}  // namespace ash
