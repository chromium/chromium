// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak/select_to_speak_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace views {
class ImageView;
}

namespace ash {

namespace {

gfx::ImageSkia GetImageOnCurrentSelectToSpeakStatus() {
  auto* shell = Shell::Get();
  const SkColor color =
      TrayIconColor(shell->session_controller()->GetSessionState());
  const auto select_to_speak_state =
      shell->accessibility_controller()->GetSelectToSpeakState();

  switch (select_to_speak_state) {
    case SelectToSpeakState::kSelectToSpeakStateInactive:
      return gfx::CreateVectorIcon(kSystemTraySelectToSpeakNewuiIcon, color);
    case SelectToSpeakState::kSelectToSpeakStateSelecting:
      return gfx::CreateVectorIcon(kSystemTraySelectToSpeakActiveNewuiIcon,
                                   color);
    case SelectToSpeakState::kSelectToSpeakStateSpeaking:
      return gfx::CreateVectorIcon(kSystemTrayStopNewuiIcon, color);
  }
}

}  // namespace

SelectToSpeakTray::SelectToSpeakTray(Shelf* shelf,
                                     TrayBackgroundViewCatalogName catalog_name)
    : TrayBackgroundView(shelf, catalog_name) {
  SetPressedCallback(base::BindRepeating([](const ui::Event& event) {
    Shell::Get()->accessibility_controller()->RequestSelectToSpeakStateChange();
  }));

  const gfx::ImageSkia inactive_image = gfx::CreateVectorIcon(
      kSystemTraySelectToSpeakNewuiIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(inactive_image);
  const int vertical_padding = (kTrayItemSize - inactive_image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - inactive_image.width()) / 2;
  icon->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));
  icon->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK));
  icon_ = tray_container()->AddChildView(std::move(icon));

  // Observe the accessibility controller state changes to know when Select to
  // Speak state is updated or when it is disabled/enabled.
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

SelectToSpeakTray::~SelectToSpeakTray() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void SelectToSpeakTray::Initialize() {
  TrayBackgroundView::Initialize();
  UpdateIconOnCurrentStatus();
}

std::u16string SelectToSpeakTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_SELECT_TO_SPEAK_TRAY_ACCESSIBLE_NAME);
}

void SelectToSpeakTray::HandleLocaleChange() {
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK));
}

void SelectToSpeakTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateIconOnColorChanges();
}

void SelectToSpeakTray::OnAccessibilityStatusChanged() {
  UpdateIconOnCurrentStatus();
}

void SelectToSpeakTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateIconOnColorChanges();
}

void SelectToSpeakTray::UpdateIconOnCurrentStatus() {
  auto* accessibility_controller = Shell::Get()->accessibility_controller();
  if (!accessibility_controller->select_to_speak().enabled()) {
    SetVisiblePreferred(false);
    return;
  }
  icon_->SetImage(GetImageOnCurrentSelectToSpeakStatus());
  SetIsActive(accessibility_controller->GetSelectToSpeakState() !=
              SelectToSpeakState::kSelectToSpeakStateInactive);
  SetVisiblePreferred(true);
}

void SelectToSpeakTray::UpdateIconOnColorChanges() {
  if (!visible_preferred() ||
      !Shell::Get()->accessibility_controller()->select_to_speak().enabled()) {
    return;
  }
  icon_->SetImage(GetImageOnCurrentSelectToSpeakStatus());
}

BEGIN_METADATA(SelectToSpeakTray, TrayBackgroundView);
END_METADATA

}  // namespace ash
