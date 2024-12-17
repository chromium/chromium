// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/focus_mode/focus_mode_util.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_textfield.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/check_op.h"
#include "base/i18n/time_formatting.h"
#include "base/i18n/unicodestring.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/measunit.h"
#include "third_party/icu/source/i18n/unicode/measure.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::focus_mode_util {
namespace {

constexpr std::pair<int, std::u16string>
    congratulatory_pair[kCongratulatoryTitleNum] = {
        std::make_pair(IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE,
                       u"🎉"),
        std::make_pair(IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE_1,
                       u"⭐"),
        std::make_pair(IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE_2,
                       u"🎉"),
        std::make_pair(IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE_3,
                       u"⚡"),
        std::make_pair(IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE_4,
                       u"🎈"),
        std::make_pair(IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE_5,
                       u"☑"),
};

}  // namespace

SelectedPlaylist::SelectedPlaylist() = default;

SelectedPlaylist::SelectedPlaylist(const SelectedPlaylist&) = default;

SelectedPlaylist& SelectedPlaylist::operator=(const SelectedPlaylist& other) =
    default;

SelectedPlaylist::~SelectedPlaylist() = default;

std::u16string GetDurationString(base::TimeDelta duration_to_format,
                                 bool digital_format) {
  UErrorCode status = U_ZERO_ERROR;

  const int64_t total_seconds =
      base::ClampRound<int64_t>(duration_to_format.InSecondsF());
  const int64_t hours =
      digital_format || FocusModeController::Get()->in_focus_session()
          ? total_seconds / base::Time::kSecondsPerHour
          : 0;

  icu::MeasureFormat measure_format(
      icu::Locale::getDefault(),
      digital_format ? UMeasureFormatWidth::UMEASFMT_WIDTH_NUMERIC
                     : UMeasureFormatWidth::UMEASFMT_WIDTH_SHORT,
      status);
  icu::UnicodeString formatted;
  icu::FieldPosition ignore(icu::FieldPosition::DONT_CARE);

  std::vector<icu::Measure> measures;

  if (hours != 0) {
    measures.emplace_back(hours, icu::MeasureUnit::createHour(status), status);
  }

  if (digital_format || total_seconds >= base::Time::kSecondsPerMinute) {
    const int64_t minutes =
        (total_seconds - hours * base::Time::kSecondsPerHour) /
        base::Time::kSecondsPerMinute;
    measures.emplace_back(minutes, icu::MeasureUnit::createMinute(status),
                          status);
  }

  if (digital_format || total_seconds < base::Time::kSecondsPerMinute) {
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

std::u16string GetNotificationDescriptionForFocusSession(
    const base::Time end_time) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_IN_FOCUS_MODE_DESCRIPTION,
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

std::u16string GetFormattedEndTimeString(const base::Time end_time) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_TIME_SUBLABEL,
      focus_mode_util::GetFormattedClockString(end_time));
}

std::string GetSourceTitleForMediaControls(const SelectedPlaylist& playlist) {
  if (playlist.empty() || playlist.type == SoundType::kNone) {
    return "";
  }

  return l10n_util::GetStringUTF8(
      playlist.type == SoundType::kYouTubeMusic
          ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_BUTTON
          : IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_SOUNDSCAPE_BUTTON);
}

std::u16string GetCongratulatoryText(const size_t index) {
  CHECK_LT(index, kCongratulatoryTitleNum);
  return l10n_util::GetStringUTF16(congratulatory_pair[index].first);
}

std::u16string GetCongratulatoryEmoji(const size_t index) {
  CHECK_LT(index, kCongratulatoryTitleNum);
  return congratulatory_pair[index].second;
}

std::u16string GetCongratulatoryTextAndEmoji(const size_t index) {
  CHECK_LT(index, kCongratulatoryTitleNum);
  return base::JoinString(
      {GetCongratulatoryText(index), GetCongratulatoryEmoji(index)}, u" ");
}

int GetNextProgressStep(double current_progress) {
  CHECK_GE(current_progress, 0.0);
  CHECK_LE(current_progress, 1.0);
  return std::min(
      (int)(std::floor(current_progress * kProgressIndicatorSteps) + 1),
      kProgressIndicatorSteps);
}

void RecordHistogramForApiStatus(const std::string& method,
                                 const google_apis::ApiErrorCode error_code) {
  base::UmaHistogramSparse(
      base::StringPrintf(focus_mode_histogram_names::kApiStatus,
                         method.c_str()),
      error_code);
}

void RecordHistogramForApiLatency(const std::string& method,
                                  const base::TimeDelta latency) {
  base::UmaHistogramTimes(
      base::StringPrintf(focus_mode_histogram_names::kApiLatency,
                         method.c_str()),
      latency);
}

void RecordHistogramForApiResult(const std::string& method,
                                 const bool successful) {
  base::UmaHistogramBoolean(
      base::StringPrintf(focus_mode_histogram_names::kApiResult,
                         method.c_str()),
      successful);
}

void RecordHistogramForApiRetryCount(const std::string& method,
                                     const int retry_count) {
  base::UmaHistogramCounts100(
      base::StringPrintf(focus_mode_histogram_names::kApiRetryCount,
                         method.c_str()),
      retry_count);
}

}  // namespace ash::focus_mode_util
