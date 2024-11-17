// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_

// Util functions relating to managed browsers.

#include <string>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "net/base/host_port_pair.h"
#include "net/ssl/client_cert_identity.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

struct AccountInfo;
class GURL;
class PrefRegistrySimple;
class Profile;

namespace enterprise_util {

enum EnterpriseProfileBadgingTemporarySetting : int {
  kHide = 0,
  kShowOnUnmanagedDevices = 1,
  kShowOnAllDevices = 2,
  kShowOnManagedDevices = 3
};

// Represents which type of managed environment we have.
enum class ManagementEnvironment { kNone, kSchool, kWork };

// Determines whether the browser with `profile` as its primary profile is
// managed. This is determined by looking it there are any policies applied or
// if `profile` is an enterprise profile.
bool IsBrowserManaged(Profile* profile);

// Extracts the domain from provided |email| if it's an email address and
// returns an empty string, otherwise.
std::string GetDomainFromEmail(const std::string& email);

// Returns an HTTPS URL for the host and port identified by `host_port_pair`.
// This is intended to be used to build a `requesting_url` for
// `AutoSelectCertificates`.
GURL GetRequestingUrl(const net::HostPortPair host_port_pair);

// Partitions |client_certs| according to the value of the
// |ContentSettingsType::AUTO_SELECT_CERTIFICATE| content setting for the
// |requesting_url|. If a filter is set, all certs that match the
// filter will be returned in |matching_client_certs|, and all certificates
// that don't in |nonmatching_client_certs|. If no filter is set, then
// all certificates will be returned in |nonmatching_client_certs|.
void AutoSelectCertificates(
    Profile* profile,
    const GURL& requesting_url,
    net::ClientCertIdentityList client_certs,
    net::ClientCertIdentityList* matching_client_certs,
    net::ClientCertIdentityList* nonmatching_client_certs);

// Returns true if the given pref is set through a machine-scope policy.
bool IsMachinePolicyPref(const std::string& pref_name);

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Sets attribute entry 'user_accepted_account_management' of `profile` to
// `accepted`.
void SetUserAcceptedAccountManagement(Profile* profile, bool accepted);

// Returns true is the user has accepted account management through the
// enterprise account confirmation dialog.
bool UserAcceptedAccountManagement(Profile* profile);

// Returns true if the user has consented to sync or has accepted account
// management through the enterprise account confirmation dialog.
bool ProfileCanBeManaged(Profile* profile);

ManagementEnvironment GetManagementEnvironment(Profile* profile,
                                               const AccountInfo& account_info);

// Returns false if the toolbar enterprise badging is disabled by policy.
bool IsEnterpriseBadgingEnabledForToolbar(Profile* profile);

bool CanShowEnterpriseBadgingForAvatar(Profile* profile);

bool CanShowEnterpriseBadgingForMenu(Profile* profile);

// Checks `email_domain` against the list of pre-defined known consumer domains.
// Use this for optimization purposes when you want to skip some code paths for
// most non-managed (=consumer) users with domains like gmail.com. Note that it
// can still return `false` for consumer domains which are not hardcoded in
// implementation.
bool IsKnownConsumerDomain(const std::string& email_domain);

// Returns an enterprise icon hosted at `url` for `profile` using `callback`.
// An empty image is returned in case `url` is invalid or we fail to fetch the
// image.
void GetManagementIcon(const GURL& url,
                       Profile* profile,
                       base::OnceCallback<void(const gfx::Image&)> callback);

}  // namespace enterprise_util

#endif  // CHROME_BROWSER_ENTERPRISE_UTIL_MANAGED_BROWSER_UTILS_H_
