// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_feature_pod_controller.h"

#include "ash/projector/projector_controller.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray_item_uma_type.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

ProjectorFeaturePodController::ProjectorFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  // TODO(llin): Observe Projector UI model to update the toggle state.
}

ProjectorFeaturePodController::~ProjectorFeaturePodController() = default;

FeaturePodButton* ProjectorFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this, /*is_togglable=*/true);
  button_->SetVectorIcon(kPaletteTrayIconProjectorIcon);
  const auto label_text =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_PROJECTOR_BUTTON_LABEL);
  button_->SetLabel(label_text);
  button_->icon_button()->SetTooltipText(label_text);
  button_->SetLabelTooltip(label_text);
  button_->SetVisible(
      !Shell::Get()->session_controller()->IsUserSessionBlocked());
  // TODO(llin): Update toggle state based on Projector UI model.
  return button_;
}

void ProjectorFeaturePodController::OnIconPressed() {
  // Close the system tray bubble. Deletes |this|.
  tray_controller_->CloseBubble();

  Shell::Get()->projector_controller()->ui_controller()->ToggleToolbar();
}

SystemTrayItemUmaType ProjectorFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_PROJECTOR;
}

}  // namespace ash
