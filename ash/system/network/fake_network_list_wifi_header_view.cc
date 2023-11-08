// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_list_wifi_header_view.h"

#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

FakeNetworkListWifiHeaderView::FakeNetworkListWifiHeaderView(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListWifiHeaderView(delegate) {}

FakeNetworkListWifiHeaderView::~FakeNetworkListWifiHeaderView() = default;

void FakeNetworkListWifiHeaderView::SetToggleState(bool enabled,
                                                   bool is_on,
                                                   bool animate_toggle) {
  is_toggle_enabled_ = enabled;
  is_toggle_on_ = is_on;
  set_toggle_state_count_++;
}

BEGIN_METADATA(FakeNetworkListWifiHeaderView)
END_METADATA

}  // namespace ash
