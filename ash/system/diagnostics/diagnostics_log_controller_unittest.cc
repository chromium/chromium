// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/fake_diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/keyboard_input_log.h"
#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom-shared.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

namespace {

const char kTestSessionLogFileName[] = "test_session_log.txt";
const char kDiangosticsDirName[] = "diagnostics";
const char kTmpDiagnosticsDir[] = "/tmp/diagnostics";
const char kTestUserEmail[] = "test-user@gmail.com";
const char kFakeUserDir[] = "fake-user";

// Log headers content.
const char kRoutineLogSubsectionHeader[] = "--- Test Routines ---";
const char kSystemLogSectionHeader[] = "=== System ===";
const char kNetworkingLogSectionHeader[] = "=== Networking ===";
const char kNetworkingLogNetworkInfoHeader[] = "--- Network Info ---";
const char kNetworkingLogNetworkEventsHeader[] = "--- Network Events ---";
const char kKeyboardLogSectionHeader[] = "=== Keyboard ===";

}  // namespace

class DiagnosticsLogControllerTest : public NoSessionAshTestBase {
 public:
  DiagnosticsLogControllerTest() = default;
  DiagnosticsLogControllerTest(DiagnosticsLogControllerTest&) = delete;
  DiagnosticsLogControllerTest& operator=(DiagnosticsLogControllerTest&) =
      delete;
  ~DiagnosticsLogControllerTest() override = default;

  void SetUp() override { NoSessionAshTestBase::SetUp(); }

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

  void SimulateLockScreen() {
    DCHECK(!Shell::Get()->session_controller()->IsScreenLocked());

    Shell::Get()->session_controller()->LockScreen();
    task_environment()->RunUntilIdle();

    EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
  }

  void SimulateUnlockScreen() {
    DCHECK(Shell::Get()->session_controller()->IsScreenLocked());

    SessionInfo info;
    info.state = session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(std::move(info));
    task_environment()->RunUntilIdle();

    EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  }

