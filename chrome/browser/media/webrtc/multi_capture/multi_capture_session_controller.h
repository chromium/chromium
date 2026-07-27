// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_SESSION_CONTROLLER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"

namespace user_manager {
class User;
}  // namespace user_manager

namespace multi_capture {

// A KeyedService that handles stopping of active multi capture sessions when
// the session state changes (e.g. active user is switched).
class MultiCaptureSessionController
    : public KeyedService,
      public user_manager::UserManager::UserSessionStateObserver,
      public session_manager::SessionManagerObserver {
 public:
  MultiCaptureSessionController();
  ~MultiCaptureSessionController() override;
  MultiCaptureSessionController(const MultiCaptureSessionController&) = delete;
  MultiCaptureSessionController& operator=(
      const MultiCaptureSessionController&) = delete;

  // KeyedService:
  void Shutdown() override;

  // Called when a new multi capture starts.
  void MultiCaptureStarted(const std::string& label,
                           base::OnceClosure stop_callback);

  // Called when an existing multi capture stops.
  void MultiCaptureStopped(const std::string& label);

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

 private:
  void StopAllCaptures();

  // A map of active multi-capture streams. The key is the unique string label
  // identifying the capture stream. The value is the callback to terminate it.
  base::flat_map<std::string, base::OnceClosure> stop_callbacks_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      user_session_state_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_SESSION_CONTROLLER_H_
