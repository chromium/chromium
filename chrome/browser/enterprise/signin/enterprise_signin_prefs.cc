// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_signin {

namespace prefs {

// Whether or not admin wants to guide users through reauth when their GAIA
// session expires. This is a ProfileReauthPrompt enum.
const char kProfileReauthPrompt[] = "enterprise_signin.profile_reauth_prompt";

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kProfileReauthPrompt,
      static_cast<int>(ProfileReauthPrompt::kDoNotPrompt));
}

}  // namespace enterprise_signin
