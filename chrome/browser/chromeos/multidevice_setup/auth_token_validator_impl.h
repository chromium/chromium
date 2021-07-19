// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_IMPL_H_

// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chromeos/services/multidevice_setup/public/cpp/auth_token_validator.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {
namespace multidevice_setup {

// Concrete AuthTokenValidator implementation.
//
// The functionality of this class is very simple, to the point that it does not
// merit a test. If this class becomes any more complex, simple unit tests
// should be added.
class AuthTokenValidatorImpl : public AuthTokenValidator, public KeyedService {
 public:
  AuthTokenValidatorImpl(
      quick_unlock::QuickUnlockStorage* quick_unlock_storage);
  ~AuthTokenValidatorImpl() override;

  bool IsAuthTokenValid(const std::string& auth_token) override;

 private:
  // KeyedService:
  void Shutdown() override;

  quick_unlock::QuickUnlockStorage* quick_unlock_storage_;

  DISALLOW_COPY_AND_ASSIGN(AuthTokenValidatorImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_IMPL_H_
