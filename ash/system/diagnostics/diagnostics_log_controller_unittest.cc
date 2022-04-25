// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom-shared.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_manager/user_type.h"
#include "routine_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

namespace {

const char kLogFileContents[] = "Diagnostics Log";
const char kTestSessionLogFileName[] = "test_session_log.txt";
const char kDiangosticsDirName[] = "diagnostics";
const char kDefaultUserDir[] = "/fake/user-dir";
const char kTmpDiagnosticsDir[] = "/tmp/diagnostics";
const char kTestUserEmail[] = "test-user@gmail.com";

// Fake delegate used to set the expected user directory path.
class FakeDiagnosticsBrowserDelegate : public DiagnosticsBrowserDelegate {
 public:
  explicit FakeDiagnosticsBrowserDelegate(
      const base::FilePath path = base::FilePath(kDefaultUserDir))
      : active_user_dir_(path) {}
  ~FakeDiagnosticsBrowserDelegate() override = default;

  base::FilePath GetActiveUserProfileDir() override { return active_user_dir_; }

 private:
  base::FilePath active_user_dir_;
};

}  // namespace

class DiagnosticsLogControllerTest : public NoSessionAshTestBase {
 public:
  DiagnosticsLogControllerTest() = default;
  DiagnosticsLogControllerTest(DiagnosticsLogControllerTest&) = delete;
  DiagnosticsLogControllerTest& operator=(DiagnosticsLogControllerTest&) =
      delete;
  ~DiagnosticsLogControllerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        ash::features::kEnableLogControllerForDiagnosticsApp);

    NoSessionAshTestBase::SetUp();
  }

 protected:
  base::FilePath GetSessionLogPath() {
    EXPECT_TRUE(save_dir_.CreateUniqueTempDir());
    return save_dir_.GetPath().Append(kTestSessionLogFileName);
  }

  void ResetLogBasePath() {
    return DiagnosticsLogController::Get()->ResetLogBasePath();
  }

  base::FilePath log_base_path() {
    return DiagnosticsLogController::Get()->log_base_path_;
  }

  void SetBrowserDelegate(
      std::unique_ptr<DiagnosticsBrowserDelegate> delegate) {
    DiagnosticsLogController::Get()->delegate_ = std::move(delegate);
  }

  void InitializeWithFakeDelegate() {
    std::unique_ptr<DiagnosticsBrowserDelegate> delegate =
        std::make_unique<FakeDiagnosticsBrowserDelegate>();
    DiagnosticsLogController::Initialize(std::move(delegate));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir save_dir_;
};

TEST_F(DiagnosticsLogControllerTest,
       ShellProvidesControllerWhenFeatureEnabled) {
  EXPECT_NO_FATAL_FAILURE(DiagnosticsLogController::Get());
  EXPECT_NE(nullptr, DiagnosticsLogController::Get());
}

TEST_F(DiagnosticsLogControllerTest, IsInitializedAfterDelegateProvided) {
  EXPECT_NE(nullptr, DiagnosticsLogController::Get());
  EXPECT_FALSE(DiagnosticsLogController::IsInitialized());

  InitializeWithFakeDelegate();
  EXPECT_TRUE(DiagnosticsLogController::IsInitialized());
}

TEST_F(DiagnosticsLogControllerTest, GenerateSessionLogOnBlockingPoolFile) {
  const base::FilePath save_file_path = GetSessionLogPath();
  EXPECT_TRUE(DiagnosticsLogController::Get()->GenerateSessionLogOnBlockingPool(
      save_file_path));
  EXPECT_TRUE(base::PathExists(save_file_path));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(save_file_path, &contents));
  EXPECT_EQ(kLogFileContents, contents);
}

TEST_F(DiagnosticsLogControllerTest,
       ResetAndInitializeShouldNotLookupProfilePath) {
  const base::FilePath expected_path_not_regular_user =
      base::FilePath(kTmpDiagnosticsDir);
  // Simulate called before delegate configured.
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());
  InitializeWithFakeDelegate();

  // Simulate sign-in user.
  ClearLogin();
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateGuestLogin();
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateKioskMode(user_manager::UserType::USER_TYPE_KIOSK_APP);
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateKioskMode(user_manager::UserType::USER_TYPE_ARC_KIOSK_APP);
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());
}

