// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_math_search.h"

#include <optional>
#include <string_view>

#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/rust/fend_core/v1/wrapper/fend_core.h"

namespace ash {

std::optional<PickerSearchResult> PickerMathSearch(std::u16string_view query) {
  std::optional<std::string> result =
      fend_core::evaluate(base::UTF16ToUTF8(query));
  if (result.has_value()) {
    return PickerSearchResult::Text(base::UTF8ToUTF16(*result));
  }
  return std::nullopt;
}

}  // namespace ash
