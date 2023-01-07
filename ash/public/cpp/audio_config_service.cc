// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/audio_config_service.h"

#include "chromeos/ash/components/audio/in_process_instance.h"

namespace ash {

void GetAudioConfigService(
    mojo::PendingReceiver<ash::audio_config::mojom::CrosAudioConfig> receiver) {
  ash::audio_config::BindToInProcessInstance(std::move(receiver));
}

}  // namespace ash
