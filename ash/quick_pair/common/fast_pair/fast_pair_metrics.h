// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_H_
#define ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_H_

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

struct Device;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// FastPairEngagementFlowEvent enum in src/tools/metrics/histograms/enums.xml.
enum COMPONENT_EXPORT(QUICK_PAIR_COMMON) FastPairEngagementFlowEvent {
  kDiscoveryUiShown = 1,
  kDiscoveryUiDismissed = 11,
  kDiscoveryUiConnectPressed = 12,
  kDiscoveryUiDismissedByUser = 13,
  kPairingFailed = 121,
  kPairingSucceeded = 122,
  kErrorUiDismissed = 1211,
  kErrorUiSettingsPressed = 1212,
  kErrorUiDismissedByUser = 1213,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// FastPairRetroactiveEngagementFlowEvent enum in
// src/tools/metrics/histograms/enums.xml.
enum COMPONENT_EXPORT(QUICK_PAIR_COMMON)
    FastPairRetroactiveEngagementFlowEvent {
      kAssociateAccountUiShown = 1,
      kAssociateAccountUiDismissedByUser = 11,
      kAssociateAccountUiDismissed = 12,
      kAssociateAccountLearnMorePressed = 13,
      kAssociateAccountSavePressed = 14,
      kAssociateAccountSavePressedAfterLearnMorePressed = 131,
      kAssociateAccountDismissedByUserAfterLearnMorePressed = 132,
      kAssociateAccountDismissedAfterLearnMorePressed = 133,
    };

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync
// with the FastPairPairingMethod enum in
// src/tools/metrics/histograms/enums.xml.
enum class COMPONENT_EXPORT(QUICK_PAIR_COMMON) PairingMethod {
  kFastPair = 0,
  kSystemPairingUi = 1,
  kMaxValue = kSystemPairingUi,
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void AttemptRecordingFastPairEngagementFlow(const Device& device,
                                            FastPairEngagementFlowEvent event);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void AttemptRecordingTotalUxPairTime(const Device& device,
                                     base::TimeDelta total_pair_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void AttemptRecordingFastPairRetroactiveEngagementFlow(
    const Device& device,
    FastPairRetroactiveEngagementFlowEvent event);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairingMethod(PairingMethod method);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordRetroactivePairingResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordTotalGattConnectionTime(base::TimeDelta total_gatt_connection_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordGattConnectionResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairingFailureReason(const Device& device, PairFailure failure);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairingResult(const Device& device, bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordAccountKeyFailureReason(const Device& device,
                                   AccountKeyFailure failure);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordAccountKeyResult(const Device& device, bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordKeyPairGenerationResult(bool success);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_H_
