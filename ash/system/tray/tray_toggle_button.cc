// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_toggle_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/utility/haptics_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

TrayToggleButton::TrayToggleButton(PressedCallback callback,
                                   int accessible_name_id)
    : ToggleButton(std::move(callback)) {
  const gfx::Size toggle_size(GetPreferredSize());
  const int vertical_padding = (kMenuButtonSize - toggle_size.height()) / 2;
  const int horizontal_padding =
      (kTrayToggleButtonWidth - toggle_size.width()) / 2;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));
  SetAccessibleName(l10n_util::GetStringUTF16(accessible_name_id));
}

void TrayToggleButton::OnThemeChanged() {
  views::ToggleButton::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  SetThumbOnColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSwitchKnobColorActive));
  SetThumbOffColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSwitchKnobColorInactive));
  SetTrackOnColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSwitchTrackColorActive));
  SetTrackOffColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSwitchTrackColorInactive));
  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
}

void TrayToggleButton::NotifyClick(const ui::Event& event) {
  haptics_util::PlayHapticToggleEffect(
      !GetIsOn(), ui::HapticTouchpadEffectStrength::kMedium);
  views::ToggleButton::NotifyClick(event);
}

}  // namespace ash
