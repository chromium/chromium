// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace enterprise_connectors {

namespace {
constexpr char kLatencyHistogramFormat[] =
    "Enterprise.DeviceTrust.Attestation.ResponseLatency.%s";
}  // namespace

void LogAttestationFunnelStep(DTAttestationFunnelStep step) {
  base::UmaHistogramEnumeration("Enterprise.DeviceTrust.Attestation.Funnel",
                                step);
}

void LogAttestationResult(DTAttestationResult result) {
  base::UmaHistogramEnumeration("Enterprise.DeviceTrust.Attestation.Result",
                                result);
}

void LogAttestationResponseLatency(base::TimeTicks start_time, bool success) {
  base::UmaHistogramTimes(base::StringPrintf(kLatencyHistogramFormat,
                                             success ? "Success" : "Failure"),
                          base::TimeTicks::Now() - start_time);
}

}  // namespace enterprise_connectors
