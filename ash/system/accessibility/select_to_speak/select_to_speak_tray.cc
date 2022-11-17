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
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace views {
class ImageView;
}

namespace ash {

namespace {

ui::ImageModel GetImageOnCurrentSelectToSpeakStatus(
    const SelectToSpeakState& select_to_speak_state) {
  switch (select_to_speak_state) {
    case SelectToSpeakState::kSelectToSpeakStateInactive:
      return ui::ImageModel::FromVectorIcon(kSystemTraySelectToSpeakNewuiIcon,
                                            kColorAshIconColorPrimary);
    case SelectToSpeakState::kSelectToSpeakStateSelecting:
      return ui::ImageModel::FromVectorIcon(
          kSystemTraySelectToSpeakActiveNewuiIcon, kColorAshIconColorPrimary);
    case SelectToSpeakState::kSelectToSpeakStateSpeaking:
      return ui::ImageModel::FromVectorIcon(kSystemTrayStopNewuiIcon,
                                            kColorAshIconColorPrimary);
  }
}

std::u16string GetTooltipTextOnCurrentSelectToSpeakStatus(
    const SelectToSpeakState& select_to_speak_state) {
  if (!::features::IsAccessibilitySelectToSpeakHoverTextImprovementsEnabled()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK);
  }

  switch (select_to_speak_state) {
    case SelectToSpeakState::kSelectToSpeakStateInactive:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK);
    case SelectToSpeakState::kSelectToSpeakStateSelecting:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK_INSTRUCTIONS);
    case SelectToSpeakState::kSelectToSpeakStateSpeaking:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK_STOP_INSTRUCTIONS);
  }
}

}  // namespace

SelectToSpeakTray::SelectToSpeakTray(Shelf* shelf,
                                     TrayBackgroundViewCatalogName catalog_name)
    : TrayBackgroundView(shelf, catalog_name) {
  SetPressedCallback(base::BindRepeating([](const ui::Event& event) {
    Shell::Get()->accessibility_controller()->RequestSelectToSpeakStateChange();
  }));

  const ui::ImageModel inactive_image = ui::ImageModel::FromVectorIcon(
      kSystemTraySelectToSpeakNewuiIcon, kColorAshIconColorPrimary);
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(inactive_image);
  const int vertical_padding =
      (kTrayItemSize - inactive_image.Size().height()) / 2;
  const int horizontal_padding =
      (kTrayItemSize - inactive_image.Size().width()) / 2;
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
  UpdateUXOnCurrentStatus();
}

std::u16string SelectToSpeakTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_SELECT_TO_SPEAK_TRAY_ACCESSIBLE_NAME);
}

void SelectToSpeakTray::HandleLocaleChange() {
  const auto select_to_speak_state =
      Shell::Get()->accessibility_controller()->GetSelectToSpeakState();
  icon_->SetTooltipText(
      GetTooltipTextOnCurrentSelectToSpeakStatus(select_to_speak_state));
}

void SelectToSpeakTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateIconOnColorChanges();
}

void SelectToSpeakTray::OnAccessibilityStatusChanged() {
  UpdateUXOnCurrentStatus();
}

void SelectToSpeakTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateIconOnColorChanges();
}

void SelectToSpeakTray::UpdateUXOnCurrentStatus() {
  auto* accessibility_controller = Shell::Get()->accessibility_controller();
  if (!accessibility_controller->select_to_speak().enabled()) {
    SetVisiblePreferred(false);
    return;
  }
  const auto select_to_speak_state =
      accessibility_controller->GetSelectToSpeakState();
  icon_->SetImage(GetImageOnCurrentSelectToSpeakStatus(select_to_speak_state));
  icon_->SetTooltipText(
      GetTooltipTextOnCurrentSelectToSpeakStatus(select_to_speak_state));
  SetIsActive(accessibility_controller->GetSelectToSpeakState() !=
              SelectToSpeakState::kSelectToSpeakStateInactive);
  SetVisiblePreferred(true);
}

void SelectToSpeakTray::UpdateIconOnColorChanges() {
  auto* accessibility_controller = Shell::Get()->accessibility_controller();
  if (!visible_preferred() ||
      !accessibility_controller->select_to_speak().enabled()) {
    return;
  }
  const auto select_to_speak_state =
      accessibility_controller->GetSelectToSpeakState();
  icon_->SetImage(GetImageOnCurrentSelectToSpeakStatus(select_to_speak_state));
}

BEGIN_METADATA(SelectToSpeakTray, TrayBackgroundView);
END_METADATA

}  // namespace ash
