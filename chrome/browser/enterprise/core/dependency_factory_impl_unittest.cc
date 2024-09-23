// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/core/dependency_factory_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_core {

TEST(DependencyFactoryImpl, SameUserCloudManager) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile test_profile;
  DependencyFactoryImpl dependency_factory(&test_profile);

  EXPECT_EQ(dependency_factory.GetUserCloudPolicyManager(),
            static_cast<policy::CloudPolicyManager*>(
                test_profile.GetUserCloudPolicyManager()));
}

}  // namespace enterprise_core
