// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_run_routine_job.h"

#include <limits>
#include <memory>

#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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

// String constant identifying the id field in the result payload.
constexpr char kIdFieldName[] = "id";
// String constant identifying the status field in the result payload.
constexpr char kStatusFieldName[] = "status";

// String constant identifying the routine enum field in the command payload.
constexpr char kRoutineEnumFieldName[] = "routineEnum";
// String constant identifying the parameter dictionary field in the command
// payload.
constexpr char kParamsFieldName[] = "params";

// String constants identifying the parameter fields for the routine.
constexpr char kLengthSecondsFieldName[] = "lengthSeconds";

// String constants identifying the parameter fields for the AC power routine.
constexpr char kExpectedStatusFieldName[] = "expectedStatus";
constexpr char kExpectedPowerTypeFieldName[] = "expectedPowerType";

// String constants identifying the parameter fields for the NVMe wear level
// routine.
constexpr char kWearLevelThresholdFieldName[] = "wearLevelThreshold";

// String constants identifying the parameter fields for the NVMe self-test
// routine.
constexpr char kNvmeSelfTestTypeFieldName[] = "nvmeSelfTestType";

// String constants identifying the parameter fields for the disk read routine
constexpr char kTypeFieldName[] = "type";
constexpr char kFileSizeMbFieldName[] = "fileSizeMb";

// String constants identifying the parameter field for the battery discharge
// routine.
constexpr char kMaximumDischargePercentAllowedFieldName[] =
    "maximumDischargePercentAllowed";

// String constants identifying the parameter field for the battery charge
// routine.
constexpr char kMinimumChargePercentRequiredFieldName[] =
    "minimumChargePercentRequired";

constexpr uint32_t kId = 11;
constexpr auto kStatus =
    chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRunning;

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 987123;

constexpr int kPositiveInt = 8789;
constexpr int kNegativeInt = -231;
constexpr auto kValidAcPowerStatusEnum =
    chromeos::cros_healthd::mojom::AcPowerStatusEnum::kConnected;
constexpr char kValidExpectedAcPowerType[] = "power_type";
constexpr auto kValidDiskReadRoutineTypeEnum =
    chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead;
constexpr char kValidStunServerHostname[] = "www.stun_server_name";

em::RemoteCommand GenerateCommandProto(
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeDelta age_of_command,
    base::TimeDelta idleness_cutoff,
    bool terminate_upon_input,
    base::Optional<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>
        routine,
    base::Optional<base::Value> params) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_RUN_DIAGNOSTIC_ROUTINE);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  base::Value root_dict(base::Value::Type::DICTIONARY);
  if (routine.has_value()) {
    root_dict.SetIntKey(kRoutineEnumFieldName,
                        static_cast<int>(routine.value()));
  }
  if (params.has_value())
    root_dict.SetKey(kParamsFieldName, std::move(params).value());
  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

std::string CreateSuccessPayload(
    uint32_t id,
    chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum status) {
  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  root_dict.SetIntKey(kIdFieldName, static_cast<int>(id));
  root_dict.SetIntKey(kStatusFieldName, static_cast<int>(status));
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

std::string CreateInvalidParametersFailurePayload() {
  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  root_dict.SetIntKey(
      kIdFieldName,
      static_cast<int>(chromeos::cros_healthd::mojom::kFailedToStartId));
  root_dict.SetIntKey(
      kStatusFieldName,
      static_cast<int>(chromeos::cros_healthd::mojom::
                           DiagnosticRoutineStatusEnum::kFailedToStart));
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

}  // namespace

class DeviceCommandRunRoutineJobTest : public testing::Test {
 protected:
  DeviceCommandRunRoutineJobTest();
  DeviceCommandRunRoutineJobTest(const DeviceCommandRunRoutineJobTest&) =
      delete;
  DeviceCommandRunRoutineJobTest& operator=(
      const DeviceCommandRunRoutineJobTest&) = delete;
  ~DeviceCommandRunRoutineJobTest() override;

  void InitializeJob(
      RemoteCommandJob* job,
      RemoteCommandJob::UniqueIDType unique_id,
      base::TimeTicks issued_time,
      base::TimeDelta idleness_cutoff,
      bool terminate_upon_input,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum routine,
      base::Value params);

  bool RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum routine,
              base::Value params_dict,
              base::RepeatingCallback<void(RemoteCommandJob*)> callback);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::TimeTicks test_start_time_;
};

