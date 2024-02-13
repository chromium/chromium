// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_VIEW_H_
#define ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_VIEW_H_

#include "ash/system/unified/unified_slider_view.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class MicGainSliderController;

class MicGainSliderView : public UnifiedSliderView,
                          public CrasAudioHandler::AudioObserver {
  METADATA_HEADER(MicGainSliderView, UnifiedSliderView)

 public:
  explicit MicGainSliderView(MicGainSliderController* controller);
  MicGainSliderView(MicGainSliderController* controller,
                    uint64_t device_id,
                    bool internal);
  ~MicGainSliderView() override;

  uint64_t device_id() const { return device_id_; }

  // CrasAudioHandler::AudioObserver:
  void OnInputNodeGainChanged(uint64_t node_id, int gain) override;
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;
  void OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) override;
  void OnActiveInputNodeChanged() override;

  // UnifiedSliderView:
  void VisibilityChanged(View* starting_from, bool is_visible) override;

 private:
  void Update(bool by_user);

  // device id for the input device tied to this slider.
  const uint64_t device_id_;

  // True if the audio device this slider represents is internal.
  const bool internal_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_VIEW_H_
