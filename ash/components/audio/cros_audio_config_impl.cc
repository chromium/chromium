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
  if (CrasAudioHandler::Get()) {
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
  }
}

uint8_t CrosAudioConfigImpl::GetOutputVolumePercent() {
  return CrasAudioHandler::Get()->GetOutputVolumePercent();
};

void CrosAudioConfigImpl::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                    int volume) {
  NotifyObserversAudioSystemPropertiesChanged();
};

}  // namespace ash::audio_config