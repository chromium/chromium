// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/affiliation.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {
namespace {

std::string GetDeviceDMTokenIfAffiliated(
    const AccountId& account_id,
    const std::vector<std::string>& user_affiliation_ids) {
  const AffiliationIDSet set_of_user_affiliation_ids(
      user_affiliation_ids.begin(), user_affiliation_ids.end());
  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  DCHECK(connector);
  const bool is_affiliated = IsUserAffiliated(
      set_of_user_affiliation_ids, connector->GetDeviceAffiliationIDs(),
      account_id.GetUserEmail());
  if (is_affiliated) {
    const enterprise_management::PolicyData* policy_data =
        DeviceSettingsService::Get()->policy_data();
    CHECK(policy_data);
    return policy_data->request_token();
  }
  return std::string();
}

}  // namespace

bool HaveCommonElement(const std::set<std::string>& set1,
                       const std::set<std::string>& set2) {
  std::set<std::string>::const_iterator it1 = set1.begin();
  std::set<std::string>::const_iterator it2 = set2.begin();

  while (it1 != set1.end() && it2 != set2.end()) {
    if (*it1 == *it2)
      return true;
    if (*it1 < *it2) {
      ++it1;
    } else {
      ++it2;
    }
  }
  return false;
}

bool IsUserAffiliated(const AffiliationIDSet& user_affiliation_ids,
                      const AffiliationIDSet& device_affiliation_ids,
                      const std::string& email) {
  // An empty username means incognito user in case of Chrome OS and no
  // logged-in user in case of Chrome (SigninService). Many tests use nonsense
  // email addresses (e.g. 'test') so treat those as non-enterprise users.
  if (email.empty() || email.find('@') == std::string::npos) {
    return false;
  }

  if (policy::IsDeviceLocalAccountUser(email, nullptr)) {
    return true;
  }

  if (!device_affiliation_ids.empty() && !user_affiliation_ids.empty()) {
    return HaveCommonElement(user_affiliation_ids, device_affiliation_ids);
  }

  return false;
}

base::RepeatingCallback<std::string(const std::vector<std::string>&)>
GetDeviceDMTokenForUserPolicyGetter(const AccountId& account_id) {
  return base::BindRepeating(&GetDeviceDMTokenIfAffiliated, account_id);
}

}  // namespace ash
