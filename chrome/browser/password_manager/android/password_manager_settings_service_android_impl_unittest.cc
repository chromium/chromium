// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
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

class PasswordManagerSettingsServiceAndroidImplTest : public testing::Test {
 protected:
  PasswordManagerSettingsServiceAndroidImplTest();

  void InitializeSettingsService(bool password_sync_enabled);

  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl> CreateNewService(
      std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper);

  void SetPasswordsSync(bool enabled);

  void ExpectSettingsRetrievalFromBackend(
      std::optional<SyncingAccount> account);

  Consumer* updater_bridge_consumer() { return settings_service_.get(); }
  password_manager::PasswordManagerSettingsService* settings_service() {
    return settings_service_.get();
  }
  TestingPrefServiceSimple& pref_service() { return test_pref_service_; }
  syncer::TestSyncService& sync_service() { return test_sync_service_; }
  FakePasswordManagerLifecycleHelper* lifecycle_helper() {
    return fake_lifecycle_helper_;
  }
  MockPasswordSettingsUpdaterBridgeHelper* bridge_helper() {
    return mock_bridge_helper_;
  }

 private:
  void RegisterPrefs();
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple test_pref_service_;
  std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl> settings_service_;
  syncer::TestSyncService test_sync_service_;
  raw_ptr<MockPasswordSettingsUpdaterBridgeHelper> mock_bridge_helper_ =
      nullptr;
  raw_ptr<FakePasswordManagerLifecycleHelper> fake_lifecycle_helper_ = nullptr;
};

PasswordManagerSettingsServiceAndroidImplTest::
    PasswordManagerSettingsServiceAndroidImplTest() {
  RegisterPrefs();
  CoreAccountInfo sync_account_info;
  sync_account_info.email = kTestAccount;
  test_sync_service_.SetSignedIn(signin::ConsentLevel::kSync,
                                 sync_account_info);
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);
}

void PasswordManagerSettingsServiceAndroidImplTest::InitializeSettingsService(
    bool password_sync_enabled) {
  std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper =
      std::make_unique<MockPasswordSettingsUpdaterBridgeHelper>();
  mock_bridge_helper_ = bridge_helper.get();

  EXPECT_CALL(*mock_bridge_helper_, SetConsumer);

  std::unique_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper =
      std::make_unique<FakePasswordManagerLifecycleHelper>();
  fake_lifecycle_helper_ = lifecycle_helper.get();

  SetPasswordsSync(password_sync_enabled);
  settings_service_ =
      std::make_unique<PasswordManagerSettingsServiceAndroidImpl>(
          base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>(),
          &test_pref_service_, &test_sync_service_, std::move(bridge_helper),
          std::move(lifecycle_helper));
}

