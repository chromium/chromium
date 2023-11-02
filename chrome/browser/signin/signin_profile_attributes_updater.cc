// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_auth_util.h"

SigninProfileAttributesUpdater::SigninProfileAttributesUpdater(
    signin::IdentityManager* identity_manager,
    ProfileAttributesStorage* profile_attributes_storage,
    const base::FilePath& profile_path,
    PrefService* prefs)
    : identity_manager_(identity_manager),
      profile_attributes_storage_(profile_attributes_storage),
      profile_path_(profile_path),
      prefs_(prefs) {
  DCHECK(identity_manager_);
  DCHECK(profile_attributes_storage_);
  identity_manager_observation_.Observe(identity_manager_.get());

  UpdateProfileAttributes();
}

SigninProfileAttributesUpdater::~SigninProfileAttributesUpdater() = default;

void SigninProfileAttributesUpdater::Shutdown() {
  identity_manager_observation_.Reset();
}

void SigninProfileAttributesUpdater::UpdateProfileAttributes() {
  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_);
  if (!entry) {
    return;
  }

  CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  bool clear_profile = account_info.IsEmpty();

  if (account_info.gaia != entry->GetGAIAId() ||
      !gaia::AreEmailsSame(account_info.email,
                           base::UTF16ToUTF8(entry->GetUserName()))) {
    // Reset prefs. Note: this will also update the |ProfileAttributesEntry|.
    prefs_->ClearPref(prefs::kProfileUsingDefaultAvatar);
    prefs_->ClearPref(prefs::kProfileUsingGAIAAvatar);
  }

  if (clear_profile) {
    entry->SetAuthInfo(std::string(), std::u16string(),
                       /*is_consented_primary_account=*/false);
  } else {
    entry->SetAuthInfo(
        account_info.gaia, base::UTF8ToUTF16(account_info.email),
        identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));
  }
}

void SigninProfileAttributesUpdater::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdateProfileAttributes();
}
