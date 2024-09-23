// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_detailed_network_view.h"

#include "ash/system/network/fake_network_list_tether_hosts_header_view.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_item_view.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_view_controller_impl.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

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
  std::unique_ptr<NetworkListWifiHeaderView> wifi_header_view =
      std::make_unique<NetworkListWifiHeaderView>(/*delegate=*/nullptr);
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
  std::unique_ptr<NetworkListMobileHeaderView> mobile_header_view =
      std::make_unique<NetworkListMobileHeaderView>(/*delegate=*/nullptr);
  mobile_header_view->SetID(static_cast<int>(
      NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
          kMobileSectionHeader));

  return network_list_->AddChildView(std::move(mobile_header_view));
}

NetworkListTetherHostsHeaderView*
FakeNetworkDetailedNetworkView::AddTetherHostsSectionHeader(
    NetworkListTetherHostsHeaderView::OnExpandedStateToggle callback) {
  std::unique_ptr<FakeNetworkListTetherHostsHeaderView>
      tether_hosts_header_view =
          std::make_unique<FakeNetworkListTetherHostsHeaderView>(
              std::move(callback));
  tether_hosts_header_view->SetID(static_cast<int>(
      NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
          kTetherHostsSectionHeader));

  return network_list_->AddChildView(std::move(tether_hosts_header_view));
}

void FakeNetworkDetailedNetworkView::UpdateScanningBarVisibility(bool visible) {
  last_scan_bar_visibility_ = visible;
}

BEGIN_METADATA(FakeNetworkDetailedNetworkView)
END_METADATA

}  // namespace ash
