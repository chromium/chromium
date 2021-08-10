// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store_impl.h"

#include "ash/constants/ash_features.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/fake_device_name_policy_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/fake_device_name_applier.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class FakeObserver : public DeviceNameStore::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // DeviceNameStore::Observer:
  void OnDeviceNameMetadataChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class DeviceNameStoreImplTest : public ::testing::Test {
 public:
  DeviceNameStoreImplTest() {
    DeviceNameStore::RegisterLocalStatePrefs(local_state_.registry());
  }

  ~DeviceNameStoreImplTest() override = default;

  void TearDown() override {
    if (device_name_store_)
      device_name_store_->RemoveObserver(&fake_observer_);

    DeviceNameStore::Shutdown();
  }

  void MakeUserOwner() { user_manager_->SwitchActiveUser(account_id_); }

  void SwitchActiveUser(const std::string& email) {
    const AccountId account_id(AccountId::FromUserEmail(email));
    user_manager_->SwitchActiveUser(account_id);
  }

  void InitializeDeviceNameStore(
      bool is_hostname_setting_flag_enabled,
      bool is_current_user_owner,
      const absl::optional<std::string>& name_in_prefs = absl::nullopt) {
    if (is_hostname_setting_flag_enabled) {
      feature_list_.InitAndEnableFeature(ash::features::kEnableHostnameSetting);
    } else {
      feature_list_.InitAndDisableFeature(
          ash::features::kEnableHostnameSetting);
    }

    auto user_manager = std::make_unique<FakeChromeUserManager>();
    user_manager_ = user_manager.get();
    account_id_ = AccountId::FromUserEmail(profile_.GetProfileUserName());
    user_manager_->AddUser(account_id_);
    user_manager_->LoginUser(account_id_);
    user_manager_->SetOwnerId(account_id_);

    if (is_current_user_owner)
      user_manager_->SwitchActiveUser(account_id_);

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    // Set the device name from the previous session of the user if any.
    if (name_in_prefs)
      local_state_.SetString(prefs::kDeviceName, *name_in_prefs);

    auto fake_device_name_applier = std::make_unique<FakeDeviceNameApplier>();
    fake_device_name_applier_ = fake_device_name_applier.get();

    device_name_store_ = base::WrapUnique(new DeviceNameStoreImpl(
        &local_state_, &fake_device_name_policy_handler_,
        std::move(fake_device_name_applier)));
    device_name_store_->AddObserver(&fake_observer_);
  }

  std::string GetDeviceNameFromPrefs() const {
    return local_state_.GetString(prefs::kDeviceName);
  }

  DeviceNameStoreImpl* device_name_store() const {
    return device_name_store_.get();
  }

  policy::FakeDeviceNamePolicyHandler* get_fake_device_name_policy_handler() {
    return &fake_device_name_policy_handler_;
  }

  void VerifyDeviceNameMetadata(
      const std::string& expected_device_name,
      DeviceNameStore::DeviceNameState expected_device_name_state) const {
    DeviceNameStore::DeviceNameMetadata metadata =
        device_name_store()->GetDeviceNameMetadata();
    EXPECT_EQ(expected_device_name, metadata.device_name);
    EXPECT_EQ(expected_device_name_state, metadata.device_name_state);

    EXPECT_EQ(GetDeviceNameFromPrefs(), expected_device_name);

    // Verify that device name has been correctly updated in DHCP too.
    EXPECT_EQ(fake_device_name_applier_->hostname(), expected_device_name);
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_;

  // Test backing store for prefs.
  TestingPrefServiceSimple local_state_;

  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  ash::FakeChromeUserManager* user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  AccountId account_id_;
  FakeDeviceNameApplier* fake_device_name_applier_;
  FakeObserver fake_observer_;
  policy::FakeDeviceNamePolicyHandler fake_device_name_policy_handler_;
  std::unique_ptr<DeviceNameStoreImpl> device_name_store_;
};

// Check that error is thrown if GetInstance() is called before
// initialization.
TEST_F(DeviceNameStoreImplTest, GetInstanceBeforeInitializeError) {
  EXPECT_DEATH(DeviceNameStore::GetInstance(), "");
}

TEST_F(DeviceNameStoreImplTest, UnmanagedDeviceOwnerFirstTimeUser) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true);
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);

  // Device owner can set a new device name.
  EXPECT_EQ(0u, GetNumObserverCalls());
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, UnmanagedDeviceNotOwnerFirstTimeUser) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/false);
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);

  // Non device owner cannot set a new device name.
  EXPECT_EQ(0u, GetNumObserverCalls());
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  EXPECT_EQ(0u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, UnmanagedDeviceOwnerNotFirstTimeUser) {
  // Verify that device name is the previously set name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true,
                            /*name_in_prefs=*/"NameFromPreviousSession");
  VerifyDeviceNameMetadata("NameFromPreviousSession",
                           DeviceNameStore::DeviceNameState::kCanBeModified);

  // Device owner can set a new device name.
  EXPECT_EQ(0u, GetNumObserverCalls());
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, UnmanagedDeviceSwitchOwnerStates) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/false);

  // User is not device owner, hence they cannot modify the device name.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  EXPECT_EQ(0u, GetNumObserverCalls());

  // Verify that if logged in user is now device owner, observer is notified and
  // user can now modify the device name.
  MakeUserOwner();
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());

  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Switch back to non-owner state and verify they cannot modify the device
  // name again. Observer should be notified of the device name state change.
  SwitchActiveUser("nonowner@tray");
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner,
            device_name_store()->SetDeviceName("TestName1"));
  VerifyDeviceNameMetadata(
      "TestName",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest,
       ManagedDeviceTemplateBeforeSessionFirstTimeUser) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameChosenByAdmin,
      "Template");

  // Verify that device name is set to the template upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true);
  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
}

