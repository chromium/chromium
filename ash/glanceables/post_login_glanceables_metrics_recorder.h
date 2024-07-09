// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_POST_LOGIN_GLANCEABLES_METRICS_RECORDER_H_
#define ASH_GLANCEABLES_POST_LOGIN_GLANCEABLES_METRICS_RECORDER_H_

#include <optional>

#include "ash/wm/overview/overview_controller.h"
#include "base/scoped_observation.h"

namespace ash {

// This class provides metrics collection to help estimate the number of queries
// that will be made for data that will be used to populated the post login
// glanceables. See `DataFetchEventSource` for a full list of the query sources
// where this data could be needed.
class ASH_EXPORT PostLoginGlanceablesMetricsRecorder : public OverviewObserver {
 public:
  // This enum is used for metrics, so enum values should not be changed. New
  // enum values can be added, but existing enums must never be renumbered or
  // deleted and reused.
  enum class DataFetchEventSource {
    kPostLoginFullRestore = 0,
    kOverview = 1,
    kCalendar = 2,
    kMaxValue = kCalendar
  };

  PostLoginGlanceablesMetricsRecorder();
  ~PostLoginGlanceablesMetricsRecorder() override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;

  void RecordPostLoginFullRestoreShown();
  void RecordCalendarFetch();

 private:
  void RecordHypotheticalFetchEvent(DataFetchEventSource source);

  std::optional<base::Time> fifteen_second_timestamp_;
  std::optional<base::Time> thirty_second_timestamp_;
  std::optional<base::Time> five_minute_timestamp_;
  std::optional<base::Time> fifteen_minute_timestamp_;
  std::optional<base::Time> thirty_minute_timestamp_;

  base::ScopedObservation<OverviewController, OverviewObserver>
      overview_observation_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_POST_LOGIN_GLANCEABLES_METRICS_RECORDER_H_
