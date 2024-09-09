// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/chrome_sync_client.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "components/browser_sync/sync_engine_factory_impl.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/service/trusted_vault_synthetic_field_trial.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "components/browser_sync/sync_client_utils.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace browser_sync {
namespace {

using content::BrowserThread;

// A global variable is needed to detect multiprofile scenarios where more than
// one profile try to register a synthetic field trial.
bool trusted_vault_synthetic_field_trial_registered = false;

#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kLoopbackServerBackendFilename[] =
    FILE_PATH_LITERAL("profile.pb");
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

ChromeSyncClient::ChromeSyncClient(Profile* profile)
    : profile_(profile), extensions_activity_monitor_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  engine_factory_ = std::make_unique<SyncEngineFactoryImpl>(
      this,
      DeviceInfoSyncServiceFactory::GetForProfile(profile_)
          ->GetDeviceInfoTracker(),
      DataTypeStoreServiceFactory::GetForProfile(profile_)->GetSyncDataPath());

#if BUILDFLAG(IS_ANDROID)
  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store = ProfilePasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);
  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store = AccountPasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);

  local_data_query_helper_ =
      std::make_unique<browser_sync::LocalDataQueryHelper>(
          profile_password_store.get(), account_password_store.get(),
          BookmarkModelFactory::GetForBrowserContext(profile_),
          ReadingListModelFactory::GetAsDualReadingListForBrowserContext(
              profile_));

  local_data_migration_helper_ =
      std::make_unique<browser_sync::LocalDataMigrationHelper>(
          profile_password_store.get(), account_password_store.get(),
          BookmarkModelFactory::GetForBrowserContext(profile_),
          ReadingListModelFactory::GetAsDualReadingListForBrowserContext(
              profile_));
#endif  // BUILDFLAG(IS_ANDROID)
}

ChromeSyncClient::~ChromeSyncClient() = default;

PrefService* ChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return profile_->GetPrefs();
}

signin::IdentityManager* ChromeSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return IdentityManagerFactory::GetForProfile(profile_);
}

base::FilePath ChromeSyncClient::GetLocalSyncBackendFolder() {
  base::FilePath local_sync_backend_folder =
      GetPrefService()->GetFilePath(syncer::prefs::kLocalSyncBackendDir);

#if BUILDFLAG(IS_WIN)
  if (local_sync_backend_folder.empty()) {
    if (!base::PathService::Get(chrome::DIR_ROAMING_USER_DATA,
                                &local_sync_backend_folder)) {
      SYSLOG(WARNING) << "Local sync can not get the roaming profile folder.";
      return base::FilePath();
    }
  }

  // This code as it is now will assume the same profile order is present on
  // all machines, which is not a given. It is to be defined if only the
  // Default profile should get this treatment or all profile as is the case
  // now.
  // TODO(pastarmovj): http://crbug.com/674928 Decide if only the Default one
  // should be considered roamed. For now the code assumes all profiles are
  // created in the same order on all machines.
  local_sync_backend_folder =
      local_sync_backend_folder.Append(profile_->GetBaseName());
  local_sync_backend_folder =
      local_sync_backend_folder.Append(kLoopbackServerBackendFilename);
#endif  // BUILDFLAG(IS_WIN)

  return local_sync_backend_folder;
}

#if BUILDFLAG(IS_ANDROID)
void ChromeSyncClient::GetLocalDataDescriptions(
    syncer::DataTypeSet types,
    base::OnceCallback<void(
        std::map<syncer::DataType, syncer::LocalDataDescription>)> callback) {
  types.RemoveAll(
      local_data_migration_helper_->GetTypesWithOngoingMigrations());
  local_data_query_helper_->Run(types, std::move(callback));
}

void ChromeSyncClient::TriggerLocalDataMigration(syncer::DataTypeSet types) {
  local_data_migration_helper_->Run(types);
}
#endif  // BUILDFLAG(IS_ANDROID)

trusted_vault::TrustedVaultClient* ChromeSyncClient::GetTrustedVaultClient() {
  return TrustedVaultServiceFactory::GetForProfile(profile_)
      ->GetTrustedVaultClient(trusted_vault::SecurityDomainId::kChromeSync);
}

syncer::SyncInvalidationsService*
ChromeSyncClient::GetSyncInvalidationsService() {
  return SyncInvalidationsServiceFactory::GetForProfile(profile_);
}

scoped_refptr<syncer::ExtensionsActivity>
ChromeSyncClient::GetExtensionsActivity() {
  return extensions_activity_monitor_.GetExtensionsActivity();
}

syncer::SyncEngineFactory* ChromeSyncClient::GetSyncEngineFactory() {
  return engine_factory_.get();
}

bool ChromeSyncClient::IsCustomPassphraseAllowed() {
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              profile_->GetProfileKey());
  if (supervised_user_settings_service) {
    return supervised_user_settings_service->IsCustomPassphraseAllowed();
  }
  return true;
}

bool ChromeSyncClient::IsPasswordSyncAllowed() {
#if BUILDFLAG(IS_ANDROID)
  return profile_->GetPrefs()->GetInteger(
             password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores) !=
         static_cast<int>(
             password_manager::prefs::UseUpmLocalAndSeparateStoresState::
                 kOffAndMigrationPending);
#else
  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeSyncClient::SetPasswordSyncAllowedChangeCb(
    const base::RepeatingClosure& cb) {
#if BUILDFLAG(IS_ANDROID)
  CHECK(!upm_pref_change_registrar_.prefs())
      << "SetPasswordSyncAllowedChangeCb() must be called at most once";
  upm_pref_change_registrar_.Init(profile_->GetPrefs());
  // This overfires: the kPasswordsUseUPMLocalAndSeparateStores pref might have
  // changed value, but not IsPasswordSyncAllowed(). That's fine, `cb` should
  // handle this case.
  upm_pref_change_registrar_.Add(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores, cb);
#else
  // IsPasswordSyncAllowed() doesn't change outside of Android.
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeSyncClient::RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
    const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group) {
  CHECK(group.is_valid());

  if (!base::FeatureList::IsEnabled(
          syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrial)) {
    // Disabled via variations, as additional safeguard.
    return;
  }

  // If `trusted_vault_synthetic_field_trial_registered` is true, and given that
  // each SyncService invokes this function at most once, it means that multiple
  // profiles are trying to register a synthetic field trial. In that case,
  // register a special "conflict" group.
  const std::string group_name =
      trusted_vault_synthetic_field_trial_registered
          ? syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
                GetMultiProfileConflictGroupName()
          : group.name();

  trusted_vault_synthetic_field_trial_registered = true;

  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

}  // namespace browser_sync
