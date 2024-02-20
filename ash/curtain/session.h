// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_SESSION_H_
#define ASH_CURTAIN_SESSION_H_

#include <memory>

#include "ash/curtain/security_curtain_controller.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"

namespace ash {
class Shell;
}  // namespace ash

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class ScopedDisableInputDevices;
}  // namespace ui

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
  void MuteAudioOutput();

  // Helper class observing all root windows being added/removed.
  class RootWindowsObserver;
  // Helper class to mute the audio output during the session.
  class ScopedAudioOutputMuter;
  // Helper class to mute the audio input during the session.
  class ScopedAudioInputMuter;
  // Helper class to disable camera access during the session.
  class ScopedCameraDisabler;
  // Helper class to disable input devices during the session.

  raw_ref<Shell> shell_;
  SecurityCurtainController::InitParams init_params_;
  std::unique_ptr<RootWindowsObserver> root_windows_observer_;
  std::unique_ptr<ScopedAudioOutputMuter> scoped_audio_output_muter_;
  std::unique_ptr<ScopedAudioInputMuter> scoped_audio_input_muter_;
  std::unique_ptr<ScopedCameraDisabler> scoped_camera_disabler_;
  std::unique_ptr<ui::ScopedDisableInputDevices> scoped_input_devices_disabler_;
  base::OneShotTimer audio_output_mute_timer_;
};

}  // namespace ash::curtain

#endif  // ASH_CURTAIN_SESSION_H_
