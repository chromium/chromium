// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_cui_tracker.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace glic {

GlicCuiTracker::GlicCuiTracker() : start_time_(base::TimeTicks::Now()) {}

GlicCuiTracker::~GlicCuiTracker() = default;

void GlicCuiTracker::Resolve(GlicCuiOutcome reason) {
  if (is_resolved_) {
    return;
  }
  is_resolved_ = true;
  base::TimeDelta duration = base::TimeTicks::Now() - start_time_;

  if (reason == GlicCuiOutcome::kSuccess) {
    if (duration > GetMaxLatency()) {
      reason = GlicCuiOutcome::kFailedLatency;
    }
  }

  const char* metric_prefix = GetMetricName();

  base::UmaHistogramEnumeration(base::StrCat({metric_prefix, ".Outcome"}),
                                reason);

  if (reason != GlicCuiOutcome::kUnknownCancel) {
    std::string reason_suffix;
    switch (reason) {
      case GlicCuiOutcome::kSuccess:
        reason_suffix = ".Success";
        break;
      case GlicCuiOutcome::kAbandoned:
        reason_suffix = ".Abandoned";
        break;
      case GlicCuiOutcome::kFailed:
        reason_suffix = ".Failed";
        break;
      case GlicCuiOutcome::kFailedLatency:
        reason_suffix = ".FailedLatency";
        break;
      default:
        break;
    }

    base::UmaHistogramCustomTimes(base::StrCat({metric_prefix, ".Latency"}),
                                  duration, base::Milliseconds(1),
                                  GetHistogramMax(), 50);
    base::UmaHistogramCustomTimes(
        base::StrCat({metric_prefix, ".Latency", reason_suffix}), duration,
        base::Milliseconds(1), GetHistogramMax(), 50);
  }
}

bool GlicCuiTracker::OnEvent(GlicInstanceEvent event) {
  if (is_resolved_) {
    return true;
  }

  std::optional<GlicCuiOutcome> outcome = GetEventOutcome(event);
  if (outcome.has_value()) {
    Resolve(*outcome);
  }
  return is_resolved_;
}

std::optional<GlicCuiOutcome> GlicCuiTracker::GetEventOutcome(
    GlicInstanceEvent event) const {
  return std::nullopt;
}

base::TimeDelta GlicCuiTracker::GetHistogramMax() const {
  // TODO(crbug.com/524764084): Re-evaluate this default value based on metrics.
  return base::Minutes(3);
}

base::TimeDelta GlicCuiTracker::GetMaxLatency() const {
  return base::Seconds(60);
}

}  // namespace glic
