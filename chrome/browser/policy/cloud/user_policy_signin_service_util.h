// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_

class Profile;
class ProfileManager;

namespace policy {

// Update the Profile attributes for when the account is signed out.
void UpdateProfileAttributesWhenSignout(Profile* profile,
                                        ProfileManager* profile_manager);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_UTIL_H_
