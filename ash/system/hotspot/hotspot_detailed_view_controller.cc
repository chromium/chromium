// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_detailed_view_controller.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {

using hotspot_config::mojom::HotspotAllowStatus;
using hotspot_config::mojom::HotspotInfoPtr;
using hotspot_config::mojom::HotspotState;

namespace {

bool NeedUpdateHotspotView(const HotspotInfoPtr& current_hotspot_info,
                           const HotspotInfoPtr& new_hotspot_info) {
  if (!current_hotspot_info) {
    return true;
  }

  // No need to update the detailed view if hotspot configuration is changed.
  return current_hotspot_info->state != new_hotspot_info->state ||
         current_hotspot_info->allow_status != new_hotspot_info->allow_status ||
         current_hotspot_info->client_count != new_hotspot_info->client_count;
}

}  // namespace

HotspotDetailedViewController::HotspotDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)),
      tray_controller_(tray_controller) {
  GetHotspotConfigService(
      remote_cros_hotspot_config_.BindNewPipeAndPassReceiver());
  remote_cros_hotspot_config_->AddObserver(
      cros_hotspot_config_observer_receiver_.BindNewPipeAndPassRemote());
}

HotspotDetailedViewController::~HotspotDetailedViewController() = default;

std::unique_ptr<views::View> HotspotDetailedViewController::CreateView() {
  CHECK(!view_);
  std::unique_ptr<HotspotDetailedView> hotspot_detailed_view =
      std::make_unique<HotspotDetailedView>(detailed_view_delegate_.get(),
                                            /*delegate=*/this);
  view_ = hotspot_detailed_view.get();
  return hotspot_detailed_view;
}

std::u16string HotspotDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_HOTSPOT_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void HotspotDetailedViewController::OnHotspotInfoChanged() {
  remote_cros_hotspot_config_->GetHotspotInfo(
      base::BindOnce(&HotspotDetailedViewController::OnGetHotspotInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotDetailedViewController::OnGetHotspotInfo(
    HotspotInfoPtr hotspot_info) {
  bool need_update_view = NeedUpdateHotspotView(hotspot_info_, hotspot_info);
  hotspot_info_ = std::move(hotspot_info);
  if (view_ && need_update_view) {
    view_->UpdateViewForHotspot(mojo::Clone(hotspot_info_));
  }
}

void HotspotDetailedViewController::OnToggleClicked(bool new_state) {
  if (new_state) {
    remote_cros_hotspot_config_->EnableHotspot(base::DoNothing());
    return;
  }
  remote_cros_hotspot_config_->DisableHotspot(base::DoNothing());
}

}  // namespace ash
