// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_party_feature_pod_controller.h"

#include "ash/public/cpp/shelf_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

ShelfPartyFeaturePodController::ShelfPartyFeaturePodController() = default;

ShelfPartyFeaturePodController::~ShelfPartyFeaturePodController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->shelf_controller()->model()->RemoveObserver(this);
}

FeaturePodButton* ShelfPartyFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->DisableLabelButtonFocus();
  button_->SetVectorIcon(kShelfPartyIcon);
  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SHELF_PARTY_LABEL));
  UpdateButton();
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->shelf_controller()->model()->AddObserver(this);
  return button_;
}

void ShelfPartyFeaturePodController::OnIconPressed() {
  Shell::Get()->shelf_controller()->model()->ToggleShelfParty();
}

SystemTrayItemUmaType ShelfPartyFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_SHELF_PARTY;
}

void ShelfPartyFeaturePodController::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateButton();
}

void ShelfPartyFeaturePodController::ShelfPartyToggled(bool in_shelf_party) {
  UpdateButton();
}

void ShelfPartyFeaturePodController::UpdateButton() {
  DCHECK(button_);
  const SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  button_->SetVisible(session_controller->GetSessionState() ==
                          session_manager::SessionState::ACTIVE &&
                      !session_controller->IsEnterpriseManaged());
  const bool toggled =
      Shell::Get()->shelf_controller()->model()->in_shelf_party();
  button_->SetToggled(toggled);
  button_->SetSubLabel(l10n_util::GetStringUTF16(
      toggled ? IDS_ASH_STATUS_TRAY_SHELF_PARTY_ON_SUBLABEL
              : IDS_ASH_STATUS_TRAY_SHELF_PARTY_OFF_SUBLABEL));
  button_->SetIconAndLabelTooltips(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_SHELF_PARTY_TOGGLE_TOOLTIP,
      l10n_util::GetStringUTF16(
          toggled ? IDS_ASH_STATUS_TRAY_SHELF_PARTY_ENABLED_STATE_TOOLTIP
                  : IDS_ASH_STATUS_TRAY_SHELF_PARTY_DISABLED_STATE_TOOLTIP)));
}

}  // namespace ash
