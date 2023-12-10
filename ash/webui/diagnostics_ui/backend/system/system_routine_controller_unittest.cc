// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/system_routine_controller.h"

#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/fake_diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash::diagnostics {

namespace {

namespace healthd = cros_healthd::mojom;

constexpr char kChargePercentKey[] = "chargePercent";
constexpr char kDischargePercentKey[] = "dischargePercent";
constexpr char kResultDetailsKey[] = "resultDetails";

void SetCrosHealthdRunRoutineResponse(
    healthd::RunRoutineResponsePtr& response) {
  cros_healthd::FakeCrosHealthd::Get()->SetRunRoutineResponseForTesting(
      response);
}

void SetRunRoutineResponse(int32_t id,
                           healthd::DiagnosticRoutineStatusEnum status) {
  auto routine_response = healthd::RunRoutineResponse::New(id, status);
  SetCrosHealthdRunRoutineResponse(routine_response);
}

void SetCrosHealthdRoutineUpdateResponse(healthd::RoutineUpdatePtr& response) {
  cros_healthd::FakeCrosHealthd::Get()->SetGetRoutineUpdateResponseForTesting(
      response);
}

void SetNonInteractiveRoutineUpdateResponse(
    uint32_t percent_complete,
    healthd::DiagnosticRoutineStatusEnum status,
    mojo::ScopedHandle output_handle) {
  DCHECK_GE(percent_complete, 0u);
  DCHECK_LE(percent_complete, 100u);

  auto non_interactive_update =
      healthd::NonInteractiveRoutineUpdate::New(status, /*message=*/"");
  auto routine_update_union =
      healthd::RoutineUpdateUnion::NewNoninteractiveUpdate(
          std::move(non_interactive_update));
  auto routine_update = healthd::RoutineUpdate::New();
  routine_update->progress_percent = percent_complete;
  routine_update->output = std::move(output_handle);
  routine_update->routine_update_union = std::move(routine_update_union);

  SetCrosHealthdRoutineUpdateResponse(routine_update);
}

void VerifyRoutineResult(const mojom::RoutineResultInfo& result_info,
                         mojom::RoutineType expected_routine_type,
                         mojom::StandardRoutineResult expected_result) {
  const mojom::StandardRoutineResult actual_result =
      result_info.result->get_simple_result();

  EXPECT_EQ(expected_result, actual_result);
  EXPECT_EQ(expected_routine_type, result_info.type);
}

void VerifyRoutineResult(const mojom::RoutineResultInfo& result_info,
                         mojom::RoutineType expected_routine_type,
                         mojom::PowerRoutineResultPtr expected_result) {
  const mojom::PowerRoutineResultPtr& actual_result =
      result_info.result->get_power_result();

  EXPECT_EQ(expected_result->simple_result, actual_result->simple_result);
  EXPECT_EQ(expected_result->percent_change, actual_result->percent_change);
  EXPECT_EQ(expected_result->time_elapsed_seconds,
            actual_result->time_elapsed_seconds);
  EXPECT_EQ(expected_routine_type, result_info.type);
}

mojom::PowerRoutineResultPtr ConstructPowerRoutineResult(
    mojom::StandardRoutineResult simple_result,
    double percent_change,
    uint32_t time_elapsed_seconds) {
  return mojom::PowerRoutineResult::New(simple_result, percent_change,
                                        time_elapsed_seconds);
}

// Constructs the Power Routine Result json. If `charge_percent` is negative,
// the discharge field will be used.
std::string ConstructPowerRoutineResultJson(double charge_percent,
                                            bool charge) {
  base::Value::Dict result_dict;
  if (charge) {
    result_dict.Set(kChargePercentKey, charge_percent);

  } else {
    result_dict.Set(kDischargePercentKey, charge_percent);
  }

  base::Value::Dict output_dict;
  output_dict.Set(kResultDetailsKey, std::move(result_dict));

  std::string json;
  const bool serialize_success = base::JSONWriter::Write(output_dict, &json);
  DCHECK(serialize_success);
  return json;
}

void SetAvailableRoutines(
    const std::vector<healthd::DiagnosticRoutineEnum>& routines) {
  cros_healthd::FakeCrosHealthd::Get()->SetAvailableRoutinesForTesting(
      routines);
}

std::vector<std::string> GetLogLines(const std::string& log) {
  return base::SplitString(log, "\n", base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

std::vector<std::string> GetLogLineContents(const std::string& log_line) {
  const std::vector<std::string> result = base::SplitString(
      log_line, "-", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return result;
}

}  // namespace

struct FakeRoutineRunner : public mojom::RoutineRunner {
  // mojom::RoutineRunner
  void OnRoutineResult(mojom::RoutineResultInfoPtr result_info) override {
    DCHECK(result.is_null()) << "OnRoutineResult should only be called once";

    result = std::move(result_info);
  }

  mojom::RoutineResultInfoPtr result;

  mojo::Receiver<mojom::RoutineRunner> receiver{this};
};

class SystemRoutineControllerTest : public AshTestBase {
 public:
  SystemRoutineControllerTest()
      : AshTestBase(content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  SystemRoutineControllerTest(const SystemRoutineControllerTest&) = delete;
  SystemRoutineControllerTest& operator=(const SystemRoutineControllerTest&) =
      delete;

  ~SystemRoutineControllerTest() override = default;

  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    cros_healthd::FakeCrosHealthd::Initialize();
    system_routine_controller_ = std::make_unique<SystemRoutineController>();
    DiagnosticsLogController::Initialize(
        std::make_unique<FakeDiagnosticsBrowserDelegate>());

    wake_lock_provider_ = std::make_unique<device::TestWakeLockProvider>();

    mojo::Remote<device::mojom::WakeLockProvider> remote_provider;
    wake_lock_provider_->BindReceiver(
        remote_provider.BindNewPipeAndPassReceiver());
    system_routine_controller_->SetWakeLockProviderForTesting(
        std::move(remote_provider));
  }

  void TearDown() override {
    system_routine_controller_.reset();
    cros_healthd::FakeCrosHealthd::Shutdown();
    base::RunLoop().RunUntilIdle();
    AshTestBase::TearDown();
  }

 protected:
  mojo::ScopedHandle CreateMojoHandleForPowerRoutine(double charge_percent,
                                                     bool charge) {
    return CreateMojoHandle(
        ConstructPowerRoutineResultJson(charge_percent, charge));
  }

  bool IsActiveWakeLock() {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_->GetActiveWakeLocksForTests(
        device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_count, int32_t count) {
              *result_count = count;
              LOG(ERROR) << *result_count;
              run_loop->Quit();
            },
            &run_loop, &result_count));
    run_loop.Run();
    return result_count == 1;
  }

  void CallSendRoutineResult(mojom::RoutineResultInfoPtr result_info) {
    system_routine_controller_->SendRoutineResult(std::move(result_info));

    task_environment()->RunUntilIdle();
  }

  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  std::unique_ptr<SystemRoutineController> system_routine_controller_;

 private:
  mojo::ScopedHandle CreateMojoHandle(const std::string& contents) {
    const bool temp_success = temp_dir_.CreateUniqueTempDir();
    DCHECK(temp_success);

    base::FilePath path;
    base::ScopedFD fd =
        base::CreateAndOpenFdForTemporaryFileInDir(temp_dir_.GetPath(), &path);
    DCHECK(fd.is_valid());
    const bool write_success = base::WriteFileDescriptor(fd.get(), contents);
    DCHECK(write_success);
    return mojo::WrapPlatformFile(std::move(fd));
  }

  base::ScopedTempDir temp_dir_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<device::TestWakeLockProvider> wake_lock_provider_;
};

TEST_F(SystemRoutineControllerTest, RejectedByCrosHealthd) {
  SetRunRoutineResponse(healthd::kFailedToStartId,
                        healthd::DiagnosticRoutineStatusEnum::kFailedToStart);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kUnableToRun);
}

TEST_F(SystemRoutineControllerTest, AlreadyInProgress) {
  // Put one routine in progress.
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner_1;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_1.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner_1.result.is_null());

  FakeRoutineRunner routine_runner_2;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_2.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the second routine is rejected.
  EXPECT_FALSE(routine_runner_2.result.is_null());
  VerifyRoutineResult(*routine_runner_2.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kUnableToRun);
}

TEST_F(SystemRoutineControllerTest, CpuStressSuccess) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment()->FastForwardBy(base::Seconds(59));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the update is fetched and processed.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);
}

TEST_F(SystemRoutineControllerTest, CpuStressFailure) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kFailed,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment()->FastForwardBy(base::Seconds(59));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the update is fetched and processed.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestFailed);
}

TEST_F(SystemRoutineControllerTest, CpuStressStillRunning) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is still running.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment()->FastForwardBy(base::Seconds(59));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the results from the routine are still not
  // available.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is completed
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // Fast forward by the refresh interval.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);
}

TEST_F(SystemRoutineControllerTest, CpuStressStillRunningMultipleIntervals) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is still running.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment()->FastForwardBy(base::Seconds(59));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the results from the routine are still not
  // available.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  // After another refresh interval, the routine is still running.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd to signify the routine is completed
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // After a second refresh interval, the routine is completed.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);
}

