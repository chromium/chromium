// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_METRICS_UTILS_H_

#include "base/time/time.h"

namespace enterprise_connectors {

// Logs time elapsed since `start_time` to the signals collection histogram
// dictated by `variant`. See all possible variants, or add new ones, in the
// enterprise/histograms.xml file at this entry:
// Enterprise.DeviceTrust.SignalsDecorator.Latency.{Variant}
void LogSignalsCollectionLatency(const char* variant,
                                 base::TimeTicks start_time);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_METRICS_UTILS_H_
