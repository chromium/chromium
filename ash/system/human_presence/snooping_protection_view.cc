// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/snooping_protection_view.h"

#include "ash/public/cpp/session/session_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/logging.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

SnoopingProtectionView::SnoopingProtectionView(Shelf* shelf)
    : TrayItemView(shelf) {
  CreateImageView();

  SnoopingProtectionController* controller =
      Shell::Get()->snooping_protection_controller();
  controller_observation_.Observe(controller);

  SetVisible(controller->SnooperPresent());
  UpdateLabelOrImageViewColor(/*active=*/false);
  image_view()->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_SYSTEM_TRAY_TOOLTIP_TEXT));
}

SnoopingProtectionView::~SnoopingProtectionView() = default;

void SnoopingProtectionView::HandleLocaleChange() {}

void SnoopingProtectionView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  image_view()->SetImage(ui::ImageModel::FromVectorIcon(
      kSystemTraySnoopingProtectionIcon,
      active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
             : cros_tokens::kCrosSysOnSurface,
      kUnifiedTrayIconSize));
}

void SnoopingProtectionView::OnSnoopingStatusChanged(bool snooper) {
  SetVisible(snooper);
}

void SnoopingProtectionView::OnSnoopingProtectionControllerDestroyed() {
  controller_observation_.Reset();
}

BEGIN_METADATA(SnoopingProtectionView)
END_METADATA

}  // namespace ash
