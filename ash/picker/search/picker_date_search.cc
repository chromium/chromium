// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_date_search.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

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
constexpr LazyRE2 kDayOfWeekRegex = {
    "(this |next |last "
    "|)(sunday|monday|tuesday|wednesday|thursday|friday|saturday)"};
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
constexpr auto kDayOfWeekToNumber =
    base::MakeFixedFlatMap<std::string_view, int>({
        {"sunday", 0},
        {"monday", 1},
        {"tuesday", 2},
        {"wednesday", 3},
        {"thursday", 4},
        {"friday", 5},
        {"saturday", 6},
    });

std::optional<int> HandleDaysOrWeeksAwayQueries(std::string_view query) {
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

std::vector<int> HandleDayOfWeekQueries(const base::Time& now,
                                        std::string_view query) {
  std::vector<int> results;
  std::string prefix, target_day_of_week_str;
  if (!RE2::FullMatch(query, *kDayOfWeekRegex, &prefix,
                      &target_day_of_week_str)) {
    return results;
  }
  const auto day_lookup = kDayOfWeekToNumber.find(target_day_of_week_str);
  CHECK(day_lookup != kDayOfWeekToNumber.end());
  int target_day_of_week = day_lookup->second;
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  int current_day_of_week = exploded.day_of_week;
  int day_diff = target_day_of_week - current_day_of_week;
  if (prefix.empty() || prefix == "this ") {
    results.push_back(day_diff);
    if (target_day_of_week < current_day_of_week) {
      results.push_back(day_diff + kDaysPerWeek);
    }
  } else if (prefix == "next ") {
    if (target_day_of_week > current_day_of_week) {
      results.push_back(day_diff);
    }
    results.push_back(day_diff + kDaysPerWeek);
  } else if (prefix == "last ") {
    if (target_day_of_week < current_day_of_week) {
      results.push_back(day_diff);
    }
    results.push_back(day_diff - kDaysPerWeek);
  }
  return results;
}
}  // namespace

std::vector<PickerSearchResult> PickerDateSearch(const base::Time& now,
                                                 std::u16string_view query) {
  std::vector<PickerSearchResult> results;
  std::string clean_query = base::UTF16ToUTF8(base::TrimWhitespace(
      base::i18n::ToLower(query), base::TrimPositions::TRIM_ALL));
  if (std::optional<int> days = HandleDaysOrWeeksAwayQueries(clean_query);
      days.has_value()) {
    results.push_back(PickerSearchResult::Text(
        base::LocalizedTimeFormatWithPattern(now + base::Days(*days), "LLLd")));
  }
  for (int days : HandleDayOfWeekQueries(now, clean_query)) {
    results.push_back(PickerSearchResult::Text(
        base::LocalizedTimeFormatWithPattern(now + base::Days(days), "LLLd")));
  }
  return results;
}

}  // namespace ash
