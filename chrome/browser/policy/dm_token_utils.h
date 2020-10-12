// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DM_TOKEN_UTILS_H_
#define CHROME_BROWSER_POLICY_DM_TOKEN_UTILS_H_

#include "components/policy/core/common/cloud/dm_token.h"

class Profile;

namespace policy {

// Returns a platform specific DM Token:
//  - Browser platforms get the CBCM DM Token.
//  - Unless only_affiliated is set to false, Chrome OS only gets the DM token
//  for the |profile| if it's an affiliated user.  If a nullptr is passed for
//  |profile|, an empty test DM Token is returned.
DMToken GetDMToken(Profile* const profile = nullptr,
                   bool only_affiliated = true);

// Overrides the DM token returned by |GetDMToken|, used for testing purposes.
void SetDMTokenForTesting(const DMToken& dm_token);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DM_TOKEN_UTILS_H_