  void SimulateLogoutActiveUser() {
    Shell::Get()->session_controller()->RequestSignOut();
    task_environment()->RunUntilIdle();

    EXPECT_FALSE(
        Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  }

 private:
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

TEST_F(DiagnosticsLogControllerTest, GenerateSessionString) {
  base::ScopedTempDir scoped_diagnostics_log_dir;

  EXPECT_TRUE(scoped_diagnostics_log_dir.CreateUniqueTempDir());
  const base::FilePath expected_path_regular_user =
      base::FilePath(scoped_diagnostics_log_dir.GetPath().Append(kFakeUserDir));
  SimulateUserLogin(kTestUserEmail);
  DiagnosticsLogController::Initialize(
      std::make_unique<FakeDiagnosticsBrowserDelegate>(
          expected_path_regular_user));

  // Create keyboard input log.
  KeyboardInputLog& keyboard_input_log =
      DiagnosticsLogController::Get()->GetKeyboardInputLog();
  keyboard_input_log.AddKeyboard(/*id=*/1, "internal keyboard");
  keyboard_input_log.CreateLogAndRemoveKeyboard(/*id=*/1);
  task_environment()->RunUntilIdle();

  const std::string contents =
      DiagnosticsLogController::Get()->GenerateSessionStringOnBlockingPool();
  const std::vector<std::string> log_lines = GetLogLines(contents);
  EXPECT_EQ(10u, log_lines.size());

  EXPECT_EQ(kSystemLogSectionHeader, log_lines[0]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, log_lines[1]);
  const std::string expected_no_routine_msg =
      "No routines of this type were run in the session.";
  EXPECT_EQ(expected_no_routine_msg, log_lines[2]);
  EXPECT_EQ(kNetworkingLogSectionHeader, log_lines[3]);
  EXPECT_EQ(kNetworkingLogNetworkInfoHeader, log_lines[4]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, log_lines[5]);
  EXPECT_EQ(expected_no_routine_msg, log_lines[6]);
  EXPECT_EQ(kNetworkingLogNetworkEventsHeader, log_lines[7]);
  EXPECT_EQ(kKeyboardLogSectionHeader, log_lines[8]);
}

TEST_F(DiagnosticsLogControllerTest, GenerateSessionLogOnBlockingPoolFile) {
  base::ScopedTempDir scoped_diagnostics_log_dir;

  EXPECT_TRUE(scoped_diagnostics_log_dir.CreateUniqueTempDir());
  const base::FilePath expected_path_regular_user =
      base::FilePath(scoped_diagnostics_log_dir.GetPath().Append(kFakeUserDir));
  const base::FilePath expected_diagnostics_log_path =
      expected_path_regular_user.Append(kDiangosticsDirName);
  SimulateUserLogin(kTestUserEmail);
  DiagnosticsLogController::Initialize(
      std::make_unique<FakeDiagnosticsBrowserDelegate>(
          expected_path_regular_user));

  // Create keyboard input log.
  KeyboardInputLog& keyboard_input_log =
      DiagnosticsLogController::Get()->GetKeyboardInputLog();
  keyboard_input_log.AddKeyboard(/*id=*/1, "internal keyboard");
  keyboard_input_log.CreateLogAndRemoveKeyboard(/*id=*/1);
  task_environment()->RunUntilIdle();

  const base::FilePath save_file_path = GetSessionLogPath();
  EXPECT_TRUE(DiagnosticsLogController::Get()->GenerateSessionLogOnBlockingPool(
      save_file_path));
  EXPECT_TRUE(base::PathExists(save_file_path));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(save_file_path, &contents));
  const std::vector<std::string> log_lines = GetLogLines(contents);
  EXPECT_EQ(10u, log_lines.size());

  EXPECT_EQ(kSystemLogSectionHeader, log_lines[0]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, log_lines[1]);
  const std::string expected_no_routine_msg =
      "No routines of this type were run in the session.";
  EXPECT_EQ(expected_no_routine_msg, log_lines[2]);
  EXPECT_EQ(kNetworkingLogSectionHeader, log_lines[3]);
  EXPECT_EQ(kNetworkingLogNetworkInfoHeader, log_lines[4]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, log_lines[5]);
  EXPECT_EQ(expected_no_routine_msg, log_lines[6]);
  EXPECT_EQ(kNetworkingLogNetworkEventsHeader, log_lines[7]);
  EXPECT_EQ(kKeyboardLogSectionHeader, log_lines[8]);
}

TEST_F(DiagnosticsLogControllerTest,
       GenerateWithRoutinesSessionLogOnBlockingPoolFile) {
  base::ScopedTempDir scoped_diagnostics_log_dir;

  EXPECT_TRUE(scoped_diagnostics_log_dir.CreateUniqueTempDir());
  const base::FilePath expected_path_regular_user =
      base::FilePath(scoped_diagnostics_log_dir.GetPath().Append(kFakeUserDir));
  const base::FilePath expected_diagnostics_log_path =
      expected_path_regular_user.Append(kDiangosticsDirName);
  SimulateUserLogin(kTestUserEmail);
  DiagnosticsLogController::Initialize(
      std::make_unique<FakeDiagnosticsBrowserDelegate>(
          expected_path_regular_user));
  RoutineLog& routine_log = DiagnosticsLogController::Get()->GetRoutineLog();
  routine_log.LogRoutineCancelled(mojom::RoutineType::kArcHttp);
  routine_log.LogRoutineCancelled(mojom::RoutineType::kBatteryCharge);
  task_environment()->RunUntilIdle();

  // Create keyboard input log.
  KeyboardInputLog& keyboard_input_log =
      DiagnosticsLogController::Get()->GetKeyboardInputLog();
  keyboard_input_log.AddKeyboard(/*id=*/1, "internal keyboard");
  keyboard_input_log.CreateLogAndRemoveKeyboard(/*id=*/1);
  task_environment()->RunUntilIdle();

  // Generate log file at test path.
  const base::FilePath save_file_path = GetSessionLogPath();
  EXPECT_TRUE(DiagnosticsLogController::Get()->GenerateSessionLogOnBlockingPool(
      save_file_path));
  EXPECT_TRUE(base::PathExists(save_file_path));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(save_file_path, &contents));
  const std::vector<std::string> log_lines = GetLogLines(contents);
  EXPECT_EQ(10u, log_lines.size());

  // System state and routine data.
  EXPECT_EQ(kSystemLogSectionHeader, log_lines[0]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, log_lines[1]);
  const std::string expected_canceled_routine_msg =
      "Inflight Routine Cancelled";
  auto system_routine_line = GetLogLineContents(log_lines[2]);
  EXPECT_EQ(2u, system_routine_line.size());
  EXPECT_EQ(expected_canceled_routine_msg, system_routine_line[1]);

  // Network state and routine data.
  EXPECT_EQ(kNetworkingLogSectionHeader, log_lines[3]);
  EXPECT_EQ(kNetworkingLogNetworkInfoHeader, log_lines[4]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, log_lines[5]);
  auto network_routine_line = GetLogLineContents(log_lines[6]);
  EXPECT_EQ(2u, network_routine_line.size());
  EXPECT_EQ(expected_canceled_routine_msg, network_routine_line[1]);
  EXPECT_EQ(kNetworkingLogNetworkEventsHeader, log_lines[7]);
  EXPECT_EQ(kKeyboardLogSectionHeader, log_lines[8]);
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

  SimulateKioskMode(user_manager::UserType::kKioskApp);
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateKioskMode(user_manager::UserType::kWebKioskApp);
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

  SimulateKioskMode(user_manager::UserType::kKioskApp);
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateKioskMode(user_manager::UserType::kWebKioskApp);
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateUserLogin(kTestUserEmail);
  const base::FilePath expected_path_regular_user =
      base::FilePath(kDefaultUserDir).Append(kDiangosticsDirName);
  EXPECT_EQ(expected_path_regular_user, log_base_path());

