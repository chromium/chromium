// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_TOKEN_MANAGED_PROFILE_CREATION_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_TOKEN_MANAGED_PROFILE_CREATION_DELEGATE_H_

#include "chrome/browser/enterprise/signin/managed_profile_creator.h"

namespace signin_util {
class CookiesMover;
}

// ManagedProfileCreationDelegate for profiles created with an enrollment token.
class TokenManagedProfileCreationDelegate
    : public ManagedProfileCreationDelegate {
 public:
  TokenManagedProfileCreationDelegate();
  explicit TokenManagedProfileCreationDelegate(
      const std::string& enrollment_token);
  TokenManagedProfileCreationDelegate(
      const TokenManagedProfileCreationDelegate&) = delete;
  TokenManagedProfileCreationDelegate& operator=(
      const TokenManagedProfileCreationDelegate&) = delete;
  ~TokenManagedProfileCreationDelegate() override;

  // ManagedProfileCreationDelegate implementation
  void SetManagedAttributesForProfile(ProfileAttributesEntry* entry) override;
  void CheckManagedProfileStatus(Profile* new_profile) override;
  void OnManagedProfileInitialized(Profile* source_profile,
                                   Profile* new_profile,
                                   ProfileCreationCallback callback) override;

 private:
  const std::string enrollment_token_;
  std::unique_ptr<signin_util::CookiesMover> cookies_mover_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_TOKEN_MANAGED_PROFILE_CREATION_DELEGATE_H_
