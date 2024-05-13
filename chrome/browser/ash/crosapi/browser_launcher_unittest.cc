// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_launcher.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/startup/startup_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_device_ownership_waiter.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace crosapi {

namespace {
const char kPrimaryProfileEmail[] = "user@test";
}  // namespace

class BrowserLauncherTest : public testing::Test {
 public:
  BrowserLauncherTest() = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    CHECK(profile_manager_.SetUp());

    // Settings required to create startup data.
    crosapi::IdleServiceAsh::DisableForTesting();
    ash::LoginState::Initialize();
    crosapi_manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);

    browser_launcher_.set_device_ownership_waiter_for_testing(
        std::make_unique<user_manager::FakeDeviceOwnershipWaiter>());
  }

  void TearDown() override {
    crosapi_manager_.reset();
    ash::LoginState::Shutdown();
  }

 protected:
  BrowserLauncher* browser_launcher() { return &browser_launcher_; }

  void CreatePrimaryProfile() {
    const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileEmail));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id,
                                  /*set_profile_created_flag=*/false);
    profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    fake_user_manager_->SimulateUserProfileLoad(account_id);
  }

  void PrepareFilesToPreload(const base::FilePath& lacros_dir) {
    base::CreateDirectory(lacros_dir.Append("locales"));
    for (const auto& file_path : BrowserLauncher::GetPreloadFiles(lacros_dir)) {
      base::WriteFile(file_path, "dummy file");
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};

  // Required to create startup data.
  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  BrowserLauncher browser_launcher_;
};

TEST_F(BrowserLauncherTest, AdditionalParametersForLaunchParams) {
  BrowserLauncher::LaunchParamsFromBackground params;
  params.lacros_additional_args.emplace_back("--switch1");
  params.lacros_additional_args.emplace_back("--switch2=value2");

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLacrosChromeAdditionalArgs, "--foo####--switch3=value3");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLacrosChromeAdditionalEnv, "foo1=bar2####switch4=value4");

  BrowserLauncher::LaunchParams parameters(
      base::CommandLine({"/bin/sleep", "30"}), base::LaunchOptionsForTest());
  browser_launcher()->SetUpAdditionalParametersForTesting(params, parameters);

  EXPECT_TRUE(parameters.command_line.HasSwitch("switch1"));
  EXPECT_TRUE(parameters.command_line.HasSwitch("foo"));
  EXPECT_EQ(parameters.command_line.GetSwitchValueASCII("switch2"), "value2");
  EXPECT_EQ(parameters.command_line.GetSwitchValueASCII("switch3"), "value3");

  EXPECT_EQ(parameters.options.environment["foo1"], "bar2");
  EXPECT_EQ(parameters.options.environment["switch4"], "value4");

  EXPECT_EQ(parameters.command_line.GetSwitches().size(), 4u);
  EXPECT_EQ(parameters.options.environment.size(), 2u);
}

TEST_F(BrowserLauncherTest, WithoutAdditionalParametersForCommandLine) {
  BrowserLauncher::LaunchParamsFromBackground params;
  base::test::ScopedCommandLine scoped_command_line;
  BrowserLauncher::LaunchParams parameters(
      base::CommandLine({"/bin/sleep", "30"}), base::LaunchOptionsForTest());
  parameters.command_line.RemoveSwitch(
      ash::switches::kLacrosChromeAdditionalArgs);
  browser_launcher()->SetUpAdditionalParametersForTesting(params, parameters);
  EXPECT_EQ(parameters.command_line.GetSwitches().size(), 0u);
  EXPECT_EQ(parameters.options.environment.size(), 0u);
}

// A --vmodule value provided via --lacros-chrome-additional-args is preserved.
TEST_F(BrowserLauncherTest, LacrosChromeAdditionalArgsVModule) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLacrosChromeAdditionalArgs, "--vmodule=foo");

  base::ScopedFD dummy_fd1, dummy_fd2;
  base::CreateSocketPair(&dummy_fd1, &dummy_fd2);
  mojo::PlatformChannel dummy_channel;

  BrowserLauncher::LaunchParamsFromBackground bg_params;
  bg_params.logfd = std::move(dummy_fd1);
  BrowserLauncher::LaunchParams params =
      browser_launcher()->CreateLaunchParamsForTesting({}, bg_params, {}, {},
                                                       {}, dummy_channel, {});

  EXPECT_EQ(params.command_line.GetSwitchValueASCII("vmodule"), "foo");
}

TEST_F(BrowserLauncherTest, LaunchAndTriggerTerminate) {
  // We'll use a process just does nothing for 30 seconds, which is long
  // enough to stably exercise the test cases we have.
  BrowserLauncher::LaunchParams parameters(
      base::CommandLine({"/bin/sleep", "30"}), base::LaunchOptionsForTest());
  browser_launcher()->LaunchProcessForTesting(parameters);
  EXPECT_TRUE(browser_launcher()->IsProcessValid());
  EXPECT_TRUE(browser_launcher()->TriggerTerminate(/*exit_code=*/0));
  int exit_code;
  EXPECT_TRUE(
      browser_launcher()->GetProcessForTesting().WaitForExit(&exit_code));
  // -1 is expected as an `exit_code` because it is compulsorily terminated by
  // signal.
  EXPECT_EQ(exit_code, -1);

  // TODO(mayukoaiba): We should reset process in order to check by
  // EXPECT_FALSE(browser_launcher()->IsProcessValid()) whether
  // "TriggerTerminate" works properly.
}

TEST_F(BrowserLauncherTest, TerminateOnBackground) {
  // We'll use a process just does nothing for 30 seconds, which is long
  // enough to stably exercise the test cases we have.
  BrowserLauncher::LaunchParams parameters(
      base::CommandLine({"/bin/sleep", "30"}), base::LaunchOptionsForTest());
  browser_launcher()->LaunchProcessForTesting(parameters);
  ASSERT_TRUE(browser_launcher()->IsProcessValid());
  base::test::TestFuture<void> future;
  browser_launcher()->EnsureProcessTerminated(future.GetCallback(),
                                              base::Seconds(5));
  EXPECT_FALSE(browser_launcher()->IsProcessValid());
}

TEST_F(BrowserLauncherTest, BackgroundWorkPreLaunch) {
  base::ScopedTempDir lacros_dir;
  ASSERT_TRUE(lacros_dir.CreateUniqueTempDir());

  // Add feature and check if it's reflected to `params`.
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(features::kLacrosResourcesFileSharing);

  BrowserLauncher::LaunchParamsFromBackground params;
  base::test::TestFuture<void> future;
  browser_launcher()->WaitForBackgroundWorkPreLaunchForTesting(
      lacros_dir.GetPath(), /*clear_shared_resource_file=*/true,
      /*launching_at_login_screen=*/false, future.GetCallback(), params);

  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(params.enable_resource_file_sharing);
}

TEST_F(BrowserLauncherTest, BackgroundWorkPreLaunchOnLaunchingAtLoginScreen) {
  base::ScopedTempDir lacros_dir;
  ASSERT_TRUE(lacros_dir.CreateUniqueTempDir());
  // Create preload files which will be loaded on launching at login screen.
  PrepareFilesToPreload(lacros_dir.GetPath());

  // Add feature and check if it's reflected to `params`.
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(features::kLacrosResourcesFileSharing);

  BrowserLauncher::LaunchParamsFromBackground params;
  base::test::TestFuture<void> future;
  browser_launcher()->WaitForBackgroundWorkPreLaunchForTesting(
      lacros_dir.GetPath(), /*clear_shared_resource_file=*/true,
      /*launching_at_login_screen=*/true, future.GetCallback(), params);

  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(params.enable_resource_file_sharing);
}

// TODO(elkurin): Add kLacrosChromeAdditionalArgsFile unit test.

TEST_F(BrowserLauncherTest, Launch) {
  base::ScopedTempDir lacros_dir;
  ASSERT_TRUE(lacros_dir.CreateUniqueTempDir());
  // Create dummy lacros binary.
  const base::FilePath lacros_path = lacros_dir.GetPath().Append("chrome");
  base::WriteFile(lacros_path, "I am chrome binary");

  base::test::TestFuture<base::expected<BrowserLauncher::LaunchResults,
                                        BrowserLauncher::LaunchFailureReason>>
      future;

  constexpr bool launching_at_login_screen = false;
  browser_launcher()->Launch(lacros_path, launching_at_login_screen,
                             ash::standalone_browser::LacrosSelection::kRootfs,
                             /*mojo_disconneciton_cb=*/{},
                             /*is_keep_alive_enabled=*/false,
                             future.GetCallback());

  // Before adding primary profile, Launch should not proceed.
  EXPECT_FALSE(user_manager::UserManager::Get()->GetPrimaryUser());
  EXPECT_FALSE(future.IsReady());

  // Create primary profile.
  CreatePrimaryProfile();
  EXPECT_TRUE(user_manager::UserManager::Get()->GetPrimaryUser());

  // Make sure that Launch completes with success.
  EXPECT_TRUE(future.Get<0>().has_value());
}

