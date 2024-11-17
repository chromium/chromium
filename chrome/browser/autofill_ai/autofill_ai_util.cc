// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_ai/autofill_ai_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace autofill_ai {

bool IsUserEligible(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return false;
  }

  // The user needs to be in a syncing or signed-in state.
  const signin_util::SignedInState state =
      signin_util::GetSignedInState(identity_manager);
  if (state != signin_util::SignedInState::kSignedIn &&
      state != signin_util::SignedInState::kSyncing) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(optimization_guide::features::internal::
                                        kModelExecutionCapabilityDisable) &&
      identity_manager
              ->FindExtendedAccountInfo(identity_manager->GetPrimaryAccountInfo(
                  signin::ConsentLevel::kSignin))
              .capabilities.can_use_model_execution_features() !=
          signin::Tribool::kTrue) {
    return false;
  }

  return true;
}

}  // namespace autofill_ai
