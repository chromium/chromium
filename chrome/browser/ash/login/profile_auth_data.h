// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_PROFILE_AUTH_DATA_H_
#define CHROME_BROWSER_ASH_LOGIN_PROFILE_AUTH_DATA_H_

#include "base/callback_forward.h"
#include "base/macros.h"

namespace content {
class StoragePartition;
}

namespace chromeos {

// Helper class that transfers authentication-related data from a BrowserContext
// used for authentication to the user's actual BrowserContext.
class ProfileAuthData {
 public:
  // Transfers authentication-related data from `from_partition` to
  // `to_partition` and invokes `completion_callback` on the UI thread when the
  // operation has completed. The following data is transferred:
  // * The proxy authentication state.
  // * All authentication cookies, if
  //   `transfer_auth_cookies_on_first_login` is true and
  //   `to_partition`'s cookie jar is empty. If the cookie jar is not empty, the
  //   authentication states in `from_partition` and `to_partition` should be
  //   merged using /MergeSession instead.
  // * The authentication cookies set by a SAML IdP, if
  //   `transfer_saml_auth_cookies_on_subsequent_login` is true and
  //   `to_partition`'s cookie jar is not empty.
  // `from_partition` and `to_partition` must live until `completion_callback`
  // is called.
  static void Transfer(content::StoragePartition* from_partition,
                       content::StoragePartition* to_partition,
                       bool transfer_auth_cookies_on_first_login,
                       bool transfer_saml_auth_cookies_on_subsequent_login,
                       base::OnceClosure completion_callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProfileAuthData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_PROFILE_AUTH_DATA_H_
