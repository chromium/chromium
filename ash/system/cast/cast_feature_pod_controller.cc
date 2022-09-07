// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_feature_pod_controller.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
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
  auto* cast_config = CastConfigController::Get();
  // If there are no devices currently available for the user, and they have
  // access code casting available, don't bother displaying an empty list.
  // Instead, launch directly into the access code UI so that they can begin
  // casting immediately.
  if (cast_config && !cast_config->HasSinksAndRoutes() &&
      cast_config->AccessCodeCastingEnabled()) {
    Shell::Get()->system_tray_model()->client()->ShowAccessCodeCastingDialog(
        AccessCodeCastDialogOpenLocation::kSystemTrayCastFeaturePod);
  } else {
    tray_controller_->ShowCastDetailedView();
  }
}

void CastFeaturePodController::OnLabelPressed() {
  // Clicking on the label should always launch the full UI.
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
  button_->SetVisible(cast_config &&
                      (cast_config->HasSinksAndRoutes() ||
                       cast_config->AccessCodeCastingEnabled()) &&
                      !cast_config->HasActiveRoute());
}

}  // namespace ash
