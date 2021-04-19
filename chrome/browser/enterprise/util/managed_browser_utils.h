// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_

// Util functions relating to managed browsers.

#include <string>

#include "net/ssl/client_cert_identity.h"

class GURL;
class PrefRegistrySimple;
class Profile;

namespace chrome {
namespace enterprise_util {

// Determines whether policies have been applied to this browser at the profile
// or machine level.
bool HasBrowserPoliciesApplied(Profile* profile);

// Extracts the domain from provided |email| if it's an email address and
// returns an empty string, otherwise.
std::string GetDomainFromEmail(const std::string& email);

// Attempts to auto-select a client certificate according to the value of
// |ContentSettingsType::AUTO_SELECT_CERTIFICATE| content setting for
// |requesting_url|. If no certificate was auto-selected, returns nullptr.
std::unique_ptr<net::ClientCertIdentity> AutoSelectCertificate(
    Profile* profile,
    const GURL& requesting_url,
    net::ClientCertIdentityList& client_certs);

// Returns true if the given pref is set through a machine-scope policy.
bool IsMachinePolicyPref(const std::string& pref_name);

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_util
}  // namespace chrome

#endif  // CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_
