// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_wipe_users_job.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_commands_factory_chromeos.h"
#include "chrome/browser/chromeos/system/user_removal_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

constexpr base::TimeDelta kCommandAge = base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kVeryoldCommandAge = base::TimeDelta::FromDays(175);

class TestingRemoteCommandsService : public RemoteCommandsService {
 public:
  explicit TestingRemoteCommandsService(MockCloudPolicyClient* client)
      : RemoteCommandsService(
            std::make_unique<DeviceCommandsFactoryChromeOS>(nullptr),
            client,
            nullptr /* store */) {}
  // RemoteCommandsService:
  void SetOnCommandAckedCallback(base::OnceClosure callback) override {
    on_command_acked_callback_ = std::move(callback);
  }

  base::OnceClosure OnCommandAckedCallback() {
    return std::move(on_command_acked_callback_);
  }

 protected:
  base::OnceClosure on_command_acked_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingRemoteCommandsService);
};

std::unique_ptr<policy::RemoteCommandJob> CreateWipeUsersJob(
    base::TimeDelta age_of_command,
    RemoteCommandsService* service) {
  // Create the job proto.
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_DEVICE_WIPE_USERS);
  constexpr policy::RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());

  // Create the job and validate.
  auto job = std::make_unique<policy::DeviceCommandWipeUsersJob>(service);

  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto, nullptr));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(policy::RemoteCommandJob::NOT_STARTED, job->status());

  return job;
}

class DeviceCommandWipeUsersJobTest : public testing::Test {
 protected:
  DeviceCommandWipeUsersJobTest();
  ~DeviceCommandWipeUsersJobTest() override;

  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;

  ScopedTestingLocalState local_state_;
  const std::unique_ptr<MockCloudPolicyClient> client_;
  const std::unique_ptr<TestingRemoteCommandsService> service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCommandWipeUsersJobTest);
};

DeviceCommandWipeUsersJobTest::DeviceCommandWipeUsersJobTest()
    : local_state_(TestingBrowserProcess::GetGlobal()),
      client_(std::make_unique<MockCloudPolicyClient>()),
      service_(std::make_unique<TestingRemoteCommandsService>(client_.get())) {}

DeviceCommandWipeUsersJobTest::~DeviceCommandWipeUsersJobTest() {}

// Make sure that the command is still valid 175 days after being issued.
TEST_F(DeviceCommandWipeUsersJobTest, TestCommandLifetime) {
  std::unique_ptr<policy::RemoteCommandJob> job =
      CreateWipeUsersJob(kVeryoldCommandAge, service_.get());

  EXPECT_TRUE(
      job->Run(base::Time::Now(), base::TimeTicks::Now(), base::Closure()));
}

// Make sure that the command's succeeded_callback is being invoked.
TEST_F(DeviceCommandWipeUsersJobTest, TestCommandSucceededCallback) {
  std::unique_ptr<policy::RemoteCommandJob> job =
      CreateWipeUsersJob(kCommandAge, service_.get());

  auto check_result_callback = base::BindOnce(
      [](base::RunLoop* run_loop, policy::RemoteCommandJob* job) {
        EXPECT_EQ(policy::RemoteCommandJob::SUCCEEDED, job->status());
        run_loop->Quit();
      },
      &run_loop_, job.get());
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       std::move(check_result_callback)));
  // This call processes the CommitPendingWrite which persists the pref to disk,
  // and runs the passed callback which is the succeeded_callback.
  run_loop_.Run();
}

// Make sure that LogOut is being called after the commands gets ACK'd to the
// server.
TEST_F(DeviceCommandWipeUsersJobTest, TestLogOutCalled) {
  std::unique_ptr<policy::RemoteCommandJob> job =
      CreateWipeUsersJob(kCommandAge, service_.get());

  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       run_loop_.QuitClosure()));
  // At this point the job is run, and the succeeded_callback is waiting to be
  // invoked.

  run_loop_.Run();
  // Now the succeeded_callback has been invoked, and normally it would cause an
  // ACK to be sent to the server, and upon receiving a response back from the
  // server LogOut would get called.

  // Simulate a response from the server by posting a task and waiting for
  // LogOut to be called.
  bool log_out_called = false;
  base::RunLoop run_loop2;
  auto log_out_callback = base::BindOnce(
      [](base::RunLoop* run_loop, bool* log_out_called) {
        *log_out_called = true;
        run_loop->Quit();
      },
      &run_loop2, &log_out_called);
  chromeos::user_removal_manager::OverrideLogOutForTesting(
      std::move(log_out_callback));

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, service_->OnCommandAckedCallback());
  run_loop2.Run();
  EXPECT_TRUE(log_out_called);
}

}  // namespace policy
