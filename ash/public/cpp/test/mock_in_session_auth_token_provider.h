// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_MOCK_IN_SESSION_AUTH_TOKEN_PROVIDER_H_
#define ASH_PUBLIC_CPP_TEST_MOCK_IN_SESSION_AUTH_TOKEN_PROVIDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ASH_PUBLIC_EXPORT MockInSessionAuthTokenProvider
    : public InSessionAuthTokenProvider {
 public:
  MockInSessionAuthTokenProvider();
  MockInSessionAuthTokenProvider(const MockInSessionAuthTokenProvider&) =
      delete;
  MockInSessionAuthTokenProvider& operator=(
      const MockInSessionAuthTokenProvider&) = delete;
  ~MockInSessionAuthTokenProvider() override;

  // InSessionAuthTokenProvider:
  MOCK_METHOD(void,
              ExchangeForToken,
              (std::unique_ptr<UserContext> user_context,
               OnAuthTokenGenerated callback),
              (override));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_MOCK_IN_SESSION_AUTH_TOKEN_PROVIDER_H_
