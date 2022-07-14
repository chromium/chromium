// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_utils.h"

#include "base/metrics/histogram_functions.h"

namespace ash {

void RecordNetworkRowClickedAction(NetworkRowClickedAction action) {
  base::UmaHistogramEnumeration("ChromeOS.SystemTray.Network.RowClickedAction",
                                action);
}

void RecordDetailedViewSection(DetailedViewSection section) {
  base::UmaHistogramEnumeration("ChromeOS.SystemTray.Network.SectionShown",
                                section);
}

}  // namespace ash