// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_CRYPTOHOME_AUTHENTICATOR_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_CRYPTOHOME_AUTHENTICATOR_H_

#include "chromeos/ash/components/login/auth/cryptohome_authenticator.h"

namespace ash {

class ChromeCryptohomeAuthenticator : public CryptohomeAuthenticator {
 public:
  explicit ChromeCryptohomeAuthenticator(AuthStatusConsumer* consumer);

  ChromeCryptohomeAuthenticator(const ChromeCryptohomeAuthenticator&) = delete;
  ChromeCryptohomeAuthenticator& operator=(
      const ChromeCryptohomeAuthenticator&) = delete;

 protected:
  ~ChromeCryptohomeAuthenticator() override;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::ChromeCryptohomeAuthenticator;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_CRYPTOHOME_AUTHENTICATOR_H_
