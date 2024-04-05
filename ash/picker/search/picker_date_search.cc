// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_date_search.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {
namespace {

constexpr int kDaysPerWeek = 7;
constexpr LazyRE2 kDaysOrWeeksAwayRegex = {
    "(\\d{1,6}|one|two|three|four|five|six|seven|eight|nine|ten) "
    "(days?|weeks?) (from now|ago)"};
constexpr LazyRE2 kDayOfWeekRegex = {
    "(this |next |last |)"
    "(sunday|monday|tuesday|wednesday|thursday|friday|saturday)"};
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
constexpr std::u16string_view kNumberToDayOfWeek[] = {
    u"Sunday",   u"Monday", u"Tuesday", u"Wednesday",
    u"Thursday", u"Friday", u"Saturday"};

constexpr std::tuple<std::u16string_view, std::u16string_view>
    kSuggestedDates[] = {
        {u"today", u"Today's date"},
        {u"tomorrow", u"Tomorrow's date"},
        {u"2 weeks from now", u"Two weeks from now"},
};

PickerSearchResult MakeResult(const base::Time time,
                              std::u16string_view secondary_text = u"") {
  return PickerSearchResult::Text(
      base::LocalizedTimeFormatWithPattern(time, "LLLd"), secondary_text,
      ui::ImageModel::FromVectorIcon(kPickerCalendarIcon,
                                     cros_tokens::kCrosSysOnSurface));
}

PickerSearchResult OverrideSecondaryText(PickerSearchResult result,
                                         std::u16string_view secondary_text) {
  const PickerSearchResult::TextData& data =
      std::get<PickerSearchResult::TextData>(result.data());
  return PickerSearchResult::Text(data.primary_text, secondary_text, data.icon);
}

void HandleSpecificDayQueries(const base::Time& now,
                              std::string_view query,
                              std::vector<PickerSearchResult>& results) {
  const auto day_lookup = kTextToDays.find(query);
  if (day_lookup == kTextToDays.end()) {
    return;
  }
  results.push_back(MakeResult(now + base::Days(day_lookup->second)));
}

void HandleDaysOrWeeksAwayQueries(const base::Time& now,
                                  std::string_view query,
                                  std::vector<PickerSearchResult>& results) {
  std::string number, unit, suffix;
  if (!RE2::FullMatch(query, *kDaysOrWeeksAwayRegex, &number, &unit, &suffix)) {
    return;
  }
  const auto word_lookup = kWordToNumber.find(number);
  int x = 0;
  if (word_lookup != kWordToNumber.end()) {
    x = word_lookup->second;
  } else {
    base::StringToInt(number, &x);
  }
  if (x <= 0) {
    return;
  }
  if (unit.starts_with("week")) {
    x *= kDaysPerWeek;
  }
  if (suffix == "ago") {
    x = -x;
  }
  results.push_back(MakeResult(now + base::Days(x)));
}

void HandleDayOfWeekQueries(const base::Time& now,
                            std::string_view query,
                            std::vector<PickerSearchResult>& results) {
  std::string prefix, target_day_of_week_str;
  if (!RE2::FullMatch(query, *kDayOfWeekRegex, &prefix,
                      &target_day_of_week_str)) {
    return;
  }
  const auto day_lookup = kDayOfWeekToNumber.find(target_day_of_week_str);
  CHECK(day_lookup != kDayOfWeekToNumber.end());
  int target_day_of_week = day_lookup->second;
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  int current_day_of_week = exploded.day_of_week;
  int day_diff = target_day_of_week - current_day_of_week;
  if (prefix.empty() || prefix == "this ") {
    if (target_day_of_week < current_day_of_week) {
      results.push_back(
          MakeResult(now + base::Days(day_diff + kDaysPerWeek),
                     base::StrCat({u"this coming ",
                                   kNumberToDayOfWeek[target_day_of_week]})));
      results.push_back(
          MakeResult(now + base::Days(day_diff),
                     base::StrCat({u"this past ",
                                   kNumberToDayOfWeek[target_day_of_week]})));
    } else {
      results.push_back(MakeResult(now + base::Days(day_diff)));
    }
  } else if (prefix == "next ") {
    if (target_day_of_week > current_day_of_week) {
      results.push_back(
          MakeResult(now + base::Days(day_diff + kDaysPerWeek),
                     base::StrCat({kNumberToDayOfWeek[target_day_of_week],
                                   u" next week"})));
      results.push_back(
          MakeResult(now + base::Days(day_diff),
                     base::StrCat({u"this coming ",
                                   kNumberToDayOfWeek[target_day_of_week]})));
    } else {
      results.push_back(MakeResult(now + base::Days(day_diff + kDaysPerWeek)));
    }
  } else if (prefix == "last ") {
    if (target_day_of_week < current_day_of_week) {
      results.push_back(
          MakeResult(now + base::Days(day_diff - kDaysPerWeek),
                     base::StrCat({kNumberToDayOfWeek[target_day_of_week],
                                   u" last week"})));
      results.push_back(
          MakeResult(now + base::Days(day_diff),
                     base::StrCat({u"this past ",
                                   kNumberToDayOfWeek[target_day_of_week]})));
    } else {
      results.push_back(MakeResult(now + base::Days(day_diff - kDaysPerWeek)));
    }
  }
}
}  // namespace

std::vector<PickerSearchResult> PickerDateSearch(const base::Time& now,
                                                 std::u16string_view query) {
  std::vector<PickerSearchResult> results;
  std::string clean_query = base::UTF16ToUTF8(base::TrimWhitespace(
      base::i18n::ToLower(query), base::TrimPositions::TRIM_ALL));
  HandleSpecificDayQueries(now, clean_query, results);
  HandleDaysOrWeeksAwayQueries(now, clean_query, results);
  HandleDayOfWeekQueries(now, clean_query, results);
  return results;
}

std::vector<PickerSearchResult> PickerSuggestedDateResults() {
  std::vector<PickerSearchResult> results;

  for (const auto& date : kSuggestedDates) {
    std::vector<PickerSearchResult> query_results =
        PickerDateSearch(base::Time::Now(), std::get<0>(date));
    for (const auto& result : query_results) {
      results.push_back(OverrideSecondaryText(result, std::get<1>(date)));
    }
  }

  return results;
}

}  // namespace ash
