// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/policy_user_manager_controller.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_impl.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "components/user_manager/user_names.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

AccountId CreateDeviceLocalAccountId(const std::string& account_id,
                                     policy::DeviceLocalAccountType type) {
  return AccountId::FromUserEmail(
      policy::GenerateDeviceLocalAccountUserId(account_id, type));
}

constexpr char kDeviceLocalAccountId[] = "device_local_account";

const AccountId kOwnerAccountId =
    AccountId::FromUserEmailGaiaId("owner@example.com", "1234567890");
const AccountId kAccountId0 =
    AccountId::FromUserEmailGaiaId("user0@example.com", "0123456789");
const AccountId kAccountId1 =
    AccountId::FromUserEmailGaiaId("user1@example.com", "9012345678");
const AccountId kKioskAccountId =
    CreateDeviceLocalAccountId(kDeviceLocalAccountId,
                               policy::DeviceLocalAccountType::kKioskApp);
}  // namespace

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

    UserDataAuthClient::InitializeFake();

    UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting();
    UserImageManagerImpl::SkipProfileImageDownloadForTesting();

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();

    // Populate the stub DeviceSettingsProvider with valid values.
    SetDeviceSettings(/* ephemeral_users_enabled= */ false, /* owner= */ "");

    // Instantiate ProfileHelper.
    ash::ProfileHelper::Get();

    // Register an in-memory local settings instance.
    local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<FakeProfileManager>(temp_dir_.GetPath()));

    ResetUserManager();

    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
  }

  void TearDown() override {
    user_image_manager_registry_.reset();
    if (user_manager_) {
      user_manager_->Destroy();
    }

    // Shut down the DeviceSettingsService.
    DeviceSettingsService::Get()->UnsetSessionManager();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);

    // Unregister the in-memory local settings instance.
    local_state_.reset();

    base::RunLoop().RunUntilIdle();
    ConciergeClient::Shutdown();

    UserDataAuthClient::Shutdown();
  }

  bool IsEphemeralAccountId(const AccountId& account_id) const {
    return user_manager_->IsEphemeralAccountId(account_id);
  }

  void SetEphemeralModeConfig(
      user_manager::UserManager::EphemeralModeConfig ephemeral_mode_config) {
    user_manager_->SetEphemeralModeConfig(std::move(ephemeral_mode_config));
  }

  AccountId GetUserManagerOwnerId() const {
    return user_manager_->GetOwnerAccountId();
  }

  void SetUserManagerOwnerId(const AccountId& owner_account_id) {
    user_manager_->SetOwnerId(owner_account_id);
  }

  void ResetUserManager() {
    // Initialize the UserManager singleton to a fresh UserManager instance.
    user_image_manager_registry_.reset();
    policy_user_manager_controller_.reset();
    if (user_manager_) {
      user_manager_->Destroy();
      user_manager_.reset();
    }
    user_manager_ = std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<UserManagerDelegateImpl>(), local_state_->Get(),
        CrosSettings::Get());
    policy_user_manager_controller_ =
        std::make_unique<PolicyUserManagerController>(
            user_manager_.get(), ash::CrosSettings::Get(),
            DeviceSettingsService::Get(), nullptr);
    user_image_manager_registry_ =
        std::make_unique<ash::UserImageManagerRegistry>(user_manager_.get());
    // Initialize `UserManager` after `UserImageManagerRegistry` creation to
    // follow initialization order in
    // `BrowserProcessPlatformPart::InitializeUserManager()`
    user_manager_->Initialize();

    // PolicyUserManagerController ctor posts a task to reload policies.
    // Also ensure that all existing ongoing user manager tasks are completed.
    task_environment_.RunUntilIdle();
  }

  void SetDeviceSettings(bool ephemeral_users_enabled,
                         const std::string& owner) {
    settings_helper_.SetBoolean(kAccountsPrefEphemeralUsersEnabled,
                                ephemeral_users_enabled);
    settings_helper_.SetString(kDeviceOwner, owner);
  }

  void SetKioskAccountPrefs(
      policy::DeviceLocalAccount::EphemeralMode ephemeral_mode,
      const std::string& account_id = kDeviceLocalAccountId,
      int type = static_cast<int>(policy::DeviceLocalAccountType::kKioskApp)) {
    settings_helper_.Set(
        kAccountsPrefDeviceLocalAccounts,
        base::Value(base::Value::List().Append(
            base::Value::Dict()
                .Set(kAccountsPrefDeviceLocalAccountsKeyId, account_id)
                .Set(kAccountsPrefDeviceLocalAccountsKeyType, type)
                .Set(kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                     static_cast<int>(ephemeral_mode))
                .Set(kAccountsPrefDeviceLocalAccountsKeyKioskAppId, ""))));
  }

  // Should be used to setup device local accounts of `TYPE_PUBLIC_SESSION`.
  void SetDeviceLocalPublicAccount(
      const std::string& account_id,
      policy::DeviceLocalAccountType type,
      policy::DeviceLocalAccount::EphemeralMode ephemeral_mode) {
    settings_helper_.Set(
        kAccountsPrefDeviceLocalAccounts,
        base::Value(base::Value::List().Append(
            base::Value::Dict()
                .Set(kAccountsPrefDeviceLocalAccountsKeyId, account_id)
                .Set(kAccountsPrefDeviceLocalAccountsKeyType,
                     static_cast<int>(type))
                .Set(kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                     static_cast<int>(ephemeral_mode)))));
  }

  void SetUpArcKioskAccountPersistentPrefs() {
    const std::string email =
        std::string("test@") + user_manager::kArcKioskDomain;

    SetKioskAccountPrefs(policy::DeviceLocalAccount::EphemeralMode::kDisable,
                         /* account_id= */ email, /* type=kArcKiosk */ 2);
    local_state_->Get()->Set(
        user_manager::prefs::kDeviceLocalAccountsWithSavedData,
        base::Value(base::Value::List().Append(email)));
    user_manager::KnownUser(local_state_->Get())
        .SaveKnownUser(AccountId::FromUserEmailGaiaId(email, "fake_gaia_id"));
  }

  size_t GetArcKioskAccountsWithSavedDataCount() {
    return local_state_->Get()
        ->GetList(user_manager::prefs::kDeviceLocalAccountsWithSavedData)
        .size();
  }

  size_t GetKnownUsersCount() {
    return user_manager::KnownUser(local_state_->Get())
        .GetKnownAccountIds()
        .size();
  }

  void RetrieveTrustedDevicePolicies() {
    policy_user_manager_controller_->RetrieveTrustedDevicePolicies();
  }

 protected:
  // The call chain
  // - `ProfileRequiresPolicyUnknown`
  // - `UserManagerImpl::UserLoggedIn()`
  // - `UserManagerImpl::NotifyOnLogin()`
  // - `UserSessionManager::InitNonKioskExtensionFeaturesSessionType()`
  // calls
  // `extensions::SetCurrentFeatureSessionType(FeatureSessionType::kRegular)`
  //
  // |session_type_| is used to capture the original session type during |SetUp|
  // and set it back to what it was during |TearDown|.
  std::unique_ptr<base::AutoReset<extensions::mojom::FeatureSessionType>>
      session_type_;

  content::BrowserTaskEnvironment task_environment_;
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  ScopedCrosSettingsTestHelper settings_helper_;
  // local_state_ should be destructed after ProfileManager.
  std::unique_ptr<ScopedTestingLocalState> local_state_;

  std::unique_ptr<user_manager::UserManagerImpl> user_manager_;
  std::unique_ptr<PolicyUserManagerController> policy_user_manager_controller_;
  std::unique_ptr<ash::UserImageManagerRegistry> user_image_manager_registry_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(UserManagerTest, RetrieveTrustedDevicePolicies) {
  SetEphemeralModeConfig(user_manager::UserManager::EphemeralModeConfig(
      /* included_by_default= */ true,
      /* include_list= */ std::vector<AccountId>{},
      /* exclude_list= */ std::vector<AccountId>{}));
  SetUserManagerOwnerId(EmptyAccountId());

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ false,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(IsEphemeralAccountId(EmptyAccountId()));

  EXPECT_EQ(GetUserManagerOwnerId(), kOwnerAccountId);
}

