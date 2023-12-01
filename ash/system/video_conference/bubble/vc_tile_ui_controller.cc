// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/vc_tile_ui_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"

namespace ash::video_conference {

VcTileUiController::VcTileUiController(const VcHostedEffect* effect)
    : effect_(effect) {
  effect_id_ = effect->id();
  effect_state_ = effect->GetState(/*index=*/0);
}

VcTileUiController::~VcTileUiController() = default;

std::unique_ptr<FeatureTile> VcTileUiController::CreateTile() {
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&VcTileUiController::OnPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      /*is_togglable=*/true, FeatureTile::TileType::kCompact);
  tile_ = tile->GetWeakPtr();

  // Set up view ids for the tile and its children.
  tile->SetID(BubbleViewID::kToggleEffectsButton);
  tile->label()->SetID(BubbleViewID::kToggleEffectLabel);
  tile->icon_button()->SetID(BubbleViewID::kToggleEffectIcon);

  // Set up the initial state of the tile, including elements like label, icon,
  // and colors based on toggle state.
  tile->SetLabel(effect_state_->label_text());
  tile->SetVectorIcon(*effect_state_->icon());
  tile->SetForegroundColorId(cros_tokens::kCrosSysOnSurface);
  std::optional<int> current_state = effect_->get_state_callback().Run();
  CHECK(current_state.has_value());
  tile->SetToggled(current_state.value() != 0);
  UpdateTooltip();

  return tile;
}

void VcTileUiController::OnPressed(const ui::Event& event) {
  if (!effect_state_ || !tile_) {
    return;
  }

  // Execute the associated tile's callback.
  views::Button::PressedCallback(effect_state_->button_callback()).Run(event);

  // Set the toggled state.
  bool toggled = !tile_->IsToggled();
  tile_->SetToggled(toggled);

  // Track UMA metrics about the toggled state.
  TrackToggleUMA(toggled);

  // Play a "toggled-on" or "toggled-off" haptic effect, depending on the toggle
  // state.
  PlayToggleHaptic(toggled);

  // Update properties about the associated tile that change when the toggle
  // state changes, e.g. colors and tooltip text.
  tile_->UpdateColors();
  UpdateTooltip();
}

void VcTileUiController::TrackToggleUMA(bool target_toggle_state) {
  base::UmaHistogramBoolean(
      video_conference_utils::GetEffectHistogramNameForClick(effect_id_),
      target_toggle_state);
}

void VcTileUiController::PlayToggleHaptic(bool target_toggle_state) {
  chromeos::haptics_util::PlayHapticToggleEffect(
      target_toggle_state, ui::HapticTouchpadEffectStrength::kMedium);
}

void VcTileUiController::UpdateTooltip() {
  if (!effect_state_ || !tile_) {
    return;
  }
  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
      l10n_util::GetStringUTF16(effect_state_->accessible_name_id()),
      l10n_util::GetStringUTF16(
          tile_->IsToggled() ? VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON
                             : VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF)));
}

}  // namespace ash::video_conference
