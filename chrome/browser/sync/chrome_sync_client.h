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

namespace browser_sync {

class SyncEngineFactoryImpl;

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
  const raw_ptr<Profile> profile_;

  // The sync engine factory in use by this client.
  std::unique_ptr<browser_sync::SyncEngineFactoryImpl> engine_factory_;

  // Generates and monitors the ExtensionsActivity object used by sync.
  browser_sync::ExtensionsActivityMonitor extensions_activity_monitor_;

#if BUILDFLAG(IS_ANDROID)
  // Watches password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores.
  PrefChangeRegistrar upm_pref_change_registrar_;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
