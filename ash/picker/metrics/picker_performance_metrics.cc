// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_performance_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/views/widget/widget.h"

namespace ash {

PickerPerformanceMetrics::PickerPerformanceMetrics(
    const base::TimeTicks trigger_start_timestamp)
    : trigger_start_timestamp_(trigger_start_timestamp) {}

PickerPerformanceMetrics::~PickerPerformanceMetrics() = default;

void PickerPerformanceMetrics::StartRecording(views::Widget& widget) {
  // Initialize presentation time recorders based on the new widget's
  // compositor. After this, a presentation latency metric is recorded every
  // time `RequestNext` is called on the recorder.
  search_field_presentation_time_recorder_ =
      CreatePresentationTimeHistogramRecorder(
          widget.GetCompositor(),
          "Ash.Picker.Session.PresentationLatency.SearchField");
  results_presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      widget.GetCompositor(),
      "Ash.Picker.Session.PresentationLatency.SearchResults");

  is_recording_ = true;
}

void PickerPerformanceMetrics::StopRecording() {
  is_recording_ = false;
}

void PickerPerformanceMetrics::MarkInputFocus() {
  if (!is_recording_ || marked_first_focus_) {
    return;
  }

  base::UmaHistogramCustomTimes(
      "Ash.Picker.Session.InputReadyLatency",
      /*sample=*/base::TimeTicks::Now() - trigger_start_timestamp_,
      /*min=*/base::Seconds(0), /*max=*/base::Seconds(10), /*buckets=*/100);
  marked_first_focus_ = true;
}

void PickerPerformanceMetrics::MarkContentsChanged() {
  if (!is_recording_) {
    return;
  }

  if (search_field_presentation_time_recorder_ != nullptr) {
    search_field_presentation_time_recorder_->RequestNext();
  }

  search_start_timestamp_ = base::TimeTicks::Now();
}

// TODO: b/349913604 - Handle the different types of `update`.
void PickerPerformanceMetrics::MarkSearchResultsUpdated(
    SearchResultsUpdate update) {
  if (!is_recording_) {
    return;
  }

  if (results_presentation_time_recorder_ != nullptr) {
    results_presentation_time_recorder_->RequestNext();
  }

  // The below metrics were written before "no results found" called this
  // method.
  // TODO: b/349913604 - Replace this metric with new ones which record when
  // "no search results found" is shown.
  if (update == SearchResultsUpdate::kNoResultsFound) {
    return;
  }

  if (search_start_timestamp_.has_value()) {
    base::UmaHistogramCustomTimes(
        "Ash.Picker.Session.SearchLatency",
        /*sample=*/base::TimeTicks::Now() - *search_start_timestamp_,
        /*min=*/base::Seconds(0), /*max=*/base::Seconds(10), /*buckets=*/100);

    search_start_timestamp_.reset();
  }
}

}  // namespace ash
