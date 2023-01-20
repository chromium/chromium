// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_appsync_optin_client.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

constexpr char kDaemonStorePath[] = "/run/daemon-store/appsync-optin";
constexpr char kDaemonStoreFileName[] = "opted-in";
constexpr char kAppsSyncOptinIOHistogram[] =
    "Cros.AppsSyncOptinFileWriteAttempts";

namespace {
bool IsAppsSyncEnabledForSyncService(const syncer::SyncService& sync_service) {
  return sync_service.GetUserSettings()->GetSelectedOsTypes().Has(
      syncer::UserSelectableOsType::kOsApps);
}

void WriteOptinFile(base::FilePath filepath, bool opted_in) {
  const std::string file_contents = opted_in ? "1" : "0";

  base::UmaHistogramEnumeration(
      kAppsSyncOptinIOHistogram,
      SyncAppsyncOptinClient::AppsSyncOptinFileWrite::kAttempt);
  if (!base::WriteFile(filepath, file_contents)) {
    DLOG(ERROR) << "Failed to persist opt-in change " << file_contents
                << " to daemon-store. State on disk will be inaccurate!";
    base::UmaHistogramEnumeration(
        kAppsSyncOptinIOHistogram,
        SyncAppsyncOptinClient::AppsSyncOptinFileWrite::kFailure);
  }
}
}  // namespace

void SyncAppsyncOptinClient::UpdateOptinFile(
    bool opted_in,
    const syncer::SyncService* sync_service) {
  CoreAccountInfo sync_user_account = sync_service->GetAccountInfo();

  if (sync_user_account.IsEmpty()) {
    DLOG(WARNING) << "No user associated with current SyncService, will not be "
                     "able to write opt-in file!";
    return;
  }

  AccountId account_id = AccountId::FromUserEmailGaiaId(sync_user_account.email,
                                                        sync_user_account.gaia);

  const user_manager::User* user = user_manager_->FindUser(account_id);

  if (!user) {
    DLOG(WARNING) << "Unable to load user for current SyncService, will not be "
                     "able to write opt-in file!";
    return;
  }

  std::string hash = user->username_hash();
  base::FilePath app_sync_optin_path =
      daemon_store_filepath_.Append(hash).Append(kDaemonStoreFileName);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteOptinFile, app_sync_optin_path, opted_in));
}

SyncAppsyncOptinClient::SyncAppsyncOptinClient(
    syncer::SyncService* sync_service,
    user_manager::UserManager* user_manager)
    : SyncAppsyncOptinClient(sync_service,
                             user_manager,
                             base::FilePath(kDaemonStorePath)) {}

SyncAppsyncOptinClient::SyncAppsyncOptinClient(
    syncer::SyncService* sync_service,
    user_manager::UserManager* user_manager,
    const base::FilePath& daemon_store_location)
    : sync_service_(sync_service),
      user_manager_(user_manager),
      is_apps_sync_enabled_(IsAppsSyncEnabledForSyncService(*sync_service)),
      daemon_store_filepath_(daemon_store_location) {
  sync_service_->AddObserver(this);
  UpdateOptinFile(is_apps_sync_enabled_, sync_service);
}

SyncAppsyncOptinClient::~SyncAppsyncOptinClient() {
  sync_service_->RemoveObserver(this);
}

void SyncAppsyncOptinClient::OnStateChanged(syncer::SyncService* sync_service) {
  bool new_is_apps_sync_enabled =
      IsAppsSyncEnabledForSyncService(*sync_service_);
  // Don't update file if we have a non-relevant state change reporter.
  if (new_is_apps_sync_enabled != is_apps_sync_enabled_) {
    UpdateOptinFile(new_is_apps_sync_enabled, sync_service);
    is_apps_sync_enabled_ = new_is_apps_sync_enabled;
  }
}

}  // namespace ash