std::unique_ptr<PasswordManagerSettingsServiceAndroidImpl>
PasswordManagerSettingsServiceAndroidImplTest::CreateNewService(
    std::unique_ptr<MockPasswordSettingsUpdaterBridgeHelper> bridge_helper) {
  return std::make_unique<PasswordManagerSettingsServiceAndroidImpl>(
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>(),
      &pref_service(), &sync_service(), std::move(bridge_helper),
      std::make_unique<FakePasswordManagerLifecycleHelper>());
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

void PasswordManagerSettingsServiceAndroidImplTest::
    ExpectSettingsRetrievalFromBackend(std::optional<SyncingAccount> account) {
  EXPECT_CALL(
      *bridge_helper(),
      GetPasswordSettingValue(
          Eq(account), Eq(PasswordManagerSetting::kOfferToSavePasswords)));

  EXPECT_CALL(*bridge_helper(),
              GetPasswordSettingValue(Eq(account),
                                      Eq(PasswordManagerSetting::kAutoSignIn)));

  if (base::FeatureList::IsEnabled(
          password_manager::features::kBiometricTouchToFill)) {
    EXPECT_CALL(
        *bridge_helper(),
        GetPasswordSettingValue(
            Eq(account),
            Eq(PasswordManagerSetting::kBiometricReauthBeforePwdFilling)));
  }
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
  if (base::FeatureList::IsEnabled(
          password_manager::features::kBiometricTouchToFill)) {
    test_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);
  }
}

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
       RequestSettingsFromBackendWhenSyncGetsDisabled) {
  InitializeSettingsService(
      /*password_sync_enabled=*/true);
  ExpectSettingsRetrievalFromBackend(std::nullopt);

  SetPasswordsSync(false);
  sync_service().FireStateChanged();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       RequestSettingsFromBackendWhenSyncGetsEnabled) {
  InitializeSettingsService(
      /*password_sync_enabled=*/false);
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount));
  SetPasswordsSync(true);
  sync_service().FireStateChanged();
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest, TurnOffAutoSignInLocal) {
  InitializeSettingsService(/*password_sync_enabled=*/false);
  ASSERT_TRUE(pref_service().GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service().GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(Eq(std::nullopt),
                                      Eq(PasswordManagerSetting::kAutoSignIn),
                                      Eq(false)));
  settings_service()->TurnOffAutoSignIn();
  EXPECT_TRUE(pref_service().GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service().GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}

TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       TurnOffAutoSignInAccount) {
  InitializeSettingsService(/*password_sync_enabled=*/true);
  ASSERT_TRUE(pref_service().GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  ASSERT_TRUE(pref_service().GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));

  EXPECT_CALL(*bridge_helper(),
              SetPasswordSettingValue(Eq(SyncingAccount(kTestAccount)),
                                      Eq(PasswordManagerSetting::kAutoSignIn),
                                      Eq(false)));
  settings_service()->TurnOffAutoSignIn();
  EXPECT_TRUE(pref_service().GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  EXPECT_FALSE(pref_service().GetBoolean(
      password_manager::prefs::kAutoSignInEnabledGMS));
}
TEST_F(PasswordManagerSettingsServiceAndroidImplTest,
       FetchesBiometricAuthenticationBeforeFillingSetting) {
  base::test::ScopedFeatureList scoped_feature_list{
      password_manager::features::kBiometricTouchToFill};
  InitializeSettingsService(/*password_sync_enabled=*/true);
  // The account param should be consistent with the sync status, but for this
  // particular setting, the value is not storage-scoped.
  ExpectSettingsRetrievalFromBackend(SyncingAccount(kTestAccount));
  lifecycle_helper()->OnForegroundSessionStart();
}

struct SettingAndSyncingParam {
  std::string old_chrome_pref;
  std::string gms_cache_pref;
  PasswordManagerSetting setting;
  bool syncing;
};

class PasswordManagerSettingsServiceAndroidPerSettingTest
    : public PasswordManagerSettingsServiceAndroidImplTest,
      public testing::WithParamInterface<SettingAndSyncingParam> {};

TEST_P(PasswordManagerSettingsServiceAndroidPerSettingTest, OnSettingFetch) {
  InitializeSettingsService(/*password_sync_enabled=*/GetParam().syncing);
  ASSERT_TRUE(pref_service().GetBoolean(GetParam().old_chrome_pref));
  ASSERT_TRUE(pref_service().GetBoolean(GetParam().gms_cache_pref));

  updater_bridge_consumer()->OnSettingValueFetched(GetParam().setting,
                                                   /*value=*/false);
  // Only the GMS prefs should change.
  EXPECT_FALSE(pref_service().GetBoolean(GetParam().gms_cache_pref));
  EXPECT_TRUE(pref_service().GetBoolean(GetParam().old_chrome_pref));
}

TEST_P(PasswordManagerSettingsServiceAndroidPerSettingTest,
       OnSettingFetchNotSyncing) {
  InitializeSettingsService(/*password_sync_enabled=*/GetParam().syncing);
  ASSERT_TRUE(pref_service().GetBoolean(GetParam().old_chrome_pref));
  ASSERT_TRUE(pref_service().GetBoolean(GetParam().gms_cache_pref));

  updater_bridge_consumer()->OnSettingValueFetched(GetParam().setting,
                                                   /*value=*/false);
  // Only the GMS prefs should change.
  EXPECT_FALSE(pref_service().GetBoolean(GetParam().gms_cache_pref));
  EXPECT_TRUE(pref_service().GetBoolean(GetParam().old_chrome_pref));
}

TEST_P(PasswordManagerSettingsServiceAndroidPerSettingTest,
       OnSaveSettingAbsentSyncingUpdatesCache) {
  InitializeSettingsService(/*password_sync_enabled=*/GetParam().syncing);
  pref_service().SetUserPref(GetParam().old_chrome_pref, base::Value(true));
  pref_service().SetUserPref(GetParam().gms_cache_pref, base::Value(true));

  updater_bridge_consumer()->OnSettingValueAbsent(GetParam().setting);

  // Only the GMS prefs should change.
  EXPECT_TRUE(pref_service()
                  .FindPreference(GetParam().gms_cache_pref)
                  ->IsDefaultValue());
  EXPECT_FALSE(pref_service()
                   .FindPreference(GetParam().old_chrome_pref)
                   ->IsDefaultValue());
}

TEST_P(PasswordManagerSettingsServiceAndroidPerSettingTest,
       IsSettingEnabledManaged) {
  InitializeSettingsService(/*password_sync_enabled=*/GetParam().syncing);
  pref_service().SetManagedPref(GetParam().old_chrome_pref, base::Value(false));
  pref_service().SetUserPref(GetParam().gms_cache_pref, base::Value(true));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(GetParam().setting));
}

