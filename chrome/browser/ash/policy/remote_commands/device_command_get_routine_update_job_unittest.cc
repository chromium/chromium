// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_get_routine_update_job.h"

#include <limits>
#include <memory>
#include <optional>

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

// String constant identifying the output field in the result payload.
constexpr char kOutputFieldName[] = "output";
// String constant identifying the progress percent field in the result payload.
constexpr char kProgressPercentFieldName[] = "progressPercent";
// String constant identifying the noninteractive update field in the result
// payload.
constexpr char kNonInteractiveUpdateFieldName[] = "nonInteractiveUpdate";
// String constant identifying the status field in the result payload.
constexpr char kStatusFieldName[] = "status";
// String constant identifying the status message field in the result payload.
constexpr char kStatusMessageFieldName[] = "statusMessage";
// String constant identifying the interactive update field in the result
// payload.
constexpr char kInteractiveUpdateFieldName[] = "interactiveUpdate";
// String constant identifying the user message field in the result payload.
constexpr char kUserMessageFieldName[] = "userMessage";

// String constant identifying the id field in the command payload.
constexpr char kIdFieldName[] = "id";
// String constant identifying the command field in the command payload.
constexpr char kCommandFieldName[] = "command";
// String constant identifying the include output field in the command payload.
constexpr char kIncludeOutputFieldName[] = "includeOutput";

// Dummy values to populate cros_healthd's GetRoutineUpdate responses.
constexpr uint32_t kProgressPercent = 97;
constexpr ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum kStatus =
    ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRunning;
constexpr char kStatusMessage[] = "status_message";
constexpr ash::cros_healthd::mojom::DiagnosticRoutineUserMessageEnum
    kUserMessage = ash::cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::
        kPlugInACPower;

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 987123;

em::RemoteCommand GenerateCommandProto(
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeDelta age_of_command,
    base::TimeDelta idleness_cutoff,
    bool terminate_upon_input,
    std::optional<int32_t> id,
    std::optional<ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum>
        command,
    std::optional<bool> include_output) {
  em::RemoteCommand command_proto;
  command_proto.set_type(
      em::RemoteCommand_Type_DEVICE_GET_DIAGNOSTIC_ROUTINE_UPDATE);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  base::Value::Dict root_dict;
  if (id.has_value()) {
    root_dict.Set(kIdFieldName, id.value());
  }
  if (command.has_value()) {
    root_dict.Set(kCommandFieldName, static_cast<int>(command.value()));
  }
  if (include_output.has_value()) {
    root_dict.Set(kIncludeOutputFieldName, include_output.value());
  }
  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

std::string CreateInteractivePayload(
    uint32_t progress_percent,
    std::optional<std::string> output,
    ash::cros_healthd::mojom::DiagnosticRoutineUserMessageEnum user_message) {
  auto root_dict = base::Value::Dict().Set(kProgressPercentFieldName,
                                           static_cast<int>(progress_percent));
  if (output.has_value()) {
    root_dict.Set(kOutputFieldName, std::move(output.value()));
  }
  auto interactive_dict = base::Value::Dict().Set(
      kUserMessageFieldName, static_cast<int>(user_message));
  root_dict.Set(kInteractiveUpdateFieldName, std::move(interactive_dict));

  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

std::string CreateNonInteractivePayload(
    uint32_t progress_percent,
    std::optional<std::string> output,
    ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum status,
    const std::string& status_message) {
  auto root_dict = base::Value::Dict().Set(kProgressPercentFieldName,
                                           static_cast<int>(progress_percent));
  if (output.has_value()) {
    root_dict.Set(kOutputFieldName, std::move(output.value()));
  }
  auto noninteractive_dict =
      base::Value::Dict()
          .Set(kStatusFieldName, static_cast<int>(status))
          .Set(kStatusMessageFieldName, status_message);
  root_dict.Set(kNonInteractiveUpdateFieldName, std::move(noninteractive_dict));

  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

}  // namespace

class DeviceCommandGetRoutineUpdateJobTest : public testing::Test {
 protected:
  DeviceCommandGetRoutineUpdateJobTest();
  DeviceCommandGetRoutineUpdateJobTest(
      const DeviceCommandGetRoutineUpdateJobTest&) = delete;
  DeviceCommandGetRoutineUpdateJobTest& operator=(
      const DeviceCommandGetRoutineUpdateJobTest&) = delete;
  ~DeviceCommandGetRoutineUpdateJobTest() override;

  void InitializeJob(
      RemoteCommandJob* job,
      RemoteCommandJob::UniqueIDType unique_id,
      base::TimeTicks issued_time,
      base::TimeDelta idleness_cutoff,
      bool terminate_upon_input,
      int32_t id,
      ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum command,
      bool include_output);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;

  base::TimeTicks test_start_time_;
};

DeviceCommandGetRoutineUpdateJobTest::DeviceCommandGetRoutineUpdateJobTest() {
  ash::cros_healthd::FakeCrosHealthd::Initialize();
  test_start_time_ = base::TimeTicks::Now();
}

DeviceCommandGetRoutineUpdateJobTest::~DeviceCommandGetRoutineUpdateJobTest() {
  ash::cros_healthd::FakeCrosHealthd::Shutdown();
}

void DeviceCommandGetRoutineUpdateJobTest::InitializeJob(
    RemoteCommandJob* job,
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeTicks issued_time,
    base::TimeDelta idleness_cutoff,
    bool terminate_upon_input,
    int32_t id,
    ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum command,
    bool include_output) {
  EXPECT_TRUE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(unique_id, base::TimeTicks::Now() - issued_time,
                           idleness_cutoff, terminate_upon_input, id, command,
                           include_output),
      em::SignedData()));

  EXPECT_EQ(unique_id, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
}

TEST_F(DeviceCommandGetRoutineUpdateJobTest,
       InvalidCommandEnumInCommandPayload) {
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetRoutineUpdateJob>();
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(
          kUniqueID, base::TimeTicks::Now() - test_start_time_,
          base::Seconds(30),
          /*terminate_upon_input=*/false, /*id=*/7979,
          static_cast<ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum>(
              std::numeric_limits<std::underlying_type<
                  ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum>::
                                      type>::max()),
          /*include_output=*/false),
      em::SignedData()));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

