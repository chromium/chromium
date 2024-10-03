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
#include "ash/constants/ash_switches.h"
#include "ash/test/ash_test_base.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
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
    feature_list.InitWithFeatureStates({{kRtVcpuDualCore, dual_core_enabled},
                                        {kRtVcpuQuadCore, quad_core_enabled}});
  }
  ~ScopedRtVcpuFeature() = default;
  ScopedRtVcpuFeature(const ScopedRtVcpuFeature&) = delete;
  ScopedRtVcpuFeature& operator=(const ScopedRtVcpuFeature&) = delete;

 private:
  base::test::ScopedFeatureList feature_list;
};

class ArcUtilTest : public ash::AshTestBase {
 public:
  ArcUtilTest() {
    ash::ConciergeClient::InitializeFake();
    ash::UpstartClient::InitializeFake();
  }
  ArcUtilTest(const ArcUtilTest&) = delete;
  ArcUtilTest& operator=(const ArcUtilTest&) = delete;
  ~ArcUtilTest() override {
    ash::UpstartClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

  void SetUp() override {
    ash::AshTestBase::SetUp();
    prefs::RegisterProfilePrefs(profile_prefs_.registry());
  }

  void TearDown() override { ash::AshTestBase::TearDown(); }

 protected:
  void InjectUpstartStartJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return ash::FakeUpstartClient::StartJobResult(job_name !=
                                                        job_name_to_fail);
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

  PrefService* profile_prefs() { return &profile_prefs_; }

 private:
  TestingPrefServiceSimple profile_prefs_;
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

  {
    ScopedArcFeature feature(true);
    EXPECT_FALSE(IsArcAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
  }

  // If ARC is installed, IsArcAvailable() should return true when EnableARC
  // feature is set.
  command_line->InitFromArgv({"", "--arc-available"});

  // Not available, by-default, too.
  EXPECT_FALSE(IsArcAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_TRUE(IsArcAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
  }

  // If ARC is installed, IsArcAvailable() should return true when EnableARC
  // feature is set.
  command_line->InitFromArgv({"", "--arc-availability=installed"});

  // Not available, by-default, too.
  EXPECT_FALSE(IsArcAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_TRUE(IsArcAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
  }
}

TEST_F(ArcUtilTest, IsArcAvailable_OfficiallySupported) {
  // Regardless of FeatureList, IsArcAvailable() should return true.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arc"});
  EXPECT_TRUE(IsArcAvailable());

  command_line->InitFromArgv({"", "--arc-availability=officially-supported"});
  EXPECT_TRUE(IsArcAvailable());
}

TEST_F(ArcUtilTest, IsArcVmEnabled) {
  EXPECT_FALSE(IsArcVmEnabled());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(IsArcVmEnabled());
}

TEST_F(ArcUtilTest, IsArcVmDlcEnabled) {
  EXPECT_FALSE(IsArcVmDlcEnabled());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm-dlc"});
  EXPECT_TRUE(IsArcVmDlcEnabled());
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

TEST_F(ArcUtilTest, GetArcUreadaheadModeVmSwitch) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  const char* mode = ash::switches::kArcVmUreadaheadMode;

  command_line->InitFromArgv({""});
  EXPECT_EQ(ArcUreadaheadMode::READAHEAD, GetArcUreadaheadMode(mode));

  command_line->InitFromArgv({"", "--arcvm-ureadahead-mode=readahead"});
  EXPECT_EQ(ArcUreadaheadMode::READAHEAD, GetArcUreadaheadMode(mode));

  command_line->InitFromArgv({"", "--arcvm-ureadahead-mode=generate"});
  EXPECT_EQ(ArcUreadaheadMode::GENERATE, GetArcUreadaheadMode(mode));

  command_line->InitFromArgv({"", "--arcvm-ureadahead-mode=disabled"});
  EXPECT_EQ(ArcUreadaheadMode::DISABLED, GetArcUreadaheadMode(mode));
}

TEST_F(ArcUtilTest, GetArcUreadaheadModeContainerSwitch) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  const char* mode = ash::switches::kArcHostUreadaheadMode;

  command_line->InitFromArgv({""});
  EXPECT_EQ(ArcUreadaheadMode::READAHEAD, GetArcUreadaheadMode(mode));

  command_line->InitFromArgv({"", "--arc-host-ureadahead-mode=readahead"});
  EXPECT_EQ(ArcUreadaheadMode::READAHEAD, GetArcUreadaheadMode(mode));

  command_line->InitFromArgv({"", "--arc-host-ureadahead-mode=generate"});
  EXPECT_EQ(ArcUreadaheadMode::GENERATE, GetArcUreadaheadMode(mode));

  command_line->InitFromArgv({"", "--arc-host-ureadahead-mode=disabled"});
  EXPECT_EQ(ArcUreadaheadMode::DISABLED, GetArcUreadaheadMode(mode));
}

TEST_F(ArcUtilTest, UseDevCachesDefault) {
  EXPECT_FALSE(IsArcUseDevCaches());
}

TEST_F(ArcUtilTest, UseDevCachesSet) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-use-dev-caches"});
  EXPECT_TRUE(IsArcUseDevCaches());
}

TEST_F(ArcUtilTest, IsArcOptInVerificationDisabled) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({""});
  EXPECT_FALSE(IsArcOptInVerificationDisabled());

  command_line->InitFromArgv({"", "--disable-arc-opt-in-verification"});
  EXPECT_TRUE(IsArcOptInVerificationDisabled());
}

TEST_F(ArcUtilTest, IsArcAllowedForUser) {
  TestingPrefServiceSimple local_state;
  ash::ScopedStubInstallAttributes install_attributes(
      ash::StubInstallAttributes::CreateCloudManaged("test-domain",
                                                     "FAKE_DEVICE_ID"));
  user_manager::TypedScopedUserManager fake_user_manager(
      std::make_unique<user_manager::FakeUserManager>(&local_state));

  EXPECT_TRUE(IsArcAllowedForUser(fake_user_manager->AddUser(
      AccountId::FromUserEmailGaiaId("user1@test.com", "1234567890-1"))));
  EXPECT_FALSE(IsArcAllowedForUser(fake_user_manager->AddGuestUser(
      AccountId::FromUserEmailGaiaId("user2@test.com", "1234567890-2"))));
  EXPECT_TRUE(IsArcAllowedForUser(fake_user_manager->AddPublicAccountUser(
      AccountId::FromUserEmailGaiaId("user3@test.com", "1234567890-3"))));
  EXPECT_FALSE(IsArcAllowedForUser(fake_user_manager->AddKioskAppUser(
      AccountId::FromUserEmailGaiaId("user4@test.com", "1234567890-4"))));
  EXPECT_TRUE(IsArcAllowedForUser(fake_user_manager->AddChildUser(
      AccountId::FromUserEmailGaiaId("user5@test.com", "1234567890-5"))));

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
      JobDesc{"arcvm_2dinstall_2dandroid_2dimage_2ddlc",
              UpstartOperation::JOB_START,
              {}},
  };
  bool result = false;
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  ConfigureUpstartJobs(
      jobs,
      base::BindLambdaForTesting(
          [&result, quit_closure = task_environment()->QuitClosure()](bool r) {
            result = r;
            quit_closure.Run();
          }));
  task_environment()->RunUntilQuit();
  EXPECT_TRUE(result);