// Tests that `IsEphemeralAccountId(account_id)` returns false when `account_id`
// is a device owner account id.
TEST_F(UserManagerTest, IsEphemeralAccountIdFalseForOwnerAccountId) {
  EXPECT_FALSE(IsEphemeralAccountId(kOwnerAccountId));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(IsEphemeralAccountId(kOwnerAccountId));
}

// Tests that `IsEphemeralAccountId(account_id)` returns true when `account_id`
// is a guest account id.
TEST_F(UserManagerTest, IsEphemeralAccountIdTrueForGuestAccountId) {
  EXPECT_TRUE(IsEphemeralAccountId(user_manager::GuestAccountId()));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ false,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();

  EXPECT_TRUE(IsEphemeralAccountId(user_manager::GuestAccountId()));
}

// Tests that `IsEphemeralAccountId(account_id)` returns false when `account_id`
// is a stub account id.
TEST_F(UserManagerTest, IsEphemeralAccountIdFalseForStubAccountId) {
  EXPECT_FALSE(IsEphemeralAccountId(user_manager::StubAccountId()));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(IsEphemeralAccountId(user_manager::StubAccountId()));
}

// Tests that `IsEphemeralAccountId(account_id)` returns true when `account_id`
// is a public account id.
TEST_F(UserManagerTest, IsEphemeralAccountIdTrueForPublicAccountId) {
  // Set all ephemeral related policies to `false` to make sure that policies
  // don't affect ephemeral mode of the public account.
  SetDeviceSettings(
      /* ephemeral_users_enabled= */ false,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  SetDeviceLocalPublicAccount(
      kDeviceLocalAccountId, policy::DeviceLocalAccountType::kPublicSession,
      policy::DeviceLocalAccount::EphemeralMode::kDisable);
  RetrieveTrustedDevicePolicies();

  const AccountId public_accout_id = CreateDeviceLocalAccountId(
      kDeviceLocalAccountId, policy::DeviceLocalAccountType::kPublicSession);
  EXPECT_TRUE(IsEphemeralAccountId(public_accout_id));
}

// Tests that `UserManager` correctly parses device-wide ephemeral users policy
// by calling `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest, IsEphemeralAccountIdUsesEphemeralUsersEnabledPolicy) {
  EXPECT_FALSE(IsEphemeralAccountId(EmptyAccountId()));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();

  EXPECT_TRUE(IsEphemeralAccountId(EmptyAccountId()));
}

