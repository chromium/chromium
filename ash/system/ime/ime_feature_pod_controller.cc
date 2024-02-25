// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/ime_feature_pod_controller.h"

#include <string>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

bool IsButtonVisible() {
  DCHECK(Shell::Get());
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  size_t ime_count = ime_controller->GetVisibleImes().size();
  return !ime_controller->is_menu_active() &&
         (ime_count > 1 || ime_controller->managed_by_policy());
}

std::u16string GetLabelString() {
  DCHECK(Shell::Get());
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  size_t ime_count = ime_controller->GetVisibleImes().size();
  if (ime_count > 1) {
    return ime_controller->current_ime().short_name;
  } else {
    return l10n_util::GetStringUTF16(
        keyboard::IsKeyboardEnabled() ? IDS_ASH_STATUS_TRAY_KEYBOARD_ENABLED
                                      : IDS_ASH_STATUS_TRAY_KEYBOARD_DISABLED);
  }
}

std::u16string GetTooltipString() {
  DCHECK(Shell::Get());
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  size_t ime_count = ime_controller->GetVisibleImes().size();
  if (ime_count > 1) {
    return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_IME_TOOLTIP_WITH_NAME,
                                      ime_controller->current_ime().name);
  } else {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME_TOOLTIP);
  }
}

}  // namespace

IMEFeaturePodController::IMEFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  Shell::Get()->system_tray_notifier()->AddIMEObserver(this);
}

IMEFeaturePodController::~IMEFeaturePodController() {
  Shell::Get()->system_tray_notifier()->RemoveIMEObserver(this);
}

std::unique_ptr<FeatureTile> IMEFeaturePodController::CreateTile(bool compact) {
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&IMEFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()),
      /*is_togglable=*/false);
  tile_ = tile.get();
  tile_->SetVectorIcon(kUnifiedMenuKeyboardIcon);
  tile_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME_SHORT));
  tile_->SetTooltipText(GetTooltipString());
  tile_->CreateDecorativeDrillInArrow();
  // `Update` will update the visibility.
  tile_->SetVisible(false);
  Update();
  return tile;
}

QsFeatureCatalogName IMEFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kIME;
}

void IMEFeaturePodController::OnIconPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowIMEDetailedView();
}

void IMEFeaturePodController::OnIMERefresh() {
  Update();
}

void IMEFeaturePodController::OnIMEMenuActivationChanged(bool is_active) {
  Update();
}

void IMEFeaturePodController::Update() {
  bool is_button_visible = IsButtonVisible();
  const std::u16string tooltip = GetTooltipString();
  std::u16string label_string = GetLabelString();
  if (label_string.empty()) {
    tile_->SetSubLabelVisibility(false);
  } else {
    tile_->SetSubLabel(label_string);
    tile_->SetSubLabelVisibility(true);
  }
  tile_->SetTooltipText(tooltip);
  // If the tile's visibility changes from invisible to visible, log its
  // visibility.
  if (!tile_->GetVisible() && is_button_visible) {
    TrackVisibilityUMA();
  }
  tile_->SetVisible(is_button_visible);
}

}  // namespace ash
