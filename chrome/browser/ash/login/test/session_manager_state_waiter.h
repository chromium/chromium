// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_SESSION_MANAGER_STATE_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_SESSION_MANAGER_STATE_WAITER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace ash {

namespace test {

// Waits for the primary user's session to start, or returns immediately if a
// session has already started.
void WaitForPrimaryUserSessionStart();

}  // namespace test

// Used to wait for session manager to get into a specific session state.
class SessionStateWaiter : public session_manager::SessionManagerObserver {
 public:
  // If `target_state` is null, SessionStateWaiter will simply wait until a
  // session starts.
  explicit SessionStateWaiter(
      std::optional<session_manager::SessionState> target_state = std::nullopt);

  SessionStateWaiter(const SessionStateWaiter&) = delete;
  SessionStateWaiter& operator=(const SessionStateWaiter&) = delete;

  ~SessionStateWaiter() override;

  void Wait();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  std::optional<session_manager::SessionState> target_state_;
  base::OnceClosure session_state_callback_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  bool done_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_SESSION_MANAGER_STATE_WAITER_H_
