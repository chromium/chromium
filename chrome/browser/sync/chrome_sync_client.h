// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
#define CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/extensions_activity_monitor.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_client.h"
#include "extensions/buildflags/buildflags.h"

class Profile;

namespace syncer {
class ModelTypeController;
class ModelTypeStoreService;
class SyncService;
class SyncableService;
}  // namespace syncer

namespace browser_sync {

class LocalDataQueryHelper;
class LocalDataMigrationHelper;
class SyncApiComponentFactoryImpl;

class ChromeSyncClient : public syncer::SyncClient {
 public:
  explicit ChromeSyncClient(Profile* profile);

  ChromeSyncClient(const ChromeSyncClient&) = delete;
  ChromeSyncClient& operator=(const ChromeSyncClient&) = delete;

  ~ChromeSyncClient() override;

  // SyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeController::TypeVector CreateModelTypeControllers(
      syncer::SyncService* sync_service) override;
  trusted_vault::TrustedVaultClient* GetTrustedVaultClient() override;
  syncer::SyncInvalidationsService* GetSyncInvalidationsService() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;
  bool IsCustomPassphraseAllowed() override;
  bool IsPasswordSyncAllowed() override;
  void SetPasswordSyncAllowedChangeCb(
      const base::RepeatingClosure& cb) override;
  void RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
      const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group)
      override;
#if BUILDFLAG(IS_ANDROID)
  void GetLocalDataDescriptions(
      syncer::ModelTypeSet types,
      base::OnceCallback<void(
          std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback)
      override;
  void TriggerLocalDataMigration(syncer::ModelTypeSet types) override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  // Convenience function that exercises ModelTypeStoreServiceFactory.
  syncer::ModelTypeStoreService* GetModelTypeStoreService();

  // Convenience function used during controller creation.
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Creates the ModelTypeController for syncer::APPS.
  std::unique_ptr<syncer::ModelTypeController> CreateAppsModelTypeController();

  // Creates the ModelTypeController for syncer::APP_SETTINGS.
  std::unique_ptr<syncer::ModelTypeController>
  CreateAppSettingsModelTypeController(syncer::SyncService* sync_service);

  // Creates the ModelTypeController for syncer::WEB_APPS.
  std::unique_ptr<syncer::ModelTypeController>
  CreateWebAppsModelTypeController();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  const raw_ptr<Profile> profile_;

  // The sync api component factory in use by this client.
  std::unique_ptr<browser_sync::SyncApiComponentFactoryImpl> component_factory_;

  // Generates and monitors the ExtensionsActivity object used by sync.
  ExtensionsActivityMonitor extensions_activity_monitor_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<browser_sync::LocalDataQueryHelper> local_data_query_helper_;
  std::unique_ptr<browser_sync::LocalDataMigrationHelper>
      local_data_migration_helper_;

  // Watches password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores.
  PrefChangeRegistrar upm_pref_change_registrar_;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
