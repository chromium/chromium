// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_SESSION_H_
#define ASH_CURTAIN_SESSION_H_

#include <memory>

#include "ash/curtain/security_curtain_controller.h"
#include "base/memory/raw_ref.h"

namespace ash {
class Shell;
}  // namespace ash

namespace aura {
class Window;
}  // namespace aura

namespace ash::curtain {

// Helper class, created when the security curtain is enabled and destroyed
// when the curtain is disabled.
//
// Will observe root window changes (adding/removing), and update the security
// curtain overlay accordingly to ensure all displays are constantly fully
// curtained off.
class Session {
 public:
  Session(Shell* shell, SecurityCurtainController::InitParams params);
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  ~Session();

  // Must be called after construction, and invokes code that calls
  // `SecurityCurtainController::IsEnabled()` (which will only be true
  // after the constructor of `Session` finishes).
  void Init();

 private:
  void CurtainOffAllRootWindows();
  void CurtainOffRootWindow(aura::Window* root_window);
  void RemoveCurtainOfAllRootWindows();
  void RemoveCurtainOfRootWindow(const aura::Window* root_window);

  // Helper class observing all root windows being added/removed.
  class RootWindowsObserver;
  // Helper class to mute the audio output during the session.
  class ScopedAudioMuter;

  raw_ref<Shell> shell_;
  SecurityCurtainController::InitParams init_params_;
  std::unique_ptr<RootWindowsObserver> root_windows_observer_;
  std::unique_ptr<ScopedAudioMuter> scoped_audio_muter_;
};

}  // namespace ash::curtain

#endif  // ASH_CURTAIN_SESSION_H_
