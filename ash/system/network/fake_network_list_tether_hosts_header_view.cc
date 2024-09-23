// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_list_tether_hosts_header_view.h"

#include "ash/constants/ash_features.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_tether_hosts_header_view.h"
#include "base/feature_list.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

FakeNetworkListTetherHostsHeaderView::FakeNetworkListTetherHostsHeaderView(
    OnExpandedStateToggle callback)
    : NetworkListTetherHostsHeaderView(std::move(callback)) {
  DCHECK(base::FeatureList::IsEnabled(ash::features::kInstantHotspotRebrand));
}

FakeNetworkListTetherHostsHeaderView::~FakeNetworkListTetherHostsHeaderView() =
    default;

BEGIN_METADATA(FakeNetworkListTetherHostsHeaderView)
END_METADATA

}  // namespace ash
