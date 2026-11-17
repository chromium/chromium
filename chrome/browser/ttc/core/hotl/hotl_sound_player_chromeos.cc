// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_is_test.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ttc/core/hotl/hotl_sound_player_internal.h"
#include "chromeos/ash/components/audio/sounds.h"

namespace ttc::internal {

void PlayAttentionChime() {
  if (browser_shutdown::HasShutdownStarted()) {
    return;
  }
  ash::AccessibilityManager* manager = ash::AccessibilityManager::Get();
  if (!manager) {
    CHECK_IS_TEST();
    return;
  }
  manager->PlayEarcon(ash::Sound::kPassthrough, ash::PlaySoundOption::kAlways);
}

}  // namespace ttc::internal
