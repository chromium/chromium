// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"

#include "base/run_loop.h"

namespace chromeos {

namespace test {

void WaitForPrimaryUserSessionStart() {
  if (!session_manager::SessionManager::Get()->IsSessionStarted())
    SessionStateWaiter().Wait();
}

}  // namespace test

SessionStateWaiter::SessionStateWaiter(
    base::Optional<session_manager::SessionState> target_state)
    : target_state_(target_state) {}

SessionStateWaiter::~SessionStateWaiter() = default;

void SessionStateWaiter::Wait() {
  if (session_manager::SessionManager::Get()->session_state() ==
      target_state_) {
    return;
  }

  session_observer_.Add(session_manager::SessionManager::Get());

  base::RunLoop run_loop;
  session_state_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void SessionStateWaiter::OnSessionStateChanged() {
  if (session_manager::SessionManager::Get()->session_state() ==
      target_state_) {
    session_observer_.RemoveAll();
    std::move(session_state_callback_).Run();
  }
}

void SessionStateWaiter::OnUserSessionStarted(bool is_primary_user) {
  if (!target_state_) {
    session_observer_.RemoveAll();
    std::move(session_state_callback_).Run();
  }
}

}  // namespace chromeos
