// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_AUTH_TOKEN_VALIDATOR_H_
#define ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_AUTH_TOKEN_VALIDATOR_H_

#include <string>

#include "ash/services/multidevice_setup/public/cpp/auth_token_validator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace multidevice_setup {

// Fake AuthTokenValidator implementation for tests.
class FakeAuthTokenValidator : public AuthTokenValidator {
 public:
  FakeAuthTokenValidator();

  FakeAuthTokenValidator(const FakeAuthTokenValidator&) = delete;
  FakeAuthTokenValidator& operator=(const FakeAuthTokenValidator&) = delete;

  ~FakeAuthTokenValidator() override;

  // AuthTokenValidator:
  bool IsAuthTokenValid(const std::string& auth_token) override;

  void set_expected_auth_token(const std::string& expected_auth_token) {
    expected_auth_token_ = expected_auth_token;
  }

 private:
  absl::optional<std::string> expected_auth_token_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_AUTH_TOKEN_VALIDATOR_H_
