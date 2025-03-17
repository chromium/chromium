// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_test_util.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

void ForceSigninAndModelExecutionCapability(Profile* profile) {
  SetFRECompletion(profile, true);
  SigninWithPrimaryAccount(profile);
  SetModelExecutionCapability(profile, true);
}

void SigninWithPrimaryAccount(Profile* profile) {
  // Sign-in and enable account capability.
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "glic-test@example.com", signin::ConsentLevel::kSignin);
  ASSERT_FALSE(account_info.IsEmpty());

  account_info.full_name = "Glic Testing";
  account_info.given_name = "Glic";
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

void SetModelExecutionCapability(Profile* profile, bool enabled) {
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  AccountInfo primary_account =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(primary_account.IsEmpty());

  AccountCapabilitiesTestMutator mutator(&primary_account.capabilities);
  mutator.set_can_use_model_execution_features(enabled);
  signin::UpdateAccountInfoForAccount(identity_manager, primary_account);
}

void SetFRECompletion(Profile* profile, bool completed) {
  profile->GetPrefs()->SetBoolean(prefs::kGlicCompletedFre, completed);
}

}  // namespace glic
