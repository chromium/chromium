// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_list_wifi_header_view.h"

#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_wifi_header_view.h"

namespace ash {

FakeNetworkListWifiHeaderView::FakeNetworkListWifiHeaderView(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListWifiHeaderView(delegate) {}

FakeNetworkListWifiHeaderView::~FakeNetworkListWifiHeaderView() = default;

void FakeNetworkListWifiHeaderView::SetToggleState(bool enabled, bool visible) {
  is_toggle_enabled_ = enabled;
  is_toggle_visible_ = visible;
  set_toggle_state_count_++;
};

void FakeNetworkListWifiHeaderView::SetJoinWifiButtonState(bool enabled,
                                                           bool visible) {
  is_join_wifi_enabled_ = enabled;
  is_join_wifi_visible_ = visible;
  set_join_wifi_button_state_count_++;
};

}  // namespace ash