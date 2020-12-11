// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_GENERATED_PASSWORD_LEAK_DETECTION_PREF_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_GENERATED_PASSWORD_LEAK_DETECTION_PREF_H_

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

extern const char kGeneratedPasswordLeakDetectionPref[];

// A generated preference which layers additional user-facing behaviors ontop of
// the passwords leak detection preference. This allows consumers of the
// preference (such as settings WebUI code) to be purposefully ignorant of the
// logic used to generate these behaviors.
class GeneratedPasswordLeakDetectionPref
    : public extensions::settings_private::GeneratedPref,
      public IdentityManagerFactory::Observer,
      public signin::IdentityManager::Observer,
      public syncer::SyncServiceObserver {
 public:
  explicit GeneratedPasswordLeakDetectionPref(Profile* profile);
  ~GeneratedPasswordLeakDetectionPref() override;

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  std::unique_ptr<extensions::api::settings_private::PrefObject> GetPrefObject()
      const override;

  // Fired when preferences used to generate this preference are changed.
  void OnSourcePreferencesChanged();

  // IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // IdentityManagerFactory::Observer implementation.
  void IdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  // Non-owning pointer to the profile this preference is generated for.
  Profile* const profile_;

  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  ScopedObserver<IdentityManagerFactory, IdentityManagerFactory::Observer>
      identity_manager_factory_observer_{this};
  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};
  PrefChangeRegistrar user_prefs_registrar_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_GENERATED_PASSWORD_LEAK_DETECTION_PREF_H_
