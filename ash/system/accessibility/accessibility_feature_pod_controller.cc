// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/system/accessibility/accessibility_feature_pod_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

std::u16string GenerateSublabelText(
    std::vector<AccessibilityController::Feature*> enabled_features,
    int max_width,
    gfx::FontList font_list) {
  CHECK(!enabled_features.empty());
  std::u16string feature_name =
      l10n_util::GetStringUTF16(enabled_features.front()->name_resource_id());

  if (enabled_features.size() == 1) {
    return feature_name;
  }
  std::u16string separator = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_ENABLED_FEATURES_SEPARATOR);
  std::u16string count_label =
      base::NumberToString16(enabled_features.size() - 1);

  int available_width_for_feature_name =
      max_width - gfx::GetStringWidth(std::u16string(gfx::kEllipsisUTF16) +
                                          separator + count_label,
                                      font_list);
  // Elide the first enabled feature's name if necessary and then append the
  // number of other enabled features. This is to ensure the number is
  // always visible.
  std::u16string label = gfx::ElideText(feature_name, gfx::FontList(),
                                        available_width_for_feature_name,
                                        gfx::ElideBehavior::ELIDE_TAIL);

  return base::JoinString({label, count_label}, separator);
}

std::u16string GenerateTooltipText(
    std::vector<AccessibilityController::Feature*> enabled_features) {
  if (enabled_features.empty()) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TOOLTIP);
  }

  std::u16string first_feature_name =
      l10n_util::GetStringUTF16(enabled_features.front()->name_resource_id());
  std::u16string separator = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_ENABLED_FEATURES_SEPARATOR);
  std::u16string enabled_features_string =
      enabled_features.size() == 1
          ? first_feature_name
          : base::JoinString(
                {first_feature_name,
                 base::NumberToString16(enabled_features.size() - 1)},
                separator);

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TOGGLED_TOOLTIP,
      enabled_features_string);
}

}  // namespace

AccessibilityFeaturePodController::AccessibilityFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

AccessibilityFeaturePodController::~AccessibilityFeaturePodController() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void AccessibilityFeaturePodController::OnAccessibilityStatusChanged() {
  UpdateTileStateIfExists();
}

std::unique_ptr<FeatureTile> AccessibilityFeaturePodController::CreateTile(
    bool compact) {
  // Should not create the A11y tile if it already exists. Currently it's only
  // created once and used in the qs bubble.
  CHECK(!tile_);

  auto feature_tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                          weak_ptr_factory_.GetWeakPtr()));
  feature_tile->SetVectorIcon(kUnifiedMenuAccessibilityIcon);
  feature_tile->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY));
  feature_tile->SetSubLabelVisibility(false);
  feature_tile->CreateDecorativeDrillInArrow();
  // The labels are created based on the title container size. So the label need
  // to be updated if the bounds of its container is changed. Even without
  // manually calling `SetSize` for the container, its bounds can still change
  // after the label is created. For example, when the labels are created, the
  // title container might not be rendered yet. It uses 0 as the size to create
  // the label first. Then after the container is rendered, it will be updated
  // to the actual size.
  feature_tile->SetOnTitleBoundsChangedCallback(base::BindRepeating(
      &AccessibilityFeaturePodController::UpdateTileStateIfExists,
      weak_ptr_factory_.GetWeakPtr()));

  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  LoginStatus login_status = Shell::Get()->session_controller()->login_status();
  const bool visible = login_status == LoginStatus::NOT_LOGGED_IN ||
                       login_status == LoginStatus::LOCKED ||
                       delegate->ShouldShowAccessibilityMenu();
  feature_tile->SetVisible(visible);
  if (visible) {
    TrackVisibilityUMA();
  }

  tile_ = feature_tile.get();
  UpdateTileStateIfExists();
  return feature_tile;
}

QsFeatureCatalogName AccessibilityFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kAccessibility;
}

void AccessibilityFeaturePodController::OnIconPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowAccessibilityDetailedView();
}

void AccessibilityFeaturePodController::UpdateTileStateIfExists() {
  if (!tile_) {
    return;
  }

  auto enabled_features = Shell::Get()
                              ->accessibility_controller()
                              ->GetEnabledFeaturesInQuickSettings();

  bool toggled = !enabled_features.empty();
  tile_->SetToggled(toggled);
  tile_->SetTooltipText(GenerateTooltipText(enabled_features));

  tile_->SetSubLabelVisibility(toggled);
  if (toggled) {
    tile_->SetSubLabel(GenerateSublabelText(enabled_features,
                                            tile_->GetSubLabelMaxWidth(),
                                            tile_->sub_label()->font_list()));
    tile_->sub_label()->SetElideBehavior(enabled_features.size() == 1
                                             ? gfx::ElideBehavior::ELIDE_TAIL
                                             : gfx::ElideBehavior::NO_ELIDE);
  }
}

}  // namespace ash
