// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/app_types.h"
#include "ash/test/ash_test_base.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "components/account_id/account_id.h"
#include "components/exo/shell_surface_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/types/display_constants.h"

namespace arc {
namespace {

// If an instance is created, based on the value passed to the constructor,
// EnableARC feature is enabled/disabled in the scope.
class ScopedArcFeature {
 public:
  explicit ScopedArcFeature(bool enabled) {
    constexpr char kArcFeatureName[] = "EnableARC";
    if (enabled) {
      feature_list.InitFromCommandLine(kArcFeatureName, std::string());
    } else {
      feature_list.InitFromCommandLine(std::string(), kArcFeatureName);
    }
  }

  ScopedArcFeature(const ScopedArcFeature&) = delete;
  ScopedArcFeature& operator=(const ScopedArcFeature&) = delete;

  ~ScopedArcFeature() = default;

 private:
  base::test::ScopedFeatureList feature_list;
};

class ScopedRtVcpuFeature {
 public:
  ScopedRtVcpuFeature(bool dual_core_enabled, bool quad_core_enabled) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (dual_core_enabled)
      enabled_features.push_back(kRtVcpuDualCore);
    else
      disabled_features.push_back(kRtVcpuDualCore);

    if (quad_core_enabled)
      enabled_features.push_back(kRtVcpuQuadCore);
    else
      disabled_features.push_back(kRtVcpuQuadCore);

    feature_list.InitWithFeatures(enabled_features, disabled_features);
  }
  ~ScopedRtVcpuFeature() = default;
  ScopedRtVcpuFeature(const ScopedRtVcpuFeature&) = delete;
  ScopedRtVcpuFeature& operator=(const ScopedRtVcpuFeature&) = delete;

 private:
  base::test::ScopedFeatureList feature_list;
};

// Fake user that can be created with a specified type.
class FakeUser : public user_manager::User {
 public:
  explicit FakeUser(user_manager::UserType user_type)
      : User(AccountId::FromUserEmailGaiaId("user@test.com", "1234567890")),
        user_type_(user_type) {}

  FakeUser(const FakeUser&) = delete;
  FakeUser& operator=(const FakeUser&) = delete;

  ~FakeUser() override = default;

  // user_manager::User:
  user_manager::UserType GetType() const override { return user_type_; }

 private:
  const user_manager::UserType user_type_;
};

class ArcUtilTest : public ash::AshTestBase {
 public:
  ArcUtilTest() { ash::UpstartClient::InitializeFake(); }
  ArcUtilTest(const ArcUtilTest&) = delete;
  ArcUtilTest& operator=(const ArcUtilTest&) = delete;
  ~ArcUtilTest() override = default;

  void SetUp() override {
    ash::AshTestBase::SetUp();
    prefs::RegisterProfilePrefs(profile_prefs_.registry());
    RemoveUpstartStartStopJobFailures();
  }

  void TearDown() override { ash::AshTestBase::TearDown(); }

 protected:
  void InjectUpstartStartJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

  void InjectUpstartStopJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_stop_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

  void StartRecordingUpstartOperations() {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(
        base::BindLambdaForTesting([this](const std::string& job_name,
                                          const std::vector<std::string>& env) {
          upstart_operations_.emplace_back(job_name, true);
          return true;
        }));
    upstart_client->set_stop_job_cb(
        base::BindLambdaForTesting([this](const std::string& job_name,
                                          const std::vector<std::string>& env) {
          upstart_operations_.emplace_back(job_name, false);
          return true;
        }));
  }

  const std::vector<std::pair<std::string, bool>>& upstart_operations() const {
    return upstart_operations_;
  }

  PrefService* profile_prefs() { return &profile_prefs_; }

 private:
  void RemoveUpstartStartStopJobFailures() {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(
        ash::FakeUpstartClient::StartStopJobCallback());
    upstart_client->set_stop_job_cb(
        ash::FakeUpstartClient::StartStopJobCallback());
  }

  TestingPrefServiceSimple profile_prefs_;

  // List of upstart operations recorded. When it's "start" the boolean is set
  // to true.
  std::vector<std::pair<std::string, bool>> upstart_operations_;
};

TEST_F(ArcUtilTest, IsArcAvailable_None) {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  command_line->InitFromArgv({"", "--arc-availability=none"});
  EXPECT_FALSE(IsArcAvailable());

  // If --arc-availability flag is set to "none", even if Finch experiment is
  // turned on, ARC cannot be used.
  {
    ScopedArcFeature feature(true);
    EXPECT_FALSE(IsArcAvailable());
  }
}

// Test --arc-available with EnableARC feature combination.
TEST_F(ArcUtilTest, IsArcAvailable_Installed) {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  // If ARC is not installed, IsArcAvailable() should return false,
  // regardless of EnableARC feature.
  command_line->InitFromArgv({""});

  // Not available, by-default.
  EXPECT_FALSE(IsArcAvailable());
  EXPECT_FALSE(IsArcKioskAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_FALSE(IsArcAvailable());
    EXPECT_FALSE(IsArcKioskAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
    EXPECT_FALSE(IsArcKioskAvailable());
  }

  // If ARC is installed, IsArcAvailable() should return true when EnableARC
  // feature is set.
  command_line->InitFromArgv({"", "--arc-available"});

  // Not available, by-default, too.
  EXPECT_FALSE(IsArcAvailable());

  // ARC is available in kiosk mode if installed.
  EXPECT_TRUE(IsArcKioskAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_TRUE(IsArcAvailable());
    EXPECT_TRUE(IsArcKioskAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
    EXPECT_TRUE(IsArcKioskAvailable());
  }

  // If ARC is installed, IsArcAvailable() should return true when EnableARC
  // feature is set.
  command_line->InitFromArgv({"", "--arc-availability=installed"});

  // Not available, by-default, too.
  EXPECT_FALSE(IsArcAvailable());

  // ARC is available in kiosk mode if installed.
  EXPECT_TRUE(IsArcKioskAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_TRUE(IsArcAvailable());
    EXPECT_TRUE(IsArcKioskAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
    EXPECT_TRUE(IsArcKioskAvailable());
  }
}

TEST_F(ArcUtilTest, IsArcAvailable_OfficiallySupported) {
  // Regardless of FeatureList, IsArcAvailable() should return true.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arc"});
  EXPECT_TRUE(IsArcAvailable());
  EXPECT_TRUE(IsArcKioskAvailable());

  command_line->InitFromArgv({"", "--arc-availability=officially-supported"});
  EXPECT_TRUE(IsArcAvailable());
  EXPECT_TRUE(IsArcKioskAvailable());
}

TEST_F(ArcUtilTest, IsArcVmEnabled) {
  EXPECT_FALSE(IsArcVmEnabled());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(IsArcVmEnabled());
}

TEST_F(ArcUtilTest, GetArcAndroidSdkVersionAsInt) {
  // Make sure that the function does not crash even when /etc/lsb-release is
  // not available (e.g. unit tests) or corrupted.
  EXPECT_EQ(kMaxArcVersion, GetArcAndroidSdkVersionAsInt());
}

TEST_F(ArcUtilTest, IsArcVmRtVcpuEnabled) {
  {
    ScopedRtVcpuFeature feature(false, false);
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(2));
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(4));
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(8));
  }
  {
    ScopedRtVcpuFeature feature(true, false);
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(2));
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(4));
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(8));
  }
  {
    ScopedRtVcpuFeature feature(false, true);
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(2));
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(4));
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(8));
  }
  {
    ScopedRtVcpuFeature feature(true, true);
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(2));
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(4));
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(8));
  }
}

TEST_F(ArcUtilTest, IsArcVmUseHugePages) {
  EXPECT_FALSE(IsArcVmUseHugePages());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arcvm-use-hugepages"});
  EXPECT_TRUE(IsArcVmUseHugePages());
}

TEST_F(ArcUtilTest, IsArcVmDevConfIgnored) {
  EXPECT_FALSE(IsArcVmDevConfIgnored());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--ignore-arcvm-dev-conf"});
  EXPECT_TRUE(IsArcVmDevConfIgnored());
}

TEST_F(ArcUtilTest, GetArcVmUreadaheadMode) {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  command_line->InitFromArgv({""});
  EXPECT_EQ(ArcVmUreadaheadMode::READAHEAD, GetArcVmUreadaheadMode());

  command_line->InitFromArgv({"", "--arc-disable-ureadahead"});
  EXPECT_EQ(ArcVmUreadaheadMode::DISABLED, GetArcVmUreadaheadMode());

  command_line->InitFromArgv(
      {"", "--arc-disable-ureadahead", "--arcvm-ureadahead-mode=readahead"});
  EXPECT_EQ(ArcVmUreadaheadMode::READAHEAD, GetArcVmUreadaheadMode());

  command_line->InitFromArgv({"", "--arcvm-ureadahead-mode=readahead"});
  EXPECT_EQ(ArcVmUreadaheadMode::READAHEAD, GetArcVmUreadaheadMode());

  command_line->InitFromArgv({"", "--arcvm-ureadahead-mode=generate"});
  EXPECT_EQ(ArcVmUreadaheadMode::GENERATE, GetArcVmUreadaheadMode());

  command_line->InitFromArgv({"", "--arcvm-ureadahead-mode=disabled"});
  EXPECT_EQ(ArcVmUreadaheadMode::DISABLED, GetArcVmUreadaheadMode());
}

TEST_F(ArcUtilTest, UreadaheadDefault) {
  EXPECT_FALSE(IsUreadaheadDisabled());
}

TEST_F(ArcUtilTest, UreadaheadDisabled) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-disable-ureadahead"});
  EXPECT_TRUE(IsUreadaheadDisabled());
}

TEST_F(ArcUtilTest, HostUreadaheadGenerationDefault) {
  EXPECT_FALSE(IsHostUreadaheadGeneration());
  EXPECT_FALSE(IsUreadaheadDisabled());
}

TEST_F(ArcUtilTest, HostUreadaheadGenerationSet) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-host-ureadahead-generation"});
  EXPECT_TRUE(IsHostUreadaheadGeneration());
  EXPECT_FALSE(IsUreadaheadDisabled());
}

// TODO(hidehiko): Add test for IsArcKioskMode().
// It depends on UserManager, but a utility to inject fake instance is
// available only in chrome/. To use it in components/, refactoring is needed.

TEST_F(ArcUtilTest, IsArcOptInVerificationDisabled) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({""});
  EXPECT_FALSE(IsArcOptInVerificationDisabled());

  command_line->InitFromArgv({"", "--disable-arc-opt-in-verification"});
  EXPECT_TRUE(IsArcOptInVerificationDisabled());
}

TEST_F(ArcUtilTest, IsArcAllowedForUser) {
  TestingPrefServiceSimple local_state;
  user_manager::FakeUserManager* fake_user_manager =
      new user_manager::FakeUserManager(&local_state);
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));

  struct {
    user_manager::UserType user_type;
    bool expected_allowed;
  } const kTestCases[] = {
      {user_manager::USER_TYPE_REGULAR, true},
      {user_manager::USER_TYPE_GUEST, false},
      {user_manager::USER_TYPE_PUBLIC_ACCOUNT, true},
      {user_manager::USER_TYPE_KIOSK_APP, false},
      {user_manager::USER_TYPE_CHILD, true},
      {user_manager::USER_TYPE_ARC_KIOSK_APP, true},
      {user_manager::USER_TYPE_ACTIVE_DIRECTORY, true},
  };
  for (const auto& test_case : kTestCases) {
    const FakeUser user(test_case.user_type);
    EXPECT_EQ(test_case.expected_allowed, IsArcAllowedForUser(&user))
        << "User type=" << test_case.user_type;
  }

  // An ephemeral user is a logged in user but unknown to UserManager when
  // ephemeral policy is set.
  fake_user_manager->SetEphemeralModeConfig(
      user_manager::UserManager::EphemeralModeConfig(
          /* included_by_default= */ true,
          /* include_list= */ std::vector<AccountId>{},
          /* exclude_list= */ std::vector<AccountId>{}));
  fake_user_manager->UserLoggedIn(
      AccountId::FromUserEmailGaiaId("test@test.com", "9876543210"),
      "test@test.com-hash", false /* browser_restart */, false /* is_child */);
  const user_manager::User* ephemeral_user = fake_user_manager->GetActiveUser();
  ASSERT_TRUE(ephemeral_user);
  ASSERT_TRUE(fake_user_manager->IsUserCryptohomeDataEphemeral(
      ephemeral_user->GetAccountId()));

  // Ephemeral user is also allowed for ARC.
  EXPECT_TRUE(IsArcAllowedForUser(ephemeral_user));
}

TEST_F(ArcUtilTest, ArcStartModeDefault) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  EXPECT_FALSE(ShouldArcAlwaysStart());
  EXPECT_FALSE(ShouldArcAlwaysStartWithNoPlayStore());
}

TEST_F(ArcUtilTest, ArcStartModeWithoutPlayStore) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv(
      {"", "--arc-availability=installed",
       "--arc-start-mode=always-start-with-no-play-store"});
  EXPECT_TRUE(ShouldArcAlwaysStart());
  EXPECT_TRUE(ShouldArcAlwaysStartWithNoPlayStore());
}

// Verifies that ARC manual start is activated by switch.
TEST_F(ArcUtilTest, ArcStartModeManually) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-start-mode=manual"});
  EXPECT_FALSE(ShouldArcAlwaysStart());
  EXPECT_TRUE(ShouldArcStartManually());
}

// Verifies that ARC manual start is disabled by default.
TEST_F(ArcUtilTest, ArcStartModeManuallyDisabledByDefault) {
  EXPECT_FALSE(ShouldArcAlwaysStart());
  EXPECT_FALSE(ShouldArcStartManually());
}

TEST_F(ArcUtilTest, ScaleFactorToDensity) {
  // Test all standard scale factors
  EXPECT_EQ(160, GetLcdDensityForDeviceScaleFactor(1.0f));
  EXPECT_EQ(160, GetLcdDensityForDeviceScaleFactor(1.25f));
  EXPECT_EQ(213, GetLcdDensityForDeviceScaleFactor(1.6f));
  EXPECT_EQ(240, GetLcdDensityForDeviceScaleFactor(display::kDsf_1_777));
  EXPECT_EQ(240, GetLcdDensityForDeviceScaleFactor(display::kDsf_1_8));
  EXPECT_EQ(240, GetLcdDensityForDeviceScaleFactor(2.0f));
  EXPECT_EQ(280, GetLcdDensityForDeviceScaleFactor(display::kDsf_2_252));
  EXPECT_EQ(280, GetLcdDensityForDeviceScaleFactor(2.4f));
  EXPECT_EQ(320, GetLcdDensityForDeviceScaleFactor(display::kDsf_2_666));

  // Bad scale factors shouldn't blow up.
  EXPECT_EQ(160, GetLcdDensityForDeviceScaleFactor(0.5f));
  EXPECT_EQ(160, GetLcdDensityForDeviceScaleFactor(-0.1f));
  EXPECT_EQ(180, GetLcdDensityForDeviceScaleFactor(1.5f));
  EXPECT_EQ(1200, GetLcdDensityForDeviceScaleFactor(10.f));

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-scale=280"});
  EXPECT_EQ(280, GetLcdDensityForDeviceScaleFactor(1.234f));

  command_line->InitFromArgv({"", "--arc-scale=120"});
  EXPECT_EQ(120, GetLcdDensityForDeviceScaleFactor(1.234f));

  command_line->InitFromArgv({"", "--arc-scale=abc"});
  EXPECT_EQ(240, GetLcdDensityForDeviceScaleFactor(2.0));
}

TEST_F(ArcUtilTest, ConfigureUpstartJobs_Success) {
  std::deque<JobDesc> jobs{
      JobDesc{"Job_2dA", UpstartOperation::JOB_STOP, {}},
      JobDesc{"Job_2dB", UpstartOperation::JOB_STOP_AND_START, {}},
      JobDesc{"Job_2dC", UpstartOperation::JOB_START, {}},
  };
  bool result = false;
  StartRecordingUpstartOperations();
  ConfigureUpstartJobs(
      jobs,
      base::BindLambdaForTesting(
          [&result, quit_closure = task_environment()->QuitClosure()](bool r) {
            result = r;
            quit_closure.Run();
          }));
  task_environment()->RunUntilQuit();
  EXPECT_TRUE(result);

  auto ops = upstart_operations();
  ASSERT_EQ(4u, ops.size());
  EXPECT_EQ(ops[0].first, "Job_2dA");
  EXPECT_FALSE(ops[0].second);
  EXPECT_EQ(ops[1].first, "Job_2dB");
  EXPECT_FALSE(ops[1].second);
  EXPECT_EQ(ops[2].first, "Job_2dB");
  EXPECT_TRUE(ops[2].second);
  EXPECT_EQ(ops[3].first, "Job_2dC");
  EXPECT_TRUE(ops[3].second);
}

