// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/android/fake_password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using password_manager::FakePasswordManagerLifecycleHelper;
using password_manager::PasswordManagerSetting;
using password_manager::PasswordSettingsUpdaterAndroidBridgeHelper;
using Consumer =
    password_manager::PasswordSettingsUpdaterAndroidReceiverBridge::Consumer;
using SyncingAccount = password_manager::
    PasswordSettingsUpdaterAndroidReceiverBridge::SyncingAccount;
using testing::_;
using testing::Eq;

const char kTestAccount[] = "testaccount@gmail.com";

class MockPasswordSettingsUpdaterBridgeHelper
    : public PasswordSettingsUpdaterAndroidBridgeHelper {
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

  void InitializeSettingsService(bool password_sync_enabled,
                                 bool setting_sync_enabled);

  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl> CreateNewService(
      std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper);

  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl>
  GetServiceWithoutBackend();

  void SetPasswordsSync(bool enabled);
  void SetSettingsSync(bool enabled);

  void ExpectSettingsRetrievalFromBackend(size_t times);

  void ExpectSettingsRetrievalFromBackend();

  Consumer* updater_bridge_consumer() { return settings_service_.get(); }
  PasswordManagerSettingsService* settings_service() {
    return settings_service_.get();
  }
  TestingPrefServiceSimple* pref_service() { return &test_pref_service_; }
  syncer::TestSyncService* sync_service() { return &test_sync_service_; }
  MockPasswordSettingsUpdaterBridgeHelper* bridge_helper() {
    return mock_bridge_helper_;
  }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  void RegisterPrefs();

  CoreAccountInfo sync_account_info_;
  TestingPrefServiceSimple test_pref_service_;
  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl> settings_service_;
  syncer::TestSyncService test_sync_service_;
  raw_ptr<MockPasswordSettingsUpdaterBridgeHelper> mock_bridge_helper_ =
      nullptr;
  raw_ptr<FakePasswordManagerLifecycleHelper> fake_lifecycle_helper_ = nullptr;
  base::HistogramTester histogram_tester_;
};

PasswordManagerSettingsServiceAndroidImplTest::
    PasswordManagerSettingsServiceAndroidImplTest() {
  RegisterPrefs();
  sync_account_info_.email = kTestAccount;
  test_sync_service_.SetAccountInfo(sync_account_info_);
}

PasswordManagerSettingsServiceAndroidImplTest::
    ~PasswordManagerSettingsServiceAndroidImplTest() {
  testing::Mock::VerifyAndClearExpectations(mock_bridge_helper_);
}

void PasswordManagerSettingsServiceAndroidImplTest::InitializeSettingsService(
    bool password_sync_enabled,
    bool setting_sync_enabled) {
  std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper =
      std::make_unique<MockPasswordSettingsUpdaterBridgeHelper>();
  mock_bridge_helper_ = bridge_helper.get();

  EXPECT_CALL(*mock_bridge_helper_, SetConsumer);

  std::unique_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper =
      std::make_unique<FakePasswordManagerLifecycleHelper>();
  fake_lifecycle_helper_ = lifecycle_helper.get();

  SetPasswordsSync(password_sync_enabled);
  SetSettingsSync(setting_sync_enabled);
  settings_service_ =
      std::make_unique<PasswordManagerSettingsServiceAndroidImpl>(
          base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>(),
          &test_pref_service_, &test_sync_service_, std::move(bridge_helper),
          std::move(lifecycle_helper));
}

std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl>
PasswordManagerSettingsServiceAndroidImplTest::GetServiceWithoutBackend() {
  return std::make_unique<PasswordManagerSettingsServiceAndroidImpl>(
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>(),
      pref_service(), sync_service(), nullptr, nullptr);
}

