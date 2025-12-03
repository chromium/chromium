// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"

#include "base/metrics/histogram_functions.h"

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
}

void GlicInstanceHelperMetrics::OnBoundToInstance(
    const InstanceId& instance_id) {
  bound_instances_.insert(instance_id);
}

void GlicInstanceHelperMetrics::OnPinnedByInstance(
    const InstanceId& instance_id) {
  pinned_by_instances_.insert(instance_id);
}

}  // namespace glic