TEST_F(SystemRoutineControllerTest, TwoConsecutiveRoutines) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner_1;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_1.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner_1.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());
  task_environment()->FastForwardBy(base::Seconds(60));
  EXPECT_FALSE(routine_runner_1.result.is_null());
  VerifyRoutineResult(*routine_runner_1.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);

  // Run the test again
  SetRunRoutineResponse(/*id=*/2,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner_2;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_2.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the second routine is not complete.
  EXPECT_TRUE(routine_runner_2.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kFailed,
      mojo::ScopedHandle());
  task_environment()->FastForwardBy(base::Seconds(60));
  EXPECT_FALSE(routine_runner_2.result.is_null());
  VerifyRoutineResult(*routine_runner_2.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestFailed);
}

TEST_F(SystemRoutineControllerTest, PowerRoutineSuccess) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kWaiting);
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/10, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kBatteryCharge,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  const uint8_t expected_percent_charge = 2;
  const uint32_t expected_time_elapsed_seconds = 30;

  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      CreateMojoHandleForPowerRoutine(expected_percent_charge,
                                      /*charge=*/true));
  task_environment()->FastForwardBy(base::Seconds(31));

  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(
      *routine_runner.result, mojom::RoutineType::kBatteryCharge,
      ConstructPowerRoutineResult(mojom::StandardRoutineResult::kTestPassed,
                                  expected_percent_charge,
                                  expected_time_elapsed_seconds));
}

TEST_F(SystemRoutineControllerTest, DischargeRoutineSuccess) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kWaiting);
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/10, healthd::DiagnosticRoutineStatusEnum::kRunning,
      mojo::ScopedHandle());

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kBatteryDischarge,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  const uint8_t expected_percent_discharge = 5;
  const uint32_t expected_time_elapsed_seconds = 30;

  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      CreateMojoHandleForPowerRoutine(expected_percent_discharge,
                                      /*charge=*/false));
  task_environment()->FastForwardBy(base::Seconds(31));

  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(
      *routine_runner.result, mojom::RoutineType::kBatteryDischarge,
      ConstructPowerRoutineResult(mojom::StandardRoutineResult::kTestPassed,
                                  expected_percent_discharge,
                                  expected_time_elapsed_seconds));
}

TEST_F(SystemRoutineControllerTest, AvailableRoutines) {
  SetAvailableRoutines(
      {healthd::DiagnosticRoutineEnum::kFloatingPointAccuracy,
       healthd::DiagnosticRoutineEnum::kMemory,
       healthd::DiagnosticRoutineEnum::kPrimeSearch,
       healthd::DiagnosticRoutineEnum::kAcPower,
       healthd::DiagnosticRoutineEnum::kBatteryCapacity,
       healthd::DiagnosticRoutineEnum::kBatteryHealth,
       healthd::DiagnosticRoutineEnum::kCaptivePortal,
       healthd::DiagnosticRoutineEnum::kDnsLatency,
       healthd::DiagnosticRoutineEnum::kDnsResolution,
       healthd::DiagnosticRoutineEnum::kDnsResolverPresent,
       healthd::DiagnosticRoutineEnum::kGatewayCanBePinged,
       healthd::DiagnosticRoutineEnum::kHasSecureWiFiConnection,
       healthd::DiagnosticRoutineEnum::kHttpFirewall,
       healthd::DiagnosticRoutineEnum::kHttpsFirewall,
       healthd::DiagnosticRoutineEnum::kHttpsLatency,
       healthd::DiagnosticRoutineEnum::kLanConnectivity,
       healthd::DiagnosticRoutineEnum::kSignalStrength,
       healthd::DiagnosticRoutineEnum::kArcHttp,
       healthd::DiagnosticRoutineEnum::kArcPing,
       healthd::DiagnosticRoutineEnum::kArcDnsResolution});

  base::RunLoop run_loop;
  system_routine_controller_->GetSupportedRoutines(base::BindLambdaForTesting(
      [&](const std::vector<mojom::RoutineType>& supported_routines) {
        EXPECT_EQ(17u, supported_routines.size());
        EXPECT_FALSE(base::Contains(supported_routines,
                                    mojom::RoutineType::kBatteryCharge));
        EXPECT_FALSE(base::Contains(supported_routines,
                                    mojom::RoutineType::kBatteryDischarge));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kCaptivePortal));
        EXPECT_FALSE(
            base::Contains(supported_routines, mojom::RoutineType::kCpuCache));
        EXPECT_FALSE(
            base::Contains(supported_routines, mojom::RoutineType::kCpuStress));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kCpuFloatingPoint));
        EXPECT_TRUE(
            base::Contains(supported_routines, mojom::RoutineType::kCpuPrime));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kDnsLatency));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kDnsResolution));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kDnsResolverPresent));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kGatewayCanBePinged));
        EXPECT_TRUE(base::Contains(
            supported_routines, mojom::RoutineType::kHasSecureWiFiConnection));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kHttpFirewall));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kHttpsFirewall));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kHttpsLatency));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kLanConnectivity));
        EXPECT_TRUE(
            base::Contains(supported_routines, mojom::RoutineType::kMemory));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kSignalStrength));
        EXPECT_TRUE(
            base::Contains(supported_routines, mojom::RoutineType::kArcHttp));
        EXPECT_TRUE(
            base::Contains(supported_routines, mojom::RoutineType::kArcPing));
        EXPECT_TRUE(base::Contains(supported_routines,
                                   mojom::RoutineType::kArcDnsResolution));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemRoutineControllerTest, CancelRoutine) {
  const int32_t expected_id = 1;
  SetRunRoutineResponse(expected_id,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  std::unique_ptr<FakeRoutineRunner> routine_runner =
      std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner->result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/0, healthd::DiagnosticRoutineStatusEnum::kCancelled,
      mojo::ScopedHandle());

  // Close the routine_runner
  routine_runner.reset();
  base::RunLoop().RunUntilIdle();

  // Verify that CrosHealthd is called with the correct parameters.
  std::optional<cros_healthd::FakeCrosHealthd::RoutineUpdateParams>
      update_params =
          cros_healthd::FakeCrosHealthd::Get()->GetRoutineUpdateParams();

  ASSERT_TRUE(update_params.has_value());
  EXPECT_EQ(expected_id, update_params->id);
  EXPECT_EQ(healthd::DiagnosticRoutineCommandEnum::kCancel,
            update_params->command);
}

