// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_OAUTH2_TOKEN_REVOKER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_OAUTH2_TOKEN_REVOKER_H_

#include "chrome/browser/ash/login/enrollment/oauth2_token_revoker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockOAuth2TokenRevoker : public OAuth2TokenRevokerBase {
 public:
  MockOAuth2TokenRevoker();
  ~MockOAuth2TokenRevoker() override;
  MOCK_METHOD(void, Start, (const std::string& token), (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_OAUTH2_TOKEN_REVOKER_H_
