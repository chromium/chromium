// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_wipe_users_job.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/system/user_removal_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

constexpr base::TimeDelta kCommandAge = base::Minutes(10);
constexpr base::TimeDelta kVeryoldCommandAge = base::Days(175);

class TestingRemoteCommandsService : public RemoteCommandsService {
 public:
  explicit TestingRemoteCommandsService(MockCloudPolicyClient* client)
      : RemoteCommandsService(
            /*factory=*/nullptr,
            client,
            /*store=*/nullptr,
            PolicyInvalidationScope::kDevice) {}

  TestingRemoteCommandsService(const TestingRemoteCommandsService&) = delete;
  TestingRemoteCommandsService& operator=(const TestingRemoteCommandsService&) =
      delete;

  // RemoteCommandsService:
  void SetOnCommandAckedCallback(base::OnceClosure callback) override {
    on_command_acked_callback_ = std::move(callback);
  }

  base::OnceClosure OnCommandAckedCallback() {
    return std::move(on_command_acked_callback_);
  }

 protected:
  base::OnceClosure on_command_acked_callback_;
};

std::unique_ptr<RemoteCommandJob> CreateWipeUsersJob(
    base::TimeDelta age_of_command,
    RemoteCommandsService* service) {
  // Create the job proto.
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_DEVICE_WIPE_USERS);
  constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());

  // Create the job and validate.
  auto job = std::make_unique<DeviceCommandWipeUsersJob>(service);

  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto,
                        enterprise_management::SignedData()));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());

  return job;
}

class DeviceCommandWipeUsersJobTest : public testing::Test {
 public:
  DeviceCommandWipeUsersJobTest(const DeviceCommandWipeUsersJobTest&) = delete;
  DeviceCommandWipeUsersJobTest& operator=(
      const DeviceCommandWipeUsersJobTest&) = delete;

 protected:
  DeviceCommandWipeUsersJobTest();
  ~DeviceCommandWipeUsersJobTest() override;

  content::BrowserTaskEnvironment task_environment_;

  ScopedTestingLocalState local_state_;
  const std::unique_ptr<MockCloudPolicyClient> client_;
  const std::unique_ptr<TestingRemoteCommandsService> service_;
};

DeviceCommandWipeUsersJobTest::DeviceCommandWipeUsersJobTest()
    : local_state_(TestingBrowserProcess::GetGlobal()),
      client_(std::make_unique<MockCloudPolicyClient>()),
      service_(std::make_unique<TestingRemoteCommandsService>(client_.get())) {}

DeviceCommandWipeUsersJobTest::~DeviceCommandWipeUsersJobTest() {}

// Make sure that the command is still valid 175 days after being issued.
TEST_F(DeviceCommandWipeUsersJobTest, TestCommandLifetime) {
  std::unique_ptr<RemoteCommandJob> job =
      CreateWipeUsersJob(kVeryoldCommandAge, service_.get());

  EXPECT_TRUE(
      job->Run(base::Time::Now(), base::TimeTicks::Now(), base::OnceClosure()));
}

// Make sure that the command's succeeded_callback is being invoked.
TEST_F(DeviceCommandWipeUsersJobTest, TestCommandSucceededCallback) {
  std::unique_ptr<RemoteCommandJob> job =
      CreateWipeUsersJob(kCommandAge, service_.get());

  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  // This call processes the CommitPendingWrite which persists the pref to disk,
  // and runs the passed callback which is the succeeded_callback.
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  EXPECT_EQ(RemoteCommandJob::SUCCEEDED, job->status());
}

// Make sure that LogOut is being called after the commands gets ACK'd to the
// server.
TEST_F(DeviceCommandWipeUsersJobTest, TestLogOutCalled) {
  std::unique_ptr<RemoteCommandJob> job =
      CreateWipeUsersJob(kCommandAge, service_.get());

  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  // At this point the job is run, and the succeeded_callback is waiting to be
  // invoked.

  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  // Now the succeeded_callback has been invoked, and normally it would cause an
  // ACK to be sent to the server, and upon receiving a response back from the
  // server LogOut would get called.

  // Simulate a response from the server by posting a task and waiting for
  // LogOut to be called.
  base::test::TestFuture<void> log_out_callback_future;
  ash::user_removal_manager::OverrideLogOutForTesting(
      log_out_callback_future.GetCallback());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, service_->OnCommandAckedCallback());
  ASSERT_TRUE(log_out_callback_future.Wait())
      << "Log out callback was not invoked.";
}

}  // namespace policy
