// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"

namespace ash {
namespace quick_pair {

void RecordFastPairEngagementFlow(FastPairEngagementFlowEvent event) {
  base::UmaHistogramSparse("Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps",
                           static_cast<int>(event));
}

}  // namespace quick_pair
}  // namespace ash
