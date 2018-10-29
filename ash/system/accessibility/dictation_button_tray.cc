// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_button_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "chromeos/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

DictationButtonTray::DictationButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf), icon_(new views::ImageView()) {
  UpdateVisibility();

  SetInkDropMode(InkDropMode::ON);

  off_image_ = gfx::CreateVectorIcon(kDictationOffNewuiIcon, kShelfIconColor);
  on_image_ = gfx::CreateVectorIcon(kDictationOnNewuiIcon, kShelfIconColor);
  icon_->SetImage(off_image_);
  const int vertical_padding = (kTrayItemSize - off_image_.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - off_image_.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  icon_->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION));
  tray_container()->AddChildView(icon_);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

DictationButtonTray::~DictationButtonTray() {
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

bool DictationButtonTray::PerformAction(const ui::Event& event) {
  Shell::Get()->accessibility_controller()->ToggleDictationFromSource(
      mojom::DictationToggleSource::kButton);

  CheckDictationStatusAndUpdateIcon();
  return true;
}

void DictationButtonTray::ClickedOutsideBubble() {}

void DictationButtonTray::OnDictationStarted() {
  UpdateIcon(/*dictation_active=*/true);
}

void DictationButtonTray::OnDictationEnded() {
  UpdateIcon(/*dictation_active=*/false);
}

void DictationButtonTray::OnAccessibilityStatusChanged() {
  UpdateVisibility();
  CheckDictationStatusAndUpdateIcon();
}

base::string16 DictationButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_DICTATION_BUTTON_ACCESSIBLE_NAME);
}

void DictationButtonTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  // This class has no bubbles to hide.
}

void DictationButtonTray::UpdateIcon(bool dictation_active) {
  if (dictation_active) {
    icon_->SetImage(on_image_);
    SetIsActive(true);
  } else {
    icon_->SetImage(off_image_);
    SetIsActive(false);
  }
}

void DictationButtonTray::UpdateVisibility() {
  bool is_visible =
      Shell::Get()->accessibility_controller()->IsDictationEnabled();
  SetVisible(is_visible);
}

void DictationButtonTray::CheckDictationStatusAndUpdateIcon() {
  UpdateIcon(Shell::Get()->accessibility_controller()->IsDictationActive());
}

}  // namespace ash
