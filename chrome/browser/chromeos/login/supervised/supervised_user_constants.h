// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CONSTANTS_H_

#include <string>

namespace chromeos {

extern const char kCryptohomeSupervisedUserKeyLabel[];
extern const char kLegacyCryptohomeSupervisedUserKeyLabel[];

// Set of privileges for usual Supervised User : Mount and UpdatePrivileged
// (update with signed key).
extern const int kCryptohomeSupervisedUserKeyPrivileges;

// Set of privileges for corner case when pre-M35 managed user got new password.
// As we don't have signature yet, Migrate is used instead of UpdatePrivileged.
// Privileges are reset to kCryptohomeSupervisedUserKeyPrivileges as soon as
// manager signs in on the machine.
extern const int kCryptohomeSupervisedUserIncompleteKeyPrivileges;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CONSTANTS_H_
