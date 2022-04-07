// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"

#include "base/logging.h"

namespace ash {

LoginOrLockScreenVisibleWaiter::LoginOrLockScreenVisibleWaiter() {
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager)
    session_observation_.Observe(session_manager);
}

LoginOrLockScreenVisibleWaiter::~LoginOrLockScreenVisibleWaiter() = default;

void LoginOrLockScreenVisibleWaiter::Wait() {
  WaitImpl(/*should_wait_if_already_shown=*/false);
}

void LoginOrLockScreenVisibleWaiter::WaitEvenIfShown() {
  WaitImpl(/*should_wait_if_already_shown=*/true);
}

void LoginOrLockScreenVisibleWaiter::WaitImpl(
    bool should_wait_if_already_shown) {
  auto* session_manager = session_manager::SessionManager::Get();
  DCHECK(session_manager);

  // The screen is already visible, no need to wait, unless we specifically want
  // to.
  if (!should_wait_if_already_shown &&
      session_manager->login_or_lock_screen_shown_for_test()) {
    return;
  }

  if (!session_observation_.IsObserving())
    session_observation_.Observe(session_manager);

  // TODO(crbug.com/1305245) - Remove once the issue is fixed.
  LOG(WARNING) << "LoginOrLockScreenVisibleWaiter::Wait() waiting...";
  run_loop_.Run();
}

void LoginOrLockScreenVisibleWaiter::OnLoginOrLockScreenVisible() {
  // TODO(crbug.com/1305245) - Remove once the issue is fixed.
  LOG(WARNING)
      << "LoginOrLockScreenVisibleWaiter::OnLoginOrLockScreenVisible()";
  run_loop_.Quit();
}

}  // namespace ash
