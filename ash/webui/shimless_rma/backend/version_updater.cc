// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/version_updater.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine/update_engine.pb.h"
#include "chromeos/dbus/update_engine/update_engine_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"

namespace ash {
namespace shimless_rma {

namespace {

void ReportUpdateFailure(const VersionUpdater::OsUpdateStatusCallback& callback,
                         update_engine::Operation operation,
                         const update_engine::ErrorCode& error_code) {
  callback.Run(operation, /*progress=*/0,
               /*rollback=*/false, /*powerwash=*/false,
               /*version=*/std::string(), /*update_size=*/0, error_code);
}

// Returns whether an update is allowed. If not, it calls the callback with
// the appropriate status. |interactive| indicates whether the user is actively
// checking for updates.
bool IsUpdateAllowed() {
  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  const chromeos::NetworkState* network =
      network_state_handler->DefaultNetwork();
  // Don't allow an update if device is currently offline or connected
  // to a network for which data is metered.
  if (!network || !network->IsConnectedState()) {
    return false;
  }

  // TODO(gavinwill): Confirm that metered networks should be excluded.
  const bool is_metered = network_state_handler->default_network_is_metered();
  if (is_metered) {
    return false;
  }

  return true;
}

}  // namespace

VersionUpdater::VersionUpdater() {
  DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
}

VersionUpdater::~VersionUpdater() {
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
}

void VersionUpdater::SetOsUpdateStatusCallback(
    OsUpdateStatusCallback callback) {
  status_callback_ = std::move(callback);
}

// TODO(gavindodd): Align with chrome/browser/ui/webui/help/version_updater.h:27
// so that the update messages are the same as normal Chrome updates.
// See
// chrome/browser/ui/webui/help/version_updater_chromeos.cc:271
// chrome/browser/resources/settings/chromeos/os_about_page/os_about_page.js:418
// chrome/browser/ui/webui/settings/settings_localized_strings_provider.cc:261
// chrome/app/shared_settings_strings.grdp:378
// chrome/app/os_settings_strings.grdp:66
// for mapping of OS update status to strings
void VersionUpdater::CheckOsUpdateAvailable() {
  // TODO(gavinwill): Does this need thread guarding.
  if (check_update_available_ == UPDATE_AVAILABLE) {
    status_callback_.Run(update_engine::Operation::UPDATE_AVAILABLE,
                         /*progress=*/0, /*rollback=*/false,
                         /*powerwash=*/false, new_version_, /*update_size=*/0,
                         update_engine::ErrorCode::kSuccess);
    return;
  }

  if (check_update_available_ == NO_UPDATE_AVAILABLE) {
    status_callback_.Run(update_engine::Operation::IDLE,
                         /*progress=*/0, /*rollback=*/false,
                         /*powerwash=*/false, new_version_, /*update_size=*/0,
                         update_engine::ErrorCode::kSuccess);
    return;
  }

  if (check_update_available_ == CHECKING) {
    return;
  }

  if (!IsUpdateAllowed()) {
    ReportUpdateFailure(status_callback_, update_engine::REPORTING_ERROR_EVENT,
                        update_engine::ErrorCode::kError);
    return;
  }

  if (!IsUpdateEngineIdle()) {
    LOG(ERROR) << "Tried to check for update when UpdateEngine not IDLE.";
    ReportUpdateFailure(status_callback_, update_engine::REPORTING_ERROR_EVENT,
                        update_engine::ErrorCode::kError);
    return;
  }

  check_update_available_ = CHECKING;
  // RequestUpdateCheckWithoutApplying() will check if an update is available
  // without installing it.
  DBusThreadManager::Get()
      ->GetUpdateEngineClient()
      ->RequestUpdateCheckWithoutApplying(
          base::BindOnce(&VersionUpdater::OnRequestUpdateCheck,
                         weak_ptr_factory_.GetWeakPtr()));
}

bool VersionUpdater::UpdateOs() {
  if (!IsUpdateAllowed()) {
    return false;
  }

  if (!IsUpdateEngineIdle()) {
    LOG(ERROR) << "Tried to start update when UpdateEngine not IDLE.";
    return false;
  }
  // RequestUpdateCheck will check if an update is available and install it.
  DBusThreadManager::Get()->GetUpdateEngineClient()->RequestUpdateCheck(
      base::BindOnce(&VersionUpdater::OnRequestUpdateCheck,
                     weak_ptr_factory_.GetWeakPtr()));
  return true;
}

bool VersionUpdater::IsUpdateEngineIdle() {
  return DBusThreadManager::Get()
             ->GetUpdateEngineClient()
             ->GetLastStatus()
             .current_operation() == update_engine::Operation::IDLE;
}

void VersionUpdater::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  if (status.current_operation() == update_engine::UPDATED_NEED_REBOOT) {
    // During RMA there are no other critical processes running so we can
    // automatically reboot.
    DBusThreadManager::Get()->GetUpdateEngineClient()->RebootAfterUpdate();
  }
  switch (status.current_operation()) {
    // If IDLE is received when there is a callback it means no update is
    // available.
    case update_engine::Operation::IDLE:
      // If we reach idle when explicitly checking for update then none is
      // available.
      if (check_update_available_ == CHECKING) {
        check_update_available_ = NO_UPDATE_AVAILABLE;
      }
      break;
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::ERROR:
    case update_engine::Operation::REPORTING_ERROR_EVENT:
      // If get an error when explicitly checking for update then allow
      // another check to be requested later.
      if (check_update_available_ == CHECKING) {
        check_update_available_ = IDLE;
      }
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
      // If we ever see update available then update the next version.
      new_version_ = status.new_version();
      check_update_available_ = UPDATE_AVAILABLE;
      break;
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::CHECKING_FOR_UPDATE:
    case update_engine::Operation::DOWNLOADING:
    case update_engine::Operation::FINALIZING:
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
    case update_engine::Operation::UPDATED_NEED_REBOOT:
    case update_engine::Operation::VERIFYING:
      break;
    // Added to avoid lint error
    case update_engine::Operation::Operation_INT_MIN_SENTINEL_DO_NOT_USE_:
    case update_engine::Operation::Operation_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      break;
  }

  status_callback_.Run(
      status.current_operation(), status.progress(), false,
      status.will_powerwash_after_reboot(), status.new_version(),
      status.new_size(),
      static_cast<update_engine::ErrorCode>(status.last_attempt_error()));
}

void VersionUpdater::OnRequestUpdateCheck(
    UpdateEngineClient::UpdateCheckResult result) {
  if (result != chromeos::UpdateEngineClient::UPDATE_RESULT_SUCCESS) {
    LOG(ERROR) << "OS update request failed.";
    ReportUpdateFailure(status_callback_, update_engine::REPORTING_ERROR_EVENT,
                        update_engine::ErrorCode::kDownloadTransferError);
  }
}

void VersionUpdater::UpdateStatusChangedForTesting(
    const update_engine::StatusResult& status) {
  UpdateStatusChanged(status);
}

}  // namespace shimless_rma
}  // namespace ash
