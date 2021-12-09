// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"

#include "ash/quick_pair/common/device.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
namespace {

const char kEngagementFlowInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps.InitialPairingProtocol";
const char kEngagementFlowSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps."
    "SubsequentPairingProtocol";
const char kTotalUxPairTimeInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.InitialPairingProtocol";
const char kTotalUxPairTimeSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.SubsequentPairingProtocol";

}  // namespace

namespace ash {
namespace quick_pair {

void AttemptRecordingFastPairEngagementFlow(const Device& device,
                                            FastPairEngagementFlowEvent event) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramSparse(kEngagementFlowInitialMetric,
                               static_cast<int>(event));
      break;
    case Protocol::kFastPairRetroactive:
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramSparse(kEngagementFlowSubsequentMetric,
                               static_cast<int>(event));
      break;
  }
}

void AttemptRecordingTotalUxPairTime(const Device& device,
                                     base::TimeDelta total_pair_time) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramTimes(kTotalUxPairTimeInitialMetric, total_pair_time);
      break;
    case Protocol::kFastPairRetroactive:
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramTimes(kTotalUxPairTimeSubsequentMetric,
                              total_pair_time);
      break;
  }
}

}  // namespace quick_pair
}  // namespace ash