// Tests that `UserManager` correctly parses device-local accounts with
// ephemeral mode equals to `kFollowDeviceWidePolicy` by calling
// `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest,
       IsEphemeralAccountIdRespectsFollowDeviceWidePolicyEphemeralMode) {
  EXPECT_FALSE(IsEphemeralAccountId(kKioskAccountId));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  SetKioskAccountPrefs(
      policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy);
  RetrieveTrustedDevicePolicies();
  EXPECT_TRUE(IsEphemeralAccountId(kKioskAccountId));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ false,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();
  EXPECT_FALSE(IsEphemeralAccountId(kKioskAccountId));
}

// Tests that `UserManager` correctly parses device-local accounts with
// ephemeral mode equals to `kUnset` by calling
// `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest, IsEphemeralAccountIdRespectsUnsetEphemeralMode) {
  EXPECT_FALSE(IsEphemeralAccountId(kKioskAccountId));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  SetKioskAccountPrefs(policy::DeviceLocalAccount::EphemeralMode::kUnset);
  RetrieveTrustedDevicePolicies();
  EXPECT_TRUE(IsEphemeralAccountId(kKioskAccountId));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ false,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();
  EXPECT_FALSE(IsEphemeralAccountId(kKioskAccountId));
}

// Tests that `UserManager` correctly parses device-local accounts with
// ephemeral mode equals to `kDisable` by calling
// `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest, IsEphemeralAccountIdRespectsDisableEphemeralMode) {
  EXPECT_FALSE(IsEphemeralAccountId(kKioskAccountId));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  SetKioskAccountPrefs(policy::DeviceLocalAccount::EphemeralMode::kDisable);
  RetrieveTrustedDevicePolicies();

  EXPECT_TRUE(IsEphemeralAccountId(EmptyAccountId()));
  EXPECT_FALSE(IsEphemeralAccountId(kKioskAccountId));
}

// Tests that `UserManager` correctly parses device-local accounts with
// ephemeral mode equals to `kEnable` by calling
// `IsEphemeralAccountId(account_id)` function.
TEST_F(UserManagerTest, IsEphemeralAccountIdRespectsEnableEphemeralMode) {
  EXPECT_FALSE(IsEphemeralAccountId(kKioskAccountId));

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ false,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  SetKioskAccountPrefs(policy::DeviceLocalAccount::EphemeralMode::kEnable);
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(IsEphemeralAccountId(EmptyAccountId()));
  EXPECT_TRUE(IsEphemeralAccountId(kKioskAccountId));
}

// This test covers b/293320330.
// User manager should contain kiosk account, but `kRegularUsersPref` local
// state should not have kiosk account.
TEST_F(UserManagerTest, DoNotSaveKioskAccountsToKRegularUsersPref) {
  SetKioskAccountPrefs(policy::DeviceLocalAccount::EphemeralMode::kEnable);
  user_manager::UserManager::Get()->UserLoggedIn(
      kKioskAccountId, kKioskAccountId.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      kAccountId0, kAccountId0.GetUserEmail(), false /* browser_restart */,
      false /* is_child */);
  ResetUserManager();

  EXPECT_EQ(1U, local_state_->Get()
                    ->GetList(user_manager::prefs::kRegularUsersPref)
                    .size());
  EXPECT_EQ(2U, user_manager::UserManager::Get()->GetUsers().size());

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();

  EXPECT_TRUE(local_state_->Get()
                  ->GetList(user_manager::prefs::kRegularUsersPref)
                  .empty());
  EXPECT_EQ(1U, user_manager::UserManager::Get()->GetUsers().size());
}

