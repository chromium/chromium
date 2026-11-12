// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"

#include <utility>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "url/gurl.h"

namespace policy {

UserPolicyTestHelper::UserPolicyTestHelper(
    const std::string& account_id,
    ash::EmbeddedPolicyTestServerMixin* embedded_policy_server)
    : account_id_(account_id),
      embedded_policy_server_(embedded_policy_server) {}

UserPolicyTestHelper::~UserPolicyTestHelper() = default;

void UserPolicyTestHelper::SetPolicy(
    const enterprise_management::CloudPolicySettings& policy) {
  DCHECK(embedded_policy_server_);
  embedded_policy_server_->UpdateUserPolicy(policy, account_id_);
}

void UserPolicyTestHelper::SetPolicyAndWait(
    const enterprise_management::CloudPolicySettings& policy,
    Profile* profile) {
  SetPolicy(policy);
  RefreshPolicyAndWait(profile);
}

void UserPolicyTestHelper::RefreshPolicyAndWait(Profile* profile) {
  ProfilePolicyConnector* const profile_connector =
      profile->GetProfilePolicyConnector();
  PolicyService* const policy_service = profile_connector->policy_service();

  base::RunLoop run_loop;
  policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                  PolicyFetchReason::kTest);
  run_loop.Run();
}

}  // namespace policy
