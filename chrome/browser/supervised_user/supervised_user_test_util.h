// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_UTIL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_UTIL_H_

#include <string_view>

#include "chrome/browser/profiles/profile.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

class AccountInfo;

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

// Returns `info` copy with populated `given_name` and other fake data needed
// for a valid `AccountInfo` structure.
[[nodiscard]] AccountInfo PopulateAccountInfoWithName(
    const AccountInfo& info,
    const std::string& given_name);

// Updates manual block/allow list with a given host.
// e.g. SetManualFilterForHost(profile, "www.example.com", false) adds the
// given host (i.e. "www.example.com") to the blocklist and the supervised user
// will not be able to access this host. Similarly
// SetManualFilterForHost(profile, "www.example.com", true) adds the host to the
// allowlist. The supervised user will be able to access this host.
void SetManualFilterForHost(Profile* profile,
                            std::string_view host,
                            bool allowlist);

// Updates manual block/allow list with a given url.
// e.g. SetManualFilterForUrl(profile, "http://www.example.com", false) adds the
// given url to the blocklist and the supervised user
// will not be able to access this url. Similarly
// SetManualFilterForUrl(profile, "www.example.com", true) adds the url to the
// allowlist. The supervised user will be able to access this url.
void SetManualFilterForUrl(Profile* profile,
                           std::string_view url,
                           bool allowlist);

// Convenience method for browser tests emulating parent changes to web
// filtering.
void SetWebFilterType(const Profile* profile,
                      supervised_user::WebFilterType web_filter_type);
}  // namespace supervised_user_test_util

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_UTIL_H_
