// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_tray_view.h"

#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

std::u16string ComputeHotspotTooltip(uint32_t client_count) {
  const std::u16string device_name = ui::GetChromeOSDeviceName();

  if (client_count == 0u) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_HOTSPOT_ON_NO_CONNECTED_DEVICES, device_name);
  }

  if (client_count == 1u) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_HOTSPOT_ON_ONE_CONNECTED_DEVICE, device_name);
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_HOTSPOT_ON_MULTIPLE_CONNECTED_DEVICES,
      base::NumberToString16(client_count), device_name);
}

}  //  namespace

using hotspot_config::mojom::HotspotInfoPtr;
using hotspot_config::mojom::HotspotState;

HotspotTrayView::HotspotTrayView(Shelf* shelf) : TrayItemView(shelf) {
  Shell::Get()->session_controller()->AddObserver(this);
  CreateImageView();
  SetVisible(false);

  GetHotspotConfigService(
      remote_cros_hotspot_config_.BindNewPipeAndPassReceiver());
  remote_cros_hotspot_config_->AddObserver(
      hotspot_config_observer_receiver_.BindNewPipeAndPassRemote());
}

HotspotTrayView::~HotspotTrayView() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

const char* HotspotTrayView::GetClassName() const {
  return "HotspotTrayView";
}

void HotspotTrayView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // A valid role must be set prior to setting the name.
  node_data->role = ax::mojom::Role::kImage;
  node_data->SetName(tooltip_);
}

std::u16string HotspotTrayView::GetAccessibleNameString() const {
  return tooltip_;
}

views::View* HotspotTrayView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

std::u16string HotspotTrayView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_;
}

void HotspotTrayView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateIconImage();
}

void HotspotTrayView::HandleLocaleChange() {
  UpdateIconVisibilityAndTooltip();
}

void HotspotTrayView::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state != session_manager::SessionState::ACTIVE) {
    return;
  }
  UpdateIconImage();
  UpdateIconVisibilityAndTooltip();
}

void HotspotTrayView::OnHotspotInfoChanged() {
  UpdateIconVisibilityAndTooltip();
}

void HotspotTrayView::UpdateIconVisibilityAndTooltip() {
  remote_cros_hotspot_config_->GetHotspotInfo(base::BindOnce(
      &HotspotTrayView::OnGetHotspotInfo, weak_ptr_factory_.GetWeakPtr()));
}

void HotspotTrayView::UpdateIconImage() {
  SkColor color;
  if (chromeos::features::IsJellyEnabled()) {
    color = GetColorProvider()->GetColor(cros_tokens::kCrosSysPrimary);
  } else {
    color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorPrimary);
  }
  image_view()->SetImage(
      gfx::CreateVectorIcon(kHotspotOnIcon, kUnifiedTrayIconSize, color));
}

void HotspotTrayView::OnGetHotspotInfo(HotspotInfoPtr hotspot_info) {
  if (hotspot_info->state != HotspotState::kEnabled) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  tooltip_ = ComputeHotspotTooltip(hotspot_info->client_count);
}

}  // namespace ash
