// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LACROS_AVAILABILITY_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ASH_CROSAPI_LACROS_AVAILABILITY_POLICY_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;
class PrefService;
class ProfileManager;

namespace crosapi {

// Watches LacrosAvailability per_profile:False policy.
// On its value update, send the message to session_manager to
// preserve the update if it is restarted.
// Specifically, this is designed to be used only for BrowserDataMigrator,
// because it requires to run under very early stage (before policy
// initialization) to avoid session data breakage.
class LacrosAvailabilityPolicyObserver : public ProfileManagerObserver {
 public:
  LacrosAvailabilityPolicyObserver();
  LacrosAvailabilityPolicyObserver(const LacrosAvailabilityPolicyObserver&) =
      delete;
  LacrosAvailabilityPolicyObserver& operator=(
      const LacrosAvailabilityPolicyObserver&) = delete;
  ~LacrosAvailabilityPolicyObserver() override;

  // ProfileManagerObserver override.
  void OnProfileAdded(Profile* profile) override;

 private:
  // Called when LacrosAvailability policy value is updated.
  void OnChanged();

  const raw_ptr<ProfileManager> profile_manager_;
  const raw_ptr<PrefService> local_state_;
  raw_ptr<Profile> primary_profile_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<LacrosAvailabilityPolicyObserver> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LACROS_AVAILABILITY_POLICY_OBSERVER_H_
