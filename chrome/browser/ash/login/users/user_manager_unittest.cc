// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kDeviceLocalAccountId[] = "device_local_account";

AccountId CreateDeviceLocalKioskAppAccountId(const std::string& account_id) {
  return AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
      kDeviceLocalAccountId, policy::DeviceLocalAccount::TYPE_KIOSK_APP));
}

}  // namespace

static constexpr char kEmail[] = "user@example.com";

class UserManagerObserverTest : public user_manager::UserManager::Observer {
 public:
  UserManagerObserverTest() = default;

  UserManagerObserverTest(const UserManagerObserverTest&) = delete;
  UserManagerObserverTest& operator=(const UserManagerObserverTest&) = delete;

  ~UserManagerObserverTest() override = default;

  // user_manager::UserManager::Observer:
  void OnUserToBeRemoved(const AccountId& account_id) override {
    ++on_user_to_be_removed_call_count_;
    expected_account_id_ = account_id;
  }

  // user_manager::UserManager::Observer:
  void OnUserRemoved(const AccountId& account_id,
                     user_manager::UserRemovalReason reason) override {
    ++on_user_removed_call_count_;
    EXPECT_EQ(expected_account_id_, account_id);
  }

  int OnUserToBeRemovedCallCount() { return on_user_to_be_removed_call_count_; }

  int OnUserRemovedCallCount() { return on_user_removed_call_count_; }

  void ResetCallCounts() {
    on_user_to_be_removed_call_count_ = 0;
    on_user_removed_call_count_ = 0;
  }

 private:
  AccountId expected_account_id_;
  int on_user_to_be_removed_call_count_ = 0;
  int on_user_removed_call_count_ = 0;
};

class MockRemoveUserManager : public ChromeUserManagerImpl {
 public:
  MOCK_CONST_METHOD1(AsyncRemoveCryptohome, void(const AccountId&));
};

class UserManagerTest : public testing::Test {
 public:
  UserManagerTest() {
    session_type_ = extensions::ScopedCurrentFeatureSessionType(
        extensions::GetCurrentFeatureSessionType());
  }

 protected:
  void SetUp() override {
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    command_line.AppendSwitch(::switches::kTestType);
    command_line.AppendSwitch(switches::kIgnoreUserProfileMappingForTests);

    UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting();

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();

    // Populate the stub DeviceSettingsProvider with valid values.
    SetDeviceSettings(false, "", false);

    // Register an in-memory local settings instance.
    local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<FakeProfileManager>(temp_dir_.GetPath()));

    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    ResetUserManager();

