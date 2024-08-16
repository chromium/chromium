// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/android/fake_password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
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
              (std::optional<SyncingAccount>, PasswordManagerSetting),
              (override));
  MOCK_METHOD(void,
              SetPasswordSettingValue,
              (std::optional<SyncingAccount>, PasswordManagerSetting, bool),
              (override));
};

}  // namespace

class PasswordManagerSettingsServiceAndroidImplBaseTest : public testing::Test {
 protected:
  PasswordManagerSettingsServiceAndroidImplBaseTest();
  ~PasswordManagerSettingsServiceAndroidImplBaseTest() override;

  void InitializeSettingsService(bool password_sync_enabled,
                                 bool setting_sync_enabled);

  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl> CreateNewService(
      std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper);

  void SetPasswordsSync(bool enabled);
  void SetSettingsSync(bool enabled);

  void ExpectSettingsRetrievalFromBackend(std::optional<SyncingAccount> account,
                                          size_t times);

  void ExpectSettingsRetrievalFromBackend();

  Consumer* updater_bridge_consumer() { return settings_service_.get(); }
  password_manager::PasswordManagerSettingsService* settings_service() {
    return settings_service_.get();
  }
  TestingPrefServiceSimple* pref_service() { return &test_pref_service_; }
  syncer::TestSyncService* sync_service() { return &test_sync_service_; }
  FakePasswordManagerLifecycleHelper* lifecycle_helper() {
    return fake_lifecycle_helper_;
  }
  MockPasswordSettingsUpdaterBridgeHelper* bridge_helper() {
    return mock_bridge_helper_;
  }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  void RegisterPrefs();

  TestingPrefServiceSimple test_pref_service_;
  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl> settings_service_;
  syncer::TestSyncService test_sync_service_;
  raw_ptr<MockPasswordSettingsUpdaterBridgeHelper> mock_bridge_helper_ =
      nullptr;
  raw_ptr<FakePasswordManagerLifecycleHelper> fake_lifecycle_helper_ = nullptr;
  base::HistogramTester histogram_tester_;
};

PasswordManagerSettingsServiceAndroidImplBaseTest::
    PasswordManagerSettingsServiceAndroidImplBaseTest() {
  RegisterPrefs();
  CoreAccountInfo sync_account_info;
  sync_account_info.email = kTestAccount;
  test_sync_service_.SetSignedIn(signin::ConsentLevel::kSync,
                                 sync_account_info);
}

PasswordManagerSettingsServiceAndroidImplBaseTest::
    ~PasswordManagerSettingsServiceAndroidImplBaseTest() {
  testing::Mock::VerifyAndClearExpectations(mock_bridge_helper_);
}

void PasswordManagerSettingsServiceAndroidImplBaseTest::
    InitializeSettingsService(bool password_sync_enabled,
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
  settings_service_ = std::make_unique<
      PasswordManagerSettingsServiceAndroidImpl>(
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplBaseTest>(),
      &test_pref_service_, &test_sync_service_, std::move(bridge_helper),
      std::move(lifecycle_helper));
}

std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl>
PasswordManagerSettingsServiceAndroidImplBaseTest::CreateNewService(
    std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper =
        nullptr) {
  if (!bridge_helper) {
    bridge_helper = std::make_unique<MockPasswordSettingsUpdaterBridgeHelper>();
  }
  std::unique_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper =
      std::make_unique<FakePasswordManagerLifecycleHelper>();
  return std::make_unique<PasswordManagerSettingsServiceAndroidImpl>(
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplBaseTest>(),
      pref_service(), sync_service(), std::move(bridge_helper),
      std::move(lifecycle_helper));
}

