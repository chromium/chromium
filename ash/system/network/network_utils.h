// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_UTILS_H_
#define ASH_SYSTEM_NETWORK_NETWORK_UTILS_H_

#include "ash/ash_export.h"

namespace ash {

// This enum is tied directly to a UMA enum |NetworkRowClickedAction| defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class NetworkRowClickedAction {
  kConnectToNetwork = 0,
  kOpenNetworkSettingsPage = 1,
  kOpenSimUnlockDialog = 2,
  kMaxValue = kOpenSimUnlockDialog
};

// This enum is tied directly to a UMA enum |DetailedViewSection| defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class DetailedViewSection {
  kWifiSection = 0,
  kMobileSection = 1,
  kEthernetSection = 2,
  kDetailedSection = 3,
  kMaxValue = kDetailedSection
};

ASH_EXPORT void RecordNetworkRowClickedAction(NetworkRowClickedAction action);

ASH_EXPORT void RecordDetailedViewSection(DetailedViewSection section);

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_UTILS_H_