// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_METRICS_PICKER_PERFORMANCE_METRICS_H_
#define ASH_PICKER_METRICS_PICKER_PERFORMANCE_METRICS_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ui {
class PresentationTimeRecorder;
}

namespace views {
class Widget;
}

namespace ash {

// Records performance metrics for a session of using Picker, such as latency.
class ASH_EXPORT PickerPerformanceMetrics {
 public:
  enum class SearchResultsUpdate {
    // Stale search results were cleared to an empty list of results.
    kClear,
    // Stale search results were replaced with a new non-empty list of results.
    kReplace,
    // The "no results found" message was shown.
    kNoResultsFound,
    // New results were appended to the (possibly empty) list of results.
    kAppend,
  };

  // `trigger_event_timestamp` is the timestamp of the event that triggered the
  // session. By default, this uses the time PickerPerformanceMetrics is
  // created. Call `StartRecording` to start recording metrics for the session.
  PickerPerformanceMetrics(
      base::TimeTicks trigger_start_timestamp = base::TimeTicks::Now());
  ~PickerPerformanceMetrics();

  // Starts recording metrics for this session.
  // `widget` is the widget that contains the Picker UI. It must outlive this
  // class.
  void StartRecording(views::Widget& widget);

  // Stops recording metrics for this session.
  // This should be called, for example, when the widget containing the Picker
  // UI is destroyed.
  void StopRecording();

  // Marks a focus event on the picker search field.
  void MarkInputFocus();

  // Marks that the search field contents changed.
  void MarkContentsChanged();

  // Marks that the search results were updated.
  void MarkSearchResultsUpdated(SearchResultsUpdate update);

 private:
  bool is_recording_ = false;

  // The timestamp of earliest the feature was triggered.
  base::TimeTicks trigger_start_timestamp_;

  // Whether the first input focus has been marked yet.
  bool marked_first_focus_ = false;

  // The timestamp of when the current search started.
  std::optional<base::TimeTicks> search_start_timestamp_;

  // Records the presentation delay when search field contents change.
  std::unique_ptr<ui::PresentationTimeRecorder>
      search_field_presentation_time_recorder_;

  // Records the presentation delay of updating the results page.
  std::unique_ptr<ui::PresentationTimeRecorder>
      results_presentation_time_recorder_;
};

}  // namespace ash

#endif  // ASH_PICKER_METRICS_PICKER_PERFORMANCE_METRICS_H_