TEST_F(UserManagerTest, RemoveUser) {
  // Create owner account and login in.
  user_manager_->UserLoggedIn(kOwnerAccountId, kOwnerAccountId.GetUserEmail(),
                              false /* browser_restart */,
                              false /* is_child */);

  // Create non-owner account  and login in.
  user_manager_->UserLoggedIn(kAccountId0, kAccountId0.GetUserEmail(),
                              false /* browser_restart */,
                              false /* is_child */);

  ASSERT_EQ(2U, user_manager_->GetUsers().size());

  // Removing logged-in account is unacceptable.
  user_manager_->RemoveUser(kAccountId0,
                            user_manager::UserRemovalReason::UNKNOWN);
  EXPECT_EQ(2U, user_manager_->GetUsers().size());

  // Recreate the user manager to log out all accounts.
  ResetUserManager();

  UserManagerObserverTest observer_test;
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observation{&observer_test};
  observation.Observe(user_manager_.get());
  ASSERT_EQ(2U, user_manager_->GetUsers().size());
  ASSERT_EQ(0U, user_manager_->GetLoggedInUsers().size());

  // Get a pointer to the user that will be removed.
  user_manager::User* user_to_remove = nullptr;
  for (user_manager::User* user : user_manager_->GetUsers()) {
    if (user->GetAccountId() == kAccountId0) {
      user_to_remove = user;
      break;
    }
  }
  ASSERT_TRUE(user_to_remove);
  ASSERT_EQ(kAccountId0, user_to_remove->GetAccountId());

  // Pass the account id of the user to be removed from the user list to verify
  // that a reference to the account id will not be used after user removal.
  user_manager_->RemoveUser(kAccountId0,
                            user_manager::UserRemovalReason::UNKNOWN);
  EXPECT_EQ(1, observer_test.OnUserToBeRemovedCallCount());
  EXPECT_EQ(1, observer_test.OnUserRemovedCallCount());
  EXPECT_EQ(1U, user_manager_->GetUsers().size());

  // Removing owner account is unacceptable.
  observer_test.ResetCallCounts();
  user_manager_->RemoveUser(kOwnerAccountId,
                            user_manager::UserRemovalReason::UNKNOWN);
  EXPECT_EQ(0, observer_test.OnUserToBeRemovedCallCount());
  EXPECT_EQ(0, observer_test.OnUserRemovedCallCount());
  EXPECT_EQ(1U, user_manager_->GetUsers().size());
}

TEST_F(UserManagerTest, RemoveRegularUsersExceptOwnerFromList) {
  user_manager::UserManager::Get()->UserLoggedIn(
      kOwnerAccountId, kOwnerAccountId.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      kAccountId0, kAccountId0.GetUserEmail(), false /* browser_restart */,
      false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      kAccountId1, kAccountId1.GetUserEmail(), false /* browser_restart */,
      false /* is_child */);
  ResetUserManager();

  SetKioskAccountPrefs(policy::DeviceLocalAccount::EphemeralMode::kEnable);
  user_manager::UserManager::Get()->UserLoggedIn(
      kKioskAccountId, kKioskAccountId.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();

  const user_manager::UserList* users =
      &user_manager::UserManager::Get()->GetUsers();
  ASSERT_EQ(4U, users->size());
  EXPECT_EQ((*users)[0]->GetAccountId(), kKioskAccountId);
  EXPECT_EQ((*users)[1]->GetAccountId(), kAccountId1);
  EXPECT_EQ((*users)[2]->GetAccountId(), kAccountId0);
  EXPECT_EQ((*users)[3]->GetAccountId(), kOwnerAccountId);

  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();

  users = &user_manager::UserManager::Get()->GetUsers();
  EXPECT_EQ(2U, users->size());
  // Kiosk is not a regular user and is not removed.
  EXPECT_EQ((*users)[0]->GetAccountId(), kKioskAccountId);
  EXPECT_EQ((*users)[1]->GetAccountId(), kOwnerAccountId);
}

TEST_F(UserManagerTest, RegularUserLoggedInAsEphemeral) {
  SetDeviceSettings(
      /* ephemeral_users_enabled= */ true,
      /* owner= */ kOwnerAccountId.GetUserEmail());
  RetrieveTrustedDevicePolicies();

  user_manager::UserManager::Get()->UserLoggedIn(
      kOwnerAccountId, kOwnerAccountId.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      kAccountId0, kAccountId0.GetUserEmail(), false /* browser_restart */,
      false /* is_child */);
  ResetUserManager();

  const user_manager::UserList* users =
      &user_manager::UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->GetAccountId(), kOwnerAccountId);
}

