// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/diagnostics/keyboard_input_log.h"
#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom-shared.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_path_override.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

namespace {

const char kTestSessionLogFileName[] = "test_session_log.txt";
const char kDiangosticsDirName[] = "diagnostics";
const char kTmpDiagnosticsDir[] = "/tmp/diagnostics";
constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test-user@gmail.com",
                                            GaiaId::Literal("12345"));

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

  void SetUp() override {
    // Ash start-up already reads DIR_USER_DATA, so redirect it first. The
    // override is dropped again after Ash is gone, in the same order.
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    user_data_dir_override_.emplace(DIR_USER_DATA, user_data_dir_.GetPath());
    NoSessionAshTestBase::SetUp();
  }

  void TearDown() override {
    NoSessionAshTestBase::TearDown();
    user_data_dir_override_.reset();
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

  // Registers `kTestAccountId` with the UserManager, which is where the
  // controller reads the user's directory from, and signs them in with the
  // session controller, which is where it reads the session state from.
  // TODO(crbug.com/546860700): drop the direct
  // UserSessionTestEnvironment::LogIn() call once SimulateUserLogin() signs
  // the user in with the UserManager too.
  void LogInTestUser() {
    auto& user_session = ash_test_helper()->user_session_test_environment();
    ASSERT_TRUE(user_session.AddRegularUser(kTestAccountId));
    user_session.LogIn(kTestAccountId);
    SimulateUserLogin(kTestAccountId);
  }

  // The directory the signed-in test user's files live in, which is what the
  // controller hangs the log directory off.
  base::FilePath GetUserDir() {
    return CHECK_DEREF(
               user_manager::UserManager::Get()->FindUser(kTestAccountId))
        .GetPath()
        .value();
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
  base::ScopedTempDir user_data_dir_;
  std::optional<base::ScopedPathOverride> user_data_dir_override_;
};

TEST_F(DiagnosticsLogControllerTest,
       ShellProvidesControllerWhenFeatureEnabled) {
  EXPECT_NO_FATAL_FAILURE(DiagnosticsLogController::Get());
  EXPECT_NE(nullptr, DiagnosticsLogController::Get());
}

TEST_F(DiagnosticsLogControllerTest, IsInitializedAfterInitializeCalled) {
  EXPECT_NE(nullptr, DiagnosticsLogController::Get());
  EXPECT_FALSE(DiagnosticsLogController::IsInitialized());

  DiagnosticsLogController::Initialize();
  EXPECT_TRUE(DiagnosticsLogController::IsInitialized());
}

TEST_F(DiagnosticsLogControllerTest, GenerateSessionLogOnBlockingPoolFile) {
  LogInTestUser();
  DiagnosticsLogController::Initialize();

  // Create keyboard input log.
  KeyboardInputLog& keyboard_input_log =
      DiagnosticsLogController::Get()->GetKeyboardInputLog();
  keyboard_input_log.AddKeyboard(/*id=*/1, "internal keyboard");
  keyboard_input_log.CreateLogAndRemoveKeyboard(/*id=*/1);
  task_environment()->RunUntilIdle();

  const base::FilePath save_file_path = GetSessionLogPath();

  // Extract data on UI thread and pass to static blocking pool method
  auto log_data = DiagnosticsLogController::Get()->GetSessionLogData();
  EXPECT_TRUE(DiagnosticsLogController::GenerateSessionLogOnBlockingPool(
      save_file_path, std::move(log_data)));

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
  LogInTestUser();
  DiagnosticsLogController::Initialize();
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

  // Extract data on UI thread and pass to static blocking pool method
  auto log_data = DiagnosticsLogController::Get()->GetSessionLogData();
  EXPECT_TRUE(DiagnosticsLogController::GenerateSessionLogOnBlockingPool(
      save_file_path, std::move(log_data)));

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
  // Simulate called before Initialize().
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());
  DiagnosticsLogController::Initialize();

  // Simulate sign-in user.
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateGuestLogin();
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  ClearLogin();
  SimulateKioskMode(user_manager::UserType::kKioskChromeApp);
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  ClearLogin();
  SimulateKioskMode(user_manager::UserType::kKioskWebApp);
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());
}

