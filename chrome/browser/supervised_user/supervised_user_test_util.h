// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_UTIL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_UTIL_H_

#include <string>

struct AccountInfo;
class Profile;

namespace supervised_user_test_util {

// Add custodians (e.g. parents) to |profile|, which must be a supervised user.
void AddCustodians(Profile* profile);

// Updates preferences relevant to requesting extensions permissions.
void SetSupervisedUserExtensionsMayRequestPermissionsPref(Profile* profile,
                                                          bool enabled);

// Populates account info with a `given_name` and other fake data needed for a
// valid `AccountInfo` structure.
void PopulateAccountInfoWithName(AccountInfo& info,
                                 const std::string& given_name);

}  // namespace supervised_user_test_util

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_UTIL_H_
