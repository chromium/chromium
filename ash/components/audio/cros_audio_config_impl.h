// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_
#define ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_

#include "ash/components/audio/cras_audio_handler.h"
#include "ash/components/audio/cros_audio_config.h"
#include "base/component_export.h"

namespace ash::audio_config {

class COMPONENT_EXPORT(ASH_COMPONENTS_AUDIO) CrosAudioConfigImpl
    : public CrosAudioConfig,
      public CrasAudioHandler::AudioObserver {
 public:
  CrosAudioConfigImpl();
  ~CrosAudioConfigImpl() override;

 private:
  // CrosAudioConfig:
  uint8_t GetOutputVolumePercent() const override;
  mojom::MuteState GetOutputMuteState() const override;

  // CrasAudioHandler::AudioObserver:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;
};

}  // namespace ash::audio_config

#endif  // ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_