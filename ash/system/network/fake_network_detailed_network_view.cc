// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_detailed_network_view.h"

#include "ash/system/network/fake_network_list_mobile_header_view.h"
#include "ash/system/network/fake_network_list_wifi_header_view.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_item_view.h"
#include "ash/system/network/network_list_mobile_header_view_impl.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_view_controller_impl.h"
#include "ash/system/network/network_list_wifi_header_view_impl.h"

namespace ash {

namespace {
using ::chromeos::network_config::mojom::NetworkType;
}

FakeNetworkDetailedNetworkView::FakeNetworkDetailedNetworkView(
    Delegate* delegate)
    : NetworkDetailedNetworkView(delegate),
      network_list_(std::make_unique<views::View>()) {}

FakeNetworkDetailedNetworkView::~FakeNetworkDetailedNetworkView() = default;

void FakeNetworkDetailedNetworkView::NotifyNetworkListChanged() {
  notify_network_list_changed_call_count_++;
}

views::View* FakeNetworkDetailedNetworkView::GetNetworkList(NetworkType type) {
  return network_list_.get();
}

views::View* FakeNetworkDetailedNetworkView::GetAsView() {
  return this;
}

void FakeNetworkDetailedNetworkView::OnViewClicked(views::View* view) {
  last_clicked_network_list_item_ = static_cast<NetworkListItemView*>(view);
}

NetworkListNetworkItemView* FakeNetworkDetailedNetworkView::AddNetworkListItem(
    NetworkType type) {
  return network_list_->AddChildView(
      std::make_unique<NetworkListNetworkItemView>(/*listener=*/nullptr));
}

NetworkListWifiHeaderView*
FakeNetworkDetailedNetworkView::AddWifiSectionHeader() {
  std::unique_ptr<FakeNetworkListWifiHeaderView> wifi_header_view =
      std::make_unique<FakeNetworkListWifiHeaderView>(/*delegate=*/nullptr);
  wifi_header_view->SetID(static_cast<int>(
      NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
          kWifiSectionHeader));

  return network_list_->AddChildView(std::move(wifi_header_view));
}

HoverHighlightView* FakeNetworkDetailedNetworkView::AddConfigureNetworkEntry(
    NetworkType type) {
  return nullptr;
}

NetworkListMobileHeaderView*
FakeNetworkDetailedNetworkView::AddMobileSectionHeader() {
  std::unique_ptr<FakeNetworkListMobileHeaderView> mobile_header_view =
      std::make_unique<FakeNetworkListMobileHeaderView>(/*delegate=*/nullptr);
  mobile_header_view->SetID(static_cast<int>(
      NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
          kMobileSectionHeader));

  return network_list_->AddChildView(std::move(mobile_header_view));
}

void FakeNetworkDetailedNetworkView::UpdateScanningBarVisibility(bool visible) {
  last_scan_bar_visibility_ = visible;
}

}  // namespace ash
