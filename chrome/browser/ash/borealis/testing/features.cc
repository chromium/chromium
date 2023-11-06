// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/testing/features.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"

namespace borealis {

void AllowBorealis(Profile* profile,
                   base::test::ScopedFeatureList* features,
                   ash::FakeChromeUserManager* user_manager,
                   bool also_enable) {
  features->InitWithFeatures(
      {features::kBorealis, ash::features::kBorealisPermitted}, {});
  AccountId account_id =
      AccountId::FromUserEmail(profile->GetProfileUserName());
  user_manager->AddUserWithAffiliation(account_id, /*is_affiliated=*/false);
  user_manager->LoginUser(account_id);
  profile->GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice,
                                  also_enable);
}

ScopedAllowBorealis::ScopedAllowBorealis(Profile* profile, bool also_enable)
    : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
      profile_(profile) {
  AllowBorealis(profile_, &features_, fake_user_manager_.Get(), also_enable);
}

ScopedAllowBorealis::~ScopedAllowBorealis() {
  profile_->GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, false);
}

}  // namespace borealis
