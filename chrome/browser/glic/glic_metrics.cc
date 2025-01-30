// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"

namespace glic {

GlicMetrics::GlicMetrics() = default;

GlicMetrics::~GlicMetrics() {
  // Destructor implies session termination.
  OnSessionTerminated();
}

void GlicMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  input_submitted_time_ = base::TimeTicks::Now();
  input_mode_ = mode;
}

void GlicMetrics::OnResponseStarted() {
  // It doesn't make sense to record response start without input submission.
  if (input_submitted_time_.is_null()) {
    return;
  }

  response_started_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_MEDIUM_TIMES("Glic.Response.StartTime",
                             response_started_time_ - input_submitted_time_);
}

void GlicMetrics::OnResponseStopped() {
  if (!input_submitted_time_.is_null()) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction("GlicResponse"));
    UMA_HISTOGRAM_MEDIUM_TIMES("Glic.Response.StopTime",
                               now - input_submitted_time_);
  }

  // Reset all times.
  input_submitted_time_ = base::TimeTicks();
  response_started_time_ = base::TimeTicks();
}

void GlicMetrics::OnSessionTerminated() {
  base::RecordAction(base::UserMetricsAction("GlicSessionEnd"));

  // Implicitly implies OnResponseStopped().
  OnResponseStopped();
}

void GlicMetrics::OnResponseRated(bool positive) {
  UMA_HISTOGRAM_BOOLEAN("Glic.Response.Rated", positive);
}

}  // namespace glic
