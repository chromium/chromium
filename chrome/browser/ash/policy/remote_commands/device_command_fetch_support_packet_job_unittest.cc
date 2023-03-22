// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

namespace {

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456;

constexpr char kCommandPayload[] =
    R"({ "supportPacketDetails":{
        "issueCaseId": "issue_case_id",
        "issueDescription": "issue description",
        "requestedDataCollectors": [17],
        "requestedPiiTypes": [1],
        "requesterMetadata": "obfuscated123"
      }
    })";

em::RemoteCommand GenerateCommandProto(RemoteCommandJob::UniqueIDType unique_id,
                                       base::TimeTicks command_start_time,
                                       std::string payload) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_FETCH_SUPPORT_PACKET);
  command_proto.set_command_id(unique_id);
  base::TimeDelta age_of_command = base::TimeTicks::Now() - command_start_time;
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  command_proto.set_payload(payload);
  return command_proto;
}

}  // namespace

class DeviceCommandFetchSupportPacketTest : public ::testing::Test {
 public:
  DeviceCommandFetchSupportPacketTest() {
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());
  }

  DeviceCommandFetchSupportPacketTest(
      const DeviceCommandFetchSupportPacketTest&) = delete;
  DeviceCommandFetchSupportPacketTest& operator=(
      const DeviceCommandFetchSupportPacketTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ash::DebugDaemonClient::InitializeFake();
    ash::LoginState::Initialize();
    // Set serial number for testing.
    statistics_provider_.SetMachineStatistic("serial_number", "000000");
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);
    test_start_time_ = base::TimeTicks::Now();
  }

  void TearDown() override {
    ash::DebugDaemonClient::Shutdown();
    if (!temp_dir_.IsValid()) {
      return;
    }
    EXPECT_TRUE(temp_dir_.Delete());
    ash::LoginState::Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  base::TimeTicks test_start_time_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(DeviceCommandFetchSupportPacketTest, Success) {
  // Set LoginState as kiosk device.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK);

  std::unique_ptr<DeviceCommandFetchSupportPacketJob> job =
      std::make_unique<DeviceCommandFetchSupportPacketJob>();

  job->SetTargetDirForTesting(temp_dir_.GetPath());

  EXPECT_TRUE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(kUniqueID, test_start_time_, kCommandPayload),
      em::SignedData()));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());

  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);

  int64_t file_size;
  ASSERT_TRUE(
      base::GetFileSize(job->GetExportedFilepathForTesting(), &file_size));
  EXPECT_GT(file_size, 0);
}

TEST_F(DeviceCommandFetchSupportPacketTest, FailWithWrongPayload) {
  std::unique_ptr<DeviceCommandFetchSupportPacketJob> job =
      std::make_unique<DeviceCommandFetchSupportPacketJob>();
  // Wrong payload with empty data collectors list.
  constexpr char kWrongPayload[] =
      R"({ "supportPacketDetails":{
        "issueCaseId": "issue_case_id",
        "issueDescription": "issue description",
        "requestedDataCollectors": [],
        "requestedPiiTypes": [1],
        "requesterMetadata": "obfuscated123"
      }
    })";

  // Shouldn't be able to initialize with wrong payload.
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(kUniqueID, test_start_time_, kWrongPayload),
      em::SignedData()));
}

TEST_F(DeviceCommandFetchSupportPacketTest, FailForNonKioskDevice) {
  // Set LoginState as non-kiosk device.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);

  std::unique_ptr<DeviceCommandFetchSupportPacketJob> job =
      std::make_unique<DeviceCommandFetchSupportPacketJob>();

  job->SetTargetDirForTesting(temp_dir_.GetPath());

  EXPECT_TRUE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(kUniqueID, test_start_time_, kCommandPayload),
      em::SignedData()));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());

  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  // Expect the job to fail for non-kiosk device.
  EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
  EXPECT_EQ(*job->GetResultPayload(), kCommandNotEnabledForUserMessage);
}

}  // namespace policy
