// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/session.h"

#include <memory>

#include "ash/curtain/security_curtain_widget_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash::curtain {

namespace {

// We can only disable the camera if the controller exists, which might
// not be the case if the privacy hub feature is disabled.
bool CanDisableCamera() {
  return CameraPrivacySwitchController::Get() != nullptr;
}

}  // namespace
////////////////////////////////////////////////////////////////////////////////
//  RootWindowsObserver
////////////////////////////////////////////////////////////////////////////////

class Session::RootWindowsObserver : public ShellObserver {
 public:
  RootWindowsObserver(Session* parent, Shell* shell);
  RootWindowsObserver(const RootWindowsObserver&) = delete;
  RootWindowsObserver& operator=(const RootWindowsObserver&) = delete;
  ~RootWindowsObserver() override;

  std::vector<display::Display> GetActiveDisplays(Shell& shell) const;

 private:
  // ShellObserver implementation:
  void OnRootWindowAdded(aura::Window* root_window) override;

  raw_ptr<Session> parent_;

  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};
};

Session::RootWindowsObserver::RootWindowsObserver(Session* parent, Shell* shell)
    : parent_(parent) {
  shell_observation_.Observe(shell);
}

Session::RootWindowsObserver::~RootWindowsObserver() = default;

void Session::RootWindowsObserver::OnRootWindowAdded(
    aura::Window* new_root_window) {
  parent_->CurtainOffRootWindow(new_root_window);
}

////////////////////////////////////////////////////////////////////////////////
//  ScopedAudioOutputMuter
////////////////////////////////////////////////////////////////////////////////
class Session::ScopedAudioOutputMuter {
 public:
  ScopedAudioOutputMuter() {
    CrasAudioHandler::Get()->SetOutputMuteLockedBySecurityCurtain(true);
  }

  ~ScopedAudioOutputMuter() {
    CrasAudioHandler::Get()->SetOutputMuteLockedBySecurityCurtain(false);
  }
};

////////////////////////////////////////////////////////////////////////////////
//  ScopedAudioInputMuter
////////////////////////////////////////////////////////////////////////////////
class Session::ScopedAudioInputMuter {
 public:
  ScopedAudioInputMuter() {
    CrasAudioHandler::Get()->SetInputMuteLockedBySecurityCurtain(true);
  }

  ~ScopedAudioInputMuter() {
    CrasAudioHandler::Get()->SetInputMuteLockedBySecurityCurtain(false);
  }
};

////////////////////////////////////////////////////////////////////////////////
//  ScopedCameraDisabler
////////////////////////////////////////////////////////////////////////////////
class Session::ScopedCameraDisabler {
 public:
  ScopedCameraDisabler() {
    CHECK_DEREF(CameraPrivacySwitchController::Get())
        .SetForceDisableCameraAccess(true);
  }

  ~ScopedCameraDisabler() {
    // Skip cleanup if the shell has been destroyed (so when Chrome is
    // shutting down). This prevents us from using a half-destroyed `shell_`
    // object.
    if (ash::Shell::HasInstance()) {
      CHECK_DEREF(CameraPrivacySwitchController::Get())
          .SetForceDisableCameraAccess(false);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//  Session
////////////////////////////////////////////////////////////////////////////////

Session::Session(Shell* shell,
                 SecurityCurtainController::InitParams init_params)
    : shell_(*shell),
      init_params_(init_params),
      root_windows_observer_(
          std::make_unique<RootWindowsObserver>(this, shell)) {
  if (init_params.mute_audio_input) {
    scoped_audio_input_muter_ = std::make_unique<ScopedAudioInputMuter>();
  }
  if (init_params.disable_camera_access && CanDisableCamera()) {
    scoped_camera_disabler_ = std::make_unique<ScopedCameraDisabler>();
  }
  if (!init_params.mute_audio_output_after.is_max()) {
    audio_output_mute_timer_.Start(
        FROM_HERE, init_params.mute_audio_output_after,
        base::BindOnce(&Session::MuteAudioOutput,
                       // Safe because `this` owns `audio_output_mute_timer_`.
                       base::Unretained(this)));
  }
  if (init_params.disable_input_devices) {
    scoped_input_devices_disabler_ =
        CHECK_DEREF(ui::OzonePlatform::GetInstance()->GetInputController())
            .DisableInputDevices();
  }

  CurtainOffAllRootWindows();
  shell_->power_button_controller()->OnSecurityCurtainEnabled();
}

void Session::Init() {
  // We must ensure we use a cursor drawn by the software, as a cursor drawn
  // by the hardware will appear above our curtain which we do not want.
  // So we tell the shell to tell the cursor manager to ask us if cursor
  // compositing should be enabled.
  // This then ends up calling `SecurityCurtainController::IsEnabled()` which is
  // only true after our constructor is finished, so we must move this in a
  // separate Init() method instead.
  shell_->UpdateCursorCompositingEnabled();
}

Session::~Session() {
  // Skip all cleanup if the shell has been destroyed (so when Chrome is
  // shutting down). This prevents us from using a half-destroyed `shell_`
  // object.
  if (ash::Shell::HasInstance()) {
    RemoveCurtainOfAllRootWindows();
    shell_->UpdateCursorCompositingEnabled();
    shell_->power_button_controller()->OnSecurityCurtainDisabled();
  }
}

void Session::CurtainOffAllRootWindows() {
  for (aura::Window* root_window : shell_->GetAllRootWindows()) {
    CurtainOffRootWindow(root_window);
  }
}

void Session::CurtainOffRootWindow(aura::Window* root_window) {
  DCHECK(root_window->IsRootWindow());
  VLOG(1) << "Adding security curtain over root window " << root_window;

  auto* controller = RootWindowController::ForWindow(root_window);
  DCHECK(controller);

  controller->SetSecurityCurtainWidgetController(
      std::make_unique<SecurityCurtainWidgetController>(
          SecurityCurtainWidgetController::CreateForRootWindow(
              root_window, init_params_.curtain_factory.Run())));
}

void Session::RemoveCurtainOfAllRootWindows() {
  for (aura::Window* root_window : shell_->GetAllRootWindows()) {
    RemoveCurtainOfRootWindow(root_window);
  }
}

void Session::RemoveCurtainOfRootWindow(const aura::Window* root_window) {
  VLOG(1) << "Removing security curtain from root window " << root_window;

  auto* controller = RootWindowController::ForWindow(root_window);
  DCHECK(controller);

  controller->ClearSecurityCurtainWidgetController();
}

void Session::MuteAudioOutput() {
  scoped_audio_output_muter_ = std::make_unique<ScopedAudioOutputMuter>();
}

}  // namespace ash::curtain
