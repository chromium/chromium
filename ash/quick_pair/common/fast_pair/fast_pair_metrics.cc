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
const char kRetroactiveEngagementFlowMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactiveEngagementFunnel.Steps";
const char kPairingMethodMetric[] = "Bluetooth.ChromeOS.FastPair.PairingMethod";
const char kRetroactivePairingResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactivePairing.Result";
const char kTotalGattConnectionTimeMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalGattConnectionTime";
const char kGattConnectionResult[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.Result";
const char kFastPairPairFailureInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.InitialPairingProtocol";
const char kFastPairPairFailureSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.SubsequentPairingProtocol";
const char kFastPairPairFailureRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.RetroactivePairingProtocol";
const char kFastPairPairResultInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.InitialPairingProtocol";
const char kFastPairPairResultSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.SubsequentPairingProtocol";
const char kFastPairPairResultRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteResultInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyWrite.Result.InitialPairingProtocol";
const char kFastPairAccountKeyWriteResultSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyWrite.Result."
    "SubsequentPairingProtocol";
const char kFastPairAccountKeyWriteResultRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyWrite.Result."
    "RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteFailureInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyFailure.InitialPairingProtocol";
const char kFastPairAccountKeyWriteFailureSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyFailure.SubsequentPairingProtocol";
const char kFastPairAccountKeyWriteFailureRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyFailure.RetroactivePairingProtocol";
const char kKeyGenerationResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.KeyGenerationResult";

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

void AttemptRecordingFastPairRetroactiveEngagementFlow(
    const Device& device,
    FastPairRetroactiveEngagementFlowEvent event) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairSubsequent:
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramSparse(kRetroactiveEngagementFlowMetric,
                               static_cast<int>(event));
      break;
  }
}

void RecordPairingMethod(PairingMethod method) {
  base::UmaHistogramEnumeration(kPairingMethodMetric, method);
}

void RecordRetroactivePairingResult(bool success) {
  base::UmaHistogramBoolean(kRetroactivePairingResultMetric, success);
}

void RecordTotalGattConnectionTime(base::TimeDelta total_gatt_connection_time) {
  base::UmaHistogramTimes(kTotalGattConnectionTimeMetric,
                          total_gatt_connection_time);
}

void RecordGattConnectionResult(bool success) {
  base::UmaHistogramBoolean(kGattConnectionResult, success);
}

void RecordPairingResult(const Device& device, bool success) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramBoolean(kFastPairPairResultInitialMetric, success);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramBoolean(kFastPairPairResultRetroactiveMetric, success);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramBoolean(kFastPairPairResultSubsequentMetric, success);
      break;
  }
}

void RecordPairingFailureReason(const Device& device, PairFailure failure) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramSparse(kFastPairPairFailureInitialMetric,
                               static_cast<int>(failure));
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramSparse(kFastPairPairFailureRetroactiveMetric,
                               static_cast<int>(failure));
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramSparse(kFastPairPairFailureSubsequentMetric,
                               static_cast<int>(failure));
      break;
  }
}

void RecordAccountKeyFailureReason(const Device& device,
                                   AccountKeyFailure failure) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramSparse(kFastPairAccountKeyWriteFailureInitialMetric,
                               static_cast<int>(failure));
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramSparse(kFastPairAccountKeyWriteFailureRetroactiveMetric,
                               static_cast<int>(failure));
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramSparse(kFastPairAccountKeyWriteFailureSubsequentMetric,
                               static_cast<int>(failure));
      break;
  }
}

void RecordAccountKeyResult(const Device& device, bool success) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultInitialMetric,
                                success);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultRetroactiveMetric,
                                success);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultSubsequentMetric,
                                success);
      break;
  }
}

void RecordKeyPairGenerationResult(bool success) {
  base::UmaHistogramBoolean(kKeyGenerationResultMetric, success);
}

}  // namespace quick_pair
}  // namespace ash
