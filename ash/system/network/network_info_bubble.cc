// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_info_bubble.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;

// This margin value is used for:
// - Margins inside the border.
// - Horizontal spacing between the border and parent bubble border.
// - Distance between top of the border and the bottom of the anchor view
//   (horizontal rule).
constexpr int kBubbleMargin = 8;

// Elevation used for the bubble shadow effect (tiny).
constexpr int kBubbleShadowElevation = 2;

// 00:00:00:00:00:00 is provided when a device MAC address cannot be retrieved.
constexpr char kMissingMacAddress[] = "00:00:00:00:00:00";

std::string ComputeMacAddress(NetworkType network_type) {
  const DeviceStateProperties* device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          network_type);

  if (!device || !device->mac_address ||
      device->mac_address == kMissingMacAddress) {
    return std::string();
  }
  return *device->mac_address;
}

}  // namespace

NetworkInfoBubble::NetworkInfoBubble(base::WeakPtr<Delegate> delegate,
                                     views::View* anchor)
    : views::BubbleDialogDelegateView(anchor, views::BubbleBorder::TOP_RIGHT),
      delegate_(delegate) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets(kBubbleMargin));
  SetArrow(views::BubbleBorder::NONE);
  set_shadow(views::BubbleBorder::NO_SHADOW);
  SetNotifyEnterExitOnChild(true);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  std::unique_ptr<views::Label> label =
      std::make_unique<views::Label>(ComputeInfoText());
  label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  label->SetID(kNetworkInfoBubbleLabelViewId);
  label->SetMultiLine(true);
  label->SetSelectable(true);

  AddChildView(label.release());
}

NetworkInfoBubble::~NetworkInfoBubble() {
  if (delegate_)
    delegate_->OnInfoBubbleDestroyed();
}

gfx::Size NetworkInfoBubble::CalculatePreferredSize() const {
  // This bubble should be inset by kBubbleMargin on the left and right relative
  // to the parent bubble.
  const gfx::Size anchor_size = GetAnchorView()->size();
  int contents_width =
      anchor_size.width() - 2 * kBubbleMargin - margins().width();
  return gfx::Size(contents_width, GetHeightForWidth(contents_width));
}

void NetworkInfoBubble::OnMouseExited(const ui::MouseEvent& event) {
  GetWidget()->Close();  // Deletes |this|.
}

void NetworkInfoBubble::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->shadow_elevation = kBubbleShadowElevation;
  params->name = "NetworkInfoBubble";
}

std::u16string NetworkInfoBubble::ComputeInfoText() {
  DCHECK(delegate_);

  std::u16string info_text;

  auto add_address_if_exists = [&info_text](std::string address, int text_id) {
    if (address.empty())
      return;
    if (!info_text.empty())
      info_text += u"\n";
    info_text +=
        l10n_util::GetStringFUTF16(text_id, base::UTF8ToUTF16(address));
  };

  const NetworkStateProperties* default_network = Shell::Get()
                                                      ->system_tray_model()
                                                      ->network_state_model()
                                                      ->default_network();
  const DeviceStateProperties* device =
      default_network
          ? Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
                default_network->type)
          : nullptr;

  if (device) {
    if (device->ipv4_address) {
      add_address_if_exists(device->ipv4_address->ToString(),
                            IDS_ASH_STATUS_TRAY_IP_ADDRESS);
    }
    if (device->ipv6_address) {
      add_address_if_exists(device->ipv6_address->ToString(),
                            IDS_ASH_STATUS_TRAY_IPV6_ADDRESS);
    }
  }

  if (delegate_->ShouldIncludeDeviceAddresses()) {
    add_address_if_exists(ComputeMacAddress(NetworkType::kEthernet),
                          IDS_ASH_STATUS_TRAY_ETHERNET_ADDRESS);
    add_address_if_exists(ComputeMacAddress(NetworkType::kWiFi),
                          IDS_ASH_STATUS_TRAY_WIFI_ADDRESS);
    add_address_if_exists(ComputeMacAddress(NetworkType::kCellular),
                          IDS_ASH_STATUS_TRAY_CELLULAR_ADDRESS);
  }

  // Avoid returning an empty bubble when no network information is available.
  if (info_text.empty())
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_NETWORKS);
  return info_text;
}

}  // namespace ash
