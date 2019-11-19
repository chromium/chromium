// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_remote_powerwash_job.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_commands_factory_chromeos.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

constexpr base::TimeDelta kCommandAge = base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kVeryoldCommandAge =
    base::TimeDelta::FromDays(5 * 365 - 1);

class TestingRemoteCommandsService : public RemoteCommandsService {
 public:
  explicit TestingRemoteCommandsService(MockCloudPolicyClient* client)
      : RemoteCommandsService(std::make_unique<DeviceCommandsFactoryChromeOS>(
                                  nullptr /* policy_manager */),
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

std::unique_ptr<policy::RemoteCommandJob> CreateRemotePowerwashJob(
    base::TimeDelta age_of_command,
    RemoteCommandsService* service) {
  // Create the job proto.
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_DEVICE_REMOTE_POWERWASH);
  constexpr policy::RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());

  // Create the job and validate.
  auto job = std::make_unique<policy::DeviceCommandRemotePowerwashJob>(service);

  enterprise_management::SignedData signed_command;
  EXPECT_TRUE(
      job->Init(base::TimeTicks::Now(), command_proto, &signed_command));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(policy::RemoteCommandJob::NOT_STARTED, job->status());

  return job;
}

class DeviceCommandRemotePowerwashJobTest : public testing::Test {
 protected:
  DeviceCommandRemotePowerwashJobTest();
  ~DeviceCommandRemotePowerwashJobTest() override;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::RunLoop run_loop_;

  const std::unique_ptr<MockCloudPolicyClient> client_;
  const std::unique_ptr<TestingRemoteCommandsService> service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCommandRemotePowerwashJobTest);
};

DeviceCommandRemotePowerwashJobTest::DeviceCommandRemotePowerwashJobTest()
    : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::TestMockTimeTaskRunner::Type::kBoundToThread)),
      client_(std::make_unique<MockCloudPolicyClient>()),
      service_(std::make_unique<TestingRemoteCommandsService>(client_.get())) {
  chromeos::SessionManagerClient::InitializeFakeInMemory();
}

DeviceCommandRemotePowerwashJobTest::~DeviceCommandRemotePowerwashJobTest() {
  chromeos::SessionManagerClient::Shutdown();
}

// Make sure that the command is still valid 5*365-1 days after being issued.
TEST_F(DeviceCommandRemotePowerwashJobTest, TestCommandLifetime) {
  std::unique_ptr<policy::RemoteCommandJob> job =
      CreateRemotePowerwashJob(kVeryoldCommandAge, service_.get());

  EXPECT_TRUE(
      job->Run(base::Time::Now(), base::TimeTicks::Now(), base::OnceClosure()));
}

// Make sure that powerwash starts once the command gets ACK'd to the server.
TEST_F(DeviceCommandRemotePowerwashJobTest, TestCommandAckStartsPowerwash) {
  std::unique_ptr<policy::RemoteCommandJob> job =
      CreateRemotePowerwashJob(kCommandAge, service_.get());

  // No powerwash at this point.
  EXPECT_EQ(0, chromeos::FakeSessionManagerClient::Get()
                   ->start_device_wipe_call_count());

  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       run_loop_.QuitClosure()));
  // At this point the job is run, and the succeeded_callback is waiting to be
  // invoked.

  run_loop_.Run();
  // Now the succeeded_callback has been invoked, and normally it would cause an
  // ACK to be sent to the server, and upon receiving a response back from the
  // server the powerwash would start.

  base::RunLoop run_loop2;
  chromeos::FakeSessionManagerClient::Get()->set_on_start_device_wipe_callback(
      base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));

  // Simulate a response from the server by posting a task and waiting for
  // StartDeviceWipe to be called.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, service_->OnCommandAckedCallback());
  run_loop2.Run();

  // One powerwash coming up.
  EXPECT_EQ(1, chromeos::FakeSessionManagerClient::Get()
                   ->start_device_wipe_call_count());
}

// Make sure that the failsafe timer starts the powerwash in case of no ACK.
TEST_F(DeviceCommandRemotePowerwashJobTest, TestFailsafeTimerStartsPowerwash) {
  std::unique_ptr<policy::RemoteCommandJob> job =
      CreateRemotePowerwashJob(kCommandAge, service_.get());

  // No powerwash at this point.
  EXPECT_EQ(0, chromeos::FakeSessionManagerClient::Get()
                   ->start_device_wipe_call_count());

  // Run job + succeeded_callback.
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       run_loop_.QuitClosure()));
  run_loop_.Run();

  // After 500ms the timer is not run yet.
  task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(500));
  EXPECT_EQ(0, chromeos::FakeSessionManagerClient::Get()
                   ->start_device_wipe_call_count());

  // After 1s the timer is run.
  task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(500));
  EXPECT_EQ(1, chromeos::FakeSessionManagerClient::Get()
                   ->start_device_wipe_call_count());
}

}  // namespace policy
