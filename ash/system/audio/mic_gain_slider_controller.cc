// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/mic_gain_slider_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/audio/mic_gain_slider_view.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

namespace {

MicGainSliderController::MapDeviceSliderCallback* g_map_slider_device_callback =
    nullptr;

}  // namespace

MicGainSliderController::MicGainSliderController()
    : input_gain_metric_delay_timer_(
          FROM_HERE,
          CrasAudioHandler::kMetricsDelayTimerInterval,
          /*receiver=*/this,
          &MicGainSliderController::RecordGainChanged) {}

MicGainSliderController::~MicGainSliderController() = default;

std::unique_ptr<MicGainSliderView> MicGainSliderController::CreateMicGainSlider(
    uint64_t device_id,
    bool internal) {
  std::unique_ptr<MicGainSliderView> slider =
      std::make_unique<MicGainSliderView>(this, device_id, internal);
  if (g_map_slider_device_callback) {
    g_map_slider_device_callback->Run(device_id, slider.get());
  }
  return slider;
}

// static
void MicGainSliderController::SetMapDeviceSliderCallbackForTest(
    MapDeviceSliderCallback* map_slider_device_callback) {
  g_map_slider_device_callback = map_slider_device_callback;
}

std::unique_ptr<UnifiedSliderView> MicGainSliderController::CreateView() {
#if DCHECK_IS_ON()
  DCHECK(!created_view_);
  created_view_ = true;
#endif
  return std::make_unique<MicGainSliderView>(this);
}

QsSliderCatalogName MicGainSliderController::GetCatalogName() {
  return QsSliderCatalogName::kMicGain;
}

void MicGainSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::SliderChangeReason::kByUser) {
    return;
  }

  // Unmute if muted.
  if (CrasAudioHandler::Get()->IsInputMuted()) {
    CrasAudioHandler::Get()->SetMuteForDevice(
        CrasAudioHandler::Get()->GetPrimaryActiveInputNode(),
        /*mute_on=*/false);
  }

  const int level = value * 100;
  if (level != CrasAudioHandler::Get()->GetInputGainPercent()) {
    TrackValueChangeUMA(/*going_up=*/level >
                        CrasAudioHandler::Get()->GetInputGainPercent());
  }

  // Manually sets the mute state since we don't distinguish muted and level is
  // 0 state.
  if (level == 0) {
    CrasAudioHandler::Get()->SetMuteForDevice(
        CrasAudioHandler::Get()->GetPrimaryActiveInputNode(), /*mute_on=*/true);
  }

  CrasAudioHandler::Get()->SetInputGainPercent(level);

  input_gain_metric_delay_timer_.Reset();
}

void MicGainSliderController::SliderButtonPressed() {
  auto* const audio_handler = CrasAudioHandler::Get();
  const bool mute = !audio_handler->IsInputMuted();

  // If the level is 0, this slider is still muted, and nothing needs to be
  // done.
  if (audio_handler->GetInputGainPercent() == 0) {
    return;
  }

  TrackToggleUMA(/*target_toggle_state=*/mute);

  audio_handler->SetMuteForDevice(
      audio_handler->GetPrimaryActiveInputNode(), mute,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);
}

void MicGainSliderController::RecordGainChanged() {
  base::UmaHistogramEnumeration(
      CrasAudioHandler::kInputGainChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  CHECK(audio_handler);
  if (!audio_handler->GetForceRespectUiGainsState()) {
    base::UmaHistogramEnumeration(
        CrasAudioHandler::kInputGainChangedHistogramName,
        CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);
  }
}

}  // namespace ash
