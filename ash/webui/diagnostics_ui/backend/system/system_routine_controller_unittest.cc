// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/system_routine_controller.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/fake_diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/backend/system/fake_system_routine_controller_delegate.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash::diagnostics {

namespace {

namespace healthd = cros_healthd::mojom;

constexpr char kChargePercentKey[] = "chargePercent";
constexpr char kDischargePercentKey[] = "dischargePercent";
constexpr char kResultDetailsKey[] = "resultDetails";
constexpr char kTestHostname[] = "clients1.google.com";
constexpr char kTestErrorMessage[] = "Connection refused";

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
  base::DictValue result_dict;
  if (charge) {
    result_dict.Set(kChargePercentKey, charge_percent);

  } else {
    result_dict.Set(kDischargePercentKey, charge_percent);
  }

  base::DictValue output_dict;
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

chromeos::network_diagnostics::mojom::RoutineResultPtr
MakeGoogleServicesConnectivityResult(
    chromeos::network_diagnostics::mojom::RoutineVerdict verdict,
    std::vector<chromeos::network_diagnostics::mojom::
                    GoogleServicesConnectivityProblemPtr> problems) {
  auto result = chromeos::network_diagnostics::mojom::RoutineResult::New();
  result->verdict = verdict;
  result->problems = chromeos::network_diagnostics::mojom::RoutineProblems::
      NewGoogleServicesConnectivityProblems(std::move(problems));
  result->timestamp = base::Time::Now();
  result->source =
      chromeos::network_diagnostics::mojom::RoutineCallSource::kUnknown;
  return result;
}

// Builds a single-element problems vector with a connection failure for the
// given `hostname` and `error_message`.
std::vector<
    chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProblemPtr>
MakeSingleConnectionProblem(const std::string& hostname,
                            const std::string& error_message) {
  using Problem =
      chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProblem;
  using ConnectionError = chromeos::network_diagnostics::mojom::
      GoogleServicesConnectivityConnectionError;
  using ConnectionInfo = chromeos::network_diagnostics::mojom::
      GoogleServicesConnectivityConnectionErrorInfo;
  using ErrorDetails = chromeos::network_diagnostics::mojom::
      GoogleServicesConnectivityErrorDetails;

  auto error_details = ErrorDetails::New(/*error_message=*/error_message,
                                         /*resolution_message=*/std::nullopt);
  auto connection_info =
      ConnectionInfo::New(/*hostname=*/hostname, std::move(error_details),
                          /*timestamp_start=*/std::nullopt,
                          /*timestamp_end=*/std::nullopt);
  auto connection_error = ConnectionError::New(
      chromeos::network_diagnostics::mojom::
          GoogleServicesConnectivityProblemType::kConnectionFailure,
      /*proxy=*/std::nullopt, std::move(connection_info));

  std::vector<chromeos::network_diagnostics::mojom::
                  GoogleServicesConnectivityProblemPtr>
      problems;
  problems.push_back(Problem::NewConnectionError(std::move(connection_error)));
  return problems;
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
    auto delegate = std::make_unique<FakeSystemRoutineControllerDelegate>();
    fake_delegate_ = delegate.get();
    system_routine_controller_ =
        std::make_unique<SystemRoutineController>(std::move(delegate));
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
    // Clear raw_ptr before destroying controller (which owns the delegate)
    // to avoid dangling pointer detection.
    fake_delegate_ = nullptr;
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

  // Sets up inflight state and calls OnDirectNetworkRoutineResult directly.
  // Used to test the null result path without running the full routine.
  void CallOnDirectNetworkRoutineResult(
      mojom::RoutineType type,
      chromeos::network_diagnostics::mojom::RoutineResultPtr result,
      mojo::PendingRemote<mojom::RoutineRunner> runner) {
    system_routine_controller_->inflight_routine_runner_.Bind(
        std::move(runner));
    system_routine_controller_->inflight_routine_type_ = type;
    system_routine_controller_->OnDirectNetworkRoutineResult(type,
                                                             std::move(result));
    task_environment()->RunUntilIdle();
  }

  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  raw_ptr<FakeSystemRoutineControllerDelegate> fake_delegate_ = nullptr;
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

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
        EXPECT_EQ(18u, supported_routines.size());
        EXPECT_FALSE(std::ranges::contains(supported_routines,
                                           mojom::RoutineType::kBatteryCharge));
        EXPECT_FALSE(std::ranges::contains(
            supported_routines, mojom::RoutineType::kBatteryDischarge));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kCaptivePortal));
        EXPECT_FALSE(std::ranges::contains(supported_routines,
                                           mojom::RoutineType::kCpuCache));
        EXPECT_FALSE(std::ranges::contains(supported_routines,
                                           mojom::RoutineType::kCpuStress));
        EXPECT_TRUE(std::ranges::contains(
            supported_routines, mojom::RoutineType::kCpuFloatingPoint));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kCpuPrime));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kDnsLatency));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kDnsResolution));
        EXPECT_TRUE(std::ranges::contains(
            supported_routines, mojom::RoutineType::kDnsResolverPresent));
        EXPECT_TRUE(std::ranges::contains(
            supported_routines, mojom::RoutineType::kGatewayCanBePinged));
        EXPECT_TRUE(std::ranges::contains(
            supported_routines, mojom::RoutineType::kHasSecureWiFiConnection));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kHttpFirewall));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kHttpsFirewall));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kHttpsLatency));
        EXPECT_TRUE(std::ranges::contains(
            supported_routines, mojom::RoutineType::kLanConnectivity));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kMemory));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kSignalStrength));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kArcHttp));
        EXPECT_TRUE(std::ranges::contains(supported_routines,
                                          mojom::RoutineType::kArcPing));
        EXPECT_TRUE(std::ranges::contains(
            supported_routines, mojom::RoutineType::kArcDnsResolution));
        EXPECT_TRUE(std::ranges::contains(
            supported_routines,
            mojom::RoutineType::kGoogleServicesConnectivity));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemRoutineControllerTest, AvailableRoutines_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

  // Same healthd set, but GSC feature flag is disabled.
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
        EXPECT_FALSE(std::ranges::contains(
            supported_routines,
            mojom::RoutineType::kGoogleServicesConnectivity));
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

  // Destroy the SystemRoutineController.
  // Clear raw_ptr before destroying controller to avoid dangling detection.
  fake_delegate_ = nullptr;
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

