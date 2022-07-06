// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_detailed_network_view.h"

#include "ash/system/network/fake_network_list_mobile_header_view.h"
#include "ash/system/network/fake_network_list_wifi_header_view.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_item_view.h"
#include "ash/system/network/network_list_mobile_header_view_impl.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_wifi_header_view_impl.h"

namespace ash {

FakeNetworkDetailedNetworkView::FakeNetworkDetailedNetworkView(
    Delegate* delegate)
    : NetworkDetailedNetworkView(delegate),
      network_list_(std::make_unique<views::View>()) {}

FakeNetworkDetailedNetworkView::~FakeNetworkDetailedNetworkView() = default;

void FakeNetworkDetailedNetworkView::NotifyNetworkListChanged() {
  notify_network_list_changed_call_count_++;
}

views::View* FakeNetworkDetailedNetworkView::network_list() {
  return network_list_.get();
};

views::View* FakeNetworkDetailedNetworkView::GetAsView() {
  return this;
}

void FakeNetworkDetailedNetworkView::OnViewClicked(views::View* view) {
  last_clicked_network_list_item_ = static_cast<NetworkListItemView*>(view);
}

NetworkListNetworkItemView*
FakeNetworkDetailedNetworkView::AddNetworkListItem() {
  return network_list_->AddChildView(
      new NetworkListNetworkItemView(/*listener=*/nullptr));
};

NetworkListWifiHeaderView*
FakeNetworkDetailedNetworkView::AddWifiSectionHeader() {
  return network_list_->AddChildView(
      new FakeNetworkListWifiHeaderView(/*delegate=*/nullptr));
};

NetworkListMobileHeaderView*
FakeNetworkDetailedNetworkView::AddMobileSectionHeader() {
  return network_list_->AddChildView(
      new FakeNetworkListMobileHeaderView(/*delegate=*/nullptr));
}

void FakeNetworkDetailedNetworkView::UpdateScanningBarVisibility(bool visible) {
  last_scan_bar_visibility_ = visible;
};

}  // namespace ash