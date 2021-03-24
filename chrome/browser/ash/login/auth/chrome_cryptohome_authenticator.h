// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_CRYPTOHOME_AUTHENTICATOR_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_CRYPTOHOME_AUTHENTICATOR_H_

#include <string>

#include "base/macros.h"
#include "chromeos/login/auth/cryptohome_authenticator.h"

namespace chromeos {

class ChromeCryptohomeAuthenticator : public CryptohomeAuthenticator {
 public:
  explicit ChromeCryptohomeAuthenticator(AuthStatusConsumer* consumer);

 protected:
  ~ChromeCryptohomeAuthenticator() override;
 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeCryptohomeAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_CRYPTOHOME_AUTHENTICATOR_H_
