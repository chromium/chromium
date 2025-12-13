// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_AUTHENTICATION_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_AUTHENTICATION_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/external_cache_delegate.h"
#include "chrome/browser/ash/extensions/external_cache_impl.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace chromeos {

// Loader of extensions force-installed into the sign-in and lock screen
// profiles using the DeviceLoginScreenExtensions policy.
//
// Overview of the initialization flow:
//   StartLoading()
//   => SubscribeAndInitializeFromPrefs()
//   => UpdateStateFromPrefs()
//   => OnExtensionListsUpdated()
//   => {LoadFinished()|OnUpdated()}.
class AuthenticationScreenExtensionsExternalLoader
    : public extensions::ExternalLoader,
      public ExternalCacheDelegate,
      public session_manager::SessionManagerObserver,
      public ProfileManagerObserver {
 public:
  explicit AuthenticationScreenExtensionsExternalLoader(Profile* profile);
  AuthenticationScreenExtensionsExternalLoader(
      const AuthenticationScreenExtensionsExternalLoader&) = delete;
  AuthenticationScreenExtensionsExternalLoader& operator=(
      const AuthenticationScreenExtensionsExternalLoader&) = delete;

  // extensions::ExternalLoader:
  void StartLoading() override;

  // ExternalCacheDelegate:
  void OnExtensionListsUpdated(const base::Value::Dict& prefs) override;
  bool IsRollbackAllowed() const override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // Allows tests to override the default production extension ID being checked.
  static void SetTestBadgeAuthExtensionIdForTesting(const char* id);

 private:
  friend class base::RefCounted<AuthenticationScreenExtensionsExternalLoader>;

  ~AuthenticationScreenExtensionsExternalLoader() override;

  // Called when the pref service gets initialized asynchronously.
  void OnPrefsInitialized(bool success);
  // Starts loading the force-installed extensions specified via prefs and
  // observing the dynamic changes of the prefs.
  void SubscribeAndInitializeFromPrefs();
  // Starts loading the force-installed extensions specified via prefs.
  void UpdateStateFromPrefs();

  const raw_ptr<Profile> profile_;
  // Owned by ExtensionService, outlives |this|.
  ExternalCacheImpl external_cache_;
  PrefChangeRegistrar pref_change_registrar_;
  // Whether the list of extensions was already passed via LoadFinished().
  bool initial_load_finished_ = false;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  // Must be the last member.
  base::WeakPtrFactory<AuthenticationScreenExtensionsExternalLoader>
      weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_AUTHENTICATION_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_
