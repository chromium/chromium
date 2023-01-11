// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_slider_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/audio/unified_volume_view.h"

namespace ash {

namespace {
UnifiedVolumeSliderController::MapDeviceSliderCallback*
    g_map_slider_device_callback = nullptr;
}  // namespace

UnifiedVolumeSliderController::Delegate::Delegate() = default;

UnifiedVolumeSliderController::Delegate::~Delegate() = default;

UnifiedVolumeSliderController::UnifiedVolumeSliderController(
    UnifiedVolumeSliderController::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

UnifiedVolumeSliderController::UnifiedVolumeSliderController()
    : delegate_(nullptr) {}

UnifiedVolumeSliderController::~UnifiedVolumeSliderController() = default;

std::unique_ptr<UnifiedVolumeView>
UnifiedVolumeSliderController::CreateVolumeSlider(uint64_t device_id) {
  auto slider = std::make_unique<UnifiedVolumeView>(
      this, device_id, /*is_active_output_node=*/false);

  if (g_map_slider_device_callback) {
    g_map_slider_device_callback->Run(device_id, slider.get());
  }

  return slider;
}

// static
void UnifiedVolumeSliderController::SetMapDeviceSliderCallbackForTest(
    MapDeviceSliderCallback* map_slider_device_callback) {
  g_map_slider_device_callback = map_slider_device_callback;
}

views::View* UnifiedVolumeSliderController::CreateView() {
  return new UnifiedVolumeView(this, delegate_, /*is_active_output_node=*/true);
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
