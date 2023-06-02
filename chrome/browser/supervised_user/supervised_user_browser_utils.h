
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_

#include <string>

#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class ProfileSelections;
class Profile;

namespace supervised_user {

// Returns true if both the extensions are enabled and the provided url is a
// Webstore or Download url.
bool IsSupportedChromeExtensionURL(const GURL& effective_url);

// Returns true if the parent allowlist should be skipped.
bool ShouldContentSkipParentAllowlistFiltering(content::WebContents* contents);

// Returns how supervised_user factories that are needed in Guest profile
// should be created.
ProfileSelections BuildProfileSelectionsForRegularAndGuest();

// Returns how several supervised_user factories are created before the
// `supervised_user::kUpdateSupervisedUserFactoryCreation` feature is enabled.
ProfileSelections BuildProfileSelectionsLegacy();

// Returns given name of the primary account associated with the profile.
std::string GetAccountGivenName(Profile& profile);

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_
