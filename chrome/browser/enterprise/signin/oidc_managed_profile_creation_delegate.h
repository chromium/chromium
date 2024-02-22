// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_MANAGED_PROFILE_CREATION_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_MANAGED_PROFILE_CREATION_DELEGATE_H_

#include "chrome/browser/enterprise/signin/managed_profile_creator.h"

// ManagedProfileCreationDelegate for profiles created by OIDC authentication
// responses.
class OidcManagedProfileCreationDelegate
    : public ManagedProfileCreationDelegate {
 public:
  OidcManagedProfileCreationDelegate();
  OidcManagedProfileCreationDelegate(const std::string& auth_token,
                                     const std::string& id_token,
                                     const bool dasher_based);
  OidcManagedProfileCreationDelegate(
      const OidcManagedProfileCreationDelegate&) = delete;
  OidcManagedProfileCreationDelegate& operator=(
      const OidcManagedProfileCreationDelegate&) = delete;
  ~OidcManagedProfileCreationDelegate() override;

  // ManagedProfileCreationDelegate implementation
  void SetManagedAttributesForProfile(ProfileAttributesEntry* entry) override;
  void CheckManagedProfileStatus(Profile* new_profile) override;
  void OnManagedProfileInitialized(Profile* source_profile,
                                   Profile* new_profile,
                                   ProfileCreationCallback callback) override;

 private:
  const std::string auth_token_;
  const std::string id_token_;
  const bool dasher_based_ = true;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_MANAGED_PROFILE_CREATION_DELEGATE_H_