void PasswordManagerSettingsServiceAndroidImplBaseTest::SetPasswordsSync(
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

void PasswordManagerSettingsServiceAndroidImplBaseTest::SetSettingsSync(
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

void PasswordManagerSettingsServiceAndroidImplBaseTest::
    ExpectSettingsRetrievalFromBackend(std::optional<SyncingAccount> account,
                                       size_t times) {
  EXPECT_CALL(
      *bridge_helper(),
      GetPasswordSettingValue(
          Eq(account), Eq(PasswordManagerSetting::kOfferToSavePasswords)))
      .Times(times);
  EXPECT_CALL(*bridge_helper(),
              GetPasswordSettingValue(Eq(account),
                                      Eq(PasswordManagerSetting::kAutoSignIn)))
      .Times(times);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kBiometricTouchToFill)) {
    EXPECT_CALL(
        *bridge_helper(),
        GetPasswordSettingValue(
            Eq(account),
            Eq(PasswordManagerSetting::kBiometricReauthBeforePwdFilling)))
        .Times(times);
  }
}

void PasswordManagerSettingsServiceAndroidImplBaseTest::RegisterPrefs() {
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableService, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableAutosignin, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kAutoSignInEnabledGMS, true);
  test_pref_service_.registry()->RegisterStringPref(
      ::prefs::kGoogleServicesLastSyncingUsername, kTestAccount);
  test_pref_service_.registry()->RegisterStringPref(
      ::prefs::kGoogleServicesLastSignedInUsername, kTestAccount);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      false);
  test_pref_service_.registry()->RegisterIntegerPref(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kSettingsMigratedToUPMLocal, false);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kBiometricTouchToFill)) {
    test_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);
  }
}

// This suite starts with the pref `kPasswordsUseUPMLocalAndSeparateStores` off.
class PasswordManagerSettingsServiceAndroidImplTest
    : public PasswordManagerSettingsServiceAndroidImplBaseTest {};

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       DoesntRequestSettingsOnServiceCreation) {
  std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper =
      std::make_unique<MockPasswordSettingsUpdaterBridgeHelper>();

  ASSERT_NE(bridge_helper, nullptr);

  // The settings shouldn't be requested upon creating the service, which
  // happens on startup, because Chrome also gets foregrounded and settings are
  // requested on Chrome foregrounding.
  EXPECT_CALL(*bridge_helper,
              GetPasswordSettingValue(
                  Eq(SyncingAccount(kTestAccount)),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords)))
      .Times(0);
  EXPECT_CALL(*bridge_helper,
              GetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                                      Eq(PasswordManagerSetting::kAutoSignIn)))
      .Times(0);

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
       PasswordSyncEnablingGMSSettingAbsentChromeHasUserSetting) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  pref_service()->SetBoolean(password_manager::prefs::kAutoSignInEnabledGMS,
                             false);

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/1);
  SetPasswordsSync(/*enabled=*/true);
  sync_service()->FireStateChanged();

  // For a syncing user, the setting stored in the account takes precedence and
  // overwrites the local setting, even if the account setting has a default
  // value.
  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kCredentialsEnableAutosignin),
            nullptr);
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kAutoSignInEnabledGMS),
            nullptr);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncEnablingGMSHasSetting) {
  // TODO(crbug.com/40286015): Split this test.
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/1);
  SetPasswordsSync(/*enabled=*/true);
  sync_service()->FireStateChanged();

  // If the local setting in Chrome differs from the setting stored in the
  // account, the account setting is stored in prefs and used.
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       PasswordSyncDisablingGMSSettingAbsent) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  pref_service()->SetBoolean(password_manager::prefs::kAutoSignInEnabledGMS,
                             false);

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/1);
  SetPasswordsSync(/*enabled=*/false);
  sync_service()->FireStateChanged();

  // Settings shouldn't be written to GMS Core.
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service()->GetBoolean(
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
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/1);
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

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
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
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/1);
  settings_service()->RequestSettingsFromBackend();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SettingsAreNotRequestedFromBackendWhenPasswordSyncDisabled) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/0);
  settings_service()->RequestSettingsFromBackend();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       SettingsAreNotRequestedFromBackendWhenUserUnenrolledFromUPM) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/0);
  settings_service()->RequestSettingsFromBackend();
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
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/1);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kAutoSignInEnabledGMS),
            nullptr);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       UnenrollmentPreventsRequestsOnSyncTurningOff) {
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);

  // Since the user was unenrolled and they don't have local backend support, it
  // means that the backend is unreachable and instead the regular prefs are
  // used, so there shouldn't be any backend request.
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount), /*times=*/0);
  SetPasswordsSync(/*enabled=*/false);
  sync_service()->FireStateChanged();
}