TEST_F(DeviceCommandGetRoutineUpdateJobTest, CommandPayloadMissingId) {
  // Test that not specifying a routine causes the job initialization to fail.
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetRoutineUpdateJob>();
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(
          kUniqueID, base::TimeTicks::Now() - test_start_time_,
          base::Seconds(30),
          /*terminate_upon_input=*/false,
          /*id=*/std::nullopt,
          ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum::kGetStatus,
          /*include_output=*/true),
      em::SignedData()));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

TEST_F(DeviceCommandGetRoutineUpdateJobTest, CommandPayloadMissingCommand) {
  // Test that not including a parameters dictionary causes the routine
  // initialization to fail.
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetRoutineUpdateJob>();
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(kUniqueID, base::TimeTicks::Now() - test_start_time_,
                           base::Seconds(30),
                           /*terminate_upon_input=*/false,
                           /*id=*/1293, /*command=*/std::nullopt,
                           /*include_output=*/true),
      em::SignedData()));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

TEST_F(DeviceCommandGetRoutineUpdateJobTest,
       CommandPayloadMissingIncludeOutput) {
  // Test that not including a parameters dictionary causes the routine
  // initialization to fail.
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetRoutineUpdateJob>();
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(
          kUniqueID, base::TimeTicks::Now() - test_start_time_,
          base::Seconds(30),
          /*terminate_upon_input=*/false,
          /*id=*/457658,
          ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum::kCancel,
          /*include_output=*/std::nullopt),
      em::SignedData()));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

TEST_F(DeviceCommandGetRoutineUpdateJobTest,
       GetInteractiveRoutineUpdateSuccess) {
  ash::cros_healthd::mojom::InteractiveRoutineUpdate interactive_update(
      kUserMessage);

  ash::cros_healthd::mojom::RoutineUpdateUnion update_union;
  update_union.set_interactive_update(interactive_update.Clone());

  auto response = ash::cros_healthd::mojom::RoutineUpdate::New(
      kProgressPercent,
      /*output=*/mojo::ScopedHandle(), update_union.Clone());
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetGetRoutineUpdateResponseForTesting(response);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetRoutineUpdateJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_, base::Seconds(30),
                /*terminate_upon_input=*/false, /*id=*/56923,
                ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum::kRemove,
                /*include_output=*/true);
  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  EXPECT_TRUE(payload);
  // TODO(crbug.com/1056323): Verify output.
  EXPECT_EQ(CreateInteractivePayload(kProgressPercent,
                                     /*output=*/std::nullopt, kUserMessage),
            *payload);
}

TEST_F(DeviceCommandGetRoutineUpdateJobTest,
       GetNonInteractiveRoutineUpdateSuccess) {
  ash::cros_healthd::mojom::NonInteractiveRoutineUpdate noninteractive_update(
      kStatus, kStatusMessage);

  ash::cros_healthd::mojom::RoutineUpdateUnion update_union;
  update_union.set_noninteractive_update(noninteractive_update.Clone());

  auto response = ash::cros_healthd::mojom::RoutineUpdate::New(
      kProgressPercent,
      /*output=*/mojo::ScopedHandle(), update_union.Clone());
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetGetRoutineUpdateResponseForTesting(response);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandGetRoutineUpdateJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_, base::Seconds(30),
                /*terminate_upon_input=*/false, /*id=*/9812,
                ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum::kRemove,
                /*include_output=*/true);
  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  EXPECT_TRUE(payload);
  // TODO(crbug.com/1056323): Verify output.
  EXPECT_EQ(CreateNonInteractivePayload(kProgressPercent,
                                        /*output=*/std::nullopt, kStatus,
                                        kStatusMessage),
            *payload);
}

}  // namespace policy