DeviceCommandRunRoutineJobTest::DeviceCommandRunRoutineJobTest() {
  chromeos::CrosHealthdClient::InitializeFake();
  test_start_time_ = base::TimeTicks::Now();
}

DeviceCommandRunRoutineJobTest::~DeviceCommandRunRoutineJobTest() {
  chromeos::CrosHealthdClient::Shutdown();

  // Wait for ServiceConnection to observe the destruction of the client.
  chromeos::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
}

void DeviceCommandRunRoutineJobTest::InitializeJob(
    RemoteCommandJob* job,
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeTicks issued_time,
    base::TimeDelta idleness_cutoff,
    bool terminate_upon_input,
    chromeos::cros_healthd::mojom::DiagnosticRoutineEnum routine,
    base::Value params) {
  EXPECT_TRUE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(unique_id, base::TimeTicks::Now() - issued_time,
                           idleness_cutoff, terminate_upon_input, routine,
                           std::move(params)),
      nullptr));

  EXPECT_EQ(unique_id, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
}

bool DeviceCommandRunRoutineJobTest::RunJob(
    chromeos::cros_healthd::mojom::DiagnosticRoutineEnum routine,
    base::Value params_dict,
    base::RepeatingCallback<void(RemoteCommandJob*)> callback) {
  auto job = std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false, routine, std::move(params_dict));
  base::RunLoop run_loop;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          base::BindLambdaForTesting([&]() {
                            std::move(callback).Run(job.get());
                            run_loop.Quit();
                          }));
  run_loop.Run();
  return success;
}

TEST_F(DeviceCommandRunRoutineJobTest, InvalidRoutineEnumInCommandPayload) {
  constexpr auto kInvalidRoutineEnum = static_cast<
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>(
      std::numeric_limits<std::underlying_type<
          chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>::type>::max());
  auto job = std::make_unique<DeviceCommandRunRoutineJob>();
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(kUniqueID, base::TimeTicks::Now() - test_start_time_,
                           base::TimeDelta::FromSeconds(30),
                           /*terminate_upon_input=*/false, kInvalidRoutineEnum,
                           std::move(params_dict)),
      nullptr));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

// Test that not specifying a routine causes the job initialization to fail.
TEST_F(DeviceCommandRunRoutineJobTest, CommandPayloadMissingRoutine) {
  auto job = std::make_unique<DeviceCommandRunRoutineJob>();
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(kUniqueID, base::TimeTicks::Now() - test_start_time_,
                           base::TimeDelta::FromSeconds(30),
                           /*terminate_upon_input=*/false,
                           /*routine=*/base::nullopt, std::move(params_dict)),
      nullptr));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

// Test that not including a parameters dictionary causes the routine
// initialization to fail.
TEST_F(DeviceCommandRunRoutineJobTest, CommandPayloadMissingParamDict) {
  constexpr auto kValidRoutineEnum =
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck;
  auto job = std::make_unique<DeviceCommandRunRoutineJob>();
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(kUniqueID, base::TimeTicks::Now() - test_start_time_,
                           base::TimeDelta::FromSeconds(30),
                           /*terminate_upon_input=*/false, kValidRoutineEnum,
                           /*params=*/base::nullopt),
      nullptr));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

// Note that the battery capacity routine has no parameters, so it's enough to
// ensure the routine can be run.
TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryCapacityRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the battery health routine has no parameters, so it's enough to
// ensure the routine can be run.
TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryHealthRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