std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl>
PasswordManagerSettingsServiceAndroidImplTest::CreateNewService(
    std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper =
        nullptr) {
  if (!bridge_helper) {
    bridge_helper = std::make_unique<MockPasswordSettingsUpdaterBridgeHelper>();
  }
  DCHECK(bridge_helper);
  std::unique_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper =
      std::make_unique<FakePasswordManagerLifecycleHelper>();
  return std::make_unique<PasswordManagerSettingsServiceAndroidImpl>(
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>(),
      pref_service(), sync_service(), std::move(bridge_helper),
      std::move(lifecycle_helper));
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

void PasswordManagerSettingsServiceAndroidImplTest::
    ExpectSettingsRetrievalFromBackend(size_t times) {
  EXPECT_CALL(*bridge_helper(),
              GetPasswordSettingValue(
                  Eq(SyncingAccount(kTestAccount)),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords)))
      .Times(times);
  EXPECT_CALL(*bridge_helper(),
              GetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                                      Eq(PasswordManagerSetting::kAutoSignIn)))
      .Times(times);
}

void PasswordManagerSettingsServiceAndroidImplTest::RegisterPrefs() {
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableService, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableAutosignin, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kSavePasswordsSuspendedByError, false);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kAutoSignInEnabledGMS, true);
  test_pref_service_.registry()->RegisterStringPref(
      ::prefs::kGoogleServicesLastSyncingUsername, kTestAccount);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      false);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       RequestsSettingsOnServiceCreation) {
  std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper =
      std::make_unique<MockPasswordSettingsUpdaterBridgeHelper>();

  ASSERT_NE(bridge_helper, nullptr);

  EXPECT_CALL(*bridge_helper,
              GetPasswordSettingValue(
                  Eq(SyncingAccount(kTestAccount)),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords)));
  EXPECT_CALL(*bridge_helper,
              GetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                                      Eq(PasswordManagerSetting::kAutoSignIn)));

  SetPasswordsSync(true);
  CreateNewService(std::move(bridge_helper));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingFetchSyncingBoth) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
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
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);
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
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
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
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
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
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);
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
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
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
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingAbsentDoesntSetValueSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  // The settings for syncing users should no longer be written to GMSCore.
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  Eq(SyncingAccount(kTestAccount)),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords), false))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingAbsentSetValueNotSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnSaveSettingAbsentUserUnenrolledFromUPM) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInAbsentDefaultSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInAbsentDontSetValueSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));

  // The settings for syncing users should no longer be written to GmsCore.
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                              Eq(PasswordManagerSetting::kAutoSignIn), false))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInAbsentSetValueNotSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));
  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OnAutoSignInAbsentSetValueUserUnenrolledFromUPM) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));
  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue(_, _, _)).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);
}

// Checks that general syncable prefs are dumped into the android-only GMS
// prefs before settings are requested when sync is enabled.
TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncEnablingDoesntMovePrefs) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(false));
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableAutosignin,
      base::Value(false));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  SetPasswordsSync(/*enabled=*/true);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