TEST_F(DiagnosticsLogControllerTest,
       ResetAndInitializeShouldLookupProfileUserEmptyPath) {
  const base::FilePath expected_path_not_regular_user =
      base::FilePath(kTmpDiagnosticsDir);
  // Simulate DiagnosticsBrowserDelegate returning empty path.
  std::unique_ptr<DiagnosticsBrowserDelegate> delegate_with_empty_file_path =
      std::make_unique<FakeDiagnosticsBrowserDelegate>(base::FilePath());
  SetBrowserDelegate(std::move(delegate_with_empty_file_path));
  SimulateUserLogin(kTestUserEmail);
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());
}

TEST_F(DiagnosticsLogControllerTest,
       ResetAndInitializeForShouldLookupProfileUserNonEmptyPath) {
  InitializeWithFakeDelegate();
  const base::FilePath expected_path_regular_user =
      base::FilePath(kDefaultUserDir).Append(kDiangosticsDirName);
  SimulateUserLogin(kTestUserEmail);
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_regular_user, log_base_path());
}

TEST_F(DiagnosticsLogControllerTest,
       LogBaseCorrectlyUpdatedOnActiveUserSessionChanged) {
  const base::FilePath expected_path_not_regular_user =
      base::FilePath(kTmpDiagnosticsDir);
  InitializeWithFakeDelegate();

  // Simulate sign-in user.
  ClearLogin();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateGuestLogin();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateKioskMode(user_manager::UserType::USER_TYPE_KIOSK_APP);
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateKioskMode(user_manager::UserType::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateUserLogin(kTestUserEmail);
  const base::FilePath expected_path_regular_user =
      base::FilePath(kDefaultUserDir).Append(kDiangosticsDirName);
  EXPECT_EQ(expected_path_regular_user, log_base_path());
}

TEST_F(DiagnosticsLogControllerTest, SetLogWritersUsingLogBasePath) {
  base::ScopedTempDir scoped_dir;

  EXPECT_TRUE(scoped_dir.CreateUniqueTempDir());
  const base::FilePath expected_path_regular_user =
      base::FilePath(scoped_dir.GetPath().Append("fake-user"));
  const base::FilePath expected_diagnostics_log_path =
      expected_path_regular_user.Append(kDiangosticsDirName);
  SimulateUserLogin(kTestUserEmail);
  DiagnosticsLogController::Initialize(
      std::make_unique<FakeDiagnosticsBrowserDelegate>(
          expected_path_regular_user));

  // After initialize log writers exist.
  EXPECT_EQ(expected_diagnostics_log_path, log_base_path());
  NetworkingLog* networking_log =
      DiagnosticsLogController::Get()->GetNetworkingLog();
  RoutineLog* routine_log = DiagnosticsLogController::Get()->GetRoutineLog();
  TelemetryLog* telemetry_log =
      DiagnosticsLogController::Get()->GetTelemetryLog();
  EXPECT_NE(nullptr, networking_log);
  EXPECT_NE(nullptr, routine_log);
  EXPECT_NE(nullptr, telemetry_log);

  // Simulate events to write files.
  const std::vector<std::string> networks{"fake_guid", "other_fake_guid"};
  networking_log->UpdateNetworkList(networks, "fake_guid");
  networking_log->UpdateNetworkState(mojom::Network::New());
  routine_log->LogRoutineCancelled(mojom::RoutineType::kDnsResolution);
  routine_log->LogRoutineCancelled(mojom::RoutineType::kCpuStress);

  // Wait for Append tasks which create the logs to complete.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(routine_log->GetContentsForCategory("network").empty());
  EXPECT_FALSE(routine_log->GetContentsForCategory("system").empty());
  EXPECT_TRUE(base::PathExists(
      expected_diagnostics_log_path.Append("network_events.log")));
  EXPECT_TRUE(base::PathExists(expected_diagnostics_log_path.Append(
      "diagnostics_routines_network.log")));
  EXPECT_TRUE(base::PathExists(
      expected_diagnostics_log_path.Append("diagnostics_routines_system.log")));
}

}  // namespace diagnostics
}  // namespace ash
