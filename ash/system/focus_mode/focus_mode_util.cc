// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_util.h"

#include <vector>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_textfield.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/i18n/time_formatting.h"
#include "base/i18n/unicodestring.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/measunit.h"
#include "third_party/icu/source/i18n/unicode/measure.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::focus_mode_util {

namespace {

bool ShouldShowHours(TimeFormatType format_type) {
  return format_type != TimeFormatType::kMinutesOnly ||
         FocusModeController::Get()->in_focus_session();
}

bool ShouldShowSeconds(TimeFormatType format_type) {
  return format_type != TimeFormatType::kMinutesOnly;
}

// Always show minutes if numeric format, always show minutes if not showing
// seconds. If showing seconds and not numeric, don't show minutes if it would
// be a leading zero, show it otherwise.
bool ShouldShowMinutes(TimeFormatType format_type,
                       const int64_t total_seconds) {
  return format_type == TimeFormatType::kDigital ||
         !ShouldShowSeconds(format_type) ||
         (total_seconds >= base::Time::kSecondsPerMinute);
}

}  // namespace

std::u16string GetDurationString(base::TimeDelta duration_to_format,
                                 TimeFormatType format_type) {
  UErrorCode status = U_ZERO_ERROR;
  bool numeric = format_type == TimeFormatType::kDigital;

  const int64_t total_seconds =
      base::ClampRound<int64_t>(duration_to_format.InSecondsF());
  const int64_t hours = ShouldShowHours(format_type)
                            ? total_seconds / base::Time::kSecondsPerHour
                            : 0;
  const int64_t minutes =
      (total_seconds - hours * base::Time::kSecondsPerHour) /
      base::Time::kSecondsPerMinute;

  icu::MeasureFormat measure_format(
      icu::Locale::getDefault(),
      numeric ? UMeasureFormatWidth::UMEASFMT_WIDTH_NUMERIC
              : UMeasureFormatWidth::UMEASFMT_WIDTH_SHORT,
      status);
  icu::UnicodeString formatted;
  icu::FieldPosition ignore(icu::FieldPosition::DONT_CARE);

  std::vector<const icu::Measure> measures;

  if (hours != 0) {
    measures.emplace_back(hours, icu::MeasureUnit::createHour(status), status);
  }

  if (ShouldShowMinutes(format_type, total_seconds)) {
    measures.emplace_back(minutes, icu::MeasureUnit::createMinute(status),
                          status);
  }

  if (ShouldShowSeconds(format_type)) {
    const int64_t seconds = total_seconds % base::Time::kSecondsPerMinute;
    measures.emplace_back(seconds, icu::MeasureUnit::createSecond(status),
                          status);
  }

  measure_format.formatMeasures(&measures[0], measures.size(), formatted,
                                ignore, status);

  if (U_SUCCESS(status)) {
    return base::i18n::UnicodeStringToString16(formatted);
  }

  return base::NumberToString16(std::ceil(duration_to_format.InSecondsF()));
}

std::u16string GetFormattedClockString(const base::Time end_time) {
  return base::TimeFormatTimeOfDayWithHourClockType(
      end_time, Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
      base::kKeepAmPm);
}

std::u16string GetNotificationTitleForFocusSession(const base::Time end_time) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_IN_FOCUS_MODE_TITLE,
      GetFormattedClockString(end_time));
}

int GetTimerTextfieldInputInMinutes(SystemTextfield* timer_textfield) {
  // If the user is trying to adjust the session duration while the textfield is
  // empty, we default to the last session duration that was set on the focus
  // mode controller.
  int duration_minutes;
  if (!base::StringToInt(timer_textfield->GetText(), &duration_minutes)) {
    duration_minutes =
        FocusModeController::Get()->session_duration().InMinutes();
  }

  return duration_minutes;
}

}  // namespace ash::focus_mode_util