TEST_F(ArcUtilTest, ConfigureUpstartJobs_StopFail) {
  std::deque<JobDesc> jobs{
      JobDesc{"Job_2dA", UpstartOperation::JOB_STOP, {}},
      JobDesc{"Job_2dB", UpstartOperation::JOB_STOP_AND_START, {}},
      JobDesc{"Job_2dC", UpstartOperation::JOB_START, {}},
  };
  // Confirm that failing to stop a job is ignored.
  bool result = false;
  InjectUpstartStopJobFailure("Job_2dA");
  ConfigureUpstartJobs(
      jobs,
      base::BindLambdaForTesting(
          [&result, quit_closure = task_environment()->QuitClosure()](bool r) {
            result = r;
            quit_closure.Run();
          }));
  task_environment()->RunUntilQuit();
  EXPECT_TRUE(result);

  // Do the same for the second task.
  result = false;
  InjectUpstartStopJobFailure("Job_2dB");
  ConfigureUpstartJobs(
      jobs,
      base::BindLambdaForTesting(
          [&result, quit_closure = task_environment()->QuitClosure()](bool r) {
            result = r;
            quit_closure.Run();
          }));
  task_environment()->RunUntilQuit();
  EXPECT_TRUE(result);
}

TEST_F(ArcUtilTest, ConfigureUpstartJobs_StartFail) {
  std::deque<JobDesc> jobs{
      JobDesc{"Job_2dA", UpstartOperation::JOB_STOP, {}},
      JobDesc{"Job_2dB", UpstartOperation::JOB_STOP_AND_START, {}},
      JobDesc{"Job_2dC", UpstartOperation::JOB_START, {}},
  };
  // Confirm that failing to start a job is not ignored.
  bool result = true;
  InjectUpstartStartJobFailure("Job_2dB");
  ConfigureUpstartJobs(
      jobs,
      base::BindLambdaForTesting(
          [&result, quit_closure = task_environment()->QuitClosure()](bool r) {
            result = r;
            quit_closure.Run();
          }));
  task_environment()->RunUntilQuit();
  EXPECT_FALSE(result);

  // Do the same for the third task.
  result = true;
  InjectUpstartStartJobFailure("Job_2dC");
  ConfigureUpstartJobs(
      std::move(jobs),
      base::BindLambdaForTesting(
          [&result, quit_closure = task_environment()->QuitClosure()](bool r) {
            result = r;
            quit_closure.Run();
          }));
  task_environment()->RunUntilQuit();
  EXPECT_FALSE(result);
}

TEST_F(ArcUtilTest, GetArcWindowTaskId) {
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(100, nullptr));

  exo::SetShellApplicationId(window.get(), "org.chromium.arc.100");

  {
    auto task_id = GetWindowTaskId(window.get());
    EXPECT_TRUE(task_id.has_value());
    EXPECT_EQ(task_id.value(), 100);
  }

  {
    auto session_id = GetWindowSessionId(window.get());
    EXPECT_FALSE(session_id.has_value());
  }

  {
    auto task_or_session_id = GetWindowTaskOrSessionId(window.get());
    EXPECT_TRUE(task_or_session_id.has_value());
    EXPECT_EQ(task_or_session_id.value(), 100);
  }
}

TEST_F(ArcUtilTest, GetArcWindowSessionId) {
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(200, nullptr));

  exo::SetShellApplicationId(window.get(), "org.chromium.arc.session.200");

  {
    auto task_id = GetWindowTaskId(window.get());
    EXPECT_FALSE(task_id.has_value());
  }

  {
    auto session_id = GetWindowSessionId(window.get());
    EXPECT_TRUE(session_id.has_value());
    EXPECT_EQ(session_id.value(), 200);
  }

  {
    auto task_or_session_id = GetWindowTaskOrSessionId(window.get());
    EXPECT_TRUE(task_or_session_id.has_value());
    EXPECT_EQ(task_or_session_id.value(), 200);
  }
}

TEST_F(ArcUtilTest, SetAndGetArcVmDataMigrationStatus) {
  constexpr ArcVmDataMigrationStatus statuses[] = {
      ArcVmDataMigrationStatus::kFinished,
      ArcVmDataMigrationStatus::kStarted,
      ArcVmDataMigrationStatus::kConfirmed,
      ArcVmDataMigrationStatus::kNotified,
      ArcVmDataMigrationStatus::kUnnotified,
  };
  for (const auto status : statuses) {
    SetArcVmDataMigrationStatus(profile_prefs(), status);
    EXPECT_EQ(status, GetArcVmDataMigrationStatus(profile_prefs()));
  }
}

