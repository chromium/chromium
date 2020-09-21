// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/dm_token_utils.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/user_manager/user.h"
#else
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

namespace policy {

namespace {

DMToken* GetTestingDMTokenStorage() {
  static base::NoDestructor<DMToken> dm_token(
      DMToken::CreateEmptyTokenForTesting());
  return dm_token.get();
}

}  // namespace

DMToken GetDMToken(Profile* const profile) {
  DMToken dm_token = *GetTestingDMTokenStorage();

#if defined(OS_CHROMEOS)
  if (!profile)
    return dm_token;
  auto* policy_manager = profile->GetUserCloudPolicyManagerChromeOS();
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (dm_token.is_empty() && user && user->IsAffiliated() && policy_manager &&
      policy_manager->IsClientRegistered()) {
    dm_token = DMToken(DMToken::Status::kValid,
                       policy_manager->core()->client()->dm_token());
  }
#elif !defined(OS_ANDROID)
  if (dm_token.is_empty() && g_browser_process->browser_policy_connector()
                                 ->chrome_browser_cloud_management_controller()
                                 ->IsEnabled()) {
    dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();
  }
#endif

  return dm_token;
}

void SetDMTokenForTesting(const DMToken& dm_token) {
  *GetTestingDMTokenStorage() = dm_token;
}

}  // namespace policy
