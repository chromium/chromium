// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/android/fake_password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using password_manager::FakePasswordManagerLifecycleHelper;
using password_manager::PasswordManagerSetting;
using password_manager::PasswordSettingsUpdaterAndroidBridge;
using testing::_;
using testing::Eq;

const char kTestAccount[] = "testaccount@gmail.com";

class MockPasswordSettingsUpdaterBridge
    : public PasswordSettingsUpdaterAndroidBridge {
 public:
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), (override));
  MOCK_METHOD(void,
              GetPasswordSettingValue,
              (absl::optional<SyncingAccount>, PasswordManagerSetting),
              (override));
  MOCK_METHOD(void,
              SetPasswordSettingValue,
              (absl::optional<SyncingAccount>, PasswordManagerSetting, bool),
              (override));
};

}  // namespace

class PasswordManagerSettingsServiceAndroidImplTest : public testing::Test {
 protected:
  PasswordManagerSettingsServiceAndroidImplTest();
  ~PasswordManagerSettingsServiceAndroidImplTest() override;

  void SetPasswordsSync(bool enabled);
  void SetSettingsSync(bool enabled);

  PasswordSettingsUpdaterAndroidBridge::Consumer* updater_bridge_consumer() {
    return updater_.get();
  }
  TestingPrefServiceSimple* pref_service() { return &test_pref_service_; }
  syncer::TestSyncService* sync_service() { return &test_sync_service_; }
  MockPasswordSettingsUpdaterBridge* bridge() { return mock_bridge_; }

 private:
  void RegisterPrefs();

  CoreAccountInfo sync_account_info_;
  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl> updater_;
  TestingPrefServiceSimple test_pref_service_;
  syncer::TestSyncService test_sync_service_;
  raw_ptr<MockPasswordSettingsUpdaterBridge> mock_bridge_;
  raw_ptr<FakePasswordManagerLifecycleHelper> fake_lifecycle_helper_;
};

PasswordManagerSettingsServiceAndroidImplTest::
    PasswordManagerSettingsServiceAndroidImplTest() {
  RegisterPrefs();
  sync_account_info_.email = kTestAccount;
  test_sync_service_.SetAccountInfo(sync_account_info_);
  SetPasswordsSync(true);
  SetSettingsSync(true);

  std::unique_ptr<MockPasswordSettingsUpdaterBridge> bridge =
      std::make_unique<MockPasswordSettingsUpdaterBridge>();
  mock_bridge_ = bridge.get();

  std::unique_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper =
      std::make_unique<FakePasswordManagerLifecycleHelper>();
  fake_lifecycle_helper_ = lifecycle_helper.get();

  updater_ = std::make_unique<PasswordManagerSettingsServiceAndroidImpl>(
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>(),
      &test_pref_service_, &test_sync_service_, std::move(bridge),
      std::move(lifecycle_helper));
}

PasswordManagerSettingsServiceAndroidImplTest::
    ~PasswordManagerSettingsServiceAndroidImplTest() {
  testing::Mock::VerifyAndClearExpectations(mock_bridge_);
}

void PasswordManagerSettingsServiceAndroidImplTest::SetPasswordsSync(
    bool enabled) {
  syncer::UserSelectableTypeSet selected_sync_types =
      test_sync_service_.GetUserSettings()->GetSelectedTypes();
  if (enabled) {
    selected_sync_types.Put(syncer::UserSelectableType::kPasswords);
  } else {
    selected_sync_types.Remove(syncer::UserSelectableType::kPasswords);
  }
  test_sync_service_.GetUserSettings()->SetSelectedTypes(false,
                                                         selected_sync_types);
}

void PasswordManagerSettingsServiceAndroidImplTest::SetSettingsSync(
    bool enabled) {
  syncer::UserSelectableTypeSet selected_sync_types =
      test_sync_service_.GetUserSettings()->GetSelectedTypes();
  if (enabled) {
    selected_sync_types.Put(syncer::UserSelectableType::kPreferences);
  } else {
    selected_sync_types.Remove(syncer::UserSelectableType::kPreferences);
  }
  test_sync_service_.GetUserSettings()->SetSelectedTypes(false,
                                                         selected_sync_types);
}

void PasswordManagerSettingsServiceAndroidImplTest::RegisterPrefs() {
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableService, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableAutosignin, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kAutoSignInEnabledGMS, true);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingFetchSyncingBoth) {
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingFetchNotSyncingSettings) {
  SetSettingsSync(/*enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingFetchNotSyncingPasswords) {
  SetPasswordsSync(/*enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInSettingFetchSyncingBoth) {
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, /*value=*/false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInFetchNotSyncingSettings) {
  SetSettingsSync(/*enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, /*value=*/false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInFetchNotSyncingPasswords) {
  SetPasswordsSync(/*enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, /*value=*/false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingAbsentDefaultSyncing) {
  EXPECT_CALL(*bridge(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingAbsentSetValueSyncing) {
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  EXPECT_CALL(*bridge(),
              SetPasswordSettingValue(
                  Eq(PasswordSettingsUpdaterAndroidBridge::SyncingAccount(
                      kTestAccount)),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords), false));
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingAbsentSetValueNotSyncing) {
  SetPasswordsSync(/*enabled=*/false);
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  EXPECT_CALL(*bridge(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInAbsentDefaultSyncing) {
  EXPECT_CALL(*bridge(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInAbsentSetValueSyncing) {
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));
  EXPECT_CALL(*bridge(),
              SetPasswordSettingValue(
                  Eq(PasswordSettingsUpdaterAndroidBridge::SyncingAccount(
                      kTestAccount)),
                  Eq(PasswordManagerSetting::kAutoSignIn), false));
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInAbsentSetValueNotSyncing) {
  SetPasswordsSync(/*enabled=*/false);
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));
  EXPECT_CALL(*bridge(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);
}
