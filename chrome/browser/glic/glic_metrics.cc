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
  kMaxValue = kResponseStartWhileHidingOrHidden,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:GlicResponseError)

}  // namespace

GlicMetrics::GlicMetrics(GlicWindowController* window_controller)
    : window_controller_(window_controller) {}

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
    base::UmaHistogramEnumeration("Glic.Response.Error",
                                  Error::kResponseStartWithoutInput);
    return;
  }

  if (!window_controller_->IsShowing()) {
    base::UmaHistogramEnumeration("Glic.Response.Error",
                                  Error::kResponseStartWhileHidingOrHidden);
    return;
  }

  response_started_time_ = base::TimeTicks::Now();
  base::UmaHistogramMediumTimes("Glic.Response.StartTime",
                                response_started_time_ - input_submitted_time_);
  base::RecordAction(base::UserMetricsAction("GlicResponse"));

  // More details metrics.
  bool attached = window_controller_->IsAttached();
  base::UmaHistogramBoolean("Glic.Response.Attached", attached);
}

void GlicMetrics::OnResponseStopped() {
  base::RecordAction(base::UserMetricsAction("GlicResponseStop"));

  if (input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Response.Error",
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
  base::RecordAction(base::UserMetricsAction("GlicSessionEnd"));
}

void GlicMetrics::OnResponseRated(bool positive) {
  base::UmaHistogramBoolean("Glic.Response.Rated", positive);
}

}  // namespace glic
