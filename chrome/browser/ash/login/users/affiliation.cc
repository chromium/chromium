// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/affiliation.h"

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace ash {
namespace {

std::string GetDeviceDMTokenIfAffiliated(
    const AccountId& account_id,
    const std::vector<std::string>& user_affiliation_ids) {
  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  DCHECK(connector);
  const bool is_affiliated = policy::IsUserAffiliated(
      base::flat_set<std::string>(user_affiliation_ids.begin(),
                                  user_affiliation_ids.end()),
      connector->device_affiliation_ids(), account_id.GetUserEmail());
  if (is_affiliated) {
    const enterprise_management::PolicyData* policy_data =
        DeviceSettingsService::Get()->policy_data();
    CHECK(policy_data);
    return policy_data->request_token();
  }
  return std::string();
}

}  // namespace

base::RepeatingCallback<std::string(const std::vector<std::string>&)>
GetDeviceDMTokenForUserPolicyGetter(const AccountId& account_id) {
  return base::BindRepeating(&GetDeviceDMTokenIfAffiliated, account_id);
}

}  // namespace ash
