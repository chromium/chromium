// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_math_search.h"

#include <optional>
#include <string_view>

#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/rust/fend_core/v1/wrapper/fend_core.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {
namespace {
constexpr int kIconSize = 20;
constexpr std::u16string_view kMathExamples[] = {
    u"1/6 + 3/4",
    u"12 ft in m",
};
}  // namespace

std::optional<QuickInsertSearchResult> PickerMathSearch(
    std::u16string_view query) {
  std::optional<std::string> result =
      fend_core::evaluate(base::UTF16ToUTF8(query));
  if (result.has_value()) {
    return QuickInsertTextResult(base::UTF8ToUTF16(*result), u"",
                                 ui::ImageModel::FromVectorIcon(
                                     kQuickInsertUnitsMathsIcon,
                                     cros_tokens::kCrosSysOnSurface, kIconSize),
                                 QuickInsertTextResult::Source::kMath);
  }
  return std::nullopt;
}

std::vector<QuickInsertSearchResult> PickerMathExamples() {
  std::vector<QuickInsertSearchResult> results;
  for (const auto& query : kMathExamples) {
    std::optional<std::string> result =
        fend_core::evaluate(base::UTF16ToUTF8(query));
    CHECK(result.has_value());

    results.push_back(QuickInsertSearchRequestResult(
        query, base::UTF8ToUTF16(*result),
        ui::ImageModel::FromVectorIcon(kQuickInsertUnitsMathsIcon,
                                       cros_tokens::kCrosSysOnSurface,
                                       kIconSize)));
  }
  return results;
}

}  // namespace ash