// This suite starts with the pref `kPasswordsUseUPMLocalAndSeparateStores` on.
class PasswordManagerSettingsServiceAndroidImplTestLocalUsers
    : public PasswordManagerSettingsServiceAndroidImplBaseTest {
 protected:
  PasswordManagerSettingsServiceAndroidImplTestLocalUsers() {
    pref_service()->SetInteger(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
  }
};

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       RespectsPolicyDespiteErrorOverrideForLocalBackend) {
  // This test checks that the managed pref has a priority over the manually set
  // values and that the password saving isn't suspended for users who are not
  // syncing passwords.
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, false);

  pref_service()->SetManagedPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(true));

  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       UnenrollmentDoesntPreventUPMLocalRequests) {
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  lifecycle_helper()->OnForegroundSessionStart();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       UnenrollmentDoesntPreventUPMLocalOnSettingValueAbsent) {
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue).Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kCredentialsEnableAutosignin),
            nullptr);
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kAutoSignInEnabledGMS),
            nullptr);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       UnenrollmentDoesntPreventUPMLocalOnSettingValueFetched) {
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(*bridge_helper(), SetPasswordSettingValue).Times(0);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_FALSE(pref_service()->GetUserPrefValue(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service()->GetUserPrefValue(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       OnSaveSettingFetchUpdatesTheCacheAndRegularPref) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       OnSaveSettingFetchUpdatesOnlyTheCache) {
  // This test is similar to OnSaveSettingFetchUpdatesTheCacheAndRegularPref,
  // but it shows that the regular pref is not updated if settings sync is on,
  // in order to prevent sync cycles.
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       OnSettingsAbsentUpdatesTheGMSAndRegularPref) {
  // This test covers the case when the cache has a non-default value and GMS
  // has a default value. The user can have non-default values in the cache
  // while they're not yet syncing. When they start syncing, the cache will be
  // overwritten by the value read from GMSCore, which could now be default,
  // since it's now read from the storage for the account.
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, false);

  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kOfferToSavePasswordsEnabledGMS),
            nullptr);
  // The regular pref also needs to be overwritten because the user might drop
  // out of the enabled group and will use the regular pref again.
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kCredentialsEnableService),
            nullptr);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       OnSettingsAbsentUpdatesOnlyTheGMSPref) {
  // This test is similar to OnSettingsAbsentUpdatesTheGMSAndRegularPref, but
  // it shows that the regular pref is not updated if settings sync is on, in
  // order to prevent sync cycles.
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, false);

  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kOfferToSavePasswordsEnabledGMS),
            nullptr);
  // The regular pref should not be overwritten because settings are synced.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       IsSettingEnabledChecksGMSPrefWhenSettingsMigratedToUPMLocal) {
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  pref_service()->SetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, false);

  // The settings migration happened so the setting from CMS Core is used.
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       RequestSettingsFromBackendFetchesSettings) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);

  settings_service()->RequestSettingsFromBackend();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       TurnOffAutoSignInWhenNotSyncingSettingsChangesTheGMSPrefAndRegularPref) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(std::nullopt),
                              Eq(PasswordManagerSetting::kAutoSignIn), false));

  settings_service()->TurnOffAutoSignIn();

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       TurnOffAutoSignInWhenSyncingSettingsChangesOnlyTheGMSPref) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/true);

  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  settings_service()->TurnOffAutoSignIn();

  // The regular pref should not be updated because settings are synced.
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       PasswordSyncDisablingGMSSettingAbsent) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  SetPasswordsSync(/*enabled=*/false);
  sync_service()->FireStateChanged();

  // Settings shouldn't be written to GMS Core.
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kCredentialsEnableAutosignin),
            nullptr);
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kAutoSignInEnabledGMS),
            nullptr);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       PasswordSyncDisablingGMSHasSetting) {
  InitializeSettingsService(/*password_sync_enabled=*/true,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  SetPasswordsSync(/*enabled=*/false);
  sync_service()->FireStateChanged();

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       PasswordSyncEnablingGMSSettingAbsent) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/1);
  SetPasswordsSync(/*enabled=*/true);
  sync_service()->FireStateChanged();

  // Settings shouldn't be written to GMS Core.
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kCredentialsEnableAutosignin),
            nullptr);
  EXPECT_EQ(pref_service()->GetUserPrefValue(
                password_manager::prefs::kAutoSignInEnabledGMS),
            nullptr);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       PasswordSyncEnablingGMSHasSetting) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  // Settings should be requested from GMS Core on sync state change.
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount),
                                     /*times=*/1);
  SetPasswordsSync(/*enabled=*/true);
  sync_service()->FireStateChanged();

  // Settings shouldn't be written to GMS Core.
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, /*value=*/false);

  // The old Chrome pref is also updated, because settings sync is off, so there
  // is no risk of sync cycles.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       DoesntRequestSettingsOnServiceCreation) {
  std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper =
      std::make_unique<MockPasswordSettingsUpdaterBridgeHelper>();

  ASSERT_NE(bridge_helper, nullptr);

  // The settings shouldn't be requested upon creating the service, which
  // happens on startup, because Chrome also gets foregrounded and settings are
  // requested on Chrome foregrounding.
  EXPECT_CALL(*bridge_helper,
              GetPasswordSettingValue(
                  Eq(SyncingAccount(kTestAccount)),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords)))
      .Times(0);
  EXPECT_CALL(*bridge_helper,
              GetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                                      Eq(PasswordManagerSetting::kAutoSignIn)))
      .Times(0);

  SetPasswordsSync(false);
  CreateNewService(std::move(bridge_helper));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       SavePasswordsSettingManagedByCustodian) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetSupervisedUserPref(
      password_manager::prefs::kCredentialsEnableService, base::Value(false));
  pref_service()->SetUserPref(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
      base::Value(true));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsChromeHadNonDefaultValueGmsHadDefaultValue) {
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(std::nullopt),
                              Eq(PasswordManagerSetting::kAutoSignIn), false));
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  Eq(std::nullopt),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords), false));
  lifecycle_helper()->OnForegroundSessionStart();

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, true);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, true);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  updater_bridge_consumer()->OnSuccessfulSettingChange(
      PasswordManagerSetting::kAutoSignIn);
  updater_bridge_consumer()->OnSuccessfulSettingChange(
      PasswordManagerSetting::kOfferToSavePasswords);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));

  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 1);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsChromeAndGmsHadNonDefaultValue) {
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  pref_service()->SetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, false);
  pref_service()->SetBoolean(password_manager::prefs::kAutoSignInEnabledGMS,
                             false);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(std::nullopt),
                              Eq(PasswordManagerSetting::kAutoSignIn), false));
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  Eq(std::nullopt),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords), false));
  lifecycle_helper()->OnForegroundSessionStart();

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, false);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, false);

  updater_bridge_consumer()->OnSuccessfulSettingChange(
      PasswordManagerSetting::kAutoSignIn);
  updater_bridge_consumer()->OnSuccessfulSettingChange(
      PasswordManagerSetting::kOfferToSavePasswords);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));

  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 1);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       SettingsMigrationNotStartedIfCompletedBefore) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  Eq(std::nullopt),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  Eq(std::nullopt), Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);
  lifecycle_helper()->OnForegroundSessionStart();

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, /*value=*/false);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, /*value=*/false);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsChromeAndGMSHadDefaultValue) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->ClearPref(
      password_manager::prefs::kCredentialsEnableAutosignin);

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  lifecycle_helper()->OnForegroundSessionStart();

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kAutoSignIn);
  updater_bridge_consumer()->OnSettingValueAbsent(
      PasswordManagerSetting::kOfferToSavePasswords);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));

  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));

  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 1);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrationCallbackNotCreatedWhenChromeAndGMSHadDefaultValue) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->ClearPref(
      password_manager::prefs::kCredentialsEnableAutosignin);

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  lifecycle_helper()->OnForegroundSessionStart();

  // The Chrome settings were default, so the migration is marked as done.
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));
  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 1);

  // If the migration callback would have been created, despite the migration
  // being marked as done, the following fetching errors would have been
  // recorded in histograms. This is the only possible way to test if the
  // callback was unnecessarily created because it can't be accessed directly
  // and there are no mocks that would be related to the callback that can be
  // tested to verify if the callback was created.
  updater_bridge_consumer()->OnSettingFetchingError(
      PasswordManagerSetting::kAutoSignIn,
      AndroidBackendAPIErrorCode::kUnexpectedError);
  updater_bridge_consumer()->OnSettingFetchingError(
      PasswordManagerSetting::kOfferToSavePasswords,
      AndroidBackendAPIErrorCode::kUnexpectedError);

  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 1);
  histogram_tester()->ExpectBucketCount(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", false, 0);
  histogram_tester()->ExpectBucketCount(
      "PasswordManager.PasswordSettingsMigrationFailed.AutoSignIn.APIError2",
      AndroidBackendAPIErrorCode::kUnexpectedError, 0);
  histogram_tester()->ExpectBucketCount(
      "PasswordManager.PasswordSettingsMigrationFailed.OfferToSavePasswords."
      "APIError2",
      AndroidBackendAPIErrorCode::kUnexpectedError, 0);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       SettingsMigrationNotPerformedWhenInterruptedByTurningOnSync) {
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);

  // The responses from GMS Core didn't yet come so the migration doesn't happen
  // yet.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount), /*times=*/1);
  SetPasswordsSync(true);
  sync_service()->FireStateChanged();

  // The local settings migraton should not be performed, even though these
  // calls will try to start it, because the user is syncing now.
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, false);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));
  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 0);
  histogram_tester()->ExpectBucketCount(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", false, 0);
}

// Checks if settings migration happens only once per browser lifetime.
TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsChromePutToForegroundTwice) {
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->ClearPref(
      password_manager::prefs::kCredentialsEnableAutosignin);

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/2);
  lifecycle_helper()->OnForegroundSessionStart();

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, true);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, true);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  // Chrome gets put to the background and to the foreground again.
  lifecycle_helper()->OnForegroundSessionStart();

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, false);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));

  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 1);
}

// Checks that the migration algorithm determines what to do for each setting
// independently from the other.
TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsEachPrefHadDifferentValue) {
  // User has changed one setting.
  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(std::nullopt),
                              Eq(PasswordManagerSetting::kAutoSignIn), false));
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  lifecycle_helper()->OnForegroundSessionStart();

  // GMS also had one setting changed, but it is different than the one in
  // Chrome.
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, true);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  updater_bridge_consumer()->OnSuccessfulSettingChange(
      PasswordManagerSetting::kAutoSignIn);

  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 1);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsGettingValueFailed) {
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, true);

  // The cached values are true until settings get fetched from GMS Core.
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  lifecycle_helper()->OnForegroundSessionStart();

  // Fetching the auto sign in setting fails.
  updater_bridge_consumer()->OnSettingFetchingError(
      PasswordManagerSetting::kAutoSignIn,
      AndroidBackendAPIErrorCode::kUnexpectedError);
  // Fetching the offer to save passwords setting succeeds and is different than
  // the value in the Chrome setting.
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, true);

  // Old Chrome settings stay intact.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));

  // The Cached GMS values in Chrome get updated on a successful fetch so offer
  // to save passwords is now false.
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  // The Chrome values get returned if the migration failed.
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));

  // Settings migration wasn't successful.
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationFailed.AutoSignIn.APIError2",
      AndroidBackendAPIErrorCode::kUnexpectedError, 1);
  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", false, 1);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsMarkedAsDoneIfChromeHadDefaultPrefs) {
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->ClearPref(
      password_manager::prefs::kCredentialsEnableAutosignin);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", true, 1);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrationNotDoneIfDefaultSettingsButNoStoreSplit) {
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->ClearPref(
      password_manager::prefs::kCredentialsEnableAutosignin);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));

  histogram_tester()->ExpectTotalCount(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", 0);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsGotValueForTheSamePrefTwice) {
  // Set prefs to user selected value so that the migration is triggered
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             true);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, true);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  _, Eq(PasswordManagerSetting::kOfferToSavePasswords), _))
      .Times(0);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(_, Eq(PasswordManagerSetting::kAutoSignIn), _))
      .Times(0);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  lifecycle_helper()->OnForegroundSessionStart();

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, false);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));
  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", false, 1);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       DefaultSettingsMigrationResetsIfLocalPwdMigrationOff) {
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->ClearPref(
      password_manager::prefs::kCredentialsEnableAutosignin);

  // Setting the settings migration pref to true to mimic the case when it was
  // incorrectly set to true.
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       DefaultSettingsMigrationDoesntResetIfLocalPwdMigrationPending) {
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::
              kOffAndMigrationPending));
  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->ClearPref(
      password_manager::prefs::kCredentialsEnableAutosignin);

  // Setting the settings migration pref to true to mimic the case when it was
  // already set to true.
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       DefaultSettingsMigrationDoesntResetIfLocalPwdMigrationOn) {
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
  pref_service()->ClearPref(password_manager::prefs::kCredentialsEnableService);
  pref_service()->ClearPref(
      password_manager::prefs::kCredentialsEnableAutosignin);

  // Setting the settings migration pref to true to mimic the case when it was
  // already set to true.
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);

  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       MigrateSettingsSettingGmsPrefFailed) {
  pref_service()->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, false);
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  pref_service()->SetBoolean(password_manager::prefs::kAutoSignInEnabledGMS,
                             true);
  pref_service()->SetBoolean(
      password_manager::prefs::kOfferToSavePasswordsEnabledGMS, true);
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  EXPECT_CALL(
      *bridge_helper(),
      SetPasswordSettingValue(Eq(std::nullopt),
                              Eq(PasswordManagerSetting::kAutoSignIn), false));
  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(
                  Eq(std::nullopt),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords), false));
  lifecycle_helper()->OnForegroundSessionStart();

  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kAutoSignIn, true);
  updater_bridge_consumer()->OnSettingValueFetched(
      PasswordManagerSetting::kOfferToSavePasswords, true);

  updater_bridge_consumer()->OnSuccessfulSettingChange(
      PasswordManagerSetting::kAutoSignIn);
  updater_bridge_consumer()->OnFailedSettingChange(
      PasswordManagerSetting::kOfferToSavePasswords,
      AndroidBackendAPIErrorCode::kUnexpectedError);

  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal));
  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationFailed.OfferToSavePasswords."
      "APIError2",
      AndroidBackendAPIErrorCode::kUnexpectedError, 1);
  histogram_tester()->ExpectUniqueSample(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", false, 1);
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTestLocalUsers,
       FetchesBiometricAuthenticationBeforeFillingSetting) {
  base::test::ScopedFeatureList scoped_feature_list{
      password_manager::features::kBiometricTouchToFill};
  InitializeSettingsService(/*password_sync_enabled=*/false,
                            /*setting_sync_enabled=*/false);

  ExpectSettingsRetrievalFromBackend(std::nullopt, /*times=*/1);
  lifecycle_helper()->OnForegroundSessionStart();
}
