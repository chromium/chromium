// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_view_controller.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {

NetworkDetailedViewController::NetworkDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  DCHECK(ash::features::IsQuickSettingsNetworkRevampEnabled());
}

NetworkDetailedViewController::~NetworkDetailedViewController() = default;

views::View* NetworkDetailedViewController::CreateView() {
  DCHECK(!network_detailed_view_);
  std::unique_ptr<NetworkDetailedNetworkView> view =
      NetworkDetailedNetworkView::Factory::Create(detailed_view_delegate_.get(),
                                                  /*delegate=*/this);
  network_detailed_view_ = view.get();
  network_list_view_controller_ =
      NetworkListViewController::Factory::Create(view.get());

  // We are expected to return an unowned pointer that the caller is responsible
  // for deleting.
  return view.release()->GetAsView();
}

std::u16string NetworkDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_NETWORK_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void NetworkDetailedViewController::OnNetworkListItemSelected(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr& network) {
}

void NetworkDetailedViewController::OnMobileToggleClicked(bool new_state) {}

void NetworkDetailedViewController::OnWifiToggleClicked(bool new_state) {}

}  // namespace ash
