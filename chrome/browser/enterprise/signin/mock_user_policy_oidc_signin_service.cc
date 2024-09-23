// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/mock_user_policy_oidc_signin_service.h"

#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

namespace policy {

MockUserPolicyOidcSigninService::MockUserPolicyOidcSigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
        policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicyOidcSigninService(profile,
                                  local_state,
                                  device_management_service,
                                  policy_manager,
                                  identity_manager,
                                  std::move(system_url_loader_factory)) {}

MockUserPolicyOidcSigninService::~MockUserPolicyOidcSigninService() = default;

}  // namespace policy
