// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_signin {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kProfileReauthPrompt,
      static_cast<int>(ProfileReauthPrompt::kDoNotPrompt));

  registry->RegisterStringPref(prefs::kProfileUserDisplayName, std::string());
  registry->RegisterStringPref(prefs::kProfileUserEmail, std::string());

  registry->RegisterStringPref(prefs::kPolicyRecoveryToken, std::string());
  registry->RegisterStringPref(prefs::kPolicyRecoveryClientId, std::string());

  registry->RegisterBooleanPref(prefs::kPolicyRecoveryRequired, false);
}

}  // namespace enterprise_signin
