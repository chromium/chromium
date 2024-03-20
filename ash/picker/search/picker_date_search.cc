// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_date_search.h"

#include <map>
#include <optional>
#include <string>

#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {
namespace {

constexpr int kDaysPerWeek = 7;
constexpr LazyRE2 kDaysOrWeeksAwayRegex = {
    "(\\d{1,6}|one|two|three|four|five|six|seven|eight|nine|ten) "
    "(days?|weeks?) (from now|ago)"};
constexpr auto kTextToDays = base::MakeFixedFlatMap<std::string_view, int>({
    {"yesterday", -1},
    {"today", 0},
    {"tomorrow", 1},
});
constexpr auto kWordToNumber = base::MakeFixedFlatMap<std::string_view, int>({
    {"one", 1},
    {"two", 2},
    {"three", 3},
    {"four", 4},
    {"five", 5},
    {"six", 6},
    {"seven", 7},
    {"eight", 8},
    {"nine", 9},
    {"ten", 10},
});

std::optional<int> TryConvertTextToDays(std::string_view query) {
  const auto day_lookup = kTextToDays.find(query);
  if (day_lookup != kTextToDays.end()) {
    return day_lookup->second;
  }

  std::string number, unit, suffix;
  if (RE2::FullMatch(query, *kDaysOrWeeksAwayRegex, &number, &unit, &suffix)) {
    const auto word_lookup = kWordToNumber.find(number);
    int x = 0;
    if (word_lookup != kWordToNumber.end()) {
      x = word_lookup->second;
    } else {
      base::StringToInt(number, &x);
    }
    if (x > 0) {
      if (unit.starts_with("week")) {
        x *= kDaysPerWeek;
      }
      if (suffix == "ago") {
        x = -x;
      }
      return x;
    }
  }

  return std::nullopt;
}
}  // namespace

std::optional<PickerSearchResult> PickerDateSearch(const base::Time& now,
                                                   std::u16string_view query) {
  std::optional<int> days =
      TryConvertTextToDays(base::UTF16ToUTF8(base::TrimWhitespace(
          base::i18n::ToLower(query), base::TrimPositions::TRIM_ALL)));
  if (days.has_value()) {
    return PickerSearchResult::Text(
        base::LocalizedTimeFormatWithPattern(now + base::Days(*days), "LLLd"));
  }
  return std::nullopt;
}

}  // namespace ash
