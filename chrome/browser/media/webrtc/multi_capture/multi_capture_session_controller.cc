// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_session_controller.h"

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace multi_capture {

MultiCaptureSessionController::MultiCaptureSessionController(
    content::BrowserContext* context)
    : context_(context) {
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
  context_ = nullptr;
  StopAllCaptures();
}

bool MultiCaptureSessionController::IsSessionEligibleForMultiCapture() const {
  return CHECK_DEREF(session_manager::SessionManager::Get()).session_state() ==
             session_manager::SessionState::ACTIVE &&
         &CHECK_DEREF(ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
             context_)) ==
             CHECK_DEREF(user_manager::UserManager::Get()).GetActiveUser();
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
  if (!IsSessionEligibleForMultiCapture()) {
    StopAllCaptures();
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
