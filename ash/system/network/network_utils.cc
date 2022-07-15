// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace ash {

namespace {

std::string GetNetworkTypeName(
    chromeos::network_config::mojom::NetworkType network_type) {
  switch (network_type) {
    case chromeos::network_config::mojom::NetworkType::kCellular:
      [[fallthrough]];
    case chromeos::network_config::mojom::NetworkType::kTether:
      [[fallthrough]];
    case chromeos::network_config::mojom::NetworkType::kMobile:
      return "Mobile";
    case chromeos::network_config::mojom::NetworkType::kWiFi:
      return "WiFi";
    default:
      // A network type of other is unexpected, and no success
      // metric for it exists.
      NOTREACHED();
      return "";
  }
}

}  // namespace

void RecordNetworkRowClickedAction(NetworkRowClickedAction action) {
  base::UmaHistogramEnumeration("ChromeOS.SystemTray.Network.RowClickedAction",
                                action);
}

void RecordDetailedViewSection(DetailedViewSection section) {
  base::UmaHistogramEnumeration("ChromeOS.SystemTray.Network.SectionShown",
                                section);
}

void RecordNetworkTypeToggled(
    chromeos::network_config::mojom::NetworkType network_type,
    bool new_state) {
  const std::string network_name = GetNetworkTypeName(network_type);

  DCHECK(!network_name.empty());

  base::UmaHistogramBoolean(
      base::StrCat({"ChromeOS.SystemTray.Network.", network_name, ".Toggled"}),
      new_state);
}

}  // namespace ash