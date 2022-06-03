// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/affiliation.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/affiliation.h"

namespace chrome {
namespace enterprise_util {

bool IsProfileAffiliated(Profile* profile) {
  return policy::IsAffiliated(
      profile->GetProfilePolicyConnector()->user_affiliation_ids(),
      g_browser_process->browser_policy_connector()->device_affiliation_ids());
}

}  // namespace enterprise_util
}  // namespace chrome
