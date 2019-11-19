// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_feature_pod_controller.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CastFeaturePodController::CastFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
}

CastFeaturePodController::~CastFeaturePodController() {
  if (CastConfigController::Get() && button_)
    CastConfigController::Get()->RemoveObserver(this);
}

FeaturePodButton* CastFeaturePodController::CreateButton() {
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuCastIcon);
  button_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_SHORT));
  button_->SetIconAndLabelTooltips(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_TOOLTIP));
  button_->ShowDetailedViewArrow();
  button_->DisableLabelButtonFocus();
  button_->SetID(VIEW_ID_CAST_MAIN_VIEW);

  if (CastConfigController::Get()) {
    CastConfigController::Get()->AddObserver(this);
    CastConfigController::Get()->RequestDeviceRefresh();
  }

  Update();
  return button_;
}

void CastFeaturePodController::OnIconPressed() {
  tray_controller_->ShowCastDetailedView();
}

SystemTrayItemUmaType CastFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_CAST;
}

void CastFeaturePodController::OnDevicesUpdated(
    const std::vector<SinkAndRoute>& devices) {
  Update();
}

void CastFeaturePodController::Update() {
  auto* cast_config = CastConfigController::Get();
  button_->SetVisible(cast_config && cast_config->HasSinksAndRoutes() &&
                      !cast_config->HasActiveRoute());
}

}  // namespace ash
