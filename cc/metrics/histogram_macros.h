// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_HISTOGRAM_MACROS_H_
#define CC_METRICS_HISTOGRAM_MACROS_H_

#include "base/metrics/histogram_macros.h"

// This macro standardizes how percentage histograms are represented in
// cc/metrics by specifying the min, max, and bucket counts to 1, 101, and 102
// respectively.
#define STATIC_HISTOGRAM_PERCENTAGE_POINTER_GROUP(                       \
    constant_histogram_name, index, constant_maximum, percentage)        \
  STATIC_HISTOGRAM_POINTER_GROUP(                                        \
      constant_histogram_name, index, constant_maximum, Add(percentage), \
      base::LinearHistogram::FactoryGet(                                 \
          constant_histogram_name, 1, 101, 102,                          \
          base::HistogramBase::kUmaTargetedHistogramFlag));

#endif  // CC_METRICS_HISTOGRAM_MACROS_H_
