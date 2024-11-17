// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
#define CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/browser_sync/sync_engine_factory_impl.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/sync_client.h"
#include "extensions/buildflags/buildflags.h"

namespace supervised_user {
class SupervisedUserSettingsService;
}  // namespace supervised_user

namespace syncer {
class DataTypeStoreService;
class DeviceInfoSyncService;
}  // namespace syncer

namespace trusted_vault {
class TrustedVaultService;
}  // namespace trusted_vault

namespace browser_sync {

class ExtensionsActivityMonitor;

class ChromeSyncClient : public syncer::SyncClient {
 public:
  ChromeSyncClient(
      const base::FilePath& profile_base_name,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      trusted_vault::TrustedVaultService* trusted_vault_service,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      syncer::DeviceInfoSyncService* device_info_sync_service,
      syncer::DataTypeStoreService* data_type_store_service,
      supervised_user::SupervisedUserSettingsService*
          supervised_user_settings_service,
      std::unique_ptr<ExtensionsActivityMonitor> extensions_activity_monitor);

  ChromeSyncClient(const ChromeSyncClient&) = delete;
  ChromeSyncClient& operator=(const ChromeSyncClient&) = delete;

  ~ChromeSyncClient() override;

  // SyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  trusted_vault::TrustedVaultClient* GetTrustedVaultClient() override;
  syncer::SyncInvalidationsService* GetSyncInvalidationsService() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  syncer::SyncEngineFactory* GetSyncEngineFactory() override;
  bool IsCustomPassphraseAllowed() override;
  bool IsPasswordSyncAllowed() override;
  void SetPasswordSyncAllowedChangeCb(
      const base::RepeatingClosure& cb) override;
  void RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
      const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group)
      override;

 private:
  const base::FilePath profile_base_name_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<trusted_vault::TrustedVaultService> trusted_vault_service_;
  const raw_ptr<syncer::SyncInvalidationsService> sync_invalidations_service_;
  const raw_ptr<supervised_user::SupervisedUserSettingsService>
      supervised_user_settings_service_;
  const std::unique_ptr<ExtensionsActivityMonitor> extensions_activity_monitor_;
  SyncEngineFactoryImpl engine_factory_;

#if BUILDFLAG(IS_ANDROID)
  // Watches password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores.
  PrefChangeRegistrar upm_pref_change_registrar_;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
