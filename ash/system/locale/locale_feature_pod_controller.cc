// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

bool IsButtonVisible() {
  return !Shell::Get()->system_tray_model()->locale()->locale_list().empty();
}

std::u16string GetSubLabelText() {
  return base::i18n::ToUpper(base::UTF8ToUTF16(l10n_util::GetLanguage(
      Shell::Get()->system_tray_model()->locale()->current_locale_iso_code())));
}

}  // namespace

LocaleFeaturePodController::LocaleFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

LocaleFeaturePodController::~LocaleFeaturePodController() = default;

std::unique_ptr<FeatureTile> LocaleFeaturePodController::CreateTile(
    bool compact) {
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&LocaleFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()),
      /*is_togglable=*/false);
  const bool visible = IsButtonVisible();
  tile->SetVisible(visible);
  if (visible) {
    TrackVisibilityUMA();
    tile->SetVectorIcon(kUnifiedMenuLocaleIcon);
    tile->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOCALE_TOOLTIP));
    tile->CreateDecorativeDrillInArrow();
    tile->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOCALE));
    tile->SetSubLabel(GetSubLabelText());
  }
  return tile;
}

QsFeatureCatalogName LocaleFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kLocale;
}

void LocaleFeaturePodController::OnIconPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowLocaleDetailedView();
}

}  // namespace ash
