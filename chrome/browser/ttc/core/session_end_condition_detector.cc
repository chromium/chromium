// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_end_condition_detector.h"

#include <utility>

#include "base/power_monitor/power_monitor.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/check_is_test.h"
#include "components/session_manager/session_manager_types.h"
#else
#include "base/functional/bind.h"
#include "ui/base/idle/idle.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace ttc {

SessionEndConditionDetector::SessionEndConditionDetector(
    base::RepeatingCallback<void(Reason)> callback)
    : callback_(std::move(callback)) {
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
#if BUILDFLAG(IS_CHROMEOS)
  // In production both singletons outlive `Profile` and its keyed services;
  // unit tests that don't exercise user switching may omit them.
  if (user_manager::UserManager::IsInitialized()) {
    user_session_state_observation_.Observe(user_manager::UserManager::Get());
  } else {
    CHECK_IS_TEST();
  }
  // `OnSessionStateChanged()` also reports the screen lock, earlier than
  // `ui::AddScreenLockCallback()` would.
  if (auto* session_manager = session_manager::SessionManager::Get()) {
    session_manager_observation_.Observe(session_manager);
  } else {
    CHECK_IS_TEST();
  }
#else
  // `base::Unretained()` is safe because the subscription is owned by this
  // object.
  screen_lock_subscription_ = ui::AddScreenLockCallback(
      base::BindRepeating(&SessionEndConditionDetector::OnScreenLockChanged,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

SessionEndConditionDetector::~SessionEndConditionDetector() {
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

void SessionEndConditionDetector::OnSuspend() {
  callback_.Run(Reason::kSuspend);
}

#if BUILDFLAG(IS_CHROMEOS)
void SessionEndConditionDetector::ActiveUserChanged(
    user_manager::User* active_user) {
  callback_.Run(Reason::kUserSwitch);
}

void SessionEndConditionDetector::OnSessionStateChanged() {
  const session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  if (state == session_manager::SessionState::LOCKED) {
    callback_.Run(Reason::kScreenLock);
  } else if (state != session_manager::SessionState::ACTIVE) {
    callback_.Run(Reason::kUserSwitch);
  }
}
#else
void SessionEndConditionDetector::OnScreenLockChanged(bool locked) {
  if (locked) {
    callback_.Run(Reason::kScreenLock);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace ttc
