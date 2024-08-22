// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/account_id/account_id.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::standalone_browser::LacrosAvailability;
using crosapi::browser_util::LacrosLaunchSwitchSource;
using user_manager::User;

namespace crosapi {

namespace {
// This implementation of RAII for LacrosAvailability is to make it easy reset
// the state between runs.
class ScopedLacrosAvailabilityCache {
 public:
  explicit ScopedLacrosAvailabilityCache(
      LacrosAvailability lacros_launch_switch) {
    SetLacrosAvailability(lacros_launch_switch);
  }
  ScopedLacrosAvailabilityCache(const ScopedLacrosAvailabilityCache&) = delete;
  ScopedLacrosAvailabilityCache& operator=(
      const ScopedLacrosAvailabilityCache&) = delete;
  ~ScopedLacrosAvailabilityCache() {
    browser_util::ClearLacrosAvailabilityCacheForTest();
    ash::standalone_browser::BrowserSupport::Shutdown();
  }

 private:
  void SetLacrosAvailability(LacrosAvailability lacros_availability) {
    policy::PolicyMap policy;
    policy.Set(
        policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value(GetLacrosAvailabilityPolicyName(lacros_availability)),
        /*external_data_fetcher=*/nullptr);
    browser_util::CacheLacrosAvailability(policy);
    if (ash::standalone_browser::BrowserSupport::
            IsInitializedForPrimaryUser()) {
      ash::standalone_browser::BrowserSupport::Shutdown();
    }
    ash::standalone_browser::BrowserSupport::InitializeForPrimaryUser(
        policy, false, false);
  }
};

}  // namespace

class BrowserUtilTest : public testing::Test {
 public:
  BrowserUtilTest() = default;
  ~BrowserUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void TearDown() override {
    if (ash::standalone_browser::BrowserSupport::
            IsInitializedForPrimaryUser()) {
      ash::standalone_browser::BrowserSupport::Shutdown();
    }
    fake_user_manager_.Reset();
    ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(
        std::nullopt);
  }

  const user_manager::User* AddRegularUser(const std::string& email,
                                           bool login = true) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    user_manager::KnownUser(fake_user_manager_->GetLocalState())
        .SaveKnownUser(account_id);
    if (login) {
      fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                       /*browser_restart=*/false,
                                       /*is_child=*/false);
      ash::standalone_browser::BrowserSupport::InitializeForPrimaryUser(
          policy::PolicyMap(), false, false);
    }
    return user;
  }

  TestingPrefServiceSimple* local_state() { return local_state_.Get(); }

  content::BrowserTaskEnvironment task_environment_;
  // Set up local_state of BrowserProcess before initializing
  // FakeChromeUserManager, since its ctor injects local_state to the instance.
  // This is to inject local_state to functions in browser_util, because
  // some of them depend on local_state returned from UserManager.
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
};

TEST_F(BrowserUtilTest, LacrosDisallowedByCommandLineFlag) {
  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisallowLacros);
  AddRegularUser("user@test.com");
  EXPECT_FALSE(browser_util::IsLacrosAllowedToBeEnabled());
}

TEST_F(BrowserUtilTest, LacrosDisableDisallowedLacrosByCommandLineFlag) {
  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisallowLacros);
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisableDisallowLacros);
  AddRegularUser("user@test.com");
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
}

TEST_F(BrowserUtilTest, LacrosDisabledWithoutMigration) {
  const user_manager::User* const user = AddRegularUser("user@test.com");

  // Lacros is enabled only after profile migration for LacrosOnly mode.
  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnableLacrosForTesting);

    EXPECT_TRUE(browser_util::IsLacrosEnabledForMigration(
        user,
        ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));
    // Since profile migration hasn't been marked as completed, this returns
    // false.
    EXPECT_FALSE(browser_util::IsLacrosEnabled());

    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        local_state(), user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kCopy);

    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, IsLacrosEnabledForMigrationBeforePolicyInit) {
  constexpr char kUserEmail[] = "user@test.com";
  const user_manager::User* const user =
      AddRegularUser(kUserEmail, /*login=*/false);

  // Lacros is not enabled yet for profile migration to happen.
  EXPECT_FALSE(browser_util::IsLacrosEnabledForMigration(
      user,
      ash::standalone_browser::migrator_util::PolicyInitState::kBeforeInit));

  // Sets command line flag to emulate the situation where the Chrome
  // restart happens.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchASCII(
      ash::standalone_browser::kLacrosAvailabilityPolicySwitch,
      ash::standalone_browser::kLacrosAvailabilityPolicyLacrosOnly);
  cmdline->AppendSwitchASCII(ash::switches::kLoginUser, kUserEmail);

  // Policy should not enable Lacros anymore.
  EXPECT_FALSE(browser_util::IsLacrosEnabledForMigration(
      user,
      ash::standalone_browser::migrator_util::PolicyInitState::kBeforeInit));
}

