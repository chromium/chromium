// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PREFS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_PREFS_ASH_H_

#include <map>
#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefService;
class PrefChangeRegistrar;
class ProfileManager;

namespace crosapi {

// The ash-chrome implementation of the Prefs crosapi interface.
// This class must only be used from the main thread.
class PrefsAsh : public mojom::Prefs, public ProfileManagerObserver {
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

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // Used to inject |profile| as a primary profile for testing.
  void OnPrimaryProfileReadyForTesting(Profile* profile) {
    OnPrimaryProfileReady(profile);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PrefsAshTest, LocalStatePrefs);

  struct State {
    PrefService* pref_service;
    PrefChangeRegistrar* registrar;
    std::string path;
  };
  base::Optional<State> GetState(mojom::PrefPath path);

  void OnPrefChanged(mojom::PrefPath path);
  void OnDisconnect(mojom::PrefPath path, mojo::RemoteSetElementId id);

  // Called when Primary logged in user profile is ready.
  void OnPrimaryProfileReady(Profile* profile);

  // In production, owned by g_browser_process, which outlives this object.
  ProfileManager* const profile_manager_;
  // In production, owned by g_browser_process, which outlives this object.
  PrefService* const local_state_;
  // Owned by the primary profile. This will be set after the profile is
  // initialized.
  PrefService* profile_prefs_ = nullptr;

  PrefChangeRegistrar local_state_registrar_;
  PrefChangeRegistrar profile_prefs_registrar_;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::Prefs> receivers_;

  // This class supports any number of observers.
  std::map<mojom::PrefPath, mojo::RemoteSet<mojom::PrefObserver>> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PREFS_ASH_H_