    wallpaper_controller_client_ = std::make_unique<
        WallpaperControllerClientImpl>(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);
  }

  void TearDown() override {
    wallpaper_controller_client_.reset();

    // Shut down the DeviceSettingsService.
    DeviceSettingsService::Get()->UnsetSessionManager();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);

    // Unregister the in-memory local settings instance.
    local_state_.reset();

    base::RunLoop().RunUntilIdle();
    ConciergeClient::Shutdown();
  }

  ChromeUserManagerImpl* GetChromeUserManager() const {
    return static_cast<ChromeUserManagerImpl*>(
        user_manager::UserManager::Get());
  }

  bool IsEphemeralAccountId(const AccountId& account_id) const {
    return GetChromeUserManager()->IsEphemeralAccountId(account_id);
  }

  void SetEphemeralModeConfig(
      user_manager::UserManager::EphemeralModeConfig ephemeral_mode_config) {
    GetChromeUserManager()->SetEphemeralModeConfig(
        std::move(ephemeral_mode_config));
  }

  AccountId GetUserManagerOwnerId() const {
    return GetChromeUserManager()->GetOwnerAccountId();
  }

  void SetUserManagerOwnerId(const AccountId& owner_account_id) {
    GetChromeUserManager()->SetOwnerId(owner_account_id);
  }

  void ResetUserManager() {
    // Reset the UserManager singleton.
    user_manager_enabler_.reset();
    // Initialize the UserManager singleton to a fresh ChromeUserManagerImpl
    // instance.
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        ChromeUserManagerImpl::CreateChromeUserManager());

    // ChromeUserManagerImpl ctor posts a task to reload policies.
    // Also ensure that all existing ongoing user manager tasks are completed.
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<MockRemoveUserManager> CreateMockRemoveUserManager() const {
    return std::make_unique<MockRemoveUserManager>();
  }

  void SetDeviceSettings(bool ephemeral_users_enabled,
                         const std::string& owner,
                         bool supervised_users_enabled) {
    settings_helper_.SetBoolean(kAccountsPrefEphemeralUsersEnabled,
                                ephemeral_users_enabled);
    settings_helper_.SetString(kDeviceOwner, owner);
  }

  void SetDeviceLocalKioskAppAccount(
      const std::string& account_id,
      const std::string& kiosk_app_id,
      policy::DeviceLocalAccount::EphemeralMode ephemeral_mode) {
    settings_helper_.Set(
        kAccountsPrefDeviceLocalAccounts,
        base::Value(base::Value::List().Append(
            base::Value::Dict()
                .Set(kAccountsPrefDeviceLocalAccountsKeyId, account_id)
                .Set(kAccountsPrefDeviceLocalAccountsKeyType,
                     static_cast<int>(
                         policy::DeviceLocalAccount::TYPE_KIOSK_APP))
                .Set(kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                     static_cast<int>(ephemeral_mode))
                .Set(kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                     kiosk_app_id))));
  }

  void RetrieveTrustedDevicePolicies() {
    GetChromeUserManager()->RetrieveTrustedDevicePolicies();
  }

  const AccountId owner_account_id_at_invalid_domain_ =
      AccountId::FromUserEmailGaiaId("owner@invalid.domain", "1234567890");
  const AccountId account_id0_at_invalid_domain_ =
      AccountId::FromUserEmailGaiaId("user0@invalid.domain", "0123456789");
  const AccountId account_id1_at_invalid_domain_ =
      AccountId::FromUserEmailGaiaId("user1@invalid.domain", "9012345678");

 protected:
  // The call chain
  // - `ProfileRequiresPolicyUnknown`
  // - `UserManagerBase::UserLoggedIn()`
  // - `ChromeUserManagerImpl::NotifyOnLogin()`
  // - `UserSessionManager::InitNonKioskExtensionFeaturesSessionType()`
  // calls
  // `extensions::SetCurrentFeatureSessionType(FeatureSessionType::kRegular)`
  //
  // |session_type_| is used to capture the original session type during |SetUp|
  // and set it back to what it was during |TearDown|.
  std::unique_ptr<base::AutoReset<extensions::mojom::FeatureSessionType>>
      session_type_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  TestWallpaperController test_wallpaper_controller_;

  content::BrowserTaskEnvironment task_environment_;
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  ScopedCrosSettingsTestHelper settings_helper_;
  // local_state_ should be destructed after ProfileManager.
  std::unique_ptr<ScopedTestingLocalState> local_state_;

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(UserManagerTest, RetrieveTrustedDevicePolicies) {
  SetEphemeralModeConfig(user_manager::UserManager::EphemeralModeConfig(
      /* included_by_default= */ true,
      /* include_list= */ std::vector<AccountId>{},
      /* exclude_list= */ std::vector<AccountId>{}));
  SetUserManagerOwnerId(EmptyAccountId());

  SetDeviceSettings(false, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(IsEphemeralAccountId(EmptyAccountId()));

  EXPECT_EQ(GetUserManagerOwnerId(), owner_account_id_at_invalid_domain_);
}

// Tests that `UserManager` correctly parses device-wide ephemeral users policy
// by calling `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest, IsEphemeralAccountIdUsesEphemeralUsersEnabledPolicy) {
  EXPECT_FALSE(IsEphemeralAccountId(EmptyAccountId()));

  SetDeviceSettings(true, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();

  EXPECT_TRUE(IsEphemeralAccountId(EmptyAccountId()));
}

// Tests that `UserManager` correctly parses device-local accounts with
// ephemeral mode equals to `kFollowDeviceWidePolicy` by calling
// `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest,
       IsEphemeralAccountIdRespectsFollowDeviceWidePolicyEphemeralMode) {
  const AccountId account_id =
      CreateDeviceLocalKioskAppAccountId(kDeviceLocalAccountId);

  EXPECT_FALSE(IsEphemeralAccountId(account_id));

  SetDeviceSettings(true, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  SetDeviceLocalKioskAppAccount(
      kDeviceLocalAccountId, "",
      policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy);
  RetrieveTrustedDevicePolicies();
  EXPECT_TRUE(IsEphemeralAccountId(account_id));

  SetDeviceSettings(false, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();
  EXPECT_FALSE(IsEphemeralAccountId(account_id));
}

// Tests that `UserManager` correctly parses device-local accounts with
// ephemeral mode equals to `kUnset` by calling
// `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest, IsEphemeralAccountIdRespectsUnsetEphemeralMode) {
  const AccountId account_id =
      CreateDeviceLocalKioskAppAccountId(kDeviceLocalAccountId);

  EXPECT_FALSE(IsEphemeralAccountId(account_id));

  SetDeviceSettings(true, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  SetDeviceLocalKioskAppAccount(
      kDeviceLocalAccountId, "",
      policy::DeviceLocalAccount::EphemeralMode::kUnset);
  RetrieveTrustedDevicePolicies();
  EXPECT_TRUE(IsEphemeralAccountId(account_id));

  SetDeviceSettings(false, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();
  EXPECT_FALSE(IsEphemeralAccountId(account_id));
}

// Tests that `UserManager` correctly parses device-local accounts with
// ephemeral mode equals to `kDisable` by calling
// `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest, IsEphemeralAccountIdRespectsDisableEphemeralMode) {
  const AccountId account_id =
      CreateDeviceLocalKioskAppAccountId(kDeviceLocalAccountId);

  EXPECT_FALSE(IsEphemeralAccountId(account_id));

  SetDeviceSettings(true, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  SetDeviceLocalKioskAppAccount(
      kDeviceLocalAccountId, "",
      policy::DeviceLocalAccount::EphemeralMode::kDisable);
  RetrieveTrustedDevicePolicies();

  EXPECT_TRUE(IsEphemeralAccountId(EmptyAccountId()));
  EXPECT_FALSE(IsEphemeralAccountId(account_id));
}

// Tests that `UserManager` correctly parses device-local accounts with
// ephemeral mode equals to `kEnable` by calling
// `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest, IsEphemeralAccountIdRespectssEnableEphemeralMode) {
  const AccountId account_id =
      CreateDeviceLocalKioskAppAccountId(kDeviceLocalAccountId);

  EXPECT_FALSE(IsEphemeralAccountId(account_id));

  SetDeviceSettings(false, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  SetDeviceLocalKioskAppAccount(
      kDeviceLocalAccountId, "",
      policy::DeviceLocalAccount::EphemeralMode::kEnable);
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(IsEphemeralAccountId(EmptyAccountId()));
  EXPECT_TRUE(IsEphemeralAccountId(account_id));
}

TEST_F(UserManagerTest, RemoveUser) {
  std::unique_ptr<MockRemoveUserManager> user_manager =
      CreateMockRemoveUserManager();

  // Create owner account and login in.
  user_manager->UserLoggedIn(owner_account_id_at_invalid_domain_,
                             owner_account_id_at_invalid_domain_.GetUserEmail(),
                             false /* browser_restart */, false /* is_child */);

  // Create non-owner account  and login in.
  user_manager->UserLoggedIn(account_id0_at_invalid_domain_,
                             account_id0_at_invalid_domain_.GetUserEmail(),
                             false /* browser_restart */, false /* is_child */);

  ASSERT_EQ(2U, user_manager->GetUsers().size());

  // Removing logged-in account is unacceptable.
  user_manager->RemoveUser(account_id0_at_invalid_domain_,
                           user_manager::UserRemovalReason::UNKNOWN);
  EXPECT_EQ(2U, user_manager->GetUsers().size());

  // Recreate the user manager to log out all accounts.
  user_manager = CreateMockRemoveUserManager();
  UserManagerObserverTest observer_test;
  user_manager->AddObserver(&observer_test);
  ASSERT_EQ(2U, user_manager->GetUsers().size());
  ASSERT_EQ(0U, user_manager->GetLoggedInUsers().size());

  // Get a pointer to the user that will be removed.
  user_manager::User* user_to_remove = nullptr;
  for (user_manager::User* user : user_manager->GetUsers()) {
    if (user->GetAccountId() == account_id0_at_invalid_domain_) {
      user_to_remove = user;
      break;
    }
  }
  ASSERT_TRUE(user_to_remove);
  ASSERT_EQ(account_id0_at_invalid_domain_, user_to_remove->GetAccountId());

  // Removing non-owner account is acceptable.
  EXPECT_CALL(*user_manager,
              AsyncRemoveCryptohome(account_id0_at_invalid_domain_))
      .Times(1);

  // Pass the account id of the user to be removed from the user list to verify
  // that a reference to the account id will not be used after user removal.
  user_manager->RemoveUser(account_id0_at_invalid_domain_,
                           user_manager::UserRemovalReason::UNKNOWN);
  testing::Mock::VerifyAndClearExpectations(user_manager.get());
  EXPECT_EQ(1, observer_test.OnUserToBeRemovedCallCount());
  EXPECT_EQ(1, observer_test.OnUserRemovedCallCount());
  EXPECT_EQ(1U, user_manager->GetUsers().size());

  // Removing owner account is unacceptable.
  EXPECT_CALL(*user_manager,
              AsyncRemoveCryptohome(owner_account_id_at_invalid_domain_))
      .Times(0);
  observer_test.ResetCallCounts();
  user_manager->RemoveUser(owner_account_id_at_invalid_domain_,
                           user_manager::UserRemovalReason::UNKNOWN);
  testing::Mock::VerifyAndClearExpectations(user_manager.get());
  EXPECT_EQ(0, observer_test.OnUserToBeRemovedCallCount());
  EXPECT_EQ(0, observer_test.OnUserRemovedCallCount());
  EXPECT_EQ(1U, user_manager->GetUsers().size());
}

TEST_F(UserManagerTest, RemoveAllExceptOwnerFromList) {
  // System salt is needed to remove user wallpaper.
  SystemSaltGetter::Initialize();
  SystemSaltGetter::Get()->SetRawSaltForTesting(
      SystemSaltGetter::RawSalt({1, 2, 3, 4, 5, 6, 7, 8}));

  user_manager::UserManager::Get()->UserLoggedIn(
      owner_account_id_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      account_id0_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      account_id1_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();

  const user_manager::UserList* users =
      &user_manager::UserManager::Get()->GetUsers();
  ASSERT_EQ(3U, users->size());
  EXPECT_EQ((*users)[0]->GetAccountId(), account_id1_at_invalid_domain_);
  EXPECT_EQ((*users)[1]->GetAccountId(), account_id0_at_invalid_domain_);
  EXPECT_EQ((*users)[2]->GetAccountId(), owner_account_id_at_invalid_domain_);

  test_wallpaper_controller_.ClearCounts();
  SetDeviceSettings(true, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();

  users = &user_manager::UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->GetAccountId(), owner_account_id_at_invalid_domain_);
  // Verify that the wallpaper is removed when user is removed.
  EXPECT_EQ(2, test_wallpaper_controller_.remove_user_wallpaper_count());
}

TEST_F(UserManagerTest, RegularUserLoggedInAsEphemeral) {
  SetDeviceSettings(true, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();

  user_manager::UserManager::Get()->UserLoggedIn(
      owner_account_id_at_invalid_domain_,
      account_id0_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      account_id0_at_invalid_domain_,
      account_id0_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();

  const user_manager::UserList* users =
      &user_manager::UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->GetAccountId(), owner_account_id_at_invalid_domain_);
}

TEST_F(UserManagerTest, ScreenLockAvailability) {
  // Log in the user and create the profile.
  user_manager::UserManager::Get()->UserLoggedIn(
      owner_account_id_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile& profile = profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(),
      ash::ProfileHelper::GetProfilePathByUserIdHash(user->username_hash()));

  // Verify that the user is allowed to lock the screen.
  EXPECT_TRUE(user_manager::UserManager::Get()->CanCurrentUserLock());
  EXPECT_EQ(1U, user_manager::UserManager::Get()->GetUnlockUsers().size());

  // The user is not allowed to lock the screen.
  profile.GetPrefs()->SetBoolean(prefs::kAllowScreenLock, false);
  EXPECT_FALSE(user_manager::UserManager::Get()->CanCurrentUserLock());
  EXPECT_EQ(0U, user_manager::UserManager::Get()->GetUnlockUsers().size());

  ResetUserManager();
}

TEST_F(UserManagerTest, ProfileRequiresPolicyUnknown) {
  user_manager::UserManager::Get()->UserLoggedIn(
      owner_account_id_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(), false, false);
  user_manager::KnownUser known_user(local_state_->Get());
  EXPECT_EQ(
      user_manager::ProfileRequiresPolicy::kUnknown,
      known_user.GetProfileRequiresPolicy(owner_account_id_at_invalid_domain_));
  ResetUserManager();
}

// Test that |RecordOwner| can save owner email into local state and
// |GetOwnerEmail| can retrieve it.
TEST_F(UserManagerTest, RecordOwner) {
  // Initially `GetOwnerEmail` should return a nullopt.
  absl::optional<std::string> owner =
      user_manager::UserManager::Get()->GetOwnerEmail();
  EXPECT_FALSE(owner.has_value());

  // Save a user as an owner.
  user_manager::UserManager::Get()->RecordOwner(
      AccountId::FromUserEmail(kEmail));

  // Now `GetOwnerEmail` should return the email of the user above.
  owner = user_manager::UserManager::Get()->GetOwnerEmail();
  ASSERT_TRUE(owner.has_value());
  EXPECT_EQ(owner.value(), kEmail);
}

}  // namespace ash
