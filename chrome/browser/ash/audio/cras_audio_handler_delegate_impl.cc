// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/audio/cras_audio_handler_delegate_impl.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check_deref.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash {

void CrasAudioHandlerDelegateImpl::OpenSettingsAudioPage() const {
  auto* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  if (!active_session) {
    return;
  }
  auto* user =
      user_manager::UserManager::Get()->FindUser(active_session->account_id());
  ash::SettingsAppManager::Get()->Open(
      CHECK_DEREF(user),
      {.sub_page = chromeos::settings::mojom::kAudioSubpagePath});
}

}  // namespace ash