TEST_F(SystemRoutineControllerTest, CancelRoutineDtor) {
  const int32_t expected_id = 2;
  SetRunRoutineResponse(expected_id,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  std::unique_ptr<FakeRoutineRunner> routine_runner =
      std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner->result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/0, healthd::DiagnosticRoutineStatusEnum::kCancelled,
      mojo::ScopedHandle());

  // Destroy the SystemRoutineController
  system_routine_controller_.reset();
  base::RunLoop().RunUntilIdle();

  // Verify that CrosHealthd is called with the correct parameters.
  std::optional<cros_healthd::FakeCrosHealthd::RoutineUpdateParams>
      update_params =
          cros_healthd::FakeCrosHealthd::Get()->GetRoutineUpdateParams();

  ASSERT_TRUE(update_params.has_value());
  EXPECT_EQ(expected_id, update_params->id);
  EXPECT_EQ(healthd::DiagnosticRoutineCommandEnum::kCancel,
            update_params->command);
}

TEST_F(SystemRoutineControllerTest, RunRoutineCount0) {
  base::HistogramTester histogram_tester;

  system_routine_controller_.reset();

  histogram_tester.ExpectBucketCount("ChromeOS.DiagnosticsUi.RoutineCount", 0,
                                     1);
}

TEST_F(SystemRoutineControllerTest, RunRoutineCount1) {
  // Run a routine.
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // Before the update interval, the routine status is not processed.
  task_environment()->FastForwardBy(base::Seconds(59));
  EXPECT_TRUE(routine_runner.result.is_null());

  // After the update interval, the update is fetched and processed.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);

  // Destroy the SystemRoutineController and check the emitted result.
  base::HistogramTester histogram_tester;

  system_routine_controller_.reset();

  histogram_tester.ExpectBucketCount("ChromeOS.DiagnosticsUi.RoutineCount", 1,
                                     1);
}

TEST_F(SystemRoutineControllerTest, RoutineLog) {
  base::ScopedTempDir temp_dir;
  base::FilePath log_path;

  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  log_path = temp_dir.GetPath().AppendASCII("routine_log");
  DiagnosticsLogController::Get()->SetRoutineLogForTesting(
      std::make_unique<RoutineLog>(log_path));

  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);
  task_environment()->RunUntilIdle();

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  task_environment()->RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Verify that the Running status appears in the log.
  std::vector<std::string> log_lines = GetLogLines(
      DiagnosticsLogController::Get()->GetRoutineLog().GetContentsForCategory(
          RoutineLog::RoutineCategory::kSystem));
  EXPECT_EQ(1u, log_lines.size());

  std::vector<std::string> log_line_contents = GetLogLineContents(log_lines[0]);
  ASSERT_EQ(3u, log_line_contents.size());
  EXPECT_EQ("CpuStress", log_line_contents[1]);
  EXPECT_EQ("Started", log_line_contents[2]);

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // After the update interval, the update is fetched and processed.
  task_environment()->FastForwardBy(base::Seconds(60));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);

  // Verify that the Passed status appears in the log.
  log_lines = GetLogLines(
      DiagnosticsLogController::Get()->GetRoutineLog().GetContentsForCategory(
          RoutineLog::RoutineCategory::kSystem));
  EXPECT_EQ(2u, log_lines.size());

  log_line_contents = GetLogLineContents(log_lines[1]);
  ASSERT_EQ(3u, log_line_contents.size());
  EXPECT_EQ("CpuStress", log_line_contents[1]);
  EXPECT_EQ("Passed", log_line_contents[2]);

  // Start another routine and cancel it and verify the cancellation appears in
  // logs. Use a unique_ptr for the RoutineRunner so we can easily destroy it.
  auto routine_runner_2 = std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuPrime,
      routine_runner_2->receiver.BindNewPipeAndPassRemote());
  task_environment()->RunUntilIdle();

  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/0, healthd::DiagnosticRoutineStatusEnum::kCancelled,
      mojo::ScopedHandle());

  // Close the routine_runner
  routine_runner_2.reset();
  task_environment()->RunUntilIdle();

  log_lines = GetLogLines(
      DiagnosticsLogController::Get()->GetRoutineLog().GetContentsForCategory(
          RoutineLog::RoutineCategory::kSystem));
  EXPECT_EQ(4u, log_lines.size());

  log_line_contents = GetLogLineContents(log_lines[3]);
  ASSERT_EQ(2u, log_line_contents.size());
  EXPECT_EQ("Inflight Routine Cancelled", log_line_contents[1]);
}