  auto ops = ash::FakeUpstartClient::Get()->upstart_operations();
  ASSERT_EQ(5u, ops.size());
  EXPECT_EQ(ops[0].name, "Job_2dA");
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[1].name, "Job_2dB");
  EXPECT_EQ(ops[1].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[2].name, "Job_2dB");
  EXPECT_EQ(ops[2].type, ash::FakeUpstartClient::UpstartOperationType::START);
  EXPECT_EQ(ops[3].name, "Job_2dC");
  EXPECT_EQ(ops[3].type, ash::FakeUpstartClient::UpstartOperationType::START);
  EXPECT_EQ(ops[4].name, "arcvm_2dinstall_2dandroid_2dimage_2ddlc");
  EXPECT_EQ(ops[4].type, ash::FakeUpstartClient::UpstartOperationType::START);
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
  EXPECT_EQ(GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(0, 0, 0),
            1ULL << 30 /* kMinimumRequiredFreeDiskSpaceInBytes = 1 GB */);

  EXPECT_EQ(GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(
                4ULL << 30 /* android_data_size_src_in_bytes = 4 GB */,
                4ULL << 30 /* android_data_size_dest_in_bytes = 4 GB */,
                32ULL << 30 /* free_disk_space_in_bytes = 32 GB */),
            3ULL * (512ULL << 20) /* 1.5 GB */);

  EXPECT_EQ(GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(
                32ULL << 30 /* android_data_size_src_in_bytes = 32 GB */,
                32ULL << 30 /* android_data_size_dest_in_bytes = 32 GB */,
                4ULL << 30 /* free_disk_space_in_bytes = 4 GB */),
            4ULL << 30 /* 4 GB */);

  EXPECT_EQ(GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(
                33ULL << 30 /* android_data_size_src_in_bytes = 33 GB */,
                32ULL << 30 /* android_data_size_dest_in_bytes = 32 GB */,
                4ULL << 30 /* free_disk_space_in_bytes = 4 GB */),
            4ULL << 30 /* 4 GB */);

  EXPECT_EQ(GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(
                16ULL << 30 /* android_data_size_src_in_bytes = 16 GB */,
                32ULL << 30 /* android_data_size_dest_in_bytes = 32 GB */,
                4ULL << 30 /* free_disk_space_in_bytes = 4 GB */),
            20ULL << 30 /* 20 GB */);
}

