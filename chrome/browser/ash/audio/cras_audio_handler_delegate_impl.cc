// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/audio/cras_audio_handler_delegate_impl.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"

namespace ash {

void CrasAudioHandlerDelegateImpl::OpenSettingsAudioPage() const {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(),
      chromeos::settings::mojom::kAudioSubpagePath);
}

}  // namespace ash
