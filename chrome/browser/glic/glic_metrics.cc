// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/glic_window_controller.h"

namespace glic {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(Error)
enum class Error {
  kResponseStartWithoutInput = 0,
  kResponseStopWithoutInput = 1,
  kResponseStartWhileHidingOrHidden = 2,
  kWindowCloseWithoutWindowOpen = 3,
  kMaxValue = kWindowCloseWithoutWindowOpen,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicResponseError)

}  // namespace

GlicMetrics::GlicMetrics() = default;
GlicMetrics::~GlicMetrics() = default;

void GlicMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  base::RecordAction(base::UserMetricsAction("GlicResponseInputSubmit"));
  input_submitted_time_ = base::TimeTicks::Now();
  input_mode_ = mode;
}

void GlicMetrics::OnResponseStarted() {
  base::RecordAction(base::UserMetricsAction("GlicResponseStart"));

  // It doesn't make sense to record response start without input submission.
  if (input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStartWithoutInput);
    return;
  }

  if (!controller_->IsShowing()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStartWhileHidingOrHidden);
    return;
  }

  response_started_time_ = base::TimeTicks::Now();
  base::UmaHistogramMediumTimes("Glic.Response.StartTime",
                                response_started_time_ - input_submitted_time_);
  switch (input_mode_) {
    case mojom::WebClientMode::kUnknown:
      base::UmaHistogramMediumTimes(
          "Glic.Response.StartTime.InputMode.Unknown",
          response_started_time_ - input_submitted_time_);
      break;
    case mojom::WebClientMode::kText:
      base::UmaHistogramMediumTimes(
          "Glic.Response.StartTime.InputMode.Text",
          response_started_time_ - input_submitted_time_);
      break;
    case mojom::WebClientMode::kAudio:
      base::UmaHistogramMediumTimes(
          "Glic.Response.StartTime.InputMode.Audio",
          response_started_time_ - input_submitted_time_);
      break;
  }
  base::RecordAction(base::UserMetricsAction("GlicResponse"));
  ++session_responses_;

  // More details metrics.
  bool attached = controller_->IsAttached();
  base::UmaHistogramBoolean("Glic.Response.Attached", attached);
}

void GlicMetrics::OnResponseStopped() {
  base::RecordAction(base::UserMetricsAction("GlicResponseStop"));

  if (input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStopWithoutInput);
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    base::UmaHistogramMediumTimes("Glic.Response.StopTime",
                                  now - input_submitted_time_);
  }

  // Reset all times.
  input_submitted_time_ = base::TimeTicks();
  response_started_time_ = base::TimeTicks();
}

void GlicMetrics::OnSessionTerminated() {
  base::RecordAction(base::UserMetricsAction("GlicWebClientSessionEnd"));
}

void GlicMetrics::OnResponseRated(bool positive) {
  base::UmaHistogramBoolean("Glic.Response.Rated", positive);
}

void GlicMetrics::OnGlicWindowOpen() {
  base::RecordAction(base::UserMetricsAction("GlicSessionBegin"));
  session_start_time_ = base::TimeTicks::Now();
}

void GlicMetrics::OnGlicWindowClose() {
  base::RecordAction(base::UserMetricsAction("GlicSessionEnd"));
  base::UmaHistogramCounts1000("Glic.Session.ResponseCount",
                               session_responses_);
  if (session_start_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kWindowCloseWithoutWindowOpen);
  } else {
    base::TimeDelta open_time = base::TimeTicks() - session_start_time_;
    base::UmaHistogramCustomTimes("Glic.Session.Duration", open_time,
                                  /*min=*/base::Seconds(1),
                                  /*max=*/base::Days(10), /*buckets=*/50);
  }
  session_responses_ = 0;
  session_start_time_ = base::TimeTicks();
}

void GlicMetrics::SetWindowController(GlicWindowController* controller) {
  controller_ = controller;
}

}  // namespace glic