// Checks that the callback is invoked with false when ARCVM is not stopped.
TEST_F(ArcUtilTest, EnsureStaleArcVmAndArcVmUpstartJobsStopped_StopVmFailure) {
  ash::FakeConciergeClient::Get()->set_stop_vm_response(std::nullopt);
  base::test::TestFuture<bool> future_no_response;
  EnsureStaleArcVmAndArcVmUpstartJobsStopped("0123456789abcdef",
                                             future_no_response.GetCallback());
  EXPECT_FALSE(future_no_response.Get());

  vm_tools::concierge::StopVmResponse stop_vm_response;
  stop_vm_response.set_success(false);
  ash::FakeConciergeClient::Get()->set_stop_vm_response(stop_vm_response);
  base::test::TestFuture<bool> future_failure;
  EnsureStaleArcVmAndArcVmUpstartJobsStopped("0123456789abcdef",
                                             future_failure.GetCallback());
  EXPECT_FALSE(future_failure.Get());
}

// Checks that the callback is invoked with true when ARCVM is stopped, and
// StopJob() is called for each of `kArcVmUpstartJobsToBeStoppedOnRestart`.
// Note that StopJob() failures are not treated as fatal; see the comment on
// ConfigureUpstartJobs().
TEST_F(ArcUtilTest, EnsureStaleArcVmAndArcVmUpstartJobsStopped_Success) {
  std::set<std::string> jobs_to_be_stopped(
      std::begin(kArcVmUpstartJobsToBeStoppedOnRestart),
      std::end(kArcVmUpstartJobsToBeStoppedOnRestart));
  ash::FakeUpstartClient::Get()->set_stop_job_cb(base::BindLambdaForTesting(
      [&jobs_to_be_stopped](const std::string& job_name,
                            const std::vector<std::string>& env) {
        jobs_to_be_stopped.erase(job_name);
        // Let StopJob() fail for some of the calls.
        return (jobs_to_be_stopped.size() % 2) == 0;
      }));

  vm_tools::concierge::StopVmResponse stop_vm_response;
  stop_vm_response.set_success(true);
  ash::FakeConciergeClient::Get()->set_stop_vm_response(stop_vm_response);

  EXPECT_EQ(ash::FakeConciergeClient::Get()->stop_vm_call_count(), 0);

  base::test::TestFuture<bool> future;
  EnsureStaleArcVmAndArcVmUpstartJobsStopped("0123456789abcdef",
                                             future.GetCallback());
  EXPECT_TRUE(future.Get());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(jobs_to_be_stopped.empty());
  EXPECT_EQ(ash::FakeConciergeClient::Get()->stop_vm_call_count(), 1);
}

TEST_F(ArcUtilTest,
       ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletionDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kDeferArcActivationUntilUserSessionStartUpTaskCompletion);

  EXPECT_FALSE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
      profile_prefs()));

  RecordFirstActivationDuringUserSessionStartUp(profile_prefs(), true);
  EXPECT_FALSE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
      profile_prefs()));

  RecordFirstActivationDuringUserSessionStartUp(profile_prefs(), false);
  EXPECT_FALSE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
      profile_prefs()));
}

TEST_F(ArcUtilTest,
       ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletionAlways) {
  std::map<std::string, std::string> params = {
      {"history_window", "0"},
      {"history_threshold", "1"},
  };
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kDeferArcActivationUntilUserSessionStartUpTaskCompletion, params);

  // ARC should be deferred always.
  EXPECT_TRUE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
      profile_prefs()));

  RecordFirstActivationDuringUserSessionStartUp(profile_prefs(), true);
  EXPECT_TRUE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
      profile_prefs()));

  RecordFirstActivationDuringUserSessionStartUp(profile_prefs(), false);
  EXPECT_TRUE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
      profile_prefs()));
}

TEST_F(ArcUtilTest,
       ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletionEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kDeferArcActivationUntilUserSessionStartUpTaskCompletion);
  constexpr int kProductionWindowSize = 5;
  constexpr int kProductionThreshold = 3;

  // First, we should wait for the session start.
  EXPECT_TRUE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
      profile_prefs()));
  for (int i = 0; i < kProductionThreshold - 1; ++i) {
    RecordFirstActivationDuringUserSessionStartUp(profile_prefs(), true);
    EXPECT_TRUE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
        profile_prefs()));
  }

  // Try to cross the threshold.
  for (int i = 0; i < kProductionWindowSize - kProductionThreshold + 1; ++i) {
    RecordFirstActivationDuringUserSessionStartUp(profile_prefs(), true);
    EXPECT_FALSE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
        profile_prefs()));
  }

  // Emulate ARC app is not launched in session start up.
  for (int i = 0; i < kProductionWindowSize - kProductionThreshold; ++i) {
    RecordFirstActivationDuringUserSessionStartUp(profile_prefs(), false);
    EXPECT_FALSE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
        profile_prefs()));
  }

  // Cross the threshold.
  RecordFirstActivationDuringUserSessionStartUp(profile_prefs(), false);
  EXPECT_TRUE(ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
      profile_prefs()));
}

}  // namespace
}  // namespace arc
