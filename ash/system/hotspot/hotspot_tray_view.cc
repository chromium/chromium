// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_tray_view.h"

#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/hotspot/hotspot_icon.h"
#include "ash/system/hotspot/hotspot_icon_animation.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
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

  GetViewAccessibility().SetRole(ax::mojom::Role::kImage);
  UpdateAccessibleName();
}

HotspotTrayView::~HotspotTrayView() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->hotspot_icon_animation()->RemoveObserver(this);
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

void HotspotTrayView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);
  UpdateIconImage();
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
  image_view()->SetImage(ui::ImageModel::FromVectorIcon(
      hotspot_icon::GetIconForHotspot(state_),
      GetColorProvider()->GetColor(
          is_active() ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                      : cros_tokens::kCrosSysOnSurface),
      kUnifiedTrayIconSize));
}

void HotspotTrayView::HotspotIconChanged() {
  UpdateIconImage();
}

void HotspotTrayView::OnGetHotspotInfo(HotspotInfoPtr hotspot_info) {
  if (hotspot_info->state == HotspotState::kDisabled) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  tooltip_ = ComputeHotspotTooltip(hotspot_info->client_count);
  UpdateAccessibleName();

  if (hotspot_info->state == HotspotState::kEnabling) {
    Shell::Get()->hotspot_icon_animation()->AddObserver(this);
  } else if (state_ == HotspotState::kEnabling) {
    Shell::Get()->hotspot_icon_animation()->RemoveObserver(this);
  }
  if (state_ != hotspot_info->state) {
    state_ = hotspot_info->state;
    UpdateIconImage();
  }
}

void HotspotTrayView::UpdateAccessibleName() {
  GetViewAccessibility().SetName(tooltip_);
}

BEGIN_METADATA(HotspotTrayView)
END_METADATA

}  // namespace ash
