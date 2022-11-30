// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace enterprise_connectors {

namespace {

constexpr char kLatencyHistogramFormat[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.%s";

}  // namespace

void LogSignalsCollectionLatency(const char* variant,
                                 base::TimeTicks start_time) {
  base::UmaHistogramTimes(base::StringPrintf(kLatencyHistogramFormat, variant),
                          base::TimeTicks::Now() - start_time);
}

}  // namespace enterprise_connectors