TEST_F(SystemRoutineControllerTest, RoutineResultEmitted) {
  // Run the CpuStress routine.
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  base::HistogramTester histogram_tester;

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // After the update interval, the update is fetched and processed.
  task_environment()->FastForwardBy(base::Seconds(60));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);

  histogram_tester.ExpectUniqueSample("ChromeOS.DiagnosticsUi.CpuStressResult",
                                      mojom::StandardRoutineResult::kTestPassed,
                                      /*expected_count=*/1);
}

TEST_F(SystemRoutineControllerTest,
       RoutineFailedToStartCalledWithCorrectRoutineType) {
  SetRunRoutineResponse(healthd::kFailedToStartId,
                        healthd::DiagnosticRoutineStatusEnum::kFailedToStart);

  base::HistogramTester histogram_tester;
  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuCache,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kCpuCache,
                      mojom::StandardRoutineResult::kUnableToRun);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.DiagnosticsUi.CpuCacheResult",
      mojom::StandardRoutineResult::kUnableToRun,
      /*expected_count=*/1);
}

TEST_F(SystemRoutineControllerTest, MemoryRuntimeEmitted) {
  // Run the CpuStress routine.
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  base::HistogramTester histogram_tester;

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kMemory,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // After the update interval, the update is fetched and processed.
  task_environment()->FastForwardBy(base::Seconds(1000));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kMemory,
                      mojom::StandardRoutineResult::kTestPassed);

  histogram_tester.ExpectUniqueTimeSample(
      "ChromeOS.DiagnosticsUi.MemoryRoutineDuration", base::Seconds(1000),
      /*expected_count=*/1);
}

TEST_F(SystemRoutineControllerTest, CancelThenStartRoutine) {
  const int32_t expected_id = 1;
  SetRunRoutineResponse(expected_id,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  auto routine_runner = std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner->result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/0, healthd::DiagnosticRoutineStatusEnum::kCancelled,
      mojo::ScopedHandle());

  // Close the routine_runner
  routine_runner.reset();
  base::RunLoop().RunUntilIdle();

  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner_2;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner_2.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner_2.result.is_null());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // After the update interval, the update is fetched and processed.
  task_environment()->FastForwardBy(base::Seconds(60));
  EXPECT_FALSE(routine_runner_2.result.is_null());
  VerifyRoutineResult(*routine_runner_2.result, mojom::RoutineType::kCpuStress,
                      mojom::StandardRoutineResult::kTestPassed);
}

TEST_F(SystemRoutineControllerTest, MemoryAcquiresWakeLock) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  FakeRoutineRunner routine_runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kMemory,
      routine_runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner.result.is_null());

  // Confirm that a wake lock is held.
  EXPECT_TRUE(IsActiveWakeLock());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/100, healthd::DiagnosticRoutineStatusEnum::kPassed,
      mojo::ScopedHandle());

  // After the update interval, the update is fetched and processed.
  task_environment()->FastForwardBy(base::Seconds(1000));
  EXPECT_FALSE(routine_runner.result.is_null());
  VerifyRoutineResult(*routine_runner.result, mojom::RoutineType::kMemory,
                      mojom::StandardRoutineResult::kTestPassed);

  // Confirm the wake lock is released.
  EXPECT_FALSE(IsActiveWakeLock());
}

