// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/picker/search/picker_date_search.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "ash/picker/picker_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
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

constexpr std::u16string_view kSuggestedDates[] = {
    {u"Today"},
    {u"Tomorrow"},
    {u"2 weeks from now"},
};

std::u16string GetLocalizedDayOfWeek(const base::Time& time) {
  return base::LocalizedTimeFormatWithPattern(time, "EEEE");
}

// The result of parsing a date expression query.
struct ResolvedDate {
  base::Time time;

  // Some optional text to disambiguate the date when the original query is
  // ambiguous.
  std::optional<std::u16string> disambiguation_text;
};

PickerSearchResult MakeResult(const ResolvedDate& date) {
  return PickerTextResult(
      base::LocalizedTimeFormatWithPattern(date.time, "LLLd"),
      date.disambiguation_text.value_or(u""),
      ui::ImageModel::FromVectorIcon(kPickerCalendarIcon,
                                     cros_tokens::kCrosSysOnSurface),
      PickerTextResult::Source::kDate);
}

PickerSearchResult MakeSuggestedResult(std::u16string_view query_text,
                                       const ResolvedDate& date) {
  CHECK(!date.disambiguation_text.has_value());
  return PickerSearchRequestResult(
      query_text, base::LocalizedTimeFormatWithPattern(date.time, "LLLd"),
      ui::ImageModel::FromVectorIcon(kPickerCalendarIcon,
                                     cros_tokens::kCrosSysOnSurface));
}

void HandleSpecificDayQueries(const base::Time& now,
                              std::string_view query,
                              std::vector<ResolvedDate>& resolved_dates) {
  const auto day_lookup = kTextToDays.find(query);
  if (day_lookup == kTextToDays.end()) {
    return;
  }
  resolved_dates.push_back({.time = now + base::Days(day_lookup->second)});
}

void HandleDaysOrWeeksAwayQueries(const base::Time& now,
                                  std::string_view query,
                                  std::vector<ResolvedDate>& resolved_dates) {
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
  resolved_dates.push_back({.time = now + base::Days(x)});
}

void HandleDayOfWeekQueries(const base::Time& now,
                            std::string_view query,
                            std::vector<ResolvedDate>& resolved_dates) {
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
      std::u16string localized_day_of_week =
          GetLocalizedDayOfWeek(now + base::Days(day_diff));
      resolved_dates.push_back({
          .time = now + base::Days(day_diff + kDaysPerWeek),
          .disambiguation_text = l10n_util::GetStringFUTF16(
              IDS_PICKER_DATE_DISAMBIGUATION_THIS_COMING_DAY,
              localized_day_of_week),
      });
      resolved_dates.push_back({
          .time = now + base::Days(day_diff),
          .disambiguation_text = l10n_util::GetStringFUTF16(
              IDS_PICKER_DATE_DISAMBIGUATION_THIS_PAST_DAY,
              std::move(localized_day_of_week)),
      });
    } else {
      resolved_dates.push_back({.time = now + base::Days(day_diff)});
    }
  } else if (prefix == "next ") {
    if (target_day_of_week > current_day_of_week) {
      std::u16string localized_day_of_week =
          GetLocalizedDayOfWeek(now + base::Days(day_diff));
      resolved_dates.push_back({
          .time = now + base::Days(day_diff + kDaysPerWeek),
          .disambiguation_text = l10n_util::GetStringFUTF16(
              IDS_PICKER_DATE_DISAMBIGUATION_NEXT_WEEK, localized_day_of_week),
      });
      resolved_dates.push_back({
          .time = now + base::Days(day_diff),
          .disambiguation_text = l10n_util::GetStringFUTF16(
              IDS_PICKER_DATE_DISAMBIGUATION_THIS_COMING_DAY,
              std::move(localized_day_of_week)),
      });
    } else {
      resolved_dates.push_back(
          {.time = now + base::Days(day_diff + kDaysPerWeek)});
    }
  } else if (prefix == "last ") {
    if (target_day_of_week < current_day_of_week) {
      std::u16string localized_day_of_week =
          GetLocalizedDayOfWeek(now + base::Days(day_diff));
      resolved_dates.push_back({
          .time = now + base::Days(day_diff - kDaysPerWeek),
          .disambiguation_text = l10n_util::GetStringFUTF16(
              IDS_PICKER_DATE_DISAMBIGUATION_LAST_WEEK, localized_day_of_week),
      });
      resolved_dates.push_back({
          .time = now + base::Days(day_diff),
          .disambiguation_text = l10n_util::GetStringFUTF16(
              IDS_PICKER_DATE_DISAMBIGUATION_THIS_PAST_DAY,
              std::move(localized_day_of_week)),
      });
    } else {
      resolved_dates.push_back(
          {.time = now + base::Days(day_diff - kDaysPerWeek)});
    }
  }
}

std::vector<ResolvedDate> ResolveQuery(const base::Time& now,
                                       std::u16string_view query) {
  std::vector<ResolvedDate> resolved_dates;
  std::string clean_query = base::UTF16ToUTF8(base::TrimWhitespace(
      base::i18n::ToLower(query), base::TrimPositions::TRIM_ALL));
  HandleSpecificDayQueries(now, clean_query, resolved_dates);
  HandleDaysOrWeeksAwayQueries(now, clean_query, resolved_dates);
  HandleDayOfWeekQueries(now, clean_query, resolved_dates);
  return resolved_dates;
}

}  // namespace

std::vector<PickerSearchResult> PickerDateSearch(const base::Time& now,
                                                 std::u16string_view query) {
  std::vector<ResolvedDate> resolved_dates = ResolveQuery(now, query);
  std::vector<PickerSearchResult> results;
  results.reserve(resolved_dates.size());
  for (const ResolvedDate& resolved_date : resolved_dates) {
    results.push_back(MakeResult(resolved_date));
  }
  return results;
}

std::vector<PickerSearchResult> PickerSuggestedDateResults() {
  std::vector<PickerSearchResult> results;

  for (const std::u16string_view query : kSuggestedDates) {
    std::vector<ResolvedDate> resolved_dates =
        ResolveQuery(base::Time::Now(), query);
    CHECK_EQ(resolved_dates.size(), 1u);
    results.push_back(MakeSuggestedResult(query, resolved_dates[0]));
  }

  return results;
}

}  // namespace ash
