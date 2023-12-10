// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"

#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/test/profile_prepared_waiter.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace test {

void WaitForPrimaryUserSessionStart() {
  if (!session_manager::SessionManager::Get()->IsSessionStarted())
    SessionStateWaiter().Wait();

  // If login UI is still there profile may not be prepared yet.
  if (ExistingUserController::current_controller()) {
    ProfilePreparedWaiter(
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId())
        .Wait();
  }
}

}  // namespace test

SessionStateWaiter::SessionStateWaiter(
    std::optional<session_manager::SessionState> target_state)
    : target_state_(target_state) {}

SessionStateWaiter::~SessionStateWaiter() = default;

void SessionStateWaiter::Wait() {
  if (done_)
    return;

  if (session_manager::SessionManager::Get()->session_state() ==
      target_state_) {
    return;
  }
  if (!target_state_ &&
      user_manager::UserManager::Get()->GetLoggedInUsers().size() > 0) {
    // Session is already active.
    return;
  }

  session_observation_.Observe(session_manager::SessionManager::Get());

  base::RunLoop run_loop;
  session_state_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void SessionStateWaiter::OnSessionStateChanged() {
  TRACE_EVENT0("login", "SessionStateWaiter::OnSessionStateChanged");
  if (session_manager::SessionManager::Get()->session_state() ==
      target_state_) {
    session_observation_.Reset();
    done_ = true;
    // This may happen before Wait() was called. See
    // UserFlagsLoginTest.PRE_RestartToApplyFlags for example.
    if (session_state_callback_)
      std::move(session_state_callback_).Run();
  }
}

void SessionStateWaiter::OnUserSessionStarted(bool is_primary_user) {
  if (!target_state_) {
    session_observation_.Reset();
    done_ = true;
    // This may happen before Wait() was called. See
    // UserFlagsLoginTest.PRE_RestartToApplyFlags for example.
    if (session_state_callback_)
      std::move(session_state_callback_).Run();
  }
}

}  // namespace ash
