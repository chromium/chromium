// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater.h"

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_auth_util.h"

SigninProfileAttributesUpdater::SigninProfileAttributesUpdater(
    signin::IdentityManager* identity_manager,
    SigninErrorController* signin_error_controller,
    ProfileAttributesStorage* profile_attributes_storage,
    const base::FilePath& profile_path,
    PrefService* prefs)
    : identity_manager_(identity_manager),
      signin_error_controller_(signin_error_controller),
      profile_attributes_storage_(profile_attributes_storage),
      profile_path_(profile_path),
      prefs_(prefs) {
  DCHECK(identity_manager_);
  DCHECK(signin_error_controller_);
  DCHECK(profile_attributes_storage_);
  identity_manager_observer_.Add(identity_manager_);
  signin_error_controller_observer_.Add(signin_error_controller);

  UpdateProfileAttributes();
  // TODO(crbug.com/908457): Call OnErrorChanged() here, to catch any change
  // that happened since the construction of SigninErrorController. Profile
  // metrics depend on this bug and must be fixed first.
}

SigninProfileAttributesUpdater::~SigninProfileAttributesUpdater() = default;

void SigninProfileAttributesUpdater::Shutdown() {
  identity_manager_observer_.RemoveAll();
  signin_error_controller_observer_.RemoveAll();
}

void SigninProfileAttributesUpdater::UpdateProfileAttributes() {
  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }

  CoreAccountInfo account_info =
      identity_manager_->GetUnconsentedPrimaryAccountInfo();

  bool clear_profile =
      account_info.IsEmpty() ||
      (!base::FeatureList::IsEnabled(kPersistUPAInProfileInfoCache) &&
       !identity_manager_->HasPrimaryAccount());

  if (account_info.gaia != entry->GetGAIAId() ||
      !gaia::AreEmailsSame(account_info.email,
                           base::UTF16ToUTF8(entry->GetUserName()))) {
    // Reset prefs. Note: this will also update the |ProfileAttributesEntry|.
    prefs_->ClearPref(prefs::kProfileUsingDefaultAvatar);
    prefs_->ClearPref(prefs::kProfileUsingGAIAAvatar);

    // If the concatenation is not enabled, we either show the GAIA name or
    // the local profile name based on |prefs::kProfileUsingDefaultName|.
    // If the profile has been created with a custom name, we need to reset
    // |prefs::kProfileUsingDefaultName| on sign in/sync events for the display
    // name to be the GAIA name otherwise it will be the custom local profile
    // name.
    if (!clear_profile &&
        !ProfileAttributesEntry::ShouldConcatenateGaiaAndProfileName()) {
      prefs_->SetString(
          prefs::kProfileName,
          base::UTF16ToUTF8(
              profile_attributes_storage_->ChooseNameForNewProfile(
                  entry->GetAvatarIconIndex())));
      prefs_->ClearPref(prefs::kProfileUsingDefaultName);
    }
  }

  if (clear_profile) {
    entry->SetLocalAuthCredentials(std::string());
    entry->SetAuthInfo(std::string(), base::string16(), false);
    if (!signin_util::IsForceSigninEnabled())
      entry->SetIsSigninRequired(false);
  } else {
    entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(account_info.email),
                       identity_manager_->HasPrimaryAccount());
  }
}

void SigninProfileAttributesUpdater::OnErrorChanged() {
  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }

  entry->SetIsAuthError(signin_error_controller_->HasError());
}

void SigninProfileAttributesUpdater::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  UpdateProfileAttributes();
}

void SigninProfileAttributesUpdater::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  UpdateProfileAttributes();
}

void SigninProfileAttributesUpdater::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  if (identity_manager_->HasPrimaryAccount() ||
      !base::FeatureList::IsEnabled(kPersistUPAInProfileInfoCache)) {
    return;
  }

  UpdateProfileAttributes();
}
