// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/password_store/android_backend_error.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

namespace {

std::string BuildCredentialManagerNotificationMetricName(
    const std::string& suffix) {
  return "PasswordManager.SyncControllerDelegateNotifiesCredentialManager." +
         suffix;
}

}  // namespace

PasswordSyncControllerDelegateAndroid::PasswordSyncControllerDelegateAndroid(
    std::unique_ptr<PasswordSyncControllerDelegateBridge> bridge)
    : bridge_(std::move(bridge)) {
  DCHECK(bridge_);
  bridge_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
}

PasswordSyncControllerDelegateAndroid::
    ~PasswordSyncControllerDelegateAndroid() = default;

void PasswordSyncControllerDelegateAndroid::SetSyncObserverCallbacks(
    base::RepeatingClosure on_pwd_sync_state_changed,
    base::OnceClosure on_sync_shutdown) {
  on_pwd_sync_state_changed_ = std::move(on_pwd_sync_state_changed);
  on_sync_shutdown_ = std::move(on_sync_shutdown);
}

void PasswordSyncControllerDelegateAndroid::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_observation_.Observe(sync_service);
  UpdateCredentialManagerSyncStatus(sync_service);
}

void PasswordSyncControllerDelegateAndroid::OnStateChanged(
    syncer::SyncService* sync) {
  UpdateCredentialManagerSyncStatus(sync);
}

void PasswordSyncControllerDelegateAndroid::OnSyncShutdown(
    syncer::SyncService* sync) {
  sync_observation_.Reset();
  if (!on_sync_shutdown_) {
    return;
  }
  std::move(on_sync_shutdown_).Run();
}

void PasswordSyncControllerDelegateAndroid::OnCredentialManagerNotified() {
  base::UmaHistogramBoolean(
      BuildCredentialManagerNotificationMetricName("Success"), 1);
}

void PasswordSyncControllerDelegateAndroid::OnCredentialManagerError(
    const AndroidBackendError& error,
    int api_error_code) {
  base::UmaHistogramBoolean(
      BuildCredentialManagerNotificationMetricName("Success"), 0);
  base::UmaHistogramEnumeration(
      BuildCredentialManagerNotificationMetricName("ErrorCode"), error.type);
  if (error.type == AndroidBackendErrorType::kExternalError) {
    base::UmaHistogramSparse(
        BuildCredentialManagerNotificationMetricName("APIErrorCode"),
        api_error_code);
  }
}

void PasswordSyncControllerDelegateAndroid::UpdateCredentialManagerSyncStatus(
    syncer::SyncService* sync_service) {
  CHECK(sync_service);
  IsPwdSyncEnabled is_enabled = IsPwdSyncEnabled(
      password_manager::sync_util::HasChosenToSyncPasswords(sync_service));
  if (credential_manager_sync_setting_.has_value() &&
      credential_manager_sync_setting_ == is_enabled) {
    return;
  }

  if (on_pwd_sync_state_changed_) {
    on_pwd_sync_state_changed_.Run();
  }

  credential_manager_sync_setting_ = is_enabled;
  if (is_enabled) {
    bridge_->NotifyCredentialManagerWhenSyncing(
        sync_service->GetAccountInfo().email);
  } else {
    bridge_->NotifyCredentialManagerWhenNotSyncing();
  }
}

}  // namespace password_manager
