// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_IMPL_H_

#include "ash/ash_export.h"

#include "ash/style/rounded_container.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_mobile_header_view_impl.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_wifi_header_view_impl.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

namespace ash {

class DetailedViewDelegate;

// This class is an implementation for NetworkDetailedNetworkView.
class ASH_EXPORT NetworkDetailedNetworkViewImpl
    : public NetworkDetailedView,
      public NetworkDetailedNetworkView,
      public NetworkListNetworkHeaderView::Delegate {
 public:
  METADATA_HEADER(NetworkDetailedNetworkViewImpl);

  NetworkDetailedNetworkViewImpl(
      DetailedViewDelegate* detailed_view_delegate,
      NetworkDetailedNetworkView::Delegate* delegate);
  NetworkDetailedNetworkViewImpl(const NetworkDetailedNetworkViewImpl&) =
      delete;
  NetworkDetailedNetworkViewImpl& operator=(
      const NetworkDetailedNetworkViewImpl&) = delete;
  ~NetworkDetailedNetworkViewImpl() override;

 private:
  friend class NetworkDetailedNetworkViewTest;

  // NetworkDetailedNetworkView:
  void NotifyNetworkListChanged() override;
  views::View* GetAsView() override;
  NetworkListNetworkItemView* AddNetworkListItem(
      chromeos::network_config::mojom::NetworkType type) override;
  HoverHighlightView* AddJoinNetworkEntry() override;
  NetworkListMobileHeaderView* AddMobileSectionHeader() override;
  NetworkListWifiHeaderView* AddWifiSectionHeader() override;
  void UpdateScanningBarVisibility(bool visible) override;
  views::View* GetNetworkList(
      chromeos::network_config::mojom::NetworkType type) override;
  void ReorderFirstListView(size_t index) override;
  void ReorderNetworkTopContainer(size_t index) override;
  void ReorderNetworkListView(size_t index) override;
  void ReorderMobileTopContainer(size_t index) override;
  void ReorderMobileListView(size_t index) override;
  void MaybeRemoveFirstListView() override;

  // NetworkListNetworkHeaderView::Delegate:
  void OnMobileToggleClicked(bool new_state) override;
  void OnWifiToggleClicked(bool new_state) override;

  // Owned by the views hierarchy. These are the containers to carry the warning
  // message, the ethernet entry, the mobile header, mobile network entries,
  // wifi header, and wifi network entries. These containers are only used and
  // added to the `network_list_` when the `features::IsQsRevampEnabled()` is
  // true.
  RoundedContainer* first_list_view_ = nullptr;
  RoundedContainer* mobile_top_container_ = nullptr;
  RoundedContainer* mobile_network_list_view_ = nullptr;
  RoundedContainer* wifi_top_container_ = nullptr;
  RoundedContainer* wifi_network_list_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
