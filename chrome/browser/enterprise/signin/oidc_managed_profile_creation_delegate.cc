// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/oidc_managed_profile_creation_delegate.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

OidcManagedProfileCreationDelegate::OidcManagedProfileCreationDelegate() =
    default;

OidcManagedProfileCreationDelegate::OidcManagedProfileCreationDelegate(
    const std::string& auth_token,
    const std::string& id_token,
    const bool dasher_based)
    : auth_token_(auth_token),
      id_token_(id_token),
      dasher_based_(dasher_based) {}

OidcManagedProfileCreationDelegate::~OidcManagedProfileCreationDelegate() =
    default;

void OidcManagedProfileCreationDelegate::SetManagedAttributesForProfile(
    ProfileAttributesEntry* entry) {
  CHECK(entry);
  if (!id_token_.empty() && !auth_token_.empty()) {
    entry->SetProfileManagementOidcTokens(ProfileManagementOicdTokens{
        .auth_token = auth_token_, .id_token = id_token_});
    entry->SetDasherlessManagement(!dasher_based_);
  }
}

void OidcManagedProfileCreationDelegate::CheckManagedProfileStatus(
    Profile* new_profile) {
  CHECK_EQ(new_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed),
           dasher_based_);
}

void OidcManagedProfileCreationDelegate::OnManagedProfileInitialized(
    Profile* source_profile,
    Profile* new_profile,
    ProfileCreationCallback callback) {
  std::move(callback).Run(new_profile->GetWeakPtr());
}