// Checks that general syncable prefs are dumped into the android-only GMS
// prefs before settings are requested when sync is enabled.
TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncEnablingPrefsUserUnenrolledFromUPM) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(false));
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableAutosignin,
      base::Value(false));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  SetPasswordsSync(/*enabled=*/true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncEnablingGMSSettingAbsentChromeSettingDefault) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(/*times=*/1);
  SetPasswordsSync(/*enabled=*/true);
  sync_service()->FireStateChanged();

  // If there is no user setting stored both in GMS Core and in Chrome,
  // no setting should be changed in GMS Core.
  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncEnablingGMSSettingAbsentChromeHasUserSetting) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(/*times=*/1);
  SetPasswordsSync(/*enabled=*/true);
  sync_service()->FireStateChanged();

  // If there is no user setting stored in GMS Core, Chrome setting should not
  // be set in it.
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                              Eq(PasswordManagerSetting::kAutoSignIn), false))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncEnablingGMSHasSetting) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(/*times=*/1);
  SetPasswordsSync(/*enabled=*/true);
  sync_service()->FireStateChanged();

  // If the setting in Chrome differs from the setting in GMS Core, GMS Core
  // setting is stored in prefs and used.
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncDisablingGMSSettingAbsent) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(/*times=*/1);
  SetPasswordsSync(/*enabled=*/false);
  sync_service()->FireStateChanged();

  // If there is no user setting stored in GMS Core, nothing should happen.
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncDisablingGMSHasSetting) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(/*times=*/1);
  SetPasswordsSync(/*enabled=*/false);
  sync_service()->FireStateChanged();

  // If the setting in Chrome differs from the setting in GMS Core, GMS Core
  // setting is stored in prefs and used.
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SavePasswordsSettingNotSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SavePasswordsSettingSyncingManaged) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetManagedPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(false));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(true));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SavePasswordsSettingSyncingNotManagedNoBackend) {
  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl>
      no_backend_service = GetServiceWithoutBackend();

  SetPasswordsSync(/*enabled=*/true);

  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  EXPECT_TRUE(no_backend_service->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SavePasswordsSettingSyncingNotManaged) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SavePasswordsSettingUserUnenrolledFromUPM) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(false));
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       AutoSignInSettingNotSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableAutosignin, base::Value(true));
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       AutoSignInSettingSyncingManaged) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetManagedPref(
      password_manager::prefs::kCredentialsEnableAutosignin,
      base::Value(false));
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(true));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       AutoSignInSettingSyncingNotManagedNoBackend) {
  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl>
      no_backend_service = GetServiceWithoutBackend();

  SetPasswordsSync(/*enabled=*/true);

  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableAutosignin, base::Value(true));
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));
  EXPECT_TRUE(no_backend_service->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       AutoSignInSettingSyncingNotManaged) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableAutosignin, base::Value(true));
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       AutoSignInSettingUserUnenrolledFromUPM) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableAutosignin, base::Value(true));
  pref_service()->SetUserPref(password_manager::prefs::kAutoSignInEnabledGMS,
                              base::Value(false));
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SettingsAreRequestedFromBackendWhenPasswordSyncEnabled) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  ExpectSettingsRetrievalFromBackend(/*times=*/1);
  settings_service()->RequestSettingsFromBackend();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SettingsAreNotRequestedFromBackendWhenPasswordSyncDisabled) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  ExpectSettingsRetrievalFromBackend(/*times=*/0);
  settings_service()->RequestSettingsFromBackend();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SettingsAreNotRequestedFromBackendWhenUserUnenrolledFromUPM) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  ExpectSettingsRetrievalFromBackend(/*times=*/0);
  settings_service()->RequestSettingsFromBackend();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       TurnOffAutoSignInNoBackend) {
  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl>
      no_backend_service = GetServiceWithoutBackend();
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  no_backend_service->TurnOffAutoSignIn();
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       TurnOffAutoSignInNotSyncingPasswords) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);
  settings_service()->TurnOffAutoSignIn();
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       TurnOffAutoSignInSyncingPasswordsNotPrefs) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                              Eq(PasswordManagerSetting::kAutoSignIn), false))
      .Times(1);
  settings_service()->TurnOffAutoSignIn();
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       TurnOffAutoSignInSyncingPasswordsAndPrefs) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                              Eq(PasswordManagerSetting::kAutoSignIn), false))
      .Times(1);
  settings_service()->TurnOffAutoSignIn();
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       TurnOffAutoSignInSyncingUserUnenrolledFromUPM) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);
  settings_service()->TurnOffAutoSignIn();
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OverrideOfferToSaveForError) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(true));
  pref_service()->SetBoolean(
      password_manager::prefs::kSavePasswordsSuspendedByError, true);

  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OverrideOfferToSaveForErrorWhenNotSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(true));
  pref_service()->SetBoolean(
      password_manager::prefs::kSavePasswordsSuspendedByError, true);
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       OverrideOfferToSaveForErrorWhenManaged) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(true));
  pref_service()->SetBoolean(
      password_manager::prefs::kSavePasswordsSuspendedByError, true);
  // Saving passwords is allowed by policy but errors enforce the stricter rule.
  pref_service()->SetManagedPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       IgnoreOverrideOfferToSaveForErrorWhenUnenrolled) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(true));
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  pref_service()->SetBoolean(
      password_manager::prefs::kSavePasswordsSuspendedByError, true);
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       TestDontMigrateSettingsOnReenrollingIntoUPM) {
  SetPasswordsSync(true);

  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  // Set an explicit value on the "Offer to save passwords" pref.
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);

  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);

  // Imitate reenrolment into UPM and triggering settings migration.
  ExpectSettingsRetrievalFromBackend(/*times=*/1);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kAutoSignInEnabledGMS),
            nullptr);
}