TEST_F(SystemRoutineControllerTest, CancelMemoryReleasesWakeLock) {
  SetRunRoutineResponse(/*id=*/1,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  auto routine_runner = std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kMemory,
      routine_runner->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner->result.is_null());

  // Confirm that a wake lock is held.
  EXPECT_TRUE(IsActiveWakeLock());

  // Update the status on cros_healthd.
  SetNonInteractiveRoutineUpdateResponse(
      /*percent_complete=*/0, healthd::DiagnosticRoutineStatusEnum::kCancelled,
      mojo::ScopedHandle());

  // Close the routine_runner
  routine_runner.reset();
  base::RunLoop().RunUntilIdle();

  // Confirm the wake lock is released.
  EXPECT_FALSE(IsActiveWakeLock());
}

TEST_F(SystemRoutineControllerTest, ResetReceiverOnDisconnect) {
  ASSERT_FALSE(system_routine_controller_->IsReceiverBoundForTesting());
  mojo::Remote<mojom::SystemRoutineController> remote;
  system_routine_controller_->BindInterface(
      remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(system_routine_controller_->IsReceiverBoundForTesting());

  // Unbind remote to trigger disconnect and disconnect handler.
  remote.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(system_routine_controller_->IsReceiverBoundForTesting());

  // Test intent is to ensure interface can be rebound when application is
  // reloaded using |CTRL + R|.  A disconnect should be signaled in which we
  // will reset the receiver to its unbound state.
  system_routine_controller_->BindInterface(
      remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(system_routine_controller_->IsReceiverBoundForTesting());
}

TEST_F(SystemRoutineControllerTest, SendRoutineResultDoesNotCrash) {
  const int32_t expected_id = 1;
  SetRunRoutineResponse(expected_id,
                        healthd::DiagnosticRoutineStatusEnum::kRunning);

  auto routine_runner = std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kCpuStress,
      routine_runner->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Assert that the first routine is not complete.
  EXPECT_TRUE(routine_runner->result.is_null());

  // SendRoutineResult does not crash and routine runner is not updated when
  // called with nullptr.
  EXPECT_NO_FATAL_FAILURE(CallSendRoutineResult(/*result_info=*/nullptr));
  EXPECT_TRUE(routine_runner->result.is_null());

  // SendRoutineResult does not crash and routine runner is not updated when
  // RoutineResultInfoPtr exists and |result| has not been configured.
  EXPECT_NO_FATAL_FAILURE(
      CallSendRoutineResult(mojom::RoutineResultInfo::New()));
  EXPECT_TRUE(routine_runner->result.is_null());

  mojom::RoutineResultInfoPtr null_result_info =
      mojom::RoutineResultInfo::New();
  null_result_info.reset();

  // SendRoutineResult does not crash and routine runner is not updated when
  // RoutineResultInfoPtr exists and has not been configured.
  EXPECT_NO_FATAL_FAILURE(CallSendRoutineResult(std::move(null_result_info)));
  EXPECT_TRUE(routine_runner->result.is_null());
}

TEST_F(SystemRoutineControllerTest,
       SendRoutineResultWithNullRunnerDoesNotCrash) {
  // Intentionally do not initialize routine_runner.
  EXPECT_NO_FATAL_FAILURE(CallSendRoutineResult(mojom::RoutineResultInfo::New(
      mojom::RoutineType::kCpuStress,
      mojom::RoutineResult::NewSimpleResult(
          mojom::StandardRoutineResult::kTestPassed))));
}

}  // namespace ash::diagnostics
