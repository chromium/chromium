// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_party_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

bool IsButtonVisible() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !session_controller->IsEnterpriseManaged();
}

}  // namespace

ShelfPartyFeaturePodController::ShelfPartyFeaturePodController() = default;

ShelfPartyFeaturePodController::~ShelfPartyFeaturePodController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->shelf_controller()->model()->RemoveObserver(this);
}

FeaturePodButton* ShelfPartyFeaturePodController::CreateButton() {
  DCHECK(!button_);
  DCHECK(!features::IsQsRevampEnabled());
  button_ = new FeaturePodButton(this);
  button_->DisableLabelButtonFocus();
  button_->SetVectorIcon(kShelfPartyIcon);
  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SHELF_PARTY_LABEL));

  // Init the button with invisible state. The `UpdateButton` method will update
  // the visibility based on the current condition.
  button_->SetVisible(false);
  UpdateButton();
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->shelf_controller()->model()->AddObserver(this);
  return button_;
}

std::unique_ptr<FeatureTile> ShelfPartyFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(!tile_);
  DCHECK(features::IsQsRevampEnabled());
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&ShelfPartyFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()));
  tile_ = tile.get();
  tile_->SetVectorIcon(kShelfPartyIcon);
  tile_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SHELF_PARTY_LABEL));

  // `UpdateTile()` will update visibility.
  tile_->SetVisible(false);
  UpdateTile();
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->shelf_controller()->model()->AddObserver(this);
  return tile;
}

QsFeatureCatalogName ShelfPartyFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kShelfParty;
}

void ShelfPartyFeaturePodController::OnIconPressed() {
  TrackToggleUMA(/*target_toggle_state=*/!Shell::Get()
                     ->shelf_controller()
                     ->model()
                     ->in_shelf_party());
  Shell::Get()->shelf_controller()->model()->ToggleShelfParty();
}

void ShelfPartyFeaturePodController::OnSessionStateChanged(
    session_manager::SessionState state) {
  Update();
}

void ShelfPartyFeaturePodController::ShelfPartyToggled(bool in_shelf_party) {
  Update();
}

void ShelfPartyFeaturePodController::Update() {
  if (features::IsQsRevampEnabled()) {
    UpdateTile();
  } else {
    UpdateButton();
  }
}

void ShelfPartyFeaturePodController::UpdateButton() {
  DCHECK(button_);
  const bool visible = IsButtonVisible();
  // If the button's visibility changes from invisible to visible, log its
  // visibility.
  if (!button_->GetVisible() && visible)
    TrackVisibilityUMA();
  button_->SetVisible(visible);

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

void ShelfPartyFeaturePodController::UpdateTile() {
  DCHECK(tile_);
  const bool visible = IsButtonVisible();
  // If the button's visibility changes from invisible to visible, log its
  // visibility.
  if (!tile_->GetVisible() && visible) {
    TrackVisibilityUMA();
  }
  tile_->SetVisible(visible);

  const bool toggled =
      Shell::Get()->shelf_controller()->model()->in_shelf_party();
  tile_->SetToggled(toggled);
  tile_->SetSubLabel(l10n_util::GetStringUTF16(
      toggled ? IDS_ASH_STATUS_TRAY_SHELF_PARTY_ON_SUBLABEL
              : IDS_ASH_STATUS_TRAY_SHELF_PARTY_OFF_SUBLABEL));
  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_SHELF_PARTY_TOGGLE_TOOLTIP,
      l10n_util::GetStringUTF16(
          toggled ? IDS_ASH_STATUS_TRAY_SHELF_PARTY_ENABLED_STATE_TOOLTIP
                  : IDS_ASH_STATUS_TRAY_SHELF_PARTY_DISABLED_STATE_TOOLTIP)));
}

}  // namespace ash
