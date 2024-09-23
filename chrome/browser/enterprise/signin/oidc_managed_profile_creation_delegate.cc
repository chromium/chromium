// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/oidc_managed_profile_creation_delegate.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

OidcManagedProfileCreationDelegate::OidcManagedProfileCreationDelegate() =
    default;

OidcManagedProfileCreationDelegate::OidcManagedProfileCreationDelegate(
    std::string auth_token,
    std::string id_token,
    const bool dasher_based,
    std::string user_display_name,
    std::string user_email)
    : auth_token_(std::move(auth_token)),
      id_token_(std::move(id_token)),
      dasher_based_(std::move(dasher_based)),
      user_display_name_(std::move(user_display_name)),
      user_email_(std::move(user_email)) {}

OidcManagedProfileCreationDelegate::OidcManagedProfileCreationDelegate(
    const OidcManagedProfileCreationDelegate&) = default;

OidcManagedProfileCreationDelegate::OidcManagedProfileCreationDelegate(
    OidcManagedProfileCreationDelegate&&) = default;

OidcManagedProfileCreationDelegate&
OidcManagedProfileCreationDelegate::operator=(
    const OidcManagedProfileCreationDelegate&) = default;

OidcManagedProfileCreationDelegate&
OidcManagedProfileCreationDelegate::operator=(
    OidcManagedProfileCreationDelegate&&) = default;

OidcManagedProfileCreationDelegate::~OidcManagedProfileCreationDelegate() =
    default;

void OidcManagedProfileCreationDelegate::SetManagedAttributesForProfile(
    ProfileAttributesEntry* entry) {
  CHECK(entry);
  if (!id_token_.empty() && !auth_token_.empty()) {
    entry->SetProfileManagementOidcTokens(ProfileManagementOidcTokens(
        auth_token_, id_token_, base::UTF8ToUTF16(user_display_name_)));
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
  auto* prefs = new_profile->GetPrefs();
  prefs->SetString(enterprise_signin::prefs::kProfileUserDisplayName,
                   user_display_name_);
  prefs->SetString(enterprise_signin::prefs::kProfileUserEmail, user_email_);

  std::move(callback).Run(new_profile->GetWeakPtr());
}
