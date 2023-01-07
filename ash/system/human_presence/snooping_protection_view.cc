// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/snooping_protection_view.h"

#include "ash/public/cpp/session/session_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/logging.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

SnoopingProtectionView::SnoopingProtectionView(Shelf* shelf)
    : TrayItemView(shelf) {
  CreateImageView();

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  session_observation_.Observe(session_controller);

  SnoopingProtectionController* controller =
      Shell::Get()->snooping_protection_controller();
  controller_observation_.Observe(controller);

  SetVisible(controller->SnooperPresent());
  UpdateIconColor(session_controller->GetSessionState());
  image_view()->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_SYSTEM_TRAY_TOOLTIP_TEXT));
}

SnoopingProtectionView::~SnoopingProtectionView() = default;

void SnoopingProtectionView::HandleLocaleChange() {}

void SnoopingProtectionView::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  UpdateIconColor(session_state);
}

void SnoopingProtectionView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateIconColor(Shell::Get()->session_controller()->GetSessionState());
}

const char* SnoopingProtectionView::GetClassName() const {
  return "SnoopingProtectionView";
}

void SnoopingProtectionView::OnSnoopingStatusChanged(bool snooper) {
  SetVisible(snooper);
}

void SnoopingProtectionView::OnSnoopingProtectionControllerDestroyed() {
  controller_observation_.Reset();
}

void SnoopingProtectionView::UpdateIconColor(
    session_manager::SessionState session_state) {
  const SkColor new_color = TrayIconColor(session_state);
  const gfx::ImageSkia new_icon = gfx::CreateVectorIcon(gfx::IconDescription(
      kSystemTraySnoopingProtectionIcon, kUnifiedTrayIconSize, new_color));
  image_view()->SetImage(new_icon);
}

}  // namespace ash
