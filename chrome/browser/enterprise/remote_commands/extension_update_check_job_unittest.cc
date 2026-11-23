// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/extension_update_check_job.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_commands {
namespace {

constexpr policy::RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

enterprise_management::RemoteCommand CreateCommandProto(
    base::TimeDelta age_of_command) {
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_BROWSER_EXTENSION_UPDATE_CHECK);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  command_proto.set_payload("{}");
  return command_proto;
}

class ExtensionUpdateCheckJobTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ExtensionUpdateCheckJobTest, NotExpiredBeforeSevenDays) {
  base::TimeTicks now = base::TimeTicks::Now();
  ExtensionUpdateCheckJob job(static_cast<Profile*>(nullptr));
  ASSERT_TRUE(job.Init(now, CreateCommandProto(base::Days(6)),
                       enterprise_management::SignedData{}));

  base::RunLoop run_loop;
  EXPECT_TRUE(job.Run(base::Time::Now(), now, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(job.status(), policy::RemoteCommandJob::FAILED);
}

TEST_F(ExtensionUpdateCheckJobTest, NotExpiredAtSevenDays) {
  base::TimeTicks now = base::TimeTicks::Now();
  ExtensionUpdateCheckJob job(static_cast<Profile*>(nullptr));
  ASSERT_TRUE(job.Init(now, CreateCommandProto(base::Days(7)),
                       enterprise_management::SignedData{}));

  base::RunLoop run_loop;
  EXPECT_TRUE(job.Run(base::Time::Now(), now, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(job.status(), policy::RemoteCommandJob::FAILED);
}

TEST_F(ExtensionUpdateCheckJobTest, ExpiredAfterSevenDays) {
  base::TimeTicks now = base::TimeTicks::Now();
  ExtensionUpdateCheckJob job(static_cast<Profile*>(nullptr));
  ASSERT_TRUE(
      job.Init(now, CreateCommandProto(base::Days(7) + base::Milliseconds(1)),
               enterprise_management::SignedData{}));

  base::RunLoop run_loop;
  EXPECT_FALSE(job.Run(base::Time::Now(), now, run_loop.QuitClosure()));
  EXPECT_EQ(job.status(), policy::RemoteCommandJob::EXPIRED);
}

}  // namespace
}  // namespace enterprise_commands
