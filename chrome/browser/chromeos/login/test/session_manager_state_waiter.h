// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SESSION_MANAGER_STATE_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SESSION_MANAGER_STATE_WAITER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {

namespace test {

// Waits for the primary user's session to start, or returns immediately if a
// session has already started.
void WaitForPrimaryUserSessionStart();

}  // namespace test

// Used to wait for session manager to get into a specific session state.
class SessionStateWaiter : public session_manager::SessionManagerObserver {
 public:
  // If |target_state| is null, SessionStateWaiter will simply wait until a
  // session starts.
  explicit SessionStateWaiter(base::Optional<session_manager::SessionState>
                                  target_state = base::nullopt);
  ~SessionStateWaiter() override;

  void Wait();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  base::Optional<session_manager::SessionState> target_state_;
  base::OnceClosure session_state_callback_;
  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionStateWaiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SESSION_MANAGER_STATE_WAITER_H_
