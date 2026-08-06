// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/screen_locker_controller.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {
ScreenLockerController* g_instance = nullptr;
}  // namespace

// static
ScreenLockerController& ScreenLockerController::Get() {
  return CHECK_DEREF(g_instance);
}

ScreenLockerController::ScreenLockerController(
    SessionTerminationManager* session_termination_manager,
    session_manager::SessionManager* session_manager,
    user_manager::UserManager* user_manager,
    UserAddingScreen* user_adding_screen)
    : session_termination_manager_(CHECK_DEREF(session_termination_manager)),
      session_manager_(CHECK_DEREF(session_manager)),
      user_manager_(CHECK_DEREF(user_manager)),
      user_adding_screen_(CHECK_DEREF(user_adding_screen)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!g_instance);
  g_instance = this;

  CHECK(session_manager_->sessions().empty());
  // Observe SessionManager to wait until the primary session becomes ACTIVE.
  session_manager_observation_.Observe(&session_manager_.get());
}

ScreenLockerController::~ScreenLockerController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // TODO(crbug.com/539761804): Change this to own the ScreenLocker object.
  ScreenLocker::ScheduleDeletion();
}

void ScreenLockerController::HandleShowLockScreenRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (user_adding_screen_->IsRunning()) {
    VLOG(1) << "Waiting for user adding screen to stop";
    if (!user_adding_screen_observation_.IsObserving()) {
      user_adding_screen_observation_.Observe(&user_adding_screen_.get());
    }
    user_adding_screen_->Cancel();
    return;
  }

  auto* active_user = user_manager_->GetActiveUser();
  if (!session_manager_observation_.IsObserving() && active_user &&
      active_user->CanLock()) {
    ScreenLocker::Show();
  } else {
    // If the current user's session cannot be locked or the user has not
    // completed all sign-in steps yet, log out instead.
    VLOG(1) << "The user session cannot be locked, logging out";
    session_termination_manager_->StopSession(
        login_manager::SessionStopReason::FAILED_TO_LOCK);
  }
}

void ScreenLockerController::OnSessionStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("login", "ScreenLockerController::OnSessionStateChanged");

  if (session_manager_->session_state() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  session_manager_observation_.Reset();

  // The user session has just started, so the user has logged in. Mark a
  // strong authentication to allow them to use PIN to unlock the device.
  user_manager::User* user = user_manager_->GetActiveUser();
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForUser(user);
  if (quick_unlock_storage) {
    quick_unlock_storage->MarkStrongAuth();
  }
}

void ScreenLockerController::OnUserAddingFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_adding_screen_observation_.Reset();
  HandleShowLockScreenRequest();
}

}  // namespace ash
