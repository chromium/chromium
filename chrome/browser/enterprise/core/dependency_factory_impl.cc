// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/core/dependency_factory_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

namespace enterprise_core {

DependencyFactoryImpl::DependencyFactoryImpl(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

DependencyFactoryImpl::~DependencyFactoryImpl() = default;

policy::CloudPolicyManager* DependencyFactoryImpl::GetUserCloudPolicyManager()
    const {
  return profile_->GetCloudPolicyManager();
}

}  // namespace enterprise_core
