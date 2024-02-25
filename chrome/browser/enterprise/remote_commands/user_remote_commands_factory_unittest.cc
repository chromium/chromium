// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/user_remote_commands_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_commands {

using UserRemoteCommandsFactoryTest = ::testing::Test;

TEST_F(UserRemoteCommandsFactoryTest, CreateClearDataJob) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  UserRemoteCommandsFactory factory{&profile};
  auto job = factory.BuildJobForType(
      enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA,
      /*service=*/nullptr);
  EXPECT_TRUE(job.get());
}

}  // namespace enterprise_commands
