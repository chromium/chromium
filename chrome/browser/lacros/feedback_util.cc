// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/feedback_util.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace feedback_util {

std::string GetSignedInUserEmail() {
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return std::string();
  // Browser sync consent is not required to use feedback.
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

}  // namespace feedback_util
