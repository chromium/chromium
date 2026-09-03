// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/screen_locker_controller.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {
ScreenLockerController* g_instance = nullptr;
}  // namespace

// static
ScreenLockerController& ScreenLockerController::Get() {
  return CHECK_DEREF(g_instance);
}

ScreenLockerController::ScreenLockerController(
    PrefService* local_state,
    const ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
    SessionManagerClient* session_manager_client,
    SessionTerminationManager* session_termination_manager,
    session_manager::SessionManager* session_manager,
    user_manager::UserManager* user_manager,
    UserAddingScreen* user_adding_screen,
    const user_manager::MultiUserSignInPolicyController*
        multi_user_sign_in_policy_controller,
    system::SystemClock* system_clock)
    : local_state_(CHECK_DEREF(local_state)),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)),
      shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      browser_policy_connector_ash_(CHECK_DEREF(browser_policy_connector_ash)),
      session_manager_client_(CHECK_DEREF(session_manager_client)),
      session_termination_manager_(CHECK_DEREF(session_termination_manager)),
      session_manager_(CHECK_DEREF(session_manager)),
      user_manager_(CHECK_DEREF(user_manager)),
      user_adding_screen_(CHECK_DEREF(user_adding_screen)),
      multi_user_sign_in_policy_controller_(
          CHECK_DEREF(multi_user_sign_in_policy_controller)),
      system_clock_(CHECK_DEREF(system_clock)) {
  CHECK(shared_url_loader_factory_);
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

  DestroyScreenLocker();
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

  if (!session_manager_observation_.IsObserving()) {
    const session_manager::Session& active_session =
        CHECK_DEREF(session_manager_->GetActiveSession());
    const user_manager::User& active_user =
        CHECK_DEREF(user_manager_->FindUser(active_session.account_id()));
    if (active_user.CanLock()) {
      ShowLockScreen();
      return;
    }
  }

  // If the current user's session cannot be locked or the user has not
  // completed all sign-in steps yet, log out instead.
  VLOG(1) << "The user session cannot be locked, logging out";
  session_termination_manager_->StopSession(
      login_manager::SessionStopReason::FAILED_TO_LOCK);
}

void ScreenLockerController::ShowLockScreen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RecordAction(base::UserMetricsAction("ScreenLocker_Show"));

  if (user_manager_->IsLoggedInAsGuest()) {
    VLOG(1) << "Refusing to lock screen for guest account";
    return;
  }

  if (!screen_locker_) {
    // TODO(crbug.com/546312582): Currently the callback runs synchronously, but
    // if it runs asynchronously, an interrupting call of `HideLockScreen` may
    // cause issues.
    // TODO(crbug.com/545532125): Pass SessionControllerClientImpl via ctor.
    SessionControllerClientImpl::Get()->PrepareForLock(
        base::BindOnce(&ScreenLockerController::CreateAndInitScreenLocker,
                       weak_factory_.GetWeakPtr()));
  } else {
    // TODO(crbug.com/546312582): Check if we need to abort an in-flight unlock
    // animation if HideLockScreen was called previously.
    VLOG(1) << "ScreenLocker already exists; calling session manager's "
               "HandleLockScreenShown D-Bus method";
    session_manager_client_->NotifyLockScreenShown();
  }
}

void ScreenLockerController::HideLockScreen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (user_manager_->IsLoggedInAsGuest()) {
    VLOG(1) << "Refusing to hide lock screen for guest account";
    return;
  }

  if (!screen_locker_) {
    return;
  }

  // TODO(crbug.com/545532125): Pass SessionControllerClientImpl via ctor.
  SessionControllerClientImpl::Get()->RunUnlockAnimation(
      base::BindOnce(&ScreenLockerController::OnUnlockAnimationFinished,
                     weak_factory_.GetWeakPtr()));
}

void ScreenLockerController::CreateAndInitScreenLocker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  screen_locker_ = std::make_unique<ScreenLocker>(
      &local_state_.get(), &application_locale_storage_.get(),
      shared_url_loader_factory_, &browser_policy_connector_ash_.get(),
      &multi_user_sign_in_policy_controller_.get(), &system_clock_.get(),
      user_manager_->GetUnlockUsers());
  VLOG(1) << "Created ScreenLocker " << screen_locker_.get();
  screen_locker_->Init();
}

void ScreenLockerController::DestroyScreenLocker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!screen_locker_) {
    return;
  }

  VLOG(1) << "Deleting ScreenLocker " << screen_locker_.get();
  screen_locker_.reset();
}

void ScreenLockerController::OnUnlockAnimationFinished(bool aborted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "ScreenLockerController::OnUnlockAnimationFinished aborted="
          << aborted;
  if (aborted) {
    if (screen_locker_) {
      screen_locker_->ResetToLockedState();
    }
    return;
  }

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  DestroyScreenLocker();
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
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForAccountId(
          CHECK_DEREF(session_manager_->GetActiveSession()).account_id());
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
