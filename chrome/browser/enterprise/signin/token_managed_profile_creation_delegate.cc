// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/token_managed_profile_creation_delegate.h"

#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

TokenManagedProfileCreationDelegate::TokenManagedProfileCreationDelegate() =
    default;

TokenManagedProfileCreationDelegate::TokenManagedProfileCreationDelegate(
    const std::string& enrollment_token)
    : enrollment_token_(enrollment_token) {}

TokenManagedProfileCreationDelegate::~TokenManagedProfileCreationDelegate() =
    default;

void TokenManagedProfileCreationDelegate::SetManagedAttributesForProfile(
    ProfileAttributesEntry* entry) {
  CHECK(entry);
  if (!enrollment_token_.empty()) {
    entry->SetProfileManagementEnrollmentToken(enrollment_token_);
  }
}

void TokenManagedProfileCreationDelegate::CheckManagedProfileStatus(
    Profile* new_profile) {
  DCHECK(!new_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
}

void TokenManagedProfileCreationDelegate::OnManagedProfileInitialized(
    Profile* source_profile,
    Profile* new_profile,
    ProfileCreationCallback callback) {
  std::move(callback).Run(new_profile->GetWeakPtr());
}
