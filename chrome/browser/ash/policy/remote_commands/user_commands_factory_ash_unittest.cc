// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/user_commands_factory_ash.h"

#include <memory>

#include "chrome/browser/enterprise/remote_commands/extension_update_check_job.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

class UserCommandsFactoryAshTest : public testing::Test {
 protected:
  UserCommandsFactoryAshTest() = default;
  ~UserCommandsFactoryAshTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(UserCommandsFactoryAshTest, BuildUserArcCommandJob) {
  UserCommandsFactoryAsh factory(&profile_);
  std::unique_ptr<RemoteCommandJob> job = factory.BuildJobForType(
      em::RemoteCommand_Type_USER_ARC_COMMAND, /*service=*/nullptr);
  ASSERT_TRUE(job);
  EXPECT_EQ(em::RemoteCommand_Type_USER_ARC_COMMAND, job->GetType());
}

TEST_F(UserCommandsFactoryAshTest, BuildExtensionUpdateCheckJob) {
  UserCommandsFactoryAsh factory(&profile_);
  std::unique_ptr<RemoteCommandJob> job = factory.BuildJobForType(
      em::RemoteCommand_Type_BROWSER_EXTENSION_UPDATE_CHECK,
      /*service=*/nullptr);
  ASSERT_TRUE(job);
  EXPECT_EQ(em::RemoteCommand_Type_BROWSER_EXTENSION_UPDATE_CHECK,
            job->GetType());
}

}  // namespace policy
