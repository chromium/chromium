// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/mic_gain_slider_controller.h"

#include "ash/system/audio/mic_gain_slider_view.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace ash {

namespace {
MicGainSliderController::MapDeviceSliderCallback* g_map_slider_device_callback =
    nullptr;
}  // namespace

MicGainSliderController::MicGainSliderController() = default;

MicGainSliderController::~MicGainSliderController() = default;

std::unique_ptr<MicGainSliderView> MicGainSliderController::CreateMicGainSlider(
    uint64_t device_id,
    bool internal) {
  std::unique_ptr<MicGainSliderView> slider =
      std::make_unique<MicGainSliderView>(this, device_id, internal);
  if (g_map_slider_device_callback)
    g_map_slider_device_callback->Run(device_id, slider.get());
  return slider;
}

// static
void MicGainSliderController::SetMapDeviceSliderCallbackForTest(
    MapDeviceSliderCallback* map_slider_device_callback) {
  g_map_slider_device_callback = map_slider_device_callback;
}

views::View* MicGainSliderController::CreateView() {
  return new MicGainSliderView(this);
}

void MicGainSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::SliderChangeReason::kByUser)
    return;

  // Unmute if muted.
  if (CrasAudioHandler::Get()->IsInputMuted()) {
    CrasAudioHandler::Get()->SetMuteForDevice(
        CrasAudioHandler::Get()->GetPrimaryActiveInputNode(), false);
  }

  base::RecordAction(base::UserMetricsAction("StatusArea_Mic_Gain_Changed"));

  CrasAudioHandler::Get()->SetInputGainPercent(value * 100);
}

void MicGainSliderController::SliderButtonPressed() {
  auto* const audio_handler = CrasAudioHandler::Get();
  const bool mute = !audio_handler->IsInputMuted();
  if (mute)
    base::RecordAction(base::UserMetricsAction("StatusArea_Mic_Muted"));
  else
    base::RecordAction(base::UserMetricsAction("StatusArea_Mic_Unmuted"));
  audio_handler->SetMuteForDevice(audio_handler->GetPrimaryActiveInputNode(),
                                  mute);
}

}  // namespace ash
