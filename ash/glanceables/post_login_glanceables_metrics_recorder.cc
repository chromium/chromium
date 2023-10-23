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

  if (!fifteen_second_timestamp_.has_value() ||
      (base::Time::Now() - fifteen_second_timestamp_.value()) >
          base::Seconds(15)) {
    // Make sure that it has been at least 15 seconds since the previous
    // time this metric was recorded.
    base::UmaHistogramEnumeration(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.15SecondDelay",
        source);
    fifteen_second_timestamp_ = base::Time::Now();
  }

  if (!thirty_second_timestamp_.has_value() ||
      (base::Time::Now() - thirty_second_timestamp_.value()) >
          base::Seconds(30)) {
    // Make sure that it has been at least 30 seconds since the previous
    // time this metric was recorded.
    base::UmaHistogramEnumeration(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.30SecondDelay",
        source);
    thirty_second_timestamp_ = base::Time::Now();
  }
}

}  // namespace ash