TEST_F(SystemRoutineControllerTest, CancelRoutineDtor_DirectPath) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

  fake_delegate_->set_hold_callback(true);

  auto routine_runner = std::make_unique<FakeRoutineRunner>();
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kGoogleServicesConnectivity,
      routine_runner->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(routine_runner->result.is_null());

  // Destroy the controller while GSC routine is inflight.
  // Destructor does NOT call cros_healthd cancel for direct-path routines.
  // Clear raw_ptr before destroying controller to avoid dangling detection.
  fake_delegate_ = nullptr;
  system_routine_controller_.reset();
  base::RunLoop().RunUntilIdle();

  // Verify cros_healthd was NOT called with kCancel.
  std::optional<cros_healthd::FakeCrosHealthd::RoutineUpdateParams>
      update_params =
          cros_healthd::FakeCrosHealthd::Get()->GetRoutineUpdateParams();
  EXPECT_FALSE(update_params.has_value());
}

TEST_F(SystemRoutineControllerTest, RunRoutineCount0) {
  base::HistogramTester histogram_tester;

  fake_delegate_ = nullptr;
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

  fake_delegate_ = nullptr;
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
          mojom::StandardRoutineResult::kTestPassed),
      /*details=*/std::nullopt)));
}

// Covers all verdict branches (kNoProblem, kNotRun, kProblem).
TEST_F(SystemRoutineControllerTest, GoogleServicesConnectivity_VerdictMapping) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

  // Helper: run a GSC routine with the given verdict and empty problems,
  // then assert the result is a simple_result matching `expected`.
  auto run_and_expect_simple =
      [&](chromeos::network_diagnostics::mojom::RoutineVerdict verdict,
          mojom::StandardRoutineResult expected) {
        SCOPED_TRACE(testing::PrintToString(verdict));
        fake_delegate_->SetGoogleServicesConnectivityResult(
            MakeGoogleServicesConnectivityResult(verdict, {}));

        FakeRoutineRunner runner;
        system_routine_controller_->RunRoutine(
            mojom::RoutineType::kGoogleServicesConnectivity,
            runner.receiver.BindNewPipeAndPassRemote());
        base::RunLoop().RunUntilIdle();

        ASSERT_FALSE(runner.result.is_null());
        ASSERT_TRUE(runner.result->result->is_simple_result());
        EXPECT_EQ(expected, runner.result->result->get_simple_result());
      };

  run_and_expect_simple(
      chromeos::network_diagnostics::mojom::RoutineVerdict::kNoProblem,
      mojom::StandardRoutineResult::kTestPassed);
  run_and_expect_simple(
      chromeos::network_diagnostics::mojom::RoutineVerdict::kNotRun,
      mojom::StandardRoutineResult::kUnableToRun);
  run_and_expect_simple(
      chromeos::network_diagnostics::mojom::RoutineVerdict::kProblem,
      mojom::StandardRoutineResult::kTestFailed);
}

