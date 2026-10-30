// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/metrics_logger.h"

#include "base/metrics/histogram_functions.h"

namespace selection {

MetricsLogger::MetricsLogger(EntryPoint entry_point) {
  base::UmaHistogramEnumeration("SmartSelection.Invocation.ByEntryPoint",
                                entry_point);
}

MetricsLogger::~MetricsLogger() = default;

}  // namespace selection
