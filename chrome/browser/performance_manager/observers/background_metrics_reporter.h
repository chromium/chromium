// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_BACKGROUND_METRICS_REPORTER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_BACKGROUND_METRICS_REPORTER_H_

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/ukm_source_id.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

// Pages can be kept in the background for a long time, metrics show 75th
// percentile of time spent in background is 2.5 hours, and the 95th is 24 hour.
// In order to guide the selection of an appropriate observation window we are
// proposing using a CUSTOM_TIMES histogram from 1s to 48h, with 100 buckets.
#define HEURISTICS_HISTOGRAM(name, sample)                                  \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, base::TimeDelta::FromSeconds(1), \
                             base::TimeDelta::FromHours(48), 100)

namespace performance_manager {

namespace internal {

enum UKMFrameReportType : uint8_t {
  kMainFrameOnly = 0,
  kMainFrameAndChildFrame
};

template <class UKMBuilderClass,
          internal::UKMFrameReportType kShouldReportChildFrameUkm>
class UKMReportDelegate {};

template <class UKMBuilderClass>
class UKMReportDelegate<UKMBuilderClass, internal::kMainFrameOnly> {
 public:
  void ReportUKM(int64_t ukm_source_id,
                 bool is_main_frame,
                 int64_t duration_in_ms,
                 ukm::UkmRecorder* ukm_recorder) {
    UKMBuilderClass ukm_builder(ukm_source_id);
    ukm_builder.SetTimeFromBackgrounded(duration_in_ms).Record(ukm_recorder);
  }
};

template <class UKMBuilderClass>
class UKMReportDelegate<UKMBuilderClass, internal::kMainFrameAndChildFrame> {
 public:
  void ReportUKM(int64_t ukm_source_id,
                 bool is_main_frame,
                 int64_t duration_in_ms,
                 ukm::UkmRecorder* ukm_recorder) {
    UKMBuilderClass ukm_builder(ukm_source_id);
    ukm_builder.SetIsMainFrame(is_main_frame)
        .SetTimeFromBackgrounded(duration_in_ms)
        .Record(ukm_recorder);
  }
};

}  // namespace internal

template <class UKMBuilderClass,
          const char* kMetricName,
          internal::UKMFrameReportType kShouldReportChildFrameUkm>
class BackgroundMetricsReporter {
 public:
  BackgroundMetricsReporter()
      : ukm_source_id_(ukm::kInvalidSourceId),
        uma_reported_(false),
        ukm_reported_(false),
        child_frame_ukm_reported_(false) {}

  void Reset() {
    uma_reported_ = false;
    ukm_reported_ = false;
    child_frame_ukm_reported_ = false;
  }

  void SetUkmSourceID(ukm::SourceId ukm_source_id) {
    ukm_source_id_ = ukm_source_id;
  }

  void OnSignalReceived(bool is_main_frame,
                        base::TimeDelta duration,
                        ukm::UkmRecorder* ukm_recorder) {
    if (!uma_reported_) {
      uma_reported_ = true;
      HEURISTICS_HISTOGRAM(kMetricName, duration);
    }

    ReportUKMIfNeeded(is_main_frame, duration, ukm_recorder);
  }

 private:
  void ReportUKMIfNeeded(bool is_main_frame,
                         base::TimeDelta duration,
                         ukm::UkmRecorder* ukm_recorder) {
    if (ukm_source_id_ == ukm::kInvalidSourceId ||
        (!kShouldReportChildFrameUkm && ukm_reported_) ||
        (kShouldReportChildFrameUkm &&
         !ShouldReportMainFrameUKM(is_main_frame) &&
         !ShouldReportChildFrameUKM(is_main_frame))) {
      return;
    }

    ukm_reporter_.ReportUKM(ukm_source_id_, is_main_frame,
                            duration.InMilliseconds(), ukm_recorder);

    if (is_main_frame) {
      ukm_reported_ = true;
    } else {
      child_frame_ukm_reported_ = true;
    }
  }

  bool ShouldReportMainFrameUKM(bool is_main_frame) const {
    return is_main_frame && !ukm_reported_;
  }

  bool ShouldReportChildFrameUKM(bool is_main_frame) const {
    return !is_main_frame && !child_frame_ukm_reported_;
  }

  ukm::SourceId ukm_source_id_;
  bool uma_reported_;
  bool ukm_reported_;
  bool child_frame_ukm_reported_;
  internal::UKMReportDelegate<UKMBuilderClass, kShouldReportChildFrameUkm>
      ukm_reporter_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_BACKGROUND_METRICS_REPORTER_H_
