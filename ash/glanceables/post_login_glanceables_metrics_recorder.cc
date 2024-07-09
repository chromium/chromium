// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/post_login_glanceables_metrics_recorder.h"

#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash {

PostLoginGlanceablesMetricsRecorder::PostLoginGlanceablesMetricsRecorder() {
  overview_observation_.Observe(Shell::Get()->overview_controller());
}

PostLoginGlanceablesMetricsRecorder::~PostLoginGlanceablesMetricsRecorder() =
    default;

void PostLoginGlanceablesMetricsRecorder::OnOverviewModeStarting() {
  RecordHypotheticalFetchEvent(DataFetchEventSource::kOverview);
}

void PostLoginGlanceablesMetricsRecorder::RecordPostLoginFullRestoreShown() {
  RecordHypotheticalFetchEvent(DataFetchEventSource::kPostLoginFullRestore);
}

void PostLoginGlanceablesMetricsRecorder::RecordCalendarFetch() {
  RecordHypotheticalFetchEvent(DataFetchEventSource::kCalendar);
}

void PostLoginGlanceablesMetricsRecorder::RecordHypotheticalFetchEvent(
    DataFetchEventSource source) {
  base::UmaHistogramEnumeration(
      "Ash.PostLoginGlanceables.HypotheticalFetchEvent.NoDelay", source);

  auto MaybeRecordUsingTimestamp =
      [source](std::optional<base::Time>& timestamp, base::TimeDelta time_delay,
               const char* histogram_name) {
        if (!timestamp.has_value() ||
            base::Time::Now() - timestamp.value() > time_delay) {
          base::UmaHistogramEnumeration(histogram_name, source);
          timestamp = base::Time::Now();
        }
      };

  MaybeRecordUsingTimestamp(
      fifteen_second_timestamp_, base::Seconds(15),
      "Ash.PostLoginGlanceables.HypotheticalFetchEvent.15SecondDelay");
  MaybeRecordUsingTimestamp(
      thirty_second_timestamp_, base::Seconds(30),
      "Ash.PostLoginGlanceables.HypotheticalFetchEvent.30SecondDelay");
  MaybeRecordUsingTimestamp(
      five_minute_timestamp_, base::Minutes(5),
      "Ash.PostLoginGlanceables.HypotheticalFetchEvent.5MinuteDelay");
  MaybeRecordUsingTimestamp(
      fifteen_minute_timestamp_, base::Minutes(15),
      "Ash.PostLoginGlanceables.HypotheticalFetchEvent.15MinuteDelay");
  MaybeRecordUsingTimestamp(
      thirty_minute_timestamp_, base::Minutes(30),
      "Ash.PostLoginGlanceables.HypotheticalFetchEvent.30MinuteDelay");
}

}  // namespace ash
