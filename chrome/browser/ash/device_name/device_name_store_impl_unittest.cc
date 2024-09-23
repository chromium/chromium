// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/device_name_store_impl.h"

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/device_name/fake_device_name_applier.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/fake_device_name_policy_handler.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
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

constexpr char kUser1Email[] = "test-user-1@example.com";
constexpr char kUser2Email[] = "test-user-2@example.com";

}  // namespace

class DeviceNameStoreImplTest : public ::testing::Test {
 public:
  DeviceNameStoreImplTest() {
    DeviceNameStore::RegisterLocalStatePrefs(local_state_.registry());
  }

  ~DeviceNameStoreImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(mock_profile_manager_.SetUp());
    scoped_cros_settings_test_helper_.ReplaceDeviceSettingsProviderWithStub();

    fake_user_manager_.Reset(std::make_unique<FakeChromeUserManager>());
  }

  void TearDown() override {
    if (device_name_store_)
      device_name_store_->RemoveObserver(&fake_observer_);

    DeviceNameStore::Shutdown();
    scoped_cros_settings_test_helper_.RestoreRealDeviceSettingsProvider();
  }

  void CreateTestingProfile(const std::string& email) {
    AccountId test_account_id(AccountId::FromUserEmail(email));

    fake_user_manager_->AddUser(test_account_id);
    fake_user_manager_->LoginUser(test_account_id);

    TestingProfile* mock_profile = mock_profile_manager_.CreateTestingProfile(
        test_account_id.GetUserEmail(),
        {TestingProfile::TestingFactory{
            OwnerSettingsServiceAshFactory::GetInstance(),
            base::BindRepeating(
                &DeviceNameStoreImplTest::CreateOwnerSettingsServiceAsh,
                base::Unretained(this))}});
    owner_settings_service_ash_ =
        OwnerSettingsServiceAshFactory::GetInstance()->GetForBrowserContext(
            mock_profile);
  }

  void FlushActiveProfileCallbacks(bool is_owner) {
    DCHECK(owner_settings_service_ash_);
    owner_settings_service_ash_->RunPendingIsOwnerCallbacksForTesting(is_owner);
  }

  void InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy initial_policy =
          policy::DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy) {
    fake_device_name_policy_handler_ =
        std::make_unique<policy::FakeDeviceNamePolicyHandler>(initial_policy);
  }

  void InitializeDeviceNameStore(
      bool is_hostname_setting_flag_enabled,
      const std::optional<std::string>& name_in_prefs = std::nullopt) {
    if (is_hostname_setting_flag_enabled)
      feature_list_.InitAndEnableFeature(features::kEnableHostnameSetting);
    else
      feature_list_.InitAndDisableFeature(features::kEnableHostnameSetting);

    // Set the device name from the previous session of the user if any.
    if (name_in_prefs)
      local_state_.SetString(prefs::kDeviceName, *name_in_prefs);

    auto fake_device_name_applier = std::make_unique<FakeDeviceNameApplier>();
    fake_device_name_applier_ = fake_device_name_applier.get();

    device_name_store_ = base::WrapUnique(new DeviceNameStoreImpl(
        &local_state_, fake_device_name_policy_handler_.get(),
        std::move(fake_device_name_applier)));
    device_name_store_->AddObserver(&fake_observer_);
  }

  std::string GetDeviceNameFromPrefs() const {
    return local_state_.GetString(prefs::kDeviceName);
  }

  DeviceNameStoreImpl* device_name_store() const {
    return device_name_store_.get();
  }

  policy::FakeDeviceNamePolicyHandler* fake_device_name_policy_handler() {
    return fake_device_name_policy_handler_.get();
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
  std::unique_ptr<KeyedService> CreateOwnerSettingsServiceAsh(
      content::BrowserContext* context) {
    return scoped_cros_settings_test_helper_.CreateOwnerSettingsService(
        Profile::FromBrowserContext(context));
  }

  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_;

  // Test backing store for prefs.
  TestingPrefServiceSimple local_state_;

  ScopedCrosSettingsTestHelper scoped_cros_settings_test_helper_;
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_;

  TestingProfileManager mock_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<OwnerSettingsServiceAsh> owner_settings_service_ash_;
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<FakeDeviceNameApplier, DanglingUntriaged> fake_device_name_applier_;
  FakeObserver fake_observer_;
  std::unique_ptr<policy::FakeDeviceNamePolicyHandler>
      fake_device_name_policy_handler_;
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

  InitializeFakeDeviceNamePolicyHandler();

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);

  // Log user in as owner. Observers should be notified because this changes the
  // state from kCannotBeModifiedBecauseNotDeviceOwner to kCanBeModified.
  EXPECT_EQ(0u, GetNumObserverCalls());
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/true);

  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Device owner can set a new device name.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, UnmanagedDeviceNotOwnerFirstTimeUser) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeFakeDeviceNamePolicyHandler();

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

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
  InitializeFakeDeviceNamePolicyHandler();

  // Verify that device name is the previously set name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*name_in_prefs=*/"NameFromPreviousSession");

  // Log user in as owner. Observers should be notified because this changes the
  // state from kCannotBeModifiedBecauseNotDeviceOwner to kCanBeModified.
  EXPECT_EQ(0u, GetNumObserverCalls());
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/true);

  VerifyDeviceNameMetadata("NameFromPreviousSession",
                           DeviceNameStore::DeviceNameState::kCanBeModified);

  // Device owner can set a new device name.
  EXPECT_EQ(1u, GetNumObserverCalls());
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, UnmanagedDeviceSwitchOwnerStates) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeFakeDeviceNamePolicyHandler();

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);

  // Log user in as owner.
  CreateTestingProfile(kUser1Email);

  // If owner callback has not been called yet, the user is not technically
  // owner yet and hence they cannot modify the device name.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  EXPECT_EQ(0u, GetNumObserverCalls());

  // Once owner callback has been called, observer is notified and the user can
  // now modify the device name.
  FlushActiveProfileCallbacks(/*is_owner=*/true);
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());

  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Log in non-owner user and verify they cannot modify the device name.
  // Observer should be notified of the device name state change.
  CreateTestingProfile(kUser2Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner,
            device_name_store()->SetDeviceName("TestName1"));
  VerifyDeviceNameMetadata(
      "TestName",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  EXPECT_EQ(3u, GetNumObserverCalls());
}

