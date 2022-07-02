// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/audio/cros_audio_config_impl.h"

#include "ash/components/audio/cras_audio_handler.h"

namespace ash::audio_config {

CrosAudioConfigImpl::CrosAudioConfigImpl() {
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

CrosAudioConfigImpl::~CrosAudioConfigImpl() {
  if (CrasAudioHandler::Get())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

uint8_t CrosAudioConfigImpl::GetOutputVolumePercent() const {
  return CrasAudioHandler::Get()->GetOutputVolumePercent();
};

mojom::MuteState CrosAudioConfigImpl::GetOutputMuteState() const {
  // TODO(owenzhang): Add kMutedExternally.
  if (CrasAudioHandler::Get()->IsOutputMutedByPolicy())
    return mojom::MuteState::kMutedByPolicy;

  if (CrasAudioHandler::Get()->IsOutputMuted())
    return mojom::MuteState::kMutedByUser;

  return mojom::MuteState::kNotMuted;
};

void CrosAudioConfigImpl::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                    int volume) {
  NotifyObserversAudioSystemPropertiesChanged();
};

void CrosAudioConfigImpl::OnOutputMuteChanged(bool mute_on) {
  NotifyObserversAudioSystemPropertiesChanged();
};

}  // namespace ash::audio_config