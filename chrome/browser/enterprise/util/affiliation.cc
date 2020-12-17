// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/affiliation.h"
#include <set>

namespace chrome {
namespace enterprise_util {

bool IsProfileAffiliated(
    const enterprise_management::PolicyData& profile_policy,
    const enterprise_management::PolicyData& browser_policy) {
  std::set<std::string> profile_affiliation_ids;
  profile_affiliation_ids.insert(profile_policy.user_affiliation_ids().begin(),
                                 profile_policy.user_affiliation_ids().end());
  for (const std::string& browser_affiliation_id :
       browser_policy.device_affiliation_ids()) {
    if (profile_affiliation_ids.count(browser_affiliation_id))
      return true;
  }
  return false;
}

}  // namespace enterprise_util
}  // namespace chrome