TEST_F(BrowserUtilTest, LacrosEnabled) {
  const user_manager::User* const user = AddRegularUser("user@test.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);

  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, ManagedAccountLacros) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);
  const user_manager::User* const user =
      AddRegularUser("user@managedchrome.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
  }
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, BlockedForChildUser) {
  AccountId account_id = AccountId::FromUserEmail("user@test.com");
  const User* user = fake_user_manager_->AddChildUser(account_id);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/true);
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);
  EXPECT_FALSE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, AshWebBrowserEnabled) {
  const user_manager::User* const user =
      AddRegularUser("user@managedchrome.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);

  // Lacros is not allowed.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);

    EXPECT_FALSE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabledForMigration(
        user,
        ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));
  }

  // Lacros is allowed but not enabled.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabledForMigration(
        user,
        ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));
  }

  // Lacros is allowed and enabled by flag.
  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnableLacrosForTesting);
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsLacrosEnabledForMigration(
        user,
        ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));
  }

  // Lacros cannot be enabled via policy.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabledForMigration(
        user,
        ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));
  }
}

TEST_F(BrowserUtilTest, IsAshWebBrowserDisabled) {
  const user_manager::User* const user =
      AddRegularUser("user@managedchrome.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);

  // Lacros cannot be enabled via policy.
  EXPECT_FALSE(browser_util::IsLacrosEnabled());
  EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
  EXPECT_FALSE(browser_util::IsLacrosEnabledForMigration(
      user,
      ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserUtilTest, IsAshWebBrowserDisabledByFlags) {
  const user_manager::User* const user = AddRegularUser("user@test.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);
  EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
  EXPECT_TRUE(browser_util::IsLacrosEnabledForMigration(
      user,
      ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserUtilTest, LacrosOnlyBrowserByFlags) {
  const user_manager::User* const user = AddRegularUser("user@test.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);
  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  // Just setting LacrosOnly should work.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, LacrosDisabledForOldHardware) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);

  const user_manager::User* const user = AddRegularUser("user@test.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());

  ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(false);
  EXPECT_FALSE(browser_util::IsLacrosEnabled());
  ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(true);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, LacrosOnlyBrowserAllowed) {
  AddRegularUser("user@test.com");
  EXPECT_TRUE(browser_util::IsLacrosOnlyBrowserAllowed());
}

TEST_F(BrowserUtilTest, ManagedAccountLacrosPrimary) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);
  const user_manager::User* const user =
      AddRegularUser("user@managedchrome.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
    EXPECT_FALSE(browser_util::IsLacrosOnlyBrowserAllowed());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
  }

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);
    EXPECT_TRUE(browser_util::IsLacrosOnlyBrowserAllowed());
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, MetadataMissing) {
  EXPECT_FALSE(browser_util::DoesMetadataSupportNewAccountManager(nullptr));
}

TEST_F(BrowserUtilTest, MetadataMissingVersion) {
  std::string json_string = R"###(
   {
     "content": {
     },
     "metadata_version": 1
   }
  )###";
  std::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataVersionBadFormat) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "91.0.4469"
     },
     "metadata_version": 1
   }
  )###";
  std::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataOldVersion) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "91.0.4469.5"
     },
     "metadata_version": 1
   }
  )###";
  std::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataNewVersion) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "9999.0.4469.5"
     },
     "metadata_version": 1
   }
  )###";
  std::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_TRUE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}


TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlock) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kVersion = "91.0.4457";
  const std::string kContent =
      "{\"content\":{\"version\":\"" + kVersion + "\"}}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_EQ(browser_util::GetRootfsLacrosVersionMayBlock(path),
            base::Version(kVersion));
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingVersion) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "{\"content\":{}}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingContent) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "{}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingFile) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  auto bad_path = tmp_dir.GetPath().Append("file");

  EXPECT_FALSE(
      browser_util::GetRootfsLacrosVersionMayBlock(bad_path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockBadJson) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "!@#$";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, GetMigrationStatusForUser) {
  using ash::standalone_browser::migrator_util::MigrationMode;
  using browser_util::GetMigrationStatusForUser;
  using browser_util::MigrationStatus;

  const user_manager::User* const user = AddRegularUser("user@test.com");

  EXPECT_EQ(GetMigrationStatusForUser(local_state(), user),
            MigrationStatus::kLacrosNotEnabled);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);

  EXPECT_EQ(GetMigrationStatusForUser(local_state(), user),
            MigrationStatus::kUncompleted);

  {
    for (int i = 0;
         i < ash::standalone_browser::migrator_util::kMaxMigrationAttemptCount;
         i++) {
      ash::standalone_browser::migrator_util::
          UpdateMigrationAttemptCountForUser(local_state(),
                                             user->username_hash());
    }

    EXPECT_EQ(GetMigrationStatusForUser(local_state(), user),
              MigrationStatus::kMaxAttemptReached);

    ash::standalone_browser::migrator_util::ClearMigrationAttemptCountForUser(
        local_state(), user->username_hash());
  }

  {
    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        local_state(), user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kCopy);

    EXPECT_EQ(GetMigrationStatusForUser(local_state(), user),
              MigrationStatus::kCopyCompleted);

    ash::standalone_browser::migrator_util::
        ClearProfileMigrationCompletedForUser(local_state(),
                                              user->username_hash());
  }

  {
    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        local_state(), user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kMove);

    EXPECT_EQ(GetMigrationStatusForUser(local_state(), user),
              MigrationStatus::kMoveCompleted);

    ash::standalone_browser::migrator_util::
        ClearProfileMigrationCompletedForUser(local_state(),
                                              user->username_hash());
  }

  {
    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        local_state(), user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kSkipForNewUser);

    EXPECT_EQ(GetMigrationStatusForUser(local_state(), user),
              MigrationStatus::kSkippedForNewUser);

    ash::standalone_browser::migrator_util::
        ClearProfileMigrationCompletedForUser(local_state(),
                                              user->username_hash());
  }
}

