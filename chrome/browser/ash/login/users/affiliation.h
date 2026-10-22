// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AFFILIATION_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AFFILIATION_H_

#include <set>
#include <string>
#include <vector>

class AccountId;

namespace policy {
class BrowserPolicyConnectorAsh;
}  // namespace policy

namespace ash {

// Returns device DMToken if the user with given `account_id` is affiliated on
// the device.
std::string GetDeviceDMTokenIfAffiliated(
    const policy::BrowserPolicyConnectorAsh& browser_policy_connector_ash,
    const AccountId& account_id,
    const std::vector<std::string>& user_affiliation_ids);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AFFILIATION_H_
