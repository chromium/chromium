// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/scheduled_update_checker/os_and_policies_update_checker.h"

#include <utility>

#include "chrome/browser/browser_process.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/network/network_handler.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

OsAndPoliciesUpdateChecker::OsAndPoliciesUpdateChecker(
    chromeos::NetworkStateHandler* network_state_handler)
    : network_state_handler_(network_state_handler),
      update_check_task_executor_(
          update_checker_internal::
              kMaxOsAndPoliciesUpdateCheckerRetryIterations,
          update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime),
      update_engine_client_(
          chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()) {}

OsAndPoliciesUpdateChecker::~OsAndPoliciesUpdateChecker() {
  // Called to remove any observers.
  ResetState();
}

void OsAndPoliciesUpdateChecker::Start(UpdateCheckCompletionCallback cb,
                                       base::TimeDelta timeout) {
  // Override any previous calls by resetting state.
  ResetState();
  is_running_ = true;

  // Must be set before starting the task runner, as callbacks may be called
  // synchronously.
  update_check_completion_cb_ = std::move(cb);

  timeout_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(
          &OsAndPoliciesUpdateChecker::RunCompletionCallbackAndResetState,
          base::Unretained(this), false /* update_check_result */));

  // If there is no network then wait for a network connection before starting
  // an update check. If then network isn't found for a maximum time then report
  // failure. It's safe to use |this| because |wait_for_network_timer_| is a
  // member of this object.
  if (!network_state_handler_->DefaultNetwork()) {
    LOGIN_LOG(EVENT) << "Unable to start update check: no network";
    wait_for_network_timer_.Start(
        FROM_HERE, update_checker_internal::kWaitForNetworkTimeout,
        base::BindOnce(&OsAndPoliciesUpdateChecker::OnNetworkWaitTimeout,
                       base::Unretained(this)));
    network_state_handler_->AddObserver(this, FROM_HERE);
    return;
  }

  ScheduleUpdateCheck();
}

void OsAndPoliciesUpdateChecker::Stop() {
  ResetState();
}

bool OsAndPoliciesUpdateChecker::IsRunning() const {
  return is_running_;
}

void OsAndPoliciesUpdateChecker::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  // If a network is found, it's okay to start an update check. Stop observing
  // for more network changes, any network flakiness will now be handled by
  // timeouts and retries.
  // If no network is found, continue observing for network changes.
  if (!network)
    return;

  wait_for_network_timer_.Stop();
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  ScheduleUpdateCheck();
}

void OsAndPoliciesUpdateChecker::ScheduleUpdateCheck() {
  // If an update was downloaded but not applied then update engine won't do
  // anything. Move straight to the policy state.
  if (update_engine_client_->GetLastStatus().current_operation() ==
      update_engine::Operation::UPDATED_NEED_REBOOT) {
    RefreshPolicies(true /* update_check_result */);
    return;
  }

  // Safe to use "this" as |update_check_task_executor_| is a member of this
  // class.
  update_check_task_executor_.Start(
      base::BindRepeating(&OsAndPoliciesUpdateChecker::StartUpdateCheck,
                          base::Unretained(this)),
      base::BindOnce(&OsAndPoliciesUpdateChecker::OnUpdateCheckFailure,
                     base::Unretained(this)));
}

void OsAndPoliciesUpdateChecker::OnUpdateCheckFailure() {
  // Refresh policies after the update check is finished successfully or
  // unsuccessfully.
  RefreshPolicies(false /* update_check_result */);
}

void OsAndPoliciesUpdateChecker::RunCompletionCallbackAndResetState(
    bool update_check_result) {
  // Flag must be set because |IsRunning| maybe queried when the callback is
  // called below.
  is_running_ = false;
  if (update_check_completion_cb_)
    std::move(update_check_completion_cb_).Run(update_check_result);
  ResetState();
}

void OsAndPoliciesUpdateChecker::OnNetworkWaitTimeout() {
  // No network has been detected, no point querying the server for an update
  // check or polivy refresh. Report failure to the caller.
  RunCompletionCallbackAndResetState(false /* update_check_result */);
}

