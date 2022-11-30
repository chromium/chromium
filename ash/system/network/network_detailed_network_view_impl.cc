// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_network_view_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/network/network_list_mobile_header_view_impl.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_wifi_header_view_impl.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

NetworkDetailedNetworkViewImpl::NetworkDetailedNetworkViewImpl(
    DetailedViewDelegate* detailed_view_delegate,
    NetworkDetailedNetworkView::Delegate* delegate)
    : NetworkDetailedView(detailed_view_delegate,
                          delegate,
                          NetworkDetailedView::ListType::LIST_TYPE_NETWORK),
      NetworkDetailedNetworkView(delegate) {
  DCHECK(ash::features::IsQuickSettingsNetworkRevampEnabled());
  RecordDetailedViewSection(DetailedViewSection::kDetailedSection);
}

NetworkDetailedNetworkViewImpl::~NetworkDetailedNetworkViewImpl() = default;

void NetworkDetailedNetworkViewImpl::NotifyNetworkListChanged() {
  scroll_content()->InvalidateLayout();
  Layout();

  if (!settings_button())
    return;

  if (Shell::Get()->session_controller()->login_status() ==
      LoginStatus::NOT_LOGGED_IN) {
    // When not logged in, only enable the settings button if there is a
    // default (i.e. connected or connecting) network to show settings for.
    settings_button()->SetEnabled(model()->default_network());
  } else {
    // Otherwise, enable if showing settings is allowed. There are situations
    // (supervised user creation flow) when the session is started but UI flow
    // continues within login UI, i.e., no browser window is yet available.
    settings_button()->SetEnabled(
        Shell::Get()->session_controller()->ShouldEnableSettings());
  }
}

views::View* NetworkDetailedNetworkViewImpl::GetAsView() {
  return this;
}

NetworkListNetworkItemView*
NetworkDetailedNetworkViewImpl::AddNetworkListItem() {
  return scroll_content()->AddChildView(
      new NetworkListNetworkItemView(/*listener=*/this));
}

NetworkListWifiHeaderView*
NetworkDetailedNetworkViewImpl::AddWifiSectionHeader() {
  return scroll_content()->AddChildView(
      new NetworkListWifiHeaderViewImpl(/*delegate=*/this));
}

NetworkListMobileHeaderView*
NetworkDetailedNetworkViewImpl::AddMobileSectionHeader() {
  return scroll_content()->AddChildView(
      new NetworkListMobileHeaderViewImpl(/*delegate=*/this));
}

views::View* NetworkDetailedNetworkViewImpl::network_list() {
  return scroll_content();
}

void NetworkDetailedNetworkViewImpl::OnMobileToggleClicked(bool new_state) {
  NetworkDetailedNetworkView::delegate()->OnMobileToggleClicked(new_state);
}

void NetworkDetailedNetworkViewImpl::OnWifiToggleClicked(bool new_state) {
  NetworkDetailedNetworkView::delegate()->OnWifiToggleClicked(new_state);
}

void NetworkDetailedNetworkViewImpl::UpdateScanningBarVisibility(bool visible) {
  ShowProgress(-1, visible);
}

BEGIN_METADATA(NetworkDetailedNetworkViewImpl, views::View)
END_METADATA

}  // namespace ash