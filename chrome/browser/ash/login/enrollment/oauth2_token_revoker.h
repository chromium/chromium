// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_OAUTH2_TOKEN_REVOKER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_OAUTH2_TOKEN_REVOKER_H_

#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace ash {

class OAuth2TokenRevokerBase {
 public:
  virtual ~OAuth2TokenRevokerBase() = default;
  virtual void Start(const std::string& token) = 0;
};

class OAuth2TokenRevoker : public OAuth2TokenRevokerBase {
 public:
  void Start(const std::string& token) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_OAUTH2_TOKEN_REVOKER_H_
