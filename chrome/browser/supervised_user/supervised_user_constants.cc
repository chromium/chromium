// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/pref_names.h"

namespace supervised_users {

const char kAuthorizationHeaderFormat[] = "Bearer %s";
const char kCameraMicDisabled[] = "CameraMicDisabled";
const char kContentPackDefaultFilteringBehavior[] =
    "ContentPackDefaultFilteringBehavior";
const char kContentPackManualBehaviorHosts[] = "ContentPackManualBehaviorHosts";
const char kContentPackManualBehaviorURLs[] = "ContentPackManualBehaviorURLs";
const char kCookiesAlwaysAllowed[] = "CookiesAlwaysAllowed";
const char kForceSafeSearch[] = "ForceSafeSearch";
const char kGeolocationDisabled[] = "GeolocationDisabled";
const char kSafeSitesEnabled[] = "SafeSites";
const char kSigninAllowed[] = "SigninAllowed";
const char kUserName[] = "UserName";

const char kChildAccountSUID[] = "ChildAccountSUID";

const char kChromeAvatarIndex[] = "chrome-avatar-index";
const char kChromeOSAvatarIndex[] = "chromeos-avatar-index";

const char kChromeOSPasswordData[] = "chromeos-password-data";

const char* const kCustodianInfoPrefs[] = {
    prefs::kSupervisedUserCustodianName,
    prefs::kSupervisedUserCustodianEmail,
    prefs::kSupervisedUserCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserCustodianProfileURL,
    prefs::kSupervisedUserCustodianProfileImageURL,
    prefs::kSupervisedUserSecondCustodianName,
    prefs::kSupervisedUserSecondCustodianEmail,
    prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserSecondCustodianProfileURL,
    prefs::kSupervisedUserSecondCustodianProfileImageURL,
};

}  // namespace supervised_users