TEST_F(DeviceCommandRunRoutineJobTest, RunUrandomRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that the urandom routine handles the optional length_seconds parameter
// being missing.
TEST_F(DeviceCommandRunRoutineJobTest, RunUrandomRoutineMissingLengthSeconds) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that a negative lengthSeconds parameter causes the urandom routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunUrandomRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kNegativeInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

// Note that the smartctl check routine has no parameters, so we only need to
// test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunSmartctlCheckRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Test that the AC power routine succeeds with all parameters specified.
TEST_F(DeviceCommandRunRoutineJobTest, RunAcPowerRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kExpectedStatusFieldName,
                        static_cast<int>(kValidAcPowerStatusEnum));
  params_dict.SetStringKey(kExpectedPowerTypeFieldName,
                           kValidExpectedAcPowerType);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that the AC power routine succeeds without the optional parameter
// expectedPowerType specified.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunAcPowerRoutineNoOptionalExpectedPowerType) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kExpectedStatusFieldName,
                        static_cast<int>(kValidAcPowerStatusEnum));
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that leaving out the expectedStatus parameter causes the AC power
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunAcPowerRoutineMissingExpectedStatus) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetStringKey(kExpectedPowerTypeFieldName,
                           kValidExpectedAcPowerType);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

// Test that an invalid value for the expectedStatus parameter causes the AC
// power routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunAcPowerRoutineInvalidExpectedStatus) {
  constexpr auto kInvalidAcPowerStatusEnum =
      static_cast<chromeos::cros_healthd::mojom::AcPowerStatusEnum>(
          std::numeric_limits<std::underlying_type<
              chromeos::cros_healthd::mojom::AcPowerStatusEnum>::type>::max());
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kExpectedStatusFieldName,
                        static_cast<int>(kInvalidAcPowerStatusEnum));
  params_dict.SetStringKey(kExpectedPowerTypeFieldName,
                           kValidExpectedAcPowerType);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

TEST_F(DeviceCommandRunRoutineJobTest, RunCpuCacheRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that the CPU cache routine handles the optional length_seconds parameter
// being missing.
TEST_F(DeviceCommandRunRoutineJobTest, RunCpuCacheRoutineMissingLengthSeconds) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that a negative lengthSeconds parameter causes the CPU cache routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunCpuCacheRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kNegativeInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

TEST_F(DeviceCommandRunRoutineJobTest, RunCpuStressRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that the CPU stress routine handles the optional length_seconds
// parameter being missing.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunCpuStressRoutineMissingLengthSeconds) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that a negative lengthSeconds parameter causes the CPU stress routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunCpuStressRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kNegativeInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

TEST_F(DeviceCommandRunRoutineJobTest, RunFloatingPointAccuracyRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
                         kFloatingPointAccuracy,
                     std::move(params_dict),
                     base::BindLambdaForTesting([](RemoteCommandJob* job) {
                       EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                       std::unique_ptr<std::string> payload =
                           job->GetResultPayload();
                       EXPECT_TRUE(payload);
                       EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                     })));
}

// Test that the floating point accuracy routine handles the optional
// length_seconds parameter being missing.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunFloatingPointAccuracyRoutineMissingLengthSeconds) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
                         kFloatingPointAccuracy,
                     std::move(params_dict),
                     base::BindLambdaForTesting([](RemoteCommandJob* job) {
                       EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                       std::unique_ptr<std::string> payload =
                           job->GetResultPayload();
                       EXPECT_TRUE(payload);
                       EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                     })));
}

// Test that a negative lengthSeconds parameter causes the floating point
// accuracy routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunFloatingPointAccuracyRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kNegativeInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
                 kFloatingPointAccuracy,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

TEST_F(DeviceCommandRunRoutineJobTest, RunNvmeWearLevelRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kWearLevelThresholdFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Test that leaving out the wearLevelThreshold parameter causes the NVMe wear
// level routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunNvmeWearLevelRoutineMissingWearLevelThreshold) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

// Test that a negative wearLevelThreshold parameter causes the NVMe wear level
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunNvmeWearLevelRoutineInvalidWearLevelThreshold) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kWearLevelThresholdFieldName, kNegativeInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

