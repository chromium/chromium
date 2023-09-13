// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_list_tether_hosts_header_view.h"

#include "ash/constants/ash_features.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_tether_hosts_header_view.h"
#include "base/feature_list.h"

namespace ash {

FakeNetworkListTetherHostsHeaderView::FakeNetworkListTetherHostsHeaderView(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListTetherHostsHeaderView(delegate) {
  DCHECK(base::FeatureList::IsEnabled(ash::features::kInstantHotspotRebrand));
}

FakeNetworkListTetherHostsHeaderView::~FakeNetworkListTetherHostsHeaderView() =
    default;

}  // namespace ash
