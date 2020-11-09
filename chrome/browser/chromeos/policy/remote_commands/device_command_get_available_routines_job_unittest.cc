// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_get_available_routines_job.h"

#include <memory>
#include <vector>

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
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
      const std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>&
          available_routines);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::TimeTicks test_start_time_;
};

DeviceCommandGetAvailableRoutinesJobTest::
    DeviceCommandGetAvailableRoutinesJobTest() {
  chromeos::CrosHealthdClient::InitializeFake();
  test_start_time_ = base::TimeTicks::Now();
}

DeviceCommandGetAvailableRoutinesJobTest::
    ~DeviceCommandGetAvailableRoutinesJobTest() {
  chromeos::CrosHealthdClient::Shutdown();

  // Wait for ServiceConnection to observe the destruction of the client.
  chromeos::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
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
      nullptr));

  EXPECT_EQ(unique_id, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
}

std::string DeviceCommandGetAvailableRoutinesJobTest::CreateSuccessPayload(
    const std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>&
        available_routines) {
  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  base::Value routine_list(base::Value::Type::LIST);
  for (const auto& routine : available_routines)
    routine_list.Append(static_cast<int>(routine));
  root_dict.SetPath(kRoutinesFieldName, std::move(routine_list));
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

TEST_F(DeviceCommandGetAvailableRoutinesJobTest, Success) {
  const std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>
      kAvailableRoutines = {
          chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom,
          chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
              kBatteryCapacity};
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetAvailableRoutinesForTesting(kAvailableRoutines);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetAvailableRoutinesJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false);
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kAvailableRoutines), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandGetAvailableRoutinesJobTest, Failure) {
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetAvailableRoutinesForTesting({});
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetAvailableRoutinesJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false);
  base::RunLoop run_loop;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          base::BindLambdaForTesting([&]() {
                            EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                            std::unique_ptr<std::string> payload =
                                job->GetResultPayload();
                            EXPECT_FALSE(payload);
                            run_loop.Quit();
                          }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

}  // namespace policy