TEST_F(DiagnosticsLogControllerTest,
       ResetAndInitializeForShouldLookupProfileUserNonEmptyPath) {
  DiagnosticsLogController::Initialize();
  LogInTestUser();
  DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  EXPECT_EQ(GetUserDir().Append(kDiangosticsDirName), log_base_path());
}

TEST_F(DiagnosticsLogControllerTest,
       LogBaseCorrectlyUpdatedOnActiveUserSessionChanged) {
  const base::FilePath expected_path_not_regular_user =
      base::FilePath(kTmpDiagnosticsDir);
  DiagnosticsLogController::Initialize();

  // Simulate sign-in user.
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  SimulateGuestLogin();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  ClearLogin();
  SimulateKioskMode(user_manager::UserType::kKioskChromeApp);
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  ClearLogin();
  SimulateKioskMode(user_manager::UserType::kKioskWebApp);
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());

  ClearLogin();
  LogInTestUser();
  const base::FilePath expected_path_regular_user =
      GetUserDir().Append(kDiangosticsDirName);
  EXPECT_EQ(expected_path_regular_user, log_base_path());

  SimulateLockScreen();
  EXPECT_EQ(expected_path_regular_user, log_base_path());

  SimulateUnlockScreen();
  EXPECT_EQ(expected_path_regular_user, log_base_path());

  SimulateLogoutActiveUser();
  EXPECT_EQ(expected_path_not_regular_user, log_base_path());
}

TEST_F(DiagnosticsLogControllerTest, LogsDeletedOnUserSignin) {
  DiagnosticsLogController::Initialize();

  // Create directory after initialize to simulate user sign in when a user ran
  // diagnostics previously.
  // The user's directory only gets a name when they sign in, and the point
  // here is that the stale directory is already there when they do, so spell
  // out what User::GetPath() is about to report.
  const base::FilePath expected_diagnostics_log_path =
      base::PathService::CheckedGet(DIR_USER_DATA)
          .Append(user_manager::User::GetDirName(
              user_manager::TestHelper::GetFakeUsernameHash(kTestAccountId)))
          .Append(kDiangosticsDirName);
  EXPECT_TRUE(base::CreateDirectory(expected_diagnostics_log_path));
  EXPECT_TRUE(base::PathExists(expected_diagnostics_log_path));

  // Sign in and verify the log directory is deleted.
  LogInTestUser();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(expected_diagnostics_log_path));
}

TEST_F(DiagnosticsLogControllerTest, SetLogWritersUsingLogBasePath) {
  LogInTestUser();
  const base::FilePath expected_diagnostics_log_path =
      GetUserDir().Append(kDiangosticsDirName);
  DiagnosticsLogController::Initialize();

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
  LogInTestUser();
  const base::FilePath expected_diagnostics_log_path =
      GetUserDir().Append(kDiangosticsDirName);
  EXPECT_TRUE(base::CreateDirectory(expected_diagnostics_log_path));
  EXPECT_TRUE(base::PathExists(expected_diagnostics_log_path));
  DiagnosticsLogController::Initialize();

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

TEST_F(DiagnosticsLogControllerTest,
       GenerateSessionLogConcurrentWithResetLogWriters) {
  LogInTestUser();
  DiagnosticsLogController::Initialize();

  const base::FilePath save_file_path = GetSessionLogPath();

  // Seed on-disk content.
  RoutineLog& routine_log = DiagnosticsLogController::Get()->GetRoutineLog();
  routine_log.LogRoutineCancelled(mojom::RoutineType::kArcHttp);
  task_environment()->RunUntilIdle();

  // Spam concurrent task creation and object resets to prove the hybrid
  // approach completely eliminates the cross-thread Use-After-Free risk.
  for (int i = 0; i < 50; i++) {
    // 1. Grab data safely on the UI thread
    auto log_data = DiagnosticsLogController::Get()->GetSessionLogData();

    // 2. Pass it BY VALUE to the static thread pool worker
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            base::IgnoreResult(
                &DiagnosticsLogController::GenerateSessionLogOnBlockingPool),
            save_file_path, std::move(log_data)));

    // 3. Concurrently wipe the underlying Log objects from the UI thread
    DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();
  }

  // Ensure all threads execute completely without crashing.
  task_environment()->RunUntilIdle();

  // The final file should have been written successfully.
  EXPECT_TRUE(base::PathExists(save_file_path));
}

}  // namespace diagnostics
}  // namespace ash
