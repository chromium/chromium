// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace views {
class ImageView;
}

namespace ash {

// This constant must be kept the same as SELECT_TO_SPEAK_TRAY_CLASS_NAME in
// chrome/browser/resources/chromeos/accessibility/select_to_speak/
// select_to_speak.js.
const char kSelectToSpeakTrayClassName[] =
    "tray/TrayBackgroundView/SelectToSpeakTray";

SelectToSpeakTray::SelectToSpeakTray(Shelf* shelf)
    : TrayBackgroundView(shelf), icon_(new views::ImageView()) {

  UpdateIconsForSession();
  icon_->SetImage(inactive_image_);
  const int vertical_padding = (kTrayItemSize - inactive_image_.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - inactive_image_.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK));
  tray_container()->AddChildView(icon_);

  // Observe the accessibility controller state changes to know when Select to
  // Speak state is updated or when it is disabled/enabled.
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

SelectToSpeakTray::~SelectToSpeakTray() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void SelectToSpeakTray::Initialize() {
  TrayBackgroundView::Initialize();
  CheckStatusAndUpdateIcon();
}

base::string16 SelectToSpeakTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_SELECT_TO_SPEAK_TRAY_ACCESSIBLE_NAME);
}

void SelectToSpeakTray::HandleLocaleChange() {
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK));
}

const char* SelectToSpeakTray::GetClassName() const {
  return kSelectToSpeakTrayClassName;
}

bool SelectToSpeakTray::PerformAction(const ui::Event& event) {
  Shell::Get()->accessibility_controller()->RequestSelectToSpeakStateChange();
  return true;
}

void SelectToSpeakTray::OnAccessibilityStatusChanged() {
  CheckStatusAndUpdateIcon();
}

void SelectToSpeakTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateIconsForSession();
  CheckStatusAndUpdateIcon();
}

void SelectToSpeakTray::UpdateIconsForSession() {
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  SkColor color = TrayIconColor(session_state);

  inactive_image_ =
      gfx::CreateVectorIcon(kSystemTraySelectToSpeakNewuiIcon, color);
  selecting_image_ =
      gfx::CreateVectorIcon(kSystemTraySelectToSpeakActiveNewuiIcon, color);
  speaking_image_ = gfx::CreateVectorIcon(kSystemTrayStopNewuiIcon, color);
}

void SelectToSpeakTray::CheckStatusAndUpdateIcon() {
  if (!Shell::Get()->accessibility_controller()->select_to_speak().enabled()) {
    SetVisiblePreferred(false);
    return;
  }

  SelectToSpeakState state =
      Shell::Get()->accessibility_controller()->GetSelectToSpeakState();
  switch (state) {
    case SelectToSpeakState::kSelectToSpeakStateInactive:
      icon_->SetImage(inactive_image_);
      SetIsActive(false);
      break;
    case SelectToSpeakState::kSelectToSpeakStateSelecting:
      // Activate the start selection button during selection.
      icon_->SetImage(selecting_image_);
      SetIsActive(true);
      break;
    case SelectToSpeakState::kSelectToSpeakStateSpeaking:
      icon_->SetImage(speaking_image_);
      SetIsActive(true);
      break;
  }

  SetVisiblePreferred(true);
}

}  // namespace ash
