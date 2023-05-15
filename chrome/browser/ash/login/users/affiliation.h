// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AFFILIATION_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AFFILIATION_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

class AccountId;

namespace ash {

// Returns a callback to retrieve device DMToken if the user with
// given `account_id` is affiliated on the device.
base::RepeatingCallback<std::string(const std::vector<std::string>&)>
GetDeviceDMTokenForUserPolicyGetter(const AccountId& account_id);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AFFILIATION_H_
