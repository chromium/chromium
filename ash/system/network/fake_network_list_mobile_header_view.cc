// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_list_mobile_header_view.h"

#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"

namespace ash {

FakeNetworkListMobileHeaderView::FakeNetworkListMobileHeaderView(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListMobileHeaderView(delegate) {}

FakeNetworkListMobileHeaderView::~FakeNetworkListMobileHeaderView() = default;

void FakeNetworkListMobileHeaderView::SetToggleState(bool enabled,
                                                     bool is_on,
                                                     bool animate_toggle) {
  is_toggle_enabled_ = enabled;
  is_toggle_on_ = is_on;
  set_toggle_state_count_++;
}

}  // namespace ash