TEST_F(BrowserUtilTest, IsAshBrowserSyncEnabled) {
  const user_manager::User* const user = AddRegularUser("user@random.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);

  {
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsAshBrowserSyncEnabled());
  }

  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnableLacrosForTesting);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_FALSE(browser_util::IsAshBrowserSyncEnabled());
  }
}

TEST_F(BrowserUtilTest, GetLacrosLaunchSwitchSourceNonGoogle) {
  AddRegularUser("user@random.com");

  // If LaunchSwitch is not set, the source is unknown.
  EXPECT_EQ(LacrosLaunchSwitchSource::kUnknown,
            browser_util::GetLacrosLaunchSwitchSource());

  // If the policy says UserChoice, lacros state may be set by user.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    EXPECT_EQ(LacrosLaunchSwitchSource::kPossiblySetByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }

  // The policy cannot be ignored by command line flag.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kLacrosAvailabilityIgnore);
    EXPECT_EQ(LacrosLaunchSwitchSource::kPossiblySetByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }

  // Otherwise, the LaunchSwitch is set by the policy.
  for (const auto launch_switch : {LacrosAvailability::kLacrosDisallowed,
                                   LacrosAvailability::kLacrosOnly}) {
    ScopedLacrosAvailabilityCache cache(launch_switch);
    EXPECT_EQ(LacrosLaunchSwitchSource::kForcedByPolicy,
              browser_util::GetLacrosLaunchSwitchSource())
        << static_cast<int>(launch_switch);
  }
}

TEST_F(BrowserUtilTest, GetLacrosLaunchSwitchSourceGoogle) {
  AddRegularUser("user@google.com");

  // If LaunchSwitch is not set, the source is unknown.
  EXPECT_EQ(LacrosLaunchSwitchSource::kUnknown,
            browser_util::GetLacrosLaunchSwitchSource());

  // If the policy says UserChoice, lacros state may be set by user.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    EXPECT_EQ(LacrosLaunchSwitchSource::kPossiblySetByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }

  // The policy can be ignored by command line flag.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kLacrosAvailabilityIgnore);
    EXPECT_EQ(LacrosLaunchSwitchSource::kForcedByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }
}

// Lacros availability has no effect on non-googlers
TEST_F(BrowserUtilTest, LacrosAvailabilityIgnoreNonGoogle) {
  AddRegularUser("user@random.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosAvailabilityIgnore);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
  EXPECT_FALSE(browser_util::IsLacrosAllowedToBeEnabled());
}

// Lacros availability has an effect on googlers
TEST_F(BrowserUtilTest, LacrosAvailabilityIgnoreGoogleDisableToUserChoice) {
  AddRegularUser("user@google.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosAvailabilityIgnore);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
}

// Lacros availability has an effect on googlers
TEST_F(BrowserUtilTest, LacrosAvailabilityIgnoreGoogleEnableToUserChoice) {
  const user_manager::User* const user = AddRegularUser("user@google.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosAvailabilityIgnore);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
  EXPECT_FALSE(browser_util::IsLacrosEnabled());
}

// Check that the exist configurations used for the Google rollout have the
// precisely intended side-effects.
TEST_F(BrowserUtilTest, LacrosGoogleRolloutUserChoice) {
  const user_manager::User* const user = AddRegularUser("user@google.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);

  // Lacros availability is set by policy to user choice.
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);

  // Check that Lacros is allowed, enabled, and set to lacros-only.
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
}

TEST_F(BrowserUtilTest, LacrosGoogleRolloutOnly) {
  const user_manager::User* const user = AddRegularUser("user@google.com");
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);

  // Lacros availability is set by policy to only.
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);

  // Check that Lacros is allowed, enabled, and set to lacros-only.
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
  EXPECT_TRUE(browser_util::IsLacrosEnabledForMigration(
      user,
      ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit));
}

}  // namespace crosapi
