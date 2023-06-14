// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/dependency_factory_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

namespace enterprise_connectors {

DependencyFactoryImpl::DependencyFactoryImpl(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

DependencyFactoryImpl::~DependencyFactoryImpl() = default;

policy::CloudPolicyManager* DependencyFactoryImpl::GetUserCloudPolicyManager()
    const {
  policy::CloudPolicyManager* user_policy_manager =
      profile_->GetUserCloudPolicyManager();
  if (!user_policy_manager) {
    user_policy_manager = profile_->GetProfileCloudPolicyManager();
  }
  return user_policy_manager;
}

}  // namespace enterprise_connectors