  SimulateLockScreen();
  EXPECT_EQ(expected_path_regular_user, log_base_path());

  SimulateUnlockScreen();
  EXPECT_EQ(expected_path_regular_user, log_base_path());

  SimulateLogoutActiveUser();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());
}

TEST_F(DiagnosticsLogControllerTest, LogsDeletedOnUserSignin) {
  base::ScopedTempDir scoped_dir;
  EXPECT_TRUE(scoped_dir.CreateUniqueTempDir());
  const base::FilePath expected_path_regular_user =
      base::FilePath(scoped_dir.GetPath().Append(kFakeUserDir));
  const base::FilePath expected_diagnostics_log_path =
      expected_path_regular_user.Append(kDiangosticsDirName);
  DiagnosticsLogController::Initialize(
      std::make_unique<FakeDiagnosticsBrowserDelegate>(
          expected_path_regular_user));

  // Create directory after initialize to simulate user sign in when a user ran
  // diagnostics previously.
  EXPECT_TRUE(base::CreateDirectory(expected_diagnostics_log_path));
  EXPECT_TRUE(base::PathExists(expected_diagnostics_log_path));

  // Sign in and verify the log directory is deleted.
  SimulateUserLogin(kTestUserEmail);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(expected_diagnostics_log_path));
}

TEST_F(DiagnosticsLogControllerTest, SetLogWritersUsingLogBasePath) {
  base::ScopedTempDir scoped_dir;

  EXPECT_TRUE(scoped_dir.CreateUniqueTempDir());
  const base::FilePath expected_path_regular_user =
      base::FilePath(scoped_dir.GetPath().Append(kFakeUserDir));
  const base::FilePath expected_diagnostics_log_path =
      expected_path_regular_user.Append(kDiangosticsDirName);
  SimulateUserLogin(kTestUserEmail);
  DiagnosticsLogController::Initialize(
      std::make_unique<FakeDiagnosticsBrowserDelegate>(
          expected_path_regular_user));

  // After initialize log writers exist.
  EXPECT_EQ(expected_diagnostics_log_path, log_base_path());
  NetworkingLog& networking_log =
      DiagnosticsLogController::Get()->GetNetworkingLog();
  RoutineLog& routine_log = DiagnosticsLogController::Get()->GetRoutineLog();

  // Simulate events to write files.
  const std::vector<std::string> networks{"fake_guid", "other_fake_guid"};
  networking_log.UpdateNetworkList(networks, "fake_guid");
  networking_log.UpdateNetworkState(mojom::Network::New());
  routine_log.LogRoutineCancelled(mojom::RoutineType::kDnsResolution);
  routine_log.LogRoutineCancelled(mojom::RoutineType::kCpuStress);

  // Wait for Append tasks which create the logs to complete.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(
      routine_log.GetContentsForCategory(RoutineLog::RoutineCategory::kNetwork)
          .empty());
  EXPECT_FALSE(
      routine_log.GetContentsForCategory(RoutineLog::RoutineCategory::kSystem)
          .empty());
  EXPECT_TRUE(base::PathExists(
      expected_diagnostics_log_path.Append("network_events.log")));
  EXPECT_TRUE(base::PathExists(expected_diagnostics_log_path.Append(
      "diagnostics_routines_network.log")));
  EXPECT_TRUE(base::PathExists(
      expected_diagnostics_log_path.Append("diagnostics_routines_system.log")));
}

TEST_F(DiagnosticsLogControllerTest, ClearLogDirectoryOnInitialize) {
  base::ScopedTempDir scoped_dir;
  EXPECT_TRUE(scoped_dir.CreateUniqueTempDir());
  const base::FilePath expected_path_regular_user =
      base::FilePath(scoped_dir.GetPath().Append(kFakeUserDir));
  const base::FilePath expected_diagnostics_log_path =
      expected_path_regular_user.Append(kDiangosticsDirName);
  EXPECT_TRUE(base::CreateDirectory(expected_diagnostics_log_path));
  EXPECT_TRUE(base::PathExists(expected_diagnostics_log_path));
  SimulateUserLogin(kTestUserEmail);
  DiagnosticsLogController::Initialize(
      std::make_unique<FakeDiagnosticsBrowserDelegate>(
          expected_path_regular_user));

  // Wait for delete to complete.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(expected_diagnostics_log_path));

  // Before routines updated log file does not exist.
  DiagnosticsLogController::Get()->GetRoutineLog().LogRoutineCancelled(
      mojom::RoutineType::kDnsResolution);

  // Wait for append to write logs.
  task_environment()->RunUntilIdle();
  EXPECT_EQ(expected_diagnostics_log_path, log_base_path());
  EXPECT_TRUE(base::PathExists(expected_diagnostics_log_path));
}

}  // namespace diagnostics
}  // namespace ash
