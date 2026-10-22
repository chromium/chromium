// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/affiliation.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace ash {

std::string GetDeviceDMTokenIfAffiliated(
    const policy::BrowserPolicyConnectorAsh& browser_policy_connector_ash,
    const AccountId& account_id,
    const std::vector<std::string>& user_affiliation_ids) {
  const bool is_affiliated = policy::IsUserAffiliated(
      base::flat_set<std::string>(user_affiliation_ids.begin(),
                                  user_affiliation_ids.end()),
      browser_policy_connector_ash.device_affiliation_ids(),
      account_id.GetUserEmail());
  if (is_affiliated) {
    const enterprise_management::PolicyData* policy_data =
        DeviceSettingsService::Get()->policy_data();
    CHECK(policy_data);
    return policy_data->request_token();
  }
  return std::string();
}

}  // namespace ash
