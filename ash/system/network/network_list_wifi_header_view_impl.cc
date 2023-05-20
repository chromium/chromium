// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_wifi_header_view_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ash {

NetworkListWifiHeaderViewImpl::NetworkListWifiHeaderViewImpl(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListWifiHeaderView(delegate) {
  if (!features::IsQsRevampEnabled()) {
    AddExtraButtons();
  }
}

NetworkListWifiHeaderViewImpl::~NetworkListWifiHeaderViewImpl() = default;

void NetworkListWifiHeaderViewImpl::AddExtraButtons() {
  std::unique_ptr<IconButton> join_wifi_button = std::make_unique<IconButton>(
      base::BindRepeating(&NetworkListWifiHeaderViewImpl::JoinWifiButtonPressed,
                          weak_factory_.GetWeakPtr()),
      IconButton::Type::kMedium, &vector_icons::kWifiAddIcon,
      IDS_ASH_STATUS_TRAY_OTHER_WIFI);

  join_wifi_button.get()->SetID(kJoinWifiButtonId);
  join_wifi_button_ = join_wifi_button.get();
  container()->AddViewAt(TriView::Container::END, join_wifi_button.release(),
                         /*index=*/0);
}

void NetworkListWifiHeaderViewImpl::SetToggleState(bool enabled,
                                                   bool is_on,
                                                   bool animate_toggle) {
  if (features::IsQsRevampEnabled()) {
    std::u16string tooltip_text = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_WIFI,
        l10n_util::GetStringUTF16(
            is_on ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                  : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED));
    entry_row()->SetTooltipText(tooltip_text);
    qs_toggle()->SetTooltipText(tooltip_text);
  } else {
    join_wifi_button_->SetEnabled(enabled && is_on);
  }
  NetworkListNetworkHeaderView::SetToggleState(enabled, is_on, animate_toggle);
}

void NetworkListWifiHeaderViewImpl::OnToggleToggled(bool is_on) {
  // |join_wifi_button_| state is not updated here, it will be updated when
  // WiFi device state changes.
  delegate()->OnWifiToggleClicked(is_on);
}

void NetworkListWifiHeaderViewImpl::JoinWifiButtonPressed() {
  base::RecordAction(base::UserMetricsAction("StatusArea_Network_JoinOther"));
  Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
      ::onc::network_type::kWiFi);
}

void NetworkListWifiHeaderViewImpl::SetJoinWifiButtonState(bool enabled,
                                                           bool visible) {
  if (!join_wifi_button_) {
    return;
  }

  join_wifi_button_->SetEnabled(enabled);
  join_wifi_button_->SetVisible(visible);
}

}  // namespace ash
