// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_DEVICE_ACTIVITY_DEVICE_ACTIVITY_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_DEVICE_ACTIVITY_DEVICE_ACTIVITY_SAMPLER_H_

#include "components/reporting/metrics/sampler.h"

namespace reporting {

// Sampler used to collect device activity state that is derived from device
// usage. Device idleness is based upon a pre-defined threshold of 5 minutes
// since the last activity.
class DeviceActivitySampler : public Sampler {
 public:
  DeviceActivitySampler() = default;
  DeviceActivitySampler(const DeviceActivitySampler& other) = delete;
  DeviceActivitySampler& operator=(const DeviceActivitySampler& other) = delete;
  ~DeviceActivitySampler() override = default;

  // Collects device activity info and reports this data using the specified
  // callback.
  // Sampler:
  void MaybeCollect(OptionalMetricCallback callback) override;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_DEVICE_ACTIVITY_DEVICE_ACTIVITY_SAMPLER_H_
