// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/metadata_processor_ash.h"
#include "chrome/browser/policy/management_utils.h"

namespace metrics::structured {

bool MetadataProcessorAsh::ShouldProcessOnEventRecord(const Event& event) {
  return true;
}

void MetadataProcessorAsh::OnEventsRecord(Event* event) {
  // no-op.
}

void MetadataProcessorAsh::OnProvideIndependentMetrics(
    ChromeUserMetricsExtension* uma_proto) {
  auto* structured_metrics = uma_proto->mutable_structured_data();
  structured_metrics->set_is_device_enrolled(IsDeviceEnrolled());
}

bool MetadataProcessorAsh::IsDeviceEnrolled() {
  return policy::IsDeviceEnterpriseManaged();
}

}  // namespace metrics::structured
