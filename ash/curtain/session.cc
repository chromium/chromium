// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/session.h"

#include <memory>

#include "ash/curtain/security_curtain_widget_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/logging.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash::curtain {

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
//  ScopedAudioMuter
////////////////////////////////////////////////////////////////////////////////
class Session::ScopedAudioMuter {
 public:
  ScopedAudioMuter() {
    CrasAudioHandler::Get()->SetOutputMuteLockedBySecurityCurtain(true);
  }

  ~ScopedAudioMuter() {
    CrasAudioHandler::Get()->SetOutputMuteLockedBySecurityCurtain(false);
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
          std::make_unique<RootWindowsObserver>(this, shell)),
      scoped_audio_muter_(std::make_unique<ScopedAudioMuter>()) {
  CurtainOffAllRootWindows();
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
  }
}

void Session::CurtainOffAllRootWindows() {
  for (auto* root_window : shell_->GetAllRootWindows()) {
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
              root_window, init_params_.event_filter,
              init_params_.curtain_factory.Run())));
}

void Session::RemoveCurtainOfAllRootWindows() {
  for (auto* root_window : shell_->GetAllRootWindows()) {
    RemoveCurtainOfRootWindow(root_window);
  }
}

void Session::RemoveCurtainOfRootWindow(const aura::Window* root_window) {
  VLOG(1) << "Removing security curtain from root window " << root_window;

  auto* controller = RootWindowController::ForWindow(root_window);
  DCHECK(controller);

  controller->ClearSecurityCurtainWidgetController();
}

}  // namespace ash::curtain
