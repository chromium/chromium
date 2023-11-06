// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_tether_hosts_header_view.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_list_network_header_view.h"

namespace ash {

// TODO(b/296865391): disable On/Off switch on header
NetworkListTetherHostsHeaderView::NetworkListTetherHostsHeaderView(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListNetworkHeaderView(delegate,
                                   IDS_ASH_STATUS_TRAY_NETWORK_TETHER_HOSTS,
                                   kUnifiedMenuSignalCellular0Icon) {
  DCHECK(base::FeatureList::IsEnabled(features::kInstantHotspotRebrand));
}

NetworkListTetherHostsHeaderView::~NetworkListTetherHostsHeaderView() = default;

}  // namespace ash