// Covers null result, network pipe disconnect, and runner disconnect.
TEST_F(SystemRoutineControllerTest, GoogleServicesConnectivity_ErrorHandling) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

  {
    SCOPED_TRACE("Null result from delegate");

    FakeRoutineRunner runner;
    CallOnDirectNetworkRoutineResult(
        mojom::RoutineType::kGoogleServicesConnectivity,
        /*result=*/nullptr, runner.receiver.BindNewPipeAndPassRemote());

    ASSERT_FALSE(runner.result.is_null());
    ASSERT_TRUE(runner.result->result->is_simple_result());
    EXPECT_EQ(mojom::StandardRoutineResult::kExecutionError,
              runner.result->result->get_simple_result());
  }

  {
    SCOPED_TRACE("Runner disconnect while inflight");

    fake_delegate_->set_hold_callback(true);

    auto routine_runner = std::make_unique<FakeRoutineRunner>();
    system_routine_controller_->RunRoutine(
        mojom::RoutineType::kGoogleServicesConnectivity,
        routine_runner->receiver.BindNewPipeAndPassRemote());
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(routine_runner->result.is_null());

    // Disconnect the RoutineRunner (simulates UI navigating away).
    routine_runner.reset();
    base::RunLoop().RunUntilIdle();

    // Fire the held delegate callback after runner disconnect.
    // Must not crash (early-return guard in
    // OnGoogleServicesConnectivityRoutineResult handles this).
    fake_delegate_->RunHeldCallback();
    base::RunLoop().RunUntilIdle();

    // Verify no crash. Verify cros_healthd was NOT called with kCancel.
    std::optional<cros_healthd::FakeCrosHealthd::RoutineUpdateParams>
        update_params =
            cros_healthd::FakeCrosHealthd::Get()->GetRoutineUpdateParams();
    EXPECT_FALSE(update_params.has_value());

    fake_delegate_->set_hold_callback(false);
  }
}

TEST_F(SystemRoutineControllerTest,
       GoogleServicesConnectivity_DisabledFlagReturnsUnableToRun) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);
  base::HistogramTester histogram_tester;

  // Feature flag is OFF. The feature gate should prevent the delegate
  // call and emit metrics via OnDirectNetworkRoutineResult.
  FakeRoutineRunner runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kGoogleServicesConnectivity,
      runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(runner.result.is_null());
  ASSERT_TRUE(runner.result->result->is_simple_result());
  EXPECT_EQ(mojom::StandardRoutineResult::kUnableToRun,
            runner.result->result->get_simple_result());

  histogram_tester.ExpectBucketCount(
      "ChromeOS.DiagnosticsUi.GoogleServicesConnectivityResult",
      mojom::StandardRoutineResult::kUnableToRun, 1);
}

