// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/signin_test_util.h"

#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

namespace extensions::signin_test_util {

AccountInfo SimulateExplicitSignIn(
    Profile* profile,
    signin::IdentityTestEnvironment* identity_test_env,
    std::optional<std::string> email) {
  CHECK(switches::IsExtensionsExplicitBrowserSigninEnabled());

  auto account_info = identity_test_env->MakeAccountAvailable(
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kExtensionInstallBubble)
          .Build(email.value_or("testy@mctestface.com")));

  bool has_explicit_sign_in =
      SigninPrefs(*profile->GetPrefs())
          .GetExtensionsExplicitBrowserSignin(account_info.gaia);
  CHECK(has_explicit_sign_in);

  return account_info;
}

}  // namespace extensions::signin_test_util
