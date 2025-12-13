// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace glic {

GlicInstanceHelperMetrics::GlicInstanceHelperMetrics() = default;

GlicInstanceHelperMetrics::~GlicInstanceHelperMetrics() {
  if (!bound_instances_.empty()) {
    base::UmaHistogramCounts100("Glic.Tab.InstanceBindCount",
                                bound_instances_.size());
  }
  if (!pinned_by_instances_.empty()) {
    base::UmaHistogramCounts100("Glic.Tab.InstancePinCount",
                                pinned_by_instances_.size());
  }
  if (is_daisy_chained_) {
    FlushMetric();
  }
}

void GlicInstanceHelperMetrics::OnBoundToInstance(
    const InstanceId& instance_id) {
  bound_instances_.insert(instance_id);
}

void GlicInstanceHelperMetrics::OnPinnedByInstance(
    const InstanceId& instance_id) {
  pinned_by_instances_.insert(instance_id);
}

void GlicInstanceHelperMetrics::SetIsDaisyChained() {
  if (is_daisy_chained_) {
    return;
  }
  is_daisy_chained_ = true;
  metric_finalized_ = false;
  current_metric_action_ = DaisyChainFirstAction::kNoAction;
  flush_timer_.Stop();
}

void GlicInstanceHelperMetrics::OnDaisyChainAction(
    DaisyChainFirstAction action) {
  if (!is_daisy_chained_ || metric_finalized_) {
    return;
  }

  current_metric_action_ = action;

  if (action == DaisyChainFirstAction::kSidePanelClosed) {
    // Ambiguous action. Start/restart timer.
    flush_timer_.Start(FROM_HERE, base::Seconds(5), this,
                       &GlicInstanceHelperMetrics::FlushMetric);
  } else {
    // Terminal action. Flush immediately.
    FlushMetric();
  }
}

void GlicInstanceHelperMetrics::FlushMetric() {
  if (metric_finalized_) {
    return;
  }
  base::UmaHistogramEnumeration("Glic.Instance.FirstActionInDaisyChainPanel",
                                current_metric_action_);
  metric_finalized_ = true;
  flush_timer_.Stop();
}

}  // namespace glic
