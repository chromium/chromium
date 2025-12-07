// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_LOCKED_FULLSCREEN_ARC_LOCKED_FULLSCREEN_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_LOCKED_FULLSCREEN_ARC_LOCKED_FULLSCREEN_MANAGER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"

class Profile;

namespace vm_tools::concierge {
class SuccessFailureResponse;
}  // namespace vm_tools::concierge

namespace arc {

// Sets up the ARC VM for locked fullscreen mode. Initialized and owned by the
// `ArcServiceLauncher` with the primary user profile.
class ArcLockedFullscreenManager : public ArcSessionManagerObserver {
 public:
  explicit ArcLockedFullscreenManager(Profile* profile);
  ArcLockedFullscreenManager(const ArcLockedFullscreenManager&) = delete;
  ArcLockedFullscreenManager& operator=(const ArcLockedFullscreenManager&) =
      delete;
  ~ArcLockedFullscreenManager() override;

  // Sets up ARC for locked fullscreen mode should the window be `locked`.
  // Restores ARC otherwise.
  void UpdateForLockedFullscreenMode(bool locked);

  // ArcSessionManagerObserver:
  void OnArcStarted() override;
  void OnShutdown() override;

 private:
  // Internal helper used to mute or unmute ARC audio.
  void MuteOrUnmuteArcAudio();

  // Callback triggered after ARC `mute` audio request was processed.
  void OnMuteOrUnmuteArcAudioProcessed(
      bool mute,
      std::optional<vm_tools::concierge::SuccessFailureResponse> response);

  // Attempts to mute ARC audio if set to true, or unmute ARC audio if set to
  // false.
  std::optional<bool> mute_audio_requested_;
  const base::WeakPtr<Profile> profile_;

  // Scoped observer used to observe ARC session events. Especially needed
  // to delay setup until ARC is up.
  base::ScopedObservation<ArcSessionManager, ArcLockedFullscreenManager>
      arc_session_manager_observation_{this};

  base::WeakPtrFactory<ArcLockedFullscreenManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_LOCKED_FULLSCREEN_ARC_LOCKED_FULLSCREEN_MANAGER_H_
