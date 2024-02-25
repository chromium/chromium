// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PREFS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_PREFS_ASH_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefService;
class PrefChangeRegistrar;

namespace crosapi {

// The ash-chrome implementation of the Prefs crosapi interface.
// This class must only be used from the main thread.
class PrefsAsh : public mojom::Prefs,
                 public ProfileManagerObserver,
                 public ProfileObserver {
 public:
  PrefsAsh(ProfileManager* profile_manager, PrefService* local_state);
  PrefsAsh(const PrefsAsh&) = delete;
  PrefsAsh& operator=(const PrefsAsh&) = delete;
  ~PrefsAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Prefs> receiver);

  // crosapi::mojom::Prefs:
  void GetPref(mojom::PrefPath path, GetPrefCallback callback) override;
  void SetPref(mojom::PrefPath path,
               base::Value value,
               SetPrefCallback callback) override;
  void AddObserver(mojom::PrefPath path,
                   mojo::PendingRemote<mojom::PrefObserver> observer) override;
  void GetExtensionPrefWithControl(
      mojom::PrefPath path,
      GetExtensionPrefWithControlCallback callback) override;
  void ClearExtensionControlledPref(
      mojom::PrefPath path,
      ClearExtensionControlledPrefCallback callback) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Used to inject |profile| as a primary profile for testing.
  void OnPrimaryProfileReadyForTesting(Profile* profile) {
    OnPrimaryProfileReady(profile);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PrefsAshTest, LocalStatePrefs);
  FRIEND_TEST_ALL_PREFIXES(PrefsAshTest, CrosSettingsPrefs);

  enum class AshPrefSource {
    kNormal = 0,
    kExtensionControlled = 1,
    kCrosSettings = 2,
  };

  struct State {
    raw_ptr<PrefService, DanglingUntriaged> pref_service;
    raw_ptr<PrefChangeRegistrar> registrar;
    AshPrefSource pref_source;
    std::string path;
  };
  std::optional<State> GetState(mojom::PrefPath path);
  const base::Value* GetValueForState(std::optional<State> state);

  void OnPrefChanged(mojom::PrefPath path);
  void OnDisconnect(mojom::PrefPath path, mojo::RemoteSetElementId id);

  // Called when Primary logged in user profile is ready.
  void OnPrimaryProfileReady(Profile* profile);

  void OnAppTerminating();

  // In production, owned by g_browser_process, which outlives this object.
  const raw_ptr<PrefService, DanglingUntriaged> local_state_;

  PrefChangeRegistrar local_state_registrar_;
  std::unique_ptr<PrefChangeRegistrar> profile_prefs_registrar_;
  PrefChangeRegistrar extension_prefs_registrar_;

  // CrosSettings doesn't support PrefService and therefore also doesn't support
  // PrefChangeRegistrar, so track these separately.
  std::map<mojom::PrefPath, base::CallbackListSubscription> cros_settings_subs_;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::Prefs> receivers_;

  // This class supports any number of observers.
  std::map<mojom::PrefPath, mojo::RemoteSet<mojom::PrefObserver>> observers_;

  // Observe profile destruction to reset prefs observation.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::CallbackListSubscription on_app_terminating_subscription_;
  // Map of extension pref paths to preference names.
  std::map<mojom::PrefPath, std::string> extension_prefpath_to_name_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PREFS_ASH_H_