TEST_F(DeviceCommandRunRoutineJobTest, RunNvmeSelfTestRoutineSuccess) {
  constexpr auto kValidNvmeSelfTestTypeEnum =
      chromeos::cros_healthd::mojom::NvmeSelfTestTypeEnum::kShortSelfTest;
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kNvmeSelfTestTypeFieldName,
                        static_cast<int>(kValidNvmeSelfTestTypeEnum));
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Test that leaving out the nvmeSelfTestType parameter causes the NVMe self
// test routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunNvmeSelfTestRoutineMissingSelfTestType) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

// Test that an invalid value for the nvmeSelfTestType parameter causes the NVMe
// self test routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunNvmeSelfTestRoutineInvalidSelfTestType) {
  constexpr auto kInvalidNvmeSelfTestTypeEnum = static_cast<
      chromeos::cros_healthd::mojom::NvmeSelfTestTypeEnum>(
      std::numeric_limits<std::underlying_type<
          chromeos::cros_healthd::mojom::NvmeSelfTestTypeEnum>::type>::max());
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kNvmeSelfTestTypeFieldName,
                        static_cast<int>(kInvalidNvmeSelfTestTypeEnum));
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

// Test that the disk read routine succeeds with all parameters specified.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kTypeFieldName,
                        static_cast<int>(kValidDiskReadRoutineTypeEnum));
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  params_dict.SetIntKey(kFileSizeMbFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that leaving out the type parameter causes the disk read routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineMissingType) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  params_dict.SetIntKey(kFileSizeMbFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

// Test that leaving out the lengthSeconds parameter causes the disk read
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineMissingLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kTypeFieldName,
                        static_cast<int>(kValidDiskReadRoutineTypeEnum));
  params_dict.SetIntKey(kFileSizeMbFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

// Test that leaving out the fileSizeMb parameter causes the disk read routine
// to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineMissingFileSizeMb) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kTypeFieldName,
                        static_cast<int>(kValidDiskReadRoutineTypeEnum));
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

// Test that an invalid value for the type parameter causes the disk read
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineInvalidType) {
  constexpr auto kInvalidDiskReadRoutineTypeEnum =
      static_cast<chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum>(
          std::numeric_limits<std::underlying_type<
              chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum>::type>::
              max());
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kTypeFieldName,
                        static_cast<int>(kInvalidDiskReadRoutineTypeEnum));
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  params_dict.SetIntKey(kFileSizeMbFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

// Test that an invalid value for the lengthSeconds parameter causes the disk
// read routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kTypeFieldName,
                        static_cast<int>(kValidDiskReadRoutineTypeEnum));
  params_dict.SetIntKey(kLengthSecondsFieldName, kNegativeInt);
  params_dict.SetIntKey(kFileSizeMbFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

// Test that an invalid value for the fileSizeMb parameter causes the disk read
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineInvalidFileSizeMb) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kTypeFieldName,
                        static_cast<int>(kValidDiskReadRoutineTypeEnum));
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  params_dict.SetIntKey(kFileSizeMbFieldName, kNegativeInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

// Test that the prime search routine succeeds with all parameters specified.
TEST_F(DeviceCommandRunRoutineJobTest, RunPrimeSearchRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that the prime search routine handles the optional length_seconds
// parameter being missing.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunPrimeSearchRoutineMissingLengthSeconds) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Test that an invalid value for the lengthSeconds parameter causes the prime
// search routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunPrimeSearchRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kNegativeInt);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
             })));
}

TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryDischargeRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  params_dict.SetIntKey(kMaximumDischargePercentAllowedFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryDischargeRoutineMissingLengthSeconds) {
  // Test that leaving out the lengthSeconds parameter causes the routine to
  // fail.
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kMaximumDischargePercentAllowedFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryDischargeRoutineMissingMaximumDischargePercentAllowed) {
  // Test that leaving out the maximumDischargePercentAllowed parameter causes
  // the routine to fail.
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryDischargeRoutineInvalidLengthSeconds) {
  // Test that a negative lengthSeconds parameter causes the routine to fail.
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kNegativeInt);
  params_dict.SetIntKey(kMaximumDischargePercentAllowedFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryDischargeRoutineInvalidMaximumDischargePercentAllowed) {
  // Test that a negative maximumDischargePercentAllowed parameter causes the
  // routine to fail.
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  params_dict.SetIntKey(kMaximumDischargePercentAllowedFieldName, kNegativeInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

// Test that the battery charge routine can be run.
TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryChargeRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  params_dict.SetIntKey(kMinimumChargePercentRequiredFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        ASSERT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Test that leaving out the lengthSeconds parameter causes the battery charge
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryChargeRoutineMissingLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kMinimumChargePercentRequiredFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

// Test that leaving out the minimumChargePercentRequired parameter causes the
// battery charge routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryChargeRoutineMissingMinimumChargePercentRequired) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

// Test that a negative lengthSeconds parameter causes the battery charge
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryChargeRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kNegativeInt);
  params_dict.SetIntKey(kMinimumChargePercentRequiredFieldName, kPositiveInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

// Test that a negative minimumChargePercentRequired parameter causes the
// battery charge routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryChargeRoutineInvalidMinimumChargePercentRequired) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, kPositiveInt);
  params_dict.SetIntKey(kMinimumChargePercentRequiredFieldName, kNegativeInt);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
      })));
}

// Note that the memory routine has no parameters, so we only need to test that
// it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunMemoryRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kMemory,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Note that the LAN connectivity routine has no parameters, so we only need to
// test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunLanConnectivityRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kLanConnectivity,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the signal strength routine has no parameters, so we only need to
// test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunSignalStrengthRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kSignalStrength,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the gateway can be pinged routine has no parameters, so we only
// need to test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunGatewayCanBePingedRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kGatewayCanBePinged,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the has secure WiFi connection routine has no parameters, so we
// only need to test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunHasSecureWiFiConnectionRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
                         kHasSecureWiFiConnection,
                     std::move(params_dict),
                     base::BindLambdaForTesting([](RemoteCommandJob* job) {
                       EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                       std::unique_ptr<std::string> payload =
                           job->GetResultPayload();
                       EXPECT_TRUE(payload);
                       EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                     })));
}

// Note that the DNS resolver present routine has no parameters, so we only need
// to test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunDnsResolverPresentRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolverPresent,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the DNS latency routine has no parameters, so we only need
// to test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunDnsLatencyRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(
      RunJob(chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsLatency,
             std::move(params_dict),
             base::BindLambdaForTesting([](RemoteCommandJob* job) {
               EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
               std::unique_ptr<std::string> payload = job->GetResultPayload();
               EXPECT_TRUE(payload);
               EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
             })));
}

// Note that the DNS resolution routine has no parameters, so we only need
// to test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunDnsResolutionRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolution,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the captive portal routine has no parameters, so we only need to
// test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunCaptivePortalRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCaptivePortal,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the HTTP firewall routine has no parameters, so we only need to
// test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunHttpFirewallRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpFirewall,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the HTTPS firewall routine has no parameters, so we only need to
// test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunHttpsFirewallRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpsFirewall,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Note that the HTTPS latency routine has no parameters, so we only need to
// test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunHttpsLatencyRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpsLatency,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Test that the video conferencing routine succeeds with all parameters
// specified.
TEST_F(DeviceCommandRunRoutineJobTest, RunVideoConferencingRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetStringKey(
      DeviceCommandRunRoutineJob::kStunServerHostnameFieldName,
      kValidStunServerHostname);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kVideoConferencing,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

// Test that the video conferencing routine succeeds without the optional
// parameter stunServerHostname specified.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunVideoConferencingRoutineNoOptionalStunServerHostname) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(RunJob(
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kVideoConferencing,
      std::move(params_dict),
      base::BindLambdaForTesting([](RemoteCommandJob* job) {
        EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
        std::unique_ptr<std::string> payload = job->GetResultPayload();
        EXPECT_TRUE(payload);
        EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
      })));
}

}  // namespace policy
