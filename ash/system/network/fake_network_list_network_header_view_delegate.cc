// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_list_network_header_view_delegate.h"

namespace ash {

FakeNetworkListNetworkHeaderViewDelegate::
    FakeNetworkListNetworkHeaderViewDelegate() {}
FakeNetworkListNetworkHeaderViewDelegate::
    ~FakeNetworkListNetworkHeaderViewDelegate() = default;

void FakeNetworkListNetworkHeaderViewDelegate::OnMobileToggleClicked(
    bool new_state) {
  mobile_toggle_clicked_count_++;
}

void FakeNetworkListNetworkHeaderViewDelegate::OnWifiToggleClicked(
    bool new_state) {
  wifi_toggle_clicked_count_++;
}

}  // namespace ash
