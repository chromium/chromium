// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_detailed_view_delegate.h"

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

namespace {

using chromeos::network_config::mojom::NetworkStatePropertiesPtr;

}

FakeNetworkDetailedViewDelegate::FakeNetworkDetailedViewDelegate() {}
FakeNetworkDetailedViewDelegate::~FakeNetworkDetailedViewDelegate() = default;

void FakeNetworkDetailedViewDelegate::OnNetworkListItemSelected(
    const NetworkStatePropertiesPtr& network) {
  last_network_list_item_selected_ = mojo::Clone(network);
}

}  // namespace ash