TEST_P(PasswordManagerSettingsServiceAndroidPerSettingTest,
       IsSettingEnabledNotManaged) {
  InitializeSettingsService(/*password_sync_enabled=*/true);
  pref_service().SetUserPref(GetParam().old_chrome_pref, base::Value(true));
  pref_service().SetUserPref(GetParam().gms_cache_pref, base::Value(false));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(GetParam().setting));
}

TEST_P(PasswordManagerSettingsServiceAndroidPerSettingTest,
       IsSettingEnabledManagedByCustodian) {
  InitializeSettingsService(GetParam().syncing);
  pref_service().SetSupervisedUserPref(GetParam().old_chrome_pref,
                                       base::Value(false));
  pref_service().SetUserPref(GetParam().gms_cache_pref, base::Value(true));
  EXPECT_FALSE(settings_service()->IsSettingEnabled(GetParam().setting));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordManagerSettingsServiceAndroidPerSettingTest,
    testing::Values(
        SettingAndSyncingParam{
            .old_chrome_pref =
                password_manager::prefs::kCredentialsEnableService,
            .gms_cache_pref =
                password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
            .setting = PasswordManagerSetting::kOfferToSavePasswords,
            .syncing = true},

        SettingAndSyncingParam{
            .old_chrome_pref =
                password_manager::prefs::kCredentialsEnableService,
            .gms_cache_pref =
                password_manager::prefs::kOfferToSavePasswordsEnabledGMS,
            .setting = PasswordManagerSetting::kOfferToSavePasswords,
            .syncing = false},

        SettingAndSyncingParam{
            .old_chrome_pref =
                password_manager::prefs::kCredentialsEnableAutosignin,
            .gms_cache_pref = password_manager::prefs::kAutoSignInEnabledGMS,
            .setting = PasswordManagerSetting::kAutoSignIn,
            .syncing = true},
        SettingAndSyncingParam{
            .old_chrome_pref =
                password_manager::prefs::kCredentialsEnableAutosignin,
            .gms_cache_pref = password_manager::prefs::kAutoSignInEnabledGMS,
            .setting = PasswordManagerSetting::kAutoSignIn,
            .syncing = false}),
    [](const ::testing::TestParamInfo<SettingAndSyncingParam>& info) {
      std::string storage = info.param.syncing ? "Account" : "Local";
      switch (info.param.setting) {
        case PasswordManagerSetting::kOfferToSavePasswords:
          return "SavePwd" + storage;
        case PasswordManagerSetting::kAutoSignIn:
          return "AutoSignIn" + storage;
          ;
        default:
          NOTREACHED();
      }
    });
