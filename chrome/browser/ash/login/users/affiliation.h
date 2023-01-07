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

typedef std::set<std::string> AffiliationIDSet;

// Returns true if there is at least one common element in two sets.
// Complexity: O(n + m), where n - size of the first set, m - size of
// the second set.
bool HaveCommonElement(const std::set<std::string>& set1,
                       const std::set<std::string>& set2);

// TODO(peletskyi): Remove email after affiliation based implementation will
// fully work. http://crbug.com/515476
// The function makes a decision if user with `user_affiliation_ids` and
// `email` is affiliated on the device with `device_affiliation_ids` and
// `enterprise_domain`.
bool IsUserAffiliated(const AffiliationIDSet& user_affiliation_ids,
                      const AffiliationIDSet& device_affiliation_ids,
                      const std::string& email);

// Returns a callback to retrieve device DMToken if the user with
// given `account_id` is affiliated on the device.
base::RepeatingCallback<std::string(const std::vector<std::string>&)>
GetDeviceDMTokenForUserPolicyGetter(const AccountId& account_id);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AFFILIATION_H_
