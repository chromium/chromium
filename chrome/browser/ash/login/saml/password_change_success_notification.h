// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_CHANGE_SUCCESS_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_CHANGE_SUCCESS_NOTIFICATION_H_

class Profile;

namespace ash {

// Utility functions to show a password change success notification.
class PasswordChangeSuccessNotification {
 public:
  // Shows a password change success notification.
  static void Show(Profile* profile);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_CHANGE_SUCCESS_NOTIFICATION_H_