// For the tests below with managed devices, the user can never be device owner.
// The initial device name policy in FakeDeviceNamePolicyHandler is
// kPolicyHostnameNotConfigurable by default for managed devices.

TEST_F(DeviceNameStoreImplTest,
       ManagedDeviceTemplateBeforeSessionFirstTimeUser) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable);
  fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameChosenByAdmin,
      "Template");

  // Verify that device name is set to the template upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
}

TEST_F(DeviceNameStoreImplTest,
       ManagedDeviceTemplateBeforeSessionNotFirstTimeUser) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable);
  fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameChosenByAdmin,
      "Template");

  // Verify that device name is set to the template upon initialization despite
  // the name set from previous session.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*name_in_prefs=*/"NameFromPreviousSession");

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  VerifyDeviceNameMetadata(
      "Template",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceTemplateDuringSession) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable);

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);

  // Device name should change to template set and notify observers.
  EXPECT_EQ(0u, GetNumObserverCalls());
  fake_device_name_policy_handler()->SetPolicyState(
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
  InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable);
  fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable,
      std::nullopt);

  // Verify that device name is set to the default name because of
  // non-configurable device name policy.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*name_in_prefs=*/"NameFromPreviousSession");

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceFirstTimeUserNameNotConfigurable) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable);

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);

  EXPECT_EQ(0u, GetNumObserverCalls());
  fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable,
      std::nullopt);
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(0u, GetNumObserverCalls());

  // SetDeviceName() should not update the name for
  // kPolicyHostnameNotConfigurable policy.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kProhibitedByPolicy,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(0u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceNotFirstTimeUserNameConfigurable) {
  InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable);
  fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameConfigurableByManagedUser,
      std::nullopt);

  // Verify that device name is the previously set name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true,
                            /*name_in_prefs=*/"NameFromPreviousSession");

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);
  VerifyDeviceNameMetadata("NameFromPreviousSession",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceFirstTimeUserNameConfigurable) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable);

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(0u, GetNumObserverCalls());

  fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameConfigurableByManagedUser,
      std::nullopt);
  VerifyDeviceNameMetadata("ChromeOS",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // SetDeviceName() should update the name for
  // kPolicyHostnameConfigurableByManagedUser policy if name is valid.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());

  // New device name set is valid but same as previous one, hence observer
  // should not be notified.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());

  // SetDeviceName() should not update the name for
  // kPolicyHostnameConfigurableByManagedUser policy if name is invalid.
  // Name contains a whitespace.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kInvalidName,
            device_name_store()->SetDeviceName("Test Name"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(DeviceNameStoreImplTest, ManagedDeviceOwnerPolicyChanges) {
  // The device name is not set yet.
  EXPECT_TRUE(GetDeviceNameFromPrefs().empty());

  InitializeFakeDeviceNamePolicyHandler(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable);

  // Verify that device name is set to the default name upon initialization.
  InitializeDeviceNameStore(/*is_hostname_setting_flag_enabled=*/true);

  // Log user in as non-owner.
  CreateTestingProfile(kUser1Email);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);

  // Setting kPolicyHostnameChosenByAdmin should change the device name to the
  // template
  EXPECT_EQ(0u, GetNumObserverCalls());
  fake_device_name_policy_handler()->SetPolicyState(
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
  fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameConfigurableByManagedUser,
      std::nullopt);
  VerifyDeviceNameMetadata("Template",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Setting kPolicyHostnameNotConfigurable policy should change the device name
  // to the default name "ChromeOS".
  fake_device_name_policy_handler()->SetPolicyState(
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::
          kPolicyHostnameNotConfigurable,
      std::nullopt);
  VerifyDeviceNameMetadata(
      "ChromeOS",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  EXPECT_EQ(3u, GetNumObserverCalls());
}

}  // namespace ash