TEST_F(ArcUtilTest, SetAndGetArcVmDataMigrationStrategy) {
  profile_prefs()->SetInteger(prefs::kArcVmDataMigrationStrategy, -1);
  EXPECT_EQ(ArcVmDataMigrationStrategy::kDoNotPrompt,
            GetArcVmDataMigrationStrategy(profile_prefs()));

  profile_prefs()->SetInteger(prefs::kArcVmDataMigrationStrategy, 0);
  EXPECT_EQ(ArcVmDataMigrationStrategy::kDoNotPrompt,
            GetArcVmDataMigrationStrategy(profile_prefs()));

  profile_prefs()->SetInteger(prefs::kArcVmDataMigrationStrategy, 1);
  EXPECT_EQ(ArcVmDataMigrationStrategy::kPrompt,
            GetArcVmDataMigrationStrategy(profile_prefs()));

  profile_prefs()->SetInteger(
      prefs::kArcVmDataMigrationStrategy,
      static_cast<int>(ArcVmDataMigrationStrategy::kMaxValue) + 1);
  EXPECT_EQ(ArcVmDataMigrationStrategy::kPrompt,
            GetArcVmDataMigrationStrategy(profile_prefs()));
}

// Tests that ShouldUseVirtioBlkData() returns true when virtio-blk /data is
// enabled via the kEnableVirtioBlkForData feature.
TEST_F(ArcUtilTest, ShouldUseVirtioBlkData_VirtioBlkForDataFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableVirtioBlkForData);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kEnableArcVmDataMigration));
  EXPECT_TRUE(ShouldUseVirtioBlkData(profile_prefs()));
}

// Tests that ShouldUseVirtioBlkData() returns false when ARCVM /data is enabled
// but the user has not been notified yet.
TEST_F(ArcUtilTest, ShouldUseVirtioBlkData_ArcVmDataMigration_Unnotified) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  SetArcVmDataMigrationStatus(profile_prefs(),
                              ArcVmDataMigrationStatus::kUnnotified);
  EXPECT_FALSE(ShouldUseVirtioBlkData(profile_prefs()));
}

// Tests that ShouldUseVirtioBlkData() returns false when ARCVM /data is enabled
// but the user has just been notified of its availability.
TEST_F(ArcUtilTest, ShouldUseVirtioBlkData_ArcVmDataMigration_Notified) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  SetArcVmDataMigrationStatus(profile_prefs(),
                              ArcVmDataMigrationStatus::kNotified);
  EXPECT_FALSE(ShouldUseVirtioBlkData(profile_prefs()));
}

// Tests that ShouldUseVirtioBlkData() returns false when ARCVM /data is enabled
// but the user has just confirmed the migration.
TEST_F(ArcUtilTest, ShouldUseVirtioBlkData_ArcVmDataMigration_Confirmed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  SetArcVmDataMigrationStatus(profile_prefs(),
                              ArcVmDataMigrationStatus::kConfirmed);
  EXPECT_FALSE(ShouldUseVirtioBlkData(profile_prefs()));
}

// Tests that ShouldUseVirtioBlkData() returns false when ARCVM /data is enabled
// and the migration has started, but not finished yet.
TEST_F(ArcUtilTest, ShouldUseVirtioBlkData_ArcVmDataMigration_Started) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  SetArcVmDataMigrationStatus(profile_prefs(),
                              ArcVmDataMigrationStatus::kStarted);
  EXPECT_FALSE(ShouldUseVirtioBlkData(profile_prefs()));
}

// Tests that ShouldUseVirtioBlkData() returns true when ARCVM /data is enabled
// and the migration has finished.
TEST_F(ArcUtilTest, ShouldUseVirtioBlkData_ArcVmDataMigration_Finished) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  SetArcVmDataMigrationStatus(profile_prefs(),
                              ArcVmDataMigrationStatus::kFinished);
  EXPECT_TRUE(ShouldUseVirtioBlkData(profile_prefs()));
}

