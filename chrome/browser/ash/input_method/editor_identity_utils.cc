// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_identity_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace ash::input_method {

absl::optional<std::string> GetSignedInUserEmailFromProfile(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);

  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

}  // namespace ash::input_method
