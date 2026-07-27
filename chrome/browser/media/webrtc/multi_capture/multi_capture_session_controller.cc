// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_session_controller.h"

#include "base/functional/callback.h"
#include "components/session_manager/core/session_manager.h"

namespace multi_capture {

MultiCaptureSessionController::MultiCaptureSessionController() {
  if (user_manager::UserManager::IsInitialized()) {
    user_session_state_observation_.Observe(user_manager::UserManager::Get());
  }
  if (session_manager::SessionManager::Get()) {
    session_manager_observation_.Observe(
        session_manager::SessionManager::Get());
  }
}

MultiCaptureSessionController::~MultiCaptureSessionController() = default;

void MultiCaptureSessionController::Shutdown() {
  user_session_state_observation_.Reset();
  session_manager_observation_.Reset();
  StopAllCaptures();
}

void MultiCaptureSessionController::MultiCaptureStarted(
    const std::string& label,
    base::OnceClosure stop_callback) {
  if (stop_callback) {
    stop_callbacks_[label] = std::move(stop_callback);
  }
}

void MultiCaptureSessionController::MultiCaptureStopped(
    const std::string& label) {
  stop_callbacks_.erase(label);
}

void MultiCaptureSessionController::ActiveUserChanged(
    user_manager::User* active_user) {
  StopAllCaptures();
}

void MultiCaptureSessionController::OnSessionStateChanged() {
  switch (session_manager::SessionManager::Get()->session_state()) {
    case session_manager::SessionState::LOCKED:
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGIN_SECONDARY:
      StopAllCaptures();
      break;
    case session_manager::SessionState::UNKNOWN:
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
    case session_manager::SessionState::RMA:
      break;
  }
}

void MultiCaptureSessionController::StopAllCaptures() {
  for (auto& [label, stop_callback] : stop_callbacks_) {
    if (stop_callback) {
      std::move(stop_callback).Run();
    }
  }
  stop_callbacks_.clear();
}

}  // namespace multi_capture
