
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_

#include <string>

#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

class ProfileSelections;
class Profile;

namespace supervised_user {

// Returns true if both the extensions are enabled and the provided url is a
// Webstore or Download url.
bool IsSupportedChromeExtensionURL(const GURL& effective_url);

// Returns true if the extension handling mode for skipping parent approval is
// enabled and the parent has authorized installing extensions without their
// approval.
// Returns false if the user is not supervised.
bool SupervisedUserCanSkipExtensionParentApprovals(const Profile* profile);

// Returns true if the extensions permissions parental control is enabled
// for supervised users.
// Returns false if the user is not supervised.
bool AreExtensionsPermissionsEnabled(Profile* profile);

// Returns true if the parent allowlist should be skipped.
bool ShouldContentSkipParentAllowlistFiltering(content::WebContents* contents);

// Returns how supervised_user factories that are needed in Guest profile
// should be created.
ProfileSelections BuildProfileSelectionsForRegularAndGuest();

// Returns given name of the primary account associated with the profile.
std::string GetAccountGivenName(Profile& profile);

// Asserts that `is_child` matches the child status of the primary user.
// Terminates user session in case of status mismatch in order to prevent
// supervision incidents. Relevant on Chrome OS platform that has the concept
// of the user.
void AssertChildStatusOfTheUser(Profile* profile, bool is_child);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Returns the html content of the reauthentication interstitial for blocked
// sites. This interstitial is associated with the given NavigationHandle.
std::string CreateReauthenticationInterstitialForBlockedSites(
    content::NavigationHandle& navigation_handle,
    FilteringBehaviorReason block_reason);

std::string CreateReauthenticationInterstitialForYouTube(
    content::NavigationHandle& navigation_handle);
#endif

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_
