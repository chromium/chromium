// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_IMPL_H_
#define CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/auth_token_validator.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {

namespace multidevice_setup {

// Concrete AuthTokenValidator implementation.
//
// The functionality of this class is very simple, to the point that it does not
// merit a test. If this class becomes any more complex, simple unit tests
// should be added.
class AuthTokenValidatorImpl : public AuthTokenValidator, public KeyedService {
 public:
  AuthTokenValidatorImpl();

  AuthTokenValidatorImpl(const AuthTokenValidatorImpl&) = delete;
  AuthTokenValidatorImpl& operator=(const AuthTokenValidatorImpl&) = delete;

  ~AuthTokenValidatorImpl() override;

  bool IsAuthTokenValid(const std::string& auth_token) override;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_IMPL_H_