TEST_F(UserManagerTest, ScreenLockAvailability) {
  // Log in the user and create the profile.
  user_manager::UserManager::Get()->UserLoggedIn(
      kOwnerAccountId, kOwnerAccountId.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);

  TestingPrefServiceSimple prefs;
  user_manager::UserManagerImpl::RegisterProfilePrefs(prefs.registry());
  // To simplify the dependency, register the pref directly.
  // In production, this is registered in ash::PowerPrefs.
  prefs.registry()->RegisterBooleanPref(prefs::kAllowScreenLock, true);

  user_manager::UserManager::Get()->OnUserProfileCreated(kOwnerAccountId,
                                                         &prefs);

  // Verify that the user is allowed to lock the screen.
  EXPECT_TRUE(user_manager::UserManager::Get()->GetActiveUser()->CanLock());
  EXPECT_EQ(1U, user_manager::UserManager::Get()->GetUnlockUsers().size());

  // The user is not allowed to lock the screen.
  prefs.SetBoolean(prefs::kAllowScreenLock, false);
  EXPECT_FALSE(user_manager::UserManager::Get()->GetActiveUser()->CanLock());
  EXPECT_EQ(0U, user_manager::UserManager::Get()->GetUnlockUsers().size());

  user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(
      kOwnerAccountId);
}

TEST_F(UserManagerTest, ProfileRequiresPolicyUnknown) {
  user_manager::UserManager::Get()->UserLoggedIn(
      kOwnerAccountId, kOwnerAccountId.GetUserEmail(), false, false);
  user_manager::KnownUser known_user(local_state_->Get());
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            known_user.GetProfileRequiresPolicy(kOwnerAccountId));
  ResetUserManager();
}

// Test that |RecordOwner| can save owner email into local state and
// |GetOwnerEmail| can retrieve it.
TEST_F(UserManagerTest, RecordOwner) {
  // Initially `GetOwnerEmail` should return a nullopt.
  std::optional<std::string> owner =
      user_manager::UserManager::Get()->GetOwnerEmail();
  EXPECT_FALSE(owner.has_value());

  // Save a user as an owner.
  user_manager::UserManager::Get()->RecordOwner(
      AccountId::FromUserEmail(kOwnerAccountId.GetUserEmail()));

  // Now `GetOwnerEmail` should return the email of the user above.
  owner = user_manager::UserManager::Get()->GetOwnerEmail();
  ASSERT_TRUE(owner.has_value());
  EXPECT_EQ(owner.value(), kOwnerAccountId.GetUserEmail());
}

TEST_F(UserManagerTest, RemoveDeprecatedArcKioskAccountOnStartUpByDefault) {
  base::HistogramTester histogram_tester;
  SetUpArcKioskAccountPersistentPrefs();

  ResetUserManager();

  EXPECT_EQ(0U, GetArcKioskAccountsWithSavedDataCount());
  EXPECT_EQ(0U, GetKnownUsersCount());
  histogram_tester.ExpectTotalCount(
      user_manager::UserManagerImpl::kDeprecatedArcKioskUsersHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      user_manager::UserManagerImpl::kDeprecatedArcKioskUsersHistogramName,
      user_manager::UserManagerImpl::DeprecatedArcKioskUserStatus::kDeleted,
      /* expected_count= */ 1);
}

TEST_F(UserManagerTest,
       HideDeprecatedArcKioskAccountOnStartUpWhenTheFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      user_manager::kRemoveDeprecatedArcKioskUsersOnStartup);

  base::HistogramTester histogram_tester;
  SetUpArcKioskAccountPersistentPrefs();

  ResetUserManager();

  EXPECT_EQ(0U, GetArcKioskAccountsWithSavedDataCount());
  // The ARC kiosk user has not been removed, just hidden.
  EXPECT_EQ(1U, GetKnownUsersCount());
  histogram_tester.ExpectTotalCount(
      user_manager::UserManagerImpl::kDeprecatedArcKioskUsersHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      user_manager::UserManagerImpl::kDeprecatedArcKioskUsersHistogramName,
      user_manager::UserManagerImpl::DeprecatedArcKioskUserStatus::kHidden,
      /* expected_count= */ 1);
}

}  // namespace ash
