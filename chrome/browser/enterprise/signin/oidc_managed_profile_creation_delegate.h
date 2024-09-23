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
  OidcManagedProfileCreationDelegate(std::string auth_token,
                                     std::string id_token,
                                     const bool dasher_based,
                                     std::string user_display_name,
                                     std::string user_email);

  OidcManagedProfileCreationDelegate(const OidcManagedProfileCreationDelegate&);
  OidcManagedProfileCreationDelegate(OidcManagedProfileCreationDelegate&&);
  OidcManagedProfileCreationDelegate& operator=(
      const OidcManagedProfileCreationDelegate&);
  OidcManagedProfileCreationDelegate& operator=(
      OidcManagedProfileCreationDelegate&&);

  ~OidcManagedProfileCreationDelegate() override;

  // ManagedProfileCreationDelegate implementation
  void SetManagedAttributesForProfile(ProfileAttributesEntry* entry) override;
  void CheckManagedProfileStatus(Profile* new_profile) override;
  void OnManagedProfileInitialized(Profile* source_profile,
                                   Profile* new_profile,
                                   ProfileCreationCallback callback) override;

 private:
  std::string auth_token_;
  std::string id_token_;
  bool dasher_based_ = true;
  std::string user_display_name_;
  std::string user_email_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_MANAGED_PROFILE_CREATION_DELEGATE_H_
