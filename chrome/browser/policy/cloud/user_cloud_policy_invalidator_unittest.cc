// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class UserCloudPolicyInvalidatorTest : public testing::Test {
 protected:
  UserCloudPolicyInvalidatorTest() = default;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(UserCloudPolicyInvalidatorTest,
       ShutdownBeforeProfileInitializationCompleteDoesNotCrash) {
  TestingProfile profile;
  auto store = std::make_unique<MockCloudPolicyStore>(
      dm_protocol::GetChromeUserPolicyType());
  MockCloudPolicyManager policy_manager(
      std::move(store), task_environment_.GetMainThreadTaskRunner());

  auto invalidator =
      std::make_unique<UserCloudPolicyInvalidator>(&profile, &policy_manager);

  // Shut down the invalidator immediately, simulating the browser closing
  // before the profile finishes initializing.
  invalidator->Shutdown();
}

}  // namespace policy
