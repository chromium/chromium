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

// Updates preferences relevant to skipping parent approval for installing
// extensions.
void SetSkipParentApprovalToInstallExtensionsPref(Profile* profile,
                                                  bool enabled);

// Sets the Geolocation content setting value.
void SetSupervisedUserGeolocationEnabledContentSetting(Profile* profile,
                                                       bool enabled);

// Populates account info with a `given_name` and other fake data needed for a
// valid `AccountInfo` structure.
void PopulateAccountInfoWithName(AccountInfo& info,
                                 const std::string& given_name);

// Updates manual block/allow list with a given host.
// e.g. SetManualFilterForHost(profile, "www.example.com", false) adds the
// given host (i.e. "www.example.com") to the blocklist and the supervised user
// will not be able to access this host. Similarly
// SetManualFilterForHost(profile, "www.example.com", true) adds the host to the
// allowlist. The supervised user will be able to access this host.
void SetManualFilterForHost(Profile* profile,
                            const std::string& host,
                            bool allowlist);

}  // namespace supervised_user_test_util

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_UTIL_H_
