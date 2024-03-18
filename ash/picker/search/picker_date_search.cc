// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_date_search.h"

#include <map>
#include <optional>
#include <string>

#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace ash {
namespace {
std::optional<int> TryConvertTextToDays(const std::u16string& query) {
  static base::NoDestructor<std::map<std::u16string, int>> kTextToDays({
      {u"today", 0},
      {u"yesterday", -1},
      {u"tomorrow", 1},
  });
  const auto day_lookup = kTextToDays->find(query);
  if (day_lookup == kTextToDays->end()) {
    return std::nullopt;
  }
  return day_lookup->second;
}
}  // namespace

std::optional<PickerSearchResult> PickerDateSearch(const base::Time& now,
                                                   std::u16string_view query) {
  std::optional<int> days =
      TryConvertTextToDays(std::u16string(base::TrimWhitespace(
          base::i18n::ToLower(query), base::TrimPositions::TRIM_ALL)));
  if (days.has_value()) {
    return PickerSearchResult::Text(
        base::LocalizedTimeFormatWithPattern(now + base::Days(*days), "LLLd"));
  }
  return std::nullopt;
}

}  // namespace ash
