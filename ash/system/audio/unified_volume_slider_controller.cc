// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_slider_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/shell.h"
#include "ash/system/audio/unified_volume_view.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"

namespace ash {

UnifiedVolumeSliderController::Delegate::Delegate() = default;

UnifiedVolumeSliderController::Delegate::~Delegate() = default;

UnifiedVolumeSliderController::UnifiedVolumeSliderController(
    UnifiedVolumeSliderController::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

UnifiedVolumeSliderController::~UnifiedVolumeSliderController() = default;

views::View* UnifiedVolumeSliderController::CreateView() {
  return new UnifiedVolumeView(this, delegate_);
}

QsSliderCatalogName UnifiedVolumeSliderController::GetCatalogName() {
  return QsSliderCatalogName::kVolume;
}

void UnifiedVolumeSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::SliderChangeReason::kByUser)
    return;

  const int level = value * 100;

  if (level != CrasAudioHandler::Get()->GetOutputVolumePercent()) {
    TrackValueChangeUMA(/*going_up=*/level >
                        CrasAudioHandler::Get()->GetOutputVolumePercent());
  }

  CrasAudioHandler::Get()->SetOutputVolumePercent(level);

  // If the volume is above certain level and it's muted, it should be unmuted.
  if (CrasAudioHandler::Get()->IsOutputMuted() &&
      level > CrasAudioHandler::Get()->GetOutputDefaultVolumeMuteThreshold()) {
    CrasAudioHandler::Get()->SetOutputMute(false);
  }
}

void UnifiedVolumeSliderController::SliderButtonPressed() {
  auto* const audio_handler = CrasAudioHandler::Get();
  const bool mute = !audio_handler->IsOutputMuted();

  TrackToggleUMA(/*target_toggle_state=*/mute);

  audio_handler->SetOutputMute(mute);
}

}  // namespace ash