void OsAndPoliciesUpdateChecker::StartUpdateCheck() {
  // Only one update check can be pending at any time.
  weak_factory_.InvalidateWeakPtrs();

  // Register observer to keep track of different stages of the update check. An
  // observer could be existing due to back to back calls to |StartUpdateCheck|.
  if (!update_engine_client_->HasObserver(this))
    update_engine_client_->AddObserver(this);

  update_engine_client_->RequestUpdateCheck(
      base::BindRepeating(&OsAndPoliciesUpdateChecker::OnUpdateCheckStarted,
                          weak_factory_.GetWeakPtr()));

  // Do nothing for the initial idle stage when the update check state machine
  // has just started.
  ignore_idle_status_ = true;
}

void OsAndPoliciesUpdateChecker::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  // Only ignore idle state if it is the first and only non-error state in the
  // state machine.
  if (ignore_idle_status_ &&
      status.current_operation() > update_engine::Operation::IDLE) {
    ignore_idle_status_ = false;
  }

  switch (status.current_operation()) {
    case update_engine::Operation::IDLE:
      if (!ignore_idle_status_) {
        // No update to download or an error occured mid-way of an existing
        // update download.
        // TODO(abhishekbh): Differentiate between the two cases and call
        // ScheduleRetry in case of error.
        RefreshPolicies(true /* update_check_result */);
      }
      break;

    case update_engine::Operation::UPDATED_NEED_REBOOT:
      // Refresh policies after the update check is finished successfully or
      // unsuccessfully.
      RefreshPolicies(true /* update_check_result */);
      break;

    case update_engine::Operation::ERROR:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
    case update_engine::Operation::REPORTING_ERROR_EVENT:
      update_check_task_executor_.ScheduleRetry(
          base::BindOnce(&OsAndPoliciesUpdateChecker::StartUpdateCheck,
                         base::Unretained(this)));
      break;

    case update_engine::Operation::FINALIZING:
    case update_engine::Operation::VERIFYING:
    case update_engine::Operation::DOWNLOADING:
    case update_engine::Operation::UPDATE_AVAILABLE:
    case update_engine::Operation::CHECKING_FOR_UPDATE:
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
      // Do nothing on intermediate states.
      break;

    default:
      NOTREACHED();
  }
}

void OsAndPoliciesUpdateChecker::OnUpdateCheckStarted(
    chromeos::UpdateEngineClient::UpdateCheckResult result) {
  switch (result) {
    case chromeos::UpdateEngineClient::UPDATE_RESULT_SUCCESS:
      // Nothing to do if the update check started successfully.
      break;
    case chromeos::UpdateEngineClient::UPDATE_RESULT_FAILED:
      update_check_task_executor_.ScheduleRetry(
          base::BindOnce(&OsAndPoliciesUpdateChecker::StartUpdateCheck,
                         base::Unretained(this)));
      break;
    case chromeos::UpdateEngineClient::UPDATE_RESULT_NOTIMPLEMENTED:
      // No point retrying if the operation is not implemented. Refresh policies
      // since the update check is done.
      LOG(ERROR) << "Update check failed: Operation not implemented";
      RefreshPolicies(false /* update_check_result */);
      break;
  }
}

void OsAndPoliciesUpdateChecker::RefreshPolicies(bool update_check_result) {
  g_browser_process->policy_service()->RefreshPolicies(base::BindRepeating(
      &OsAndPoliciesUpdateChecker::OnRefreshPoliciesCompletion,
      weak_factory_.GetWeakPtr(), update_check_result));
}

void OsAndPoliciesUpdateChecker::OnRefreshPoliciesCompletion(
    bool update_check_result) {
  RunCompletionCallbackAndResetState(update_check_result);
}

void OsAndPoliciesUpdateChecker::ResetState() {
  weak_factory_.InvalidateWeakPtrs();
  update_engine_client_->RemoveObserver(this);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  update_check_task_executor_.Stop();
  ignore_idle_status_ = true;
  is_running_ = false;
  wait_for_network_timer_.Stop();
  timeout_timer_.Stop();
}

}  // namespace policy
