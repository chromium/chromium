// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_

#include <string>

#include "base/time/time.h"

class Profile;

namespace ash {

// Utility functions to show or hide a password expiry notification.
class PasswordExpiryNotification {
 public:
  // Shows a password expiry notification. The password has expired if
  // `time_until_expiry` is zero or negative.
  static void Show(Profile* profile, base::TimeDelta time_until_expiry);

  // Returns localized title text appropriate for `time_until_expiry`, eg:
  // "Password expires in 7 days".
  static std::u16string GetTitleText(base::TimeDelta time_until_expiry);

  // Hides the password expiry notification if it is currently shown.
  static void Dismiss(Profile* profile);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_
