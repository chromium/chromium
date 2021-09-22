// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_H_
#define ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_H_

#include "base/component_export.h"

namespace ash {
namespace quick_pair {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// FastPairEngagementFlowEvent enum in src/tools/metrics/histograms/enums.xml.
enum COMPONENT_EXPORT(QUICK_PAIR_COMMON) FastPairEngagementFlowEvent {
  kDiscoveryUiShown = 1,
  kDiscoveryUiDismissed = 11,
  kDiscoveryUiConnectPressed = 12,
  kPairingFailed = 121,
  kPairingSucceeded = 122,
  kErrorUiDismissed = 1211,
  kErrorUiSettingsPressed = 1212,
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordFastPairEngagementFlow(FastPairEngagementFlowEvent event);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_LOGGER_H_