TEST_F(BrowserLauncherTest, LaunchAtLoginScreen) {
  base::ScopedTempDir lacros_dir;
  ASSERT_TRUE(lacros_dir.CreateUniqueTempDir());
  // Create preload files which will be loaded on launching at login screen.
  // This will create chrome binary as well.
  PrepareFilesToPreload(lacros_dir.GetPath());
  const base::FilePath lacros_path = lacros_dir.GetPath().Append("chrome");

  base::test::TestFuture<base::expected<BrowserLauncher::LaunchResults,
                                        BrowserLauncher::LaunchFailureReason>>
      future;

  constexpr bool launching_at_login_screen = true;
  browser_launcher()->Launch(lacros_path, launching_at_login_screen,
                             ash::standalone_browser::LacrosSelection::kRootfs,
                             /*mojo_disconneciton_cb=*/{},
                             /*is_keep_alive_enabled=*/false,
                             future.GetCallback());

  // Make sure that Launch completes with success. In launching at login screen
  // scenario, we completes Launch flow without waiting for the primary profile.
  EXPECT_TRUE(future.Get<0>().has_value());
  EXPECT_FALSE(user_manager::UserManager::Get()->GetPrimaryUser());
}

TEST_F(BrowserLauncherTest, ResumeLaunch) {
  // Creates postlogin data pipe.
  base::ScopedFD read_postlogin_fd =
      browser_launcher()->CreatePostLoginPipeForTesting();
  ASSERT_TRUE(read_postlogin_fd.is_valid());

  // Creates a dedicated thread to read postlogin data which mocks Lacros
  // process.
  base::test::TestFuture<void> wait_read;
  base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::MayBlock()}, base::SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              [](base::ScopedFD fd) {
                base::ScopedFILE file(fdopen(fd.get(), "r"));
                std::string content;
                EXPECT_TRUE(base::ReadStreamToString(file.get(), &content));
                fd.reset();
              },
              std::move(read_postlogin_fd)),
          base::BindOnce(wait_read.GetCallback()));

  // Resume launch. This must be called after potlogin data pipe is constructed.
  base::test::TestFuture<
      base::expected<base::TimeTicks, BrowserLauncher::LaunchFailureReason>>
      future;
  browser_launcher()->ResumeLaunch(future.GetCallback());
  // On resume launch, we need to wait for device owner and primary profile to
  // be ready, so should not complete ResumeLaunch at this point.
  EXPECT_FALSE(future.IsReady());

  // Create primary profile.
  CreatePrimaryProfile();
  EXPECT_TRUE(user_manager::UserManager::Get()->GetPrimaryUser());

  // Make sure that ResumeLaunch complets with success.
  EXPECT_TRUE(future.Get<0>().has_value());

  // Wait for fd to close for clean up.
  EXPECT_TRUE(wait_read.Wait());
}

TEST_F(BrowserLauncherTest, ShutdownRequestedDuringLaunch) {
  base::test::TestFuture<base::expected<BrowserLauncher::LaunchResults,
                                        BrowserLauncher::LaunchFailureReason>>
      future;

  // To test asynchronous behavior, we assume it's not launching at login
  // screen.
  constexpr bool launching_at_login_screen = false;
  browser_launcher()->Launch(base::FilePath(), launching_at_login_screen,
                             ash::standalone_browser::LacrosSelection::kRootfs,
                             /*mojo_disconneciton_cb=*/{},
                             /*is_keep_alive_enabled=*/false,
                             future.GetCallback());
  // Shutdown is synchronous while Launch preparation is asynchronously waiting,
  // for primary profiel to be ready so Shutdown request runs earlier.
  browser_launcher()->Shutdown();

  // Create primary profile and proceed Launch.
  CreatePrimaryProfile();
  EXPECT_TRUE(user_manager::UserManager::Get()->GetPrimaryUser());

  // Launch should fail due to shutdown requested.
  EXPECT_FALSE(future.Get<0>().has_value());
  EXPECT_EQ(BrowserLauncher::LaunchFailureReason::kShutdownRequested,
            future.Get<0>().error());
}

}  // namespace crosapi