TEST_F(SystemRoutineControllerTest,
       GoogleServicesConnectivity_AvailableWithoutCrosHealthd) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

  // cros_healthd reports CPU and Memory only -- NOT
  // kGoogleServicesConnectivity.
  SetAvailableRoutines({healthd::DiagnosticRoutineEnum::kCpuStress,
                        healthd::DiagnosticRoutineEnum::kMemory});

  base::RunLoop run_loop;
  system_routine_controller_->GetSupportedRoutines(base::BindLambdaForTesting(
      [&](const std::vector<mojom::RoutineType>& supported_routines) {
        // GoogleServicesConnectivity should be available based
        // on the feature flag alone, regardless of cros_healthd.
        // Exactly once -- not duplicated.
        EXPECT_EQ(1, std::ranges::count(
                         supported_routines,
                         mojom::RoutineType::kGoogleServicesConnectivity));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemRoutineControllerTest,
       GoogleServicesConnectivity_ProblemsPopulateDetails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

  fake_delegate_->SetGoogleServicesConnectivityResult(
      MakeGoogleServicesConnectivityResult(
          chromeos::network_diagnostics::mojom::RoutineVerdict::kProblem,
          MakeSingleConnectionProblem(kTestHostname, kTestErrorMessage)));

  FakeRoutineRunner runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kGoogleServicesConnectivity,
      runner.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(runner.result.is_null());
  ASSERT_TRUE(runner.result->result->is_simple_result());
  EXPECT_EQ(mojom::StandardRoutineResult::kTestFailed,
            runner.result->result->get_simple_result());

  // The details field should contain the formatted problem text.
  ASSERT_TRUE(runner.result->details.has_value());
  const std::string& details = runner.result->details.value();
  EXPECT_NE(std::string::npos, details.find(kTestHostname));
  EXPECT_NE(std::string::npos, details.find("ConnectionFailure"));
  EXPECT_NE(std::string::npos, details.find(kTestErrorMessage));
}

// Verifies that LogRoutineStarted is called before
// OnDirectNetworkRoutineResult even on sync error paths
// (feature-disabled completes the routine synchronously).
TEST_F(SystemRoutineControllerTest,
       GoogleServicesConnectivity_LogStartedBeforeCompleted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath log_path = temp_dir.GetPath().AppendASCII("routine_log");
  DiagnosticsLogController::Get()->SetRoutineLogForTesting(
      std::make_unique<RoutineLog>(log_path));

  // Feature flag is OFF. The feature-disabled path in
  // ExecuteNetworkRoutineDirect completes the routine synchronously,
  // which exposes the logging-order bug.
  FakeRoutineRunner runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kGoogleServicesConnectivity,
      runner.receiver.BindNewPipeAndPassRemote());
  task_environment()->RunUntilIdle();

  const std::string log =
      DiagnosticsLogController::Get()->GetRoutineLog().GetContentsForCategory(
          RoutineLog::RoutineCategory::kNetwork);
  const std::vector<std::string> lines = GetLogLines(log);
  ASSERT_EQ(2u, lines.size());

  std::vector<std::string> first = GetLogLineContents(lines[0]);
  EXPECT_EQ("GoogleServicesConnectivity", first[1]);
  EXPECT_EQ("Started", first[2]);

  std::vector<std::string> second = GetLogLineContents(lines[1]);
  EXPECT_EQ("GoogleServicesConnectivity", second[1]);
  // kNotRun maps to kUnableToRun which logs "Unable to run".
  EXPECT_EQ("Unable to run", second[2]);
}

// Verifies that routine detail text reaches the session log when problems
// are present in the GoogleServicesConnectivity result.
TEST_F(SystemRoutineControllerTest,
       GoogleServicesConnectivity_DetailsReachSessionLog) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kGoogleServicesConnectivityRoutine);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath log_path = temp_dir.GetPath().AppendASCII("routine_log");
  DiagnosticsLogController::Get()->SetRoutineLogForTesting(
      std::make_unique<RoutineLog>(log_path));

  fake_delegate_->SetGoogleServicesConnectivityResult(
      MakeGoogleServicesConnectivityResult(
          chromeos::network_diagnostics::mojom::RoutineVerdict::kProblem,
          MakeSingleConnectionProblem(kTestHostname, kTestErrorMessage)));

  FakeRoutineRunner runner;
  system_routine_controller_->RunRoutine(
      mojom::RoutineType::kGoogleServicesConnectivity,
      runner.receiver.BindNewPipeAndPassRemote());
  task_environment()->RunUntilIdle();

  ASSERT_FALSE(runner.result.is_null());

  const std::string log =
      DiagnosticsLogController::Get()->GetRoutineLog().GetContentsForCategory(
          RoutineLog::RoutineCategory::kNetwork);

  // The log should contain the detail text with the hostname and error.
  EXPECT_NE(std::string::npos, log.find("Details:"));
  EXPECT_NE(std::string::npos, log.find(kTestHostname));
  EXPECT_NE(std::string::npos, log.find(kTestErrorMessage));
}

}  // namespace ash::diagnostics
