// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_list_item_view.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

NetworkDetailedView::NetworkDetailedView(
    DetailedViewDelegate* detailed_view_delegate,
    Delegate* delegate,
    ListType list_type)
    : TrayDetailedView(detailed_view_delegate),
      list_type_(list_type),
      login_(Shell::Get()->session_controller()->login_status()),
      model_(Shell::Get()->system_tray_model()->network_state_model()),
      delegate_(delegate) {
  CreateTitleRow(list_type_ == ListType::LIST_TYPE_NETWORK
                     ? IDS_ASH_STATUS_TRAY_NETWORK
                     : IDS_ASH_STATUS_TRAY_VPN);
  CreateScrollableList();
  // TODO(b/207089013): add metrics for UI surface displayed.
}

NetworkDetailedView::~NetworkDetailedView() = default;

void NetworkDetailedView::HandleViewClicked(views::View* view) {
  if (login_ == LoginStatus::LOCKED) {
    return;
  }

  if (view->GetID() == VIEW_ID_JOIN_NETWORK_ENTRY) {
    base::RecordAction(
        base::UserMetricsAction("QS_Subpage_Network_JoinNetwork"));
    Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
        onc::network_type::kWiFi);
    return;
  }
  delegate()->OnNetworkListItemSelected(
      static_cast<NetworkListItemView*>(view)->network_properties());
}

void NetworkDetailedView::CreateExtraTitleRowButtons() {
  DCHECK(!info_button_);
  tri_view()->SetContainerVisible(TriView::Container::END, true);

  std::unique_ptr<views::Button> info = base::WrapUnique(
      CreateInfoButton(base::BindRepeating(&NetworkDetailedView::OnInfoClicked,
                                           weak_ptr_factory_.GetWeakPtr()),
                       IDS_ASH_STATUS_TRAY_NETWORK_INFO));
  info->SetID(static_cast<int>(NetworkDetailedViewChildId::kInfoButton));
  info_button_ = tri_view()->AddView(TriView::Container::END, std::move(info));

  DCHECK(!settings_button_);

  std::unique_ptr<views::Button> settings =
      base::WrapUnique(CreateSettingsButton(
          base::BindRepeating(&NetworkDetailedView::OnSettingsClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS));
  settings->SetID(
      static_cast<int>(NetworkDetailedViewChildId::kSettingsButton));
  settings_button_ =
      tri_view()->AddView(TriView::Container::END, std::move(settings));
}

bool NetworkDetailedView::ShouldIncludeDeviceAddresses() {
  return list_type_ == LIST_TYPE_NETWORK;
}

void NetworkDetailedView::OnInfoBubbleDestroyed() {
  info_bubble_ = nullptr;

  // Widget of info bubble is activated while info bubble is shown. To move
  // focus back to the widget of this view, activate it again here.
  GetWidget()->Activate();
}

void NetworkDetailedView::OnInfoClicked() {
  if (CloseInfoBubble()) {
    return;
  }

  info_bubble_ =
      new NetworkInfoBubble(weak_ptr_factory_.GetWeakPtr(), tri_view());
  views::BubbleDialogDelegateView::CreateBubble(info_bubble_)->Show();
  info_bubble_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, false);
}

bool NetworkDetailedView::CloseInfoBubble() {
  if (!info_bubble_) {
    return false;
  }

  info_bubble_->GetWidget()->Close();
  return true;
}

void NetworkDetailedView::OnSettingsClicked() {
  base::RecordAction(
      list_type_ == LIST_TYPE_VPN
          ? base::UserMetricsAction("StatusArea_VPN_Settings")
          : base::UserMetricsAction("StatusArea_Network_Settings"));

  base::RecordAction(base::UserMetricsAction(
      "ChromeOS.SystemTray.Network.SettingsButtonPressed"));

  const std::string guid = model_->default_network()
                               ? model_->default_network()->guid
                               : std::string();

  CloseBubble();  // Deletes |this|.

  SystemTrayClient* system_tray_client =
      Shell::Get()->system_tray_model()->client();
  if (system_tray_client) {
    system_tray_client->ShowNetworkSettings(guid);
  }
}

BEGIN_METADATA(NetworkDetailedView, TrayDetailedView)
END_METADATA

}  // namespace ash
