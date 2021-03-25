// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/affiliation.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"

namespace chrome {
namespace enterprise_util {

bool IsProfileAffiliated(Profile* profile) {
  auto profile_affiliation_ids =
      profile->GetProfilePolicyConnector()->user_affiliation_ids();
  for (const std::string& browser_affiliation_id :
       g_browser_process->browser_policy_connector()
           ->device_affiliation_ids()) {
    if (profile_affiliation_ids.count(browser_affiliation_id))
      return true;
  }
  return false;
}

}  // namespace enterprise_util
}  // namespace chrome
