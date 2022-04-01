// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"

class Profile;

class PrimaryAccountPolicyManager : public KeyedService {
 public:
  explicit PrimaryAccountPolicyManager(Profile* profile);
  ~PrimaryAccountPolicyManager() override;

  PrimaryAccountPolicyManager(const PrimaryAccountPolicyManager&) = delete;
  PrimaryAccountPolicyManager& operator=(const PrimaryAccountPolicyManager&) =
      delete;

  void Initialize();
  void Shutdown() override;

 private:
  void OnSigninAllowedPrefChanged();
  void OnGoogleServicesUsernamePatternChanged();

  raw_ptr<Profile> profile_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  base::WeakPtrFactory<PrimaryAccountPolicyManager> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_H_