TEST_F(DeviceNameStoreImplTest,
       ManagedDeviceTemplateBeforeSessionNotFirstTimeUser) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameChosenByAdmin,
      "Template");

  // Verify that device name is set to the template upon initialization despite
  // the name set from previous session.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true,
                            /*name_in_prefs=*/"NameFromPreviousSession");
  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceTemplateDuringSession) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true);
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);

  // Device name should change to template set and notify observers.
  EXPECT_EQ(0u, GetNumObserverCalls());
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameChosenByAdmin,
      "Template");
  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // SetDeviceName() should not update the name for
  // kPolicyHostnameChosenByAdmin policy.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kProhibitedByPolicy,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest,
       ManagedDeviceNotFirstTimeUserNameNotConfigurable) {
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable,
      absl::nullopt);

  // Verify that device name is set to the default name because of
  // non-configurable device name policy.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true,
                            /*name_in_prefs=*/"NameFromPreviousSession");
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceFirstTimeUserNameNotConfigurable) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true);
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);

  EXPECT_EQ(0u, GetNumObserverCalls());
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable,
      absl::nullopt);
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // SetDeviceName() should not update the name for
  // kPolicyHostnameNotConfigurable policy.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kProhibitedByPolicy,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest,
       ManagedDeviceNotFirstTimeUserDeviceNameConfigurable) {
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameConfigurableByManagedUser,
      absl::nullopt);

  // Verify that device name is the previously set name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true,
                            /*name_in_prefs=*/"NameFromPreviousSession");
  VerifyDeviceNameMetadata("NameFromPreviousSession",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceFirstTimeUserNameConfigurable) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true);
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);

  EXPECT_EQ(0u, GetNumObserverCalls());
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameConfigurableByManagedUser,
      absl::nullopt);
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(0u, GetNumObserverCalls());

  // SetDeviceName() should update the name for
  // kPolicyHostnameConfigurableByManagedUser policy if name is valid.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // New device name set is valid but same as previous one, hence observer
  // should not be notified.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // SetDeviceName() should not update the name for
  // kPolicyHostnameConfigurableByManagedUser policy if name is invalid.
  // Name contains a whitespace.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kInvalidName,
            device_name_store()->SetDeviceName("Test Name"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceOwnerPolicyChanges) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/true);
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);

  // Setting kPolicyHostnameChosenByAdmin should change the device name to the
  // template
  EXPECT_EQ(0u, GetNumObserverCalls());
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameChosenByAdmin,
      "Template");
  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Setting kPolicyHostnameConfigurableByManagedUser policy should not change
  // the device name since it is still same as the one previously set. Observer
  // should still be notified since the device name state changes.
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameConfigurableByManagedUser,
      absl::nullopt);
  VerifyDeviceNameMetadata("Template",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Setting kNoPolicy policy should not change the device name since it is
  // still same as the one previously set.
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
      absl::nullopt);
  VerifyDeviceNameMetadata("Template",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Setting kPolicyHostnameNotConfigurable policy should change the device name
  // to the default name "ChromeOS".
  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable,
      absl::nullopt);
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceNonOwnerPolicyChanges) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*is_current_user_owner=*/false);

  // User is not device owner, hence they cannot modify the device name
  // regardless of the policy in place. Observer is still notified for changes
  // in device name state.
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  EXPECT_EQ(0u, GetNumObserverCalls());

  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameChosenByAdmin,
      "Template");
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kProhibitedByPolicy,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());

  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameConfigurableByManagedUser,
      absl::nullopt);
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  EXPECT_EQ(2u, GetNumObserverCalls());

  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy,
      absl::nullopt);
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  EXPECT_EQ(2u, GetNumObserverCalls());

  get_fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable,
      absl::nullopt);
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kProhibitedByPolicy,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(3u, GetNumObserverCalls());
}

}  // namespace chromeos