// Tests that GetDaysUntilArcVmDataMigrationDeadline() returns the correct value
// when it is called just after the ARCVM /data migration notification is shown
// for the first time.
TEST_F(ArcUtilTest,
       GetDaysUntilArcVmDataMigrationDeadline_JustAfterFirstNotification) {
  profile_prefs()->SetTime(prefs::kArcVmDataMigrationNotificationFirstShownTime,
                           base::Time::Now());
  const int days_until_deadline =
      GetDaysUntilArcVmDataMigrationDeadline(profile_prefs());
  // Take into account cases where the test is executed around midnight.
  EXPECT_TRUE(
      days_until_deadline == kArcVmDataMigrationNumberOfDismissibleDays ||
      days_until_deadline == (kArcVmDataMigrationNumberOfDismissibleDays - 1))
      << "days_until_deadline = " << days_until_deadline;
}

// Tests that GetDaysUntilArcVmDataMigrationDeadline() returns the correct value
// when it is called after kArcVmDataMigrationDismissibleTimeDelta has passed.
TEST_F(ArcUtilTest, GetDaysUntilArcVmDataMigrationDeadline_JustAfterDeadline) {
  // Remaining days should be 1 (i.e., the migration should be done today).
  profile_prefs()->SetTime(
      prefs::kArcVmDataMigrationNotificationFirstShownTime,
      base::Time::Now() - kArcVmDataMigrationDismissibleTimeDelta);
  EXPECT_EQ(GetDaysUntilArcVmDataMigrationDeadline(profile_prefs()), 1);
}

// Tests that GetDaysUntilArcVmDataMigrationDeadline() returns the correct value
// when it is called after more days than kArcVmDataMigrationDismissibleDays
// have passed.
TEST_F(ArcUtilTest, GetDaysUntilArcVmDataMigrationDeadline_Overdue) {
  // Remaining days should be kept 1.
  profile_prefs()->SetTime(
      prefs::kArcVmDataMigrationNotificationFirstShownTime,
      base::Time::Now() -
          (kArcVmDataMigrationDismissibleTimeDelta + base::Days(10)));
  EXPECT_EQ(GetDaysUntilArcVmDataMigrationDeadline(profile_prefs()), 1);
}

// Tests that GetDaysUntilArcVmDataMigrationDeadline() returns the correct value
// when the migration is in progress.
TEST_F(ArcUtilTest, GetDaysUntilArcVmDataMigrationDeadline_MigrationStarted) {
  SetArcVmDataMigrationStatus(profile_prefs(),
                              ArcVmDataMigrationStatus::kStarted);
  profile_prefs()->SetTime(prefs::kArcVmDataMigrationNotificationFirstShownTime,
                           base::Time::Now());
  EXPECT_EQ(GetDaysUntilArcVmDataMigrationDeadline(profile_prefs()), 1);
}

TEST_F(ArcUtilTest, GetDesiredDiskImageSizeForArcVmDataMigrationInBytes) {
  EXPECT_EQ(GetDesiredDiskImageSizeForArcVmDataMigrationInBytes(0, 0),
            4ULL << 30 /* kMinimumDiskImageSizeInBytes = 4 GB */);

  EXPECT_EQ(GetDesiredDiskImageSizeForArcVmDataMigrationInBytes(
                4ULL << 30 /* android_data_size_in_bytes = 4 GB */,
                32ULL << 30 /* free_disk_space_in_bytes = 32 GB */),
            35782443008ULL /* ~33 GB */);

  EXPECT_EQ(GetDesiredDiskImageSizeForArcVmDataMigrationInBytes(
                32ULL << 30 /* android_data_size_in_bytes = 32 GB */,
                4ULL << 30 /* free_disk_space_in_bytes = 4 GB */),
            41795399680ULL /* ~39 GB */);
}

TEST_F(ArcUtilTest, GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes) {
  EXPECT_EQ(GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(0, 0),
            1ULL << 30 /* kMinimumRequiredFreeDiskSpaceInBytes = 1 GB */);

  EXPECT_EQ(GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(
                4ULL << 30 /* android_data_size_in_bytes = 4 GB */,
                32ULL << 30 /* free_disk_space_in_bytes = 32 GB */),
            3ULL * (512ULL << 20) /* 1.5 GB */);

  EXPECT_EQ(GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(
                32ULL << 30 /* android_data_size_in_bytes = 32 GB */,
                4ULL << 30 /* free_disk_space_in_bytes = 4 GB */),
            4ULL << 30 /* 4 GB */);
}

}  // namespace
}  // namespace arc
