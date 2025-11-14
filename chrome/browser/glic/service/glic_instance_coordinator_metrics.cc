// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_coordinator_metrics.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/glic/public/glic_instance.h"

namespace glic {

GlicInstanceCoordinatorMetrics::GlicInstanceCoordinatorMetrics(
    DataProvider* data_provider)
    : data_provider_(data_provider) {}

GlicInstanceCoordinatorMetrics::~GlicInstanceCoordinatorMetrics() {
  EndConcurrentVisibility();
}

void GlicInstanceCoordinatorMetrics::OnInstanceVisibilityChanged() {
  int visible_count = GetVisibleInstanceCount();

  if (visible_count >= 2) {
    // We are in a concurrent visibility period.
    if (!concurrent_visibility_start_time_.has_value()) {
      // Start new concurrent visibility period.
      concurrent_visibility_start_time_ = base::TimeTicks::Now();
      concurrent_visibility_peak_count_ = visible_count;
    } else {
      // Update peak if needed.
      concurrent_visibility_peak_count_ =
          std::max(concurrent_visibility_peak_count_, visible_count);
    }
  } else {
    EndConcurrentVisibility();
  }
}

int GlicInstanceCoordinatorMetrics::GetVisibleInstanceCount() const {
  int count = 0;
  for (const auto* instance : data_provider_->GetInstances()) {
    if (instance->IsShowing()) {
      count++;
    }
  }
  return count;
}

void GlicInstanceCoordinatorMetrics::EndConcurrentVisibility() {
  if (concurrent_visibility_start_time_.has_value()) {
    // We were in a concurrent visibility period, but now we are not. End it.
    base::TimeDelta duration =
        base::TimeTicks::Now() - concurrent_visibility_start_time_.value();
    base::UmaHistogramLongTimes("Glic.ConcurrentVisibility.Duration", duration);
    base::UmaHistogramExactLinear("Glic.ConcurrentVisibility.PeakCount",
                                  concurrent_visibility_peak_count_, 10);

    // Reset state.
    concurrent_visibility_start_time_.reset();
    concurrent_visibility_peak_count_ = 0;
  }
}

}  // namespace glic
