// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_get_available_routines_job.h"

#include <memory>
#include <vector>

#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

namespace {

// String constant identifying the routines field in the result payload.
const char* const kRoutinesFieldName = "routines";

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 987123;

em::RemoteCommand GenerateCommandProto(RemoteCommandJob::UniqueIDType unique_id,
                                       base::TimeDelta age_of_command,
                                       base::TimeDelta idleness_cutoff,
                                       bool terminate_upon_input) {
  em::RemoteCommand command_proto;
  command_proto.set_type(
      em::RemoteCommand_Type_DEVICE_GET_AVAILABLE_DIAGNOSTIC_ROUTINES);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  return command_proto;
}

}  // namespace

class DeviceCommandGetAvailableRoutinesJobTest : public testing::Test {
 protected:
  DeviceCommandGetAvailableRoutinesJobTest();
  DeviceCommandGetAvailableRoutinesJobTest(
      const DeviceCommandGetAvailableRoutinesJobTest&) = delete;
  DeviceCommandGetAvailableRoutinesJobTest& operator=(
      const DeviceCommandGetAvailableRoutinesJobTest&) = delete;
  ~DeviceCommandGetAvailableRoutinesJobTest() override;

  void InitializeJob(RemoteCommandJob* job,
                     RemoteCommandJob::UniqueIDType unique_id,
                     base::TimeTicks issued_time,
                     base::TimeDelta idleness_cutoff,
                     bool terminate_upon_input);

  std::string CreateSuccessPayload(
      const std::vector<ash::cros_healthd::mojom::DiagnosticRoutineEnum>&
          available_routines);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;

  base::TimeTicks test_start_time_;
};

DeviceCommandGetAvailableRoutinesJobTest::
    DeviceCommandGetAvailableRoutinesJobTest() {
  ash::cros_healthd::FakeCrosHealthd::Initialize();
  test_start_time_ = base::TimeTicks::Now();
}

DeviceCommandGetAvailableRoutinesJobTest::
    ~DeviceCommandGetAvailableRoutinesJobTest() {
  ash::cros_healthd::FakeCrosHealthd::Shutdown();
}

void DeviceCommandGetAvailableRoutinesJobTest::InitializeJob(
    RemoteCommandJob* job,
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeTicks issued_time,
    base::TimeDelta idleness_cutoff,
    bool terminate_upon_input) {
  EXPECT_TRUE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(unique_id, base::TimeTicks::Now() - issued_time,
                           idleness_cutoff, terminate_upon_input),
      em::SignedData()));

  EXPECT_EQ(unique_id, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
}

std::string DeviceCommandGetAvailableRoutinesJobTest::CreateSuccessPayload(
    const std::vector<ash::cros_healthd::mojom::DiagnosticRoutineEnum>&
        available_routines) {
  std::string payload;

  base::Value::List routine_list;
  for (const auto& routine : available_routines) {
    routine_list.Append(static_cast<int>(routine));
  }
  auto root_dict =
      base::Value::Dict().Set(kRoutinesFieldName, std::move(routine_list));
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

TEST_F(DeviceCommandGetAvailableRoutinesJobTest, Success) {
  const std::vector<ash::cros_healthd::mojom::DiagnosticRoutineEnum>
      kAvailableRoutines = {
          ash::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom,
          ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity};
  ash::cros_healthd::FakeCrosHealthd::Get()->SetAvailableRoutinesForTesting(
      kAvailableRoutines);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetAvailableRoutinesJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_, base::Seconds(30),
                /*terminate_upon_input=*/false);
  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  EXPECT_TRUE(payload);
  EXPECT_EQ(CreateSuccessPayload(kAvailableRoutines), *payload);
}

TEST_F(DeviceCommandGetAvailableRoutinesJobTest, Failure) {
  ash::cros_healthd::FakeCrosHealthd::Get()->SetAvailableRoutinesForTesting({});
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetAvailableRoutinesJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_, base::Seconds(30),
                /*terminate_upon_input=*/false);
  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  EXPECT_FALSE(payload);
}

}  // namespace policy
