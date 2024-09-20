// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <limits>
#include <memory>

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/task/bind_post_task.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/sync_to_signin_migration.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::GetLocalUpmMinGmsVersion;
using password_manager::UsesSplitStoresAndUPMForLocal;
using password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::
    kOffAndMigrationPending;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn;
using password_manager_android_util::PasswordAccessLossWarningType;

namespace password_manager_android_util {
namespace {

// Duplicated from password_manager_android_util.cc, which is fine since the
// enum values should never change.
enum class ActivationError {
  // The user was activated or the local passwords/settings migration was
  // scheduled if one is needed.
  kNone = 0,
  kUnenrolled = 1,
  kInitialUpmMigrationMissing = 2,
  kLoginDbFileMoveFailed = 3,
  kOutdatedGmsCore = 4,
  kMigrationWarningUnacknowledged = 6,
  kMax = kMigrationWarningUnacknowledged,
};

password_manager::PasswordForm MakeExampleForm() {
  password_manager::PasswordForm form;
  form.signon_realm = "https://g.com";
  form.url = GURL(form.signon_realm);
  form.username_value = u"username";
  form.password_value = u"password";
  return form;
}

class SyncDataTypeActiveWaiter : public syncer::SyncServiceObserver {
 public:
  SyncDataTypeActiveWaiter(syncer::SyncService* sync_service,
                           syncer::DataType data_type)
      : sync_service_(sync_service), data_type_(data_type) {}
  SyncDataTypeActiveWaiter(const SyncDataTypeActiveWaiter&) = delete;
  SyncDataTypeActiveWaiter& operator=(const SyncDataTypeActiveWaiter&) = delete;
  ~SyncDataTypeActiveWaiter() override = default;

  [[nodiscard]] bool Wait() {
    observation_.Observe(sync_service_);
    run_loop_.Run();
    // OnStateChanged() resets `observation_` if successful.
    return !observation_.IsObserving();
  }

 private:
  // syncer::SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* service) override {
    if (service->GetActiveDataTypes().Has(data_type_)) {
      observation_.Reset();
      run_loop_.Quit();
    }
  }

  const raw_ptr<syncer::SyncService> sync_service_;
  const syncer::DataType data_type_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      observation_{this};
  base::RunLoop run_loop_;
};

class PasswordManagerAndroidUtilTest : public testing::Test {
 public:
  PasswordManagerAndroidUtilTest() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
        false);
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
        0);
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(kOff));
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableAutosignin, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
    pref_service_.registry()->RegisterBooleanPref(
        syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, false);
    pref_service_.registry()->RegisterBooleanPref(
        syncer::prefs::internal::kSyncKeepEverythingSynced, false);
    pref_service_.registry()->RegisterBooleanPref(
        base::StrCat(
            {syncer::prefs::internal::
                 kSyncDataTypeStatusForSyncToSigninMigrationPrefix,
             ".", syncer::DataTypeToStableLowerCaseString(syncer::PASSWORDS)}),
        false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::
            kUserAcknowledgedLocalPasswordsMigrationWarning,
        false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kSettingsMigratedToUPMLocal, false);

    SetPasswordSyncEnabledPref(false);
    base::WriteFile(login_db_directory_.Append(
                        password_manager::kLoginDataForProfileFileName),
                    "");

    // Most tests check the modern GmsCore case.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion()));
  }

  // SetUsesSplitStoresAndUPMForLocal() reads whether password sync is enabled
  // from a pref rather than the SyncService. This helper sets such pref.
  void SetPasswordSyncEnabledPref(bool enabled) {
    if (enabled) {
      pref_service_.SetBoolean(
          syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
      pref_service_.SetBoolean(
          syncer::prefs::internal::kSyncKeepEverythingSynced, true);
      pref_service_.SetBoolean(
          base::StrCat(
              {syncer::prefs::internal::
                   kSyncDataTypeStatusForSyncToSigninMigrationPrefix,
               ".",
               syncer::DataTypeToStableLowerCaseString(syncer::PASSWORDS)}),
          true);
      ASSERT_EQ(browser_sync::GetSyncToSigninMigrationDataTypeDecision(
                    &pref_service_, syncer::PASSWORDS,
                    syncer::prefs::internal::kSyncPasswords),
                browser_sync::SyncToSigninMigrationDataTypeDecision::kMigrate);
    } else {
      pref_service_.SetBoolean(
          syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, false);
      ASSERT_EQ(browser_sync::GetSyncToSigninMigrationDataTypeDecision(
                    &pref_service_, syncer::PASSWORDS,
                    syncer::prefs::internal::kSyncPasswords),
                browser_sync::SyncToSigninMigrationDataTypeDecision::
                    kDontMigrateTypeDisabled);
    }
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  const base::FilePath& login_db_directory() { return login_db_directory_; }

 private:
  TestingPrefServiceSimple pref_service_;
  const base::FilePath login_db_directory_ =
      base::CreateUniqueTempDirectoryScopedToTest();
};

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringFalseWhenNotSyncingAndSplitStoresOff) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedOut();

  EXPECT_FALSE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringFalseWhenNotSyncingAndSplitStoresMigrationPending) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedOut();
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::
              kOffAndMigrationPending));

  EXPECT_FALSE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenNotSyncingAndSplitStoresOn) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedOut();
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(kOn));

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenNotSyncingAndSplitStoresEnabledAndUnenrolled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedOut();
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(kOn));

  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringFalseWhenSyncingAndUnenrolled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedIn(signin::ConsentLevel::kSync);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  EXPECT_FALSE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenSyncingAndSplitStoresDisabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedIn(signin::ConsentLevel::kSync);

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenSyncingAndSplitStoresEnabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedIn(signin::ConsentLevel::kSync);
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(kOn));

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenSignedInWithoutSyncAndSplitStoresDisabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedIn(signin::ConsentLevel::kSignin);
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(kOff));

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenSignedInWithoutSyncAndSplitStoresEnabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedIn(signin::ConsentLevel::kSignin);
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(kOn));

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_SignedOutWithNoPasswordsAndDefaultSettings) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, true);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The user got activated.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOn));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingNoMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       true, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOn, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();

  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // After activation, next calls are no-ops, even if settings are customized.
  // The histogram is now recorded for NonSyncingWithMigration though, which is
  // a bit misleading.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOn));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       true, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOn, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SignedOutNoPasswordsAndCustomSettings) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, true);
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      false);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The migration is pending.
  // Even though the migration warning was not acknowledged, the migration
  // should happen because there are no local passwords. Only settings should be
  // migrated.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOffAndMigrationPending));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOffAndMigrationPending, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SignedOutWithFreshProfile) {
  base::HistogramTester histogram_tester;
  // kEmptyProfileStoreLoginDatabase is false, so in principle there would be
  // local passwords to migrate. But actually the pref is just in its default
  // state. This is a fresh profile without a DB file.
  base::DeleteFile(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName));
  const PrefService::Preference* no_passwords_pref =
      pref_service()->FindPreference(
          password_manager::prefs::kEmptyProfileStoreLoginDatabase);
  ASSERT_TRUE(no_passwords_pref->IsDefaultValue());
  ASSERT_EQ(no_passwords_pref->GetValue()->GetBool(), false);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // User got activated.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOn));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingNoMigration",
      ActivationError::kNone, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOn, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SignedOutWithPasswords) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    // The migration warning was not acknowledged so the migration attempt
    // fails.
    EXPECT_EQ(
        pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
        static_cast<int>(kOff));
    histogram_tester->ExpectUniqueSample(
        "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
        ActivationError::kMigrationWarningUnacknowledged, 1);
    histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                         false, 1);
    histogram_tester->ExpectUniqueSample(
        "PasswordManager.LocalUpmActivationStatus", kOff, 1);
    histogram_tester = std::make_unique<base::HistogramTester>();
    pref_service()->SetBoolean(
        password_manager::prefs::
            kUserAcknowledgedLocalPasswordsMigrationWarning,
        true);

    // Try again.
    SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());
  } else {
    // On Android Auto, the migration warning is not shown, so acknowledging is
    // not required.
  }

  // The migration got marked as pending (but the user is not considered
  // activated).
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOffAndMigrationPending));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOffAndMigrationPending, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The first migration didn't finish/succeed, so a new migration is scheduled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOffAndMigrationPending));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOffAndMigrationPending, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();

  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOn));
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The migration finished successfully, the user is activated, so next calls
  // are no-ops.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOn));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       true, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOn, 1);
}

// Tests that acknowledging the migration warning is no longer required for
// migration.
TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SkipMigrationWarningAcknowledgement) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The migration got marked as pending (but the user is not considered
  // activated).
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOffAndMigrationPending));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOffAndMigrationPending, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();
}

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_SignedOutWithCustomEnableServiceSetting) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The migration got marked as pending (but the user is not considered
  // activated yet).
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOffAndMigrationPending));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOffAndMigrationPending, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SignedOutWithCustomAutoSigninSetting) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The migration got marked as pending (but the user is not considered
  // activated yet).
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOffAndMigrationPending));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOffAndMigrationPending, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_KeepMigrationPendingIfSyncEnabled) {
  // Set up a user who was signed out with saved passwords (thus got into
  // kOffAndMigrationPending), failed to migrate (thus stayed in
  // kOffAndMigrationPending) and later enabled sync.
  // kLoginDataForAccountFileName exists because the account store was created
  // when the migration got scheduled, even if it was never used.
  base::HistogramTester histogram_tester;
  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOffAndMigrationPending));
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  SetPasswordSyncEnabledPref(true);
  base::WriteFile(login_db_directory().Append(
                      password_manager::kLoginDataForAccountFileName),
                  "");
  ASSERT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  ASSERT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The browser should keep trying to migrate existing passwords to the *local*
  // Android backend. The login database files should be untouched.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOffAndMigrationPending));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOffAndMigrationPending, 1);

  // Advanced case: deactivate too, by downgrading Gmscore.
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // kOn syncing users that get rolled back will "undo" the login DB file move,
  // i.e. they replace the "profile" loginDB with the "account" one. This isn't
  // always perfect, see comment MaybeDeactivateSplitStoresAndLocalUpm(). The
  // "account" DB might even be empty and overwrite a non-empty "profile" one.
  // However: for kOffAndMigrationPending users, the "account" DB is *surely*
  // empty (password sync is suppressed). So replacing the file can only be
  // worse. Instead, the DB files should just be untouched. The account one is
  // empty anyway, so no data is leftover.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 2);
  histogram_tester.ExpectBucketCount("PasswordManager.LocalUpmActivationStatus",
                                     kOff, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SyncingHealthy) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  SetPasswordSyncEnabledPref(true);
  pref_service()->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      1);
  // Custom password manager settings should not matter for syncing users.
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  ASSERT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  ASSERT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The user should've been activated and the profile DB file should've become
  // the account DB file.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOn));
  EXPECT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.Syncing", ActivationError::kNone,
      1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       true, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOn, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_StaysActivatedIfEnabledSyncLater) {
  // Set up a user that got activated while being signed out and later enabled
  // sync, but didn't have kCurrentMigrationVersionToGoogleMobileServices set
  // (that pref is part of a migration logic that's no longer triggered when
  // the local UPM is activated).
  base::HistogramTester histogram_tester;
  SetPasswordSyncEnabledPref(true);
  ASSERT_EQ(pref_service()->GetInteger(
                password_manager::prefs::
                    kCurrentMigrationVersionToGoogleMobileServices),
            0);
  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOn));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The histogram records for "Syncing", which is a bit misleading.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOn));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.Syncing", ActivationError::kNone,
      1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOn, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_DeactivatingSyncUserMovesDBFile) {
  // Set up a healthy syncing user that got previously activated.
  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOn));
  SetPasswordSyncEnabledPref(true);
  pref_service()->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      1);
  base::WriteFile(login_db_directory().Append(
                      password_manager::kLoginDataForAccountFileName),
                  "");
  ASSERT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  ASSERT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Downgrading GmsCore undoes the process, including the file move.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
}

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_OldGmsNotActivatedIfSignedOutWithoutPasswords) {
  base::HistogramTester histogram_tester;
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, true);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingNoMigration",
      ActivationError::kOutdatedGmsCore, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
}

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_OldGmsNotActivatedIfSignedOutWithPasswords) {
  base::HistogramTester histogram_tester;
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kOutdatedGmsCore, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_OldGmsNotActivatedIfSyncing) {
  base::HistogramTester histogram_tester;
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
  SetPasswordSyncEnabledPref(true);
  pref_service()->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      1);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.Syncing",
      ActivationError::kOutdatedGmsCore, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_DowngradingGmsCoreDeactivates) {
  base::HistogramTester histogram_tester;
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOn));
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, true);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingNoMigration",
      ActivationError::kOutdatedGmsCore, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_DowngradingGmsCoreCancelsMigration) {
  // In the past the migration got scheduled, but never finished. GmsCore has
  // since been downgraded.
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOffAndMigrationPending));
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Migration should have been canceled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_DeletesLoginDataFilesForMigratedUsers) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kClearLoginDatabaseForAllMigratedUPMUsers);

  base::HistogramTester histogram_tester;
  const char kRemovalStatusProfileMetric[] =
      "PasswordManager.ProfileLoginData.RemovalStatus";
  const char kRemovalStatusAccountMetric[] =
      "PasswordManager.AccountLoginData.RemovalStatus";

  // This is a state of a local user that has just been migrated.
  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOn));
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);

  // Creating the login data files for testing.
  base::FilePath profile_db_path = login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName);
  base::FilePath account_db_path = login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName);
  base::FilePath profile_db_journal_path = login_db_directory().Append(
      password_manager::kLoginDataJournalForProfileFileName);
  base::FilePath account_db_journal_path = login_db_directory().Append(
      password_manager::kLoginDataJournalForAccountFileName);

  base::WriteFile(profile_db_path, "Test content");
  base::WriteFile(account_db_path, "Test content");
  base::WriteFile(profile_db_journal_path, "Test content");
  base::WriteFile(account_db_journal_path, "Test content");

  EXPECT_TRUE(PathExists(profile_db_path));
  EXPECT_TRUE(PathExists(account_db_path));
  EXPECT_TRUE(PathExists(profile_db_journal_path));
  EXPECT_TRUE(PathExists(account_db_journal_path));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The user wasn't deactivated, so the login data file should have been
  // cleared because the user was already migrated to UPM with split stores.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOn));
  EXPECT_FALSE(PathExists(profile_db_path));
  EXPECT_FALSE(PathExists(account_db_path));
  EXPECT_FALSE(PathExists(profile_db_journal_path));
  EXPECT_FALSE(PathExists(account_db_journal_path));
  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase));

  histogram_tester.ExpectUniqueSample(kRemovalStatusProfileMetric, true, 1);
  histogram_tester.ExpectUniqueSample(kRemovalStatusAccountMetric, true, 1);
}

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_NoLoginDataFilesCreatedForDeactivatedAccountUsers) {
  // This test simulated a case when the GMS Core version was manually
  // downgraded after UPM activation.
  base::test::ScopedFeatureList enable_clearing_flag{
      password_manager::features::kClearLoginDatabaseForAllMigratedUPMUsers};
  // In this test UPM should get deactivated because of low GMS Core version.
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      base::NumberToString(GetLocalUpmMinGmsVersion() - 1));

  // The initial state of the test is that the user is activated for UPM with
  // split stores and the login data files were deleted.
  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOn));
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);

  base::FilePath profile_db_path = login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName);
  base::FilePath account_db_path = login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName);

  base::DeleteFile(profile_db_path);
  base::DeleteFile(account_db_path);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The user was deactivated and there are still no login data files.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_FALSE(PathExists(profile_db_path));
  EXPECT_FALSE(PathExists(account_db_path));
}

// Integration test for UsesSplitStoresAndUPMForLocal(), which emulates restarts
// by creating and destroying TestingProfiles. This doesn't exercise any of the
// Java layers.
// TODO(b/324196888): Replace with PRE_ AndroidBrowserTests when those
// are supported, preferably using a FakePasswordStoreAndroidBackend.
class UsesSplitStoresAndUPMForLocalTest : public ::testing::Test {
 public:
  UsesSplitStoresAndUPMForLocalTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        syncer::kSyncDeferredStartupTimeoutSeconds, "0");
    // Override the GMS version to be big enough for local UPM support, so these
    // tests still pass in bots with an outdated version.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion()));
  }

  // Can be invoked more than once, calling DestroyProfile() in-between.
  // Most of the relevant sync/passwords state is kept between calls.
  void CreateProfile() {
    ASSERT_FALSE(profile_) << "Call DestroyProfile() first";

    // Use a fixed profile path, so files like the LoginDBs are kept.
    TestingProfile::Builder builder;
    builder.SetPath(profile_path_);

    // Similarly, use a fixed `user_pref_store_`.
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
        base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
    RegisterUserProfilePrefs(pref_registry.get());
    builder.SetPrefService(
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>(
            base::MakeRefCounted<TestingPrefStore>(),
            base::MakeRefCounted<TestingPrefStore>(),
            base::MakeRefCounted<TestingPrefStore>(),
            base::MakeRefCounted<TestingPrefStore>(),
            /*user_pref_store=*/user_pref_store_,
            base::MakeRefCounted<TestingPrefStore>(), pref_registry,
            std::make_unique<PrefNotifierImpl>()));

    // Add the real factories for Sync/Passwords but not the IdentityManager,
    // which is harder to control.
    builder.AddTestingFactories(IdentityTestEnvironmentProfileAdaptor::
                                    GetIdentityTestEnvironmentFactories());
    builder.AddTestingFactories(
        {TestingProfile::TestingFactory{
             TrustedVaultServiceFactory::GetInstance(),
             TrustedVaultServiceFactory::GetDefaultFactory()},
         // Unretained() is safe because `this` outlives `profile_`.
         TestingProfile::TestingFactory{
             SyncServiceFactory::GetInstance(),
             base::BindRepeating(
                 &UsesSplitStoresAndUPMForLocalTest::BuildSyncService,
                 base::Unretained(this))}});
    profile_ = builder.Build();

    SetUpPasswordStores(profile_.get());

    // `identity_test_env_adaptor_` is initialized lazily with the SyncService,
    // force it to happen now.
    ASSERT_FALSE(identity_test_env_adaptor_);
    sync_service();
    ASSERT_TRUE(identity_test_env_adaptor_);
  }

  void SetUpPasswordStores(Profile* profile) {
    // This block of tests is designed to test the behavior of login database
    // (namely that the profile database file is renamed to be the account
    // database file when using the split stores feature).
    std::unique_ptr<password_manager::LoginDatabase> login_db(
        password_manager::CreateLoginDatabaseForProfileStorage(
            profile->GetPath(), profile->GetPrefs()));
    password_manager::LoginDatabase* login_db_ptr = login_db.get();
    std::unique_ptr<password_manager::PasswordStoreBackend> profile_backend =
        std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
            std::move(login_db),
            syncer::WipeModelUponSyncDisabledBehavior::kNever,
            profile->GetPrefs());
    auto is_db_empty_cb =
        base::BindPostTaskToCurrentDefault(base::BindRepeating(
            &password_manager::IntermediateCallbackForSettingPrefs,
            profile_backend->AsWeakPtr(),
            base::BindRepeating(
                &password_manager::SetEmptyStorePref, profile->GetPrefs(),
                password_manager::prefs::kEmptyProfileStoreLoginDatabase)));
    login_db_ptr->SetIsEmptyCb(std::move(is_db_empty_cb));
    ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindRepeating(
            &password_manager::BuildPasswordStoreWithArgs<
                content::BrowserContext, password_manager::PasswordStore,
                std::unique_ptr<password_manager::PasswordStoreBackend>>,
            base::Passed(std::move(profile_backend))));

    std::unique_ptr<password_manager::PasswordStoreBackend> account_backend =
        std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
            password_manager::CreateLoginDatabaseForAccountStorage(
                profile->GetPath(), profile->GetPrefs()),
            syncer::WipeModelUponSyncDisabledBehavior::kAlways,
            profile->GetPrefs());
    AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindRepeating(
            &password_manager::BuildPasswordStoreWithArgs<
                content::BrowserContext, password_manager::PasswordStore,
                std::unique_ptr<password_manager::PasswordStoreBackend>>,
            base::Passed(std::move(account_backend))));
  }

  void DestroyProfile() {
    ASSERT_TRUE(profile_) << "Call CreateProfile() first";

    task_environment_.RunUntilIdle();
    identity_test_env_adaptor_.reset();
    profile_.reset();
  }

  std::unique_ptr<KeyedService> BuildSyncService(
      content::BrowserContext* context) {
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            Profile::FromBrowserContext(context));
    if (signed_in_) {
      // The refresh token is not persisted in the test, so set it again before
      // creating the SyncService.
      identity_test_env_adaptor_->identity_test_env()
          ->SetRefreshTokenForPrimaryAccount();
    }

    std::unique_ptr<KeyedService> sync_service =
        SyncServiceFactory::GetDefaultFactory().Run(context);
    static_cast<syncer::SyncServiceImpl*>(sync_service.get())
        ->OverrideNetworkForTest(
            fake_server::CreateFakeServerHttpPostProviderFactory(
                fake_server_.AsWeakPtr()));
    return sync_service;
  }

  void SignInAndEnableSync() {
    ASSERT_TRUE(identity_test_env_adaptor_);
    signin::IdentityTestEnvironment* env =
        identity_test_env_adaptor_->identity_test_env();
    ASSERT_FALSE(env->identity_manager()->HasPrimaryAccount(
        signin::ConsentLevel::kSync));
    env->SetAutomaticIssueOfAccessTokens(true);
    env->MakePrimaryAccountAvailable("foo@gmail.com",
                                     signin::ConsentLevel::kSync);
    signed_in_ = true;

    // Sync few types to avoid setting up dependencies for most of them.
    std::unique_ptr<syncer::SyncSetupInProgressHandle> handle =
        sync_service()->GetSetupInProgressHandle();
    sync_service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kPreferences,
                                    syncer::UserSelectableType::kPasswords});
    sync_service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
        syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  }

  syncer::SyncService* sync_service() {
    return SyncServiceFactory::GetForProfile(profile_.get());
  }

  password_manager::PasswordStoreInterface* profile_password_store() {
    return ProfilePasswordStoreFactory::GetForProfile(
               profile_.get(), ServiceAccessType::IMPLICIT_ACCESS)
        .get();
  }

  password_manager::PasswordStoreInterface* account_password_store() {
    return AccountPasswordStoreFactory::GetForProfile(
               profile_.get(), ServiceAccessType::IMPLICIT_ACCESS)
        .get();
  }

  PrefService* pref_service() { return profile_->GetPrefs(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  const base::FilePath profile_path_ =
      base::CreateUniqueTempDirectoryScopedToTest();
  const scoped_refptr<TestingPrefStore> user_pref_store_ =
      base::MakeRefCounted<TestingPrefStore>();
  const ScopedTestingLocalState local_state_ =
      ScopedTestingLocalState(TestingBrowserProcess::GetGlobal());
  fake_server::FakeServer fake_server_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  bool signed_in_ = false;
};

TEST_F(UsesSplitStoresAndUPMForLocalTest, SignedOutWithoutPasswords) {
  {
    // Prevent activation on the first run by faking an outdated GmsCore.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
    CreateProfile();
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    // Now GmsCore was upgraded and activation can proceed.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion()));
    CreateProfile();
    EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }
}

TEST_F(UsesSplitStoresAndUPMForLocalTest, SignedOutWithPasswords) {
  {
    // Set up a signed-out user, with saved passwords, who already acknowledged
    // the migration warning. Prevent activation before the passwords are added,
    // by faking an outdated GmsCore.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
    CreateProfile();
    profile_password_store()->AddLogin(MakeExampleForm());
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    pref_service()->SetBoolean(
        password_manager::prefs::
            kUserAcknowledgedLocalPasswordsMigrationWarning,
        true);
    DestroyProfile();
  }

  {
    // Now GmsCore was upgraded, so the migration gets scheduled.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion()));
    CreateProfile();

    // Until the migration finishes, UsesSplitStoresAndUPMForLocal() should be
    // false and password sync should be suppressed.
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    SignInAndEnableSync();
    ASSERT_TRUE(
        SyncDataTypeActiveWaiter(sync_service(), syncer::PREFERENCES).Wait());
    ASSERT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
        syncer::UserSelectableType::kPasswords));

    // Pretend the migration finished.
    // TODO(b/324196888): Once the migration is implemented, make this a
    // call to a fake instead of directly setting the pref.
    pref_service()->SetInteger(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(kOn));
    EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
    ASSERT_TRUE(
        SyncDataTypeActiveWaiter(sync_service(), syncer::PASSWORDS).Wait());

    DestroyProfile();
  }
}

TEST_F(UsesSplitStoresAndUPMForLocalTest, SyncingHealthy) {
  {
    // Prevent activation before sync is enabled, by faking an outdated GmsCore.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
    CreateProfile();
    profile_password_store()->AddLogin(MakeExampleForm());
    SignInAndEnableSync();
    ASSERT_TRUE(
        SyncDataTypeActiveWaiter(sync_service(), syncer::PASSWORDS).Wait());
    pref_service()->SetInteger(
        password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
        1);
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    // Now GmsCore was upgraded and activation can proceed.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion()));
    CreateProfile();
    ASSERT_TRUE(
        SyncDataTypeActiveWaiter(sync_service(), syncer::PASSWORDS).Wait());
    EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
    // Passwords in the profile store must have moved to the account store.
    password_manager::PasswordStoreResultsObserver profile_store_observer;
    password_manager::PasswordStoreResultsObserver account_store_observer;
    profile_password_store()->GetAllLogins(profile_store_observer.GetWeakPtr());
    account_password_store()->GetAllLogins(account_store_observer.GetWeakPtr());
    EXPECT_EQ(profile_store_observer.WaitForResults().size(), 0u);
    EXPECT_EQ(account_store_observer.WaitForResults().size(), 1u);
    DestroyProfile();
  }
}

TEST_F(UsesSplitStoresAndUPMForLocalTest, SyncingButUnenrolledAndM4Enabled) {
  // Test setup where one password was saved to profile store and user is
  // unenrolled from UPM.
  {
    // Prevent activation before sync is enabled, by faking an outdated GmsCore.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
    CreateProfile();
    profile_password_store()->AddLogin(MakeExampleForm());
    SignInAndEnableSync();
    ASSERT_TRUE(
        SyncDataTypeActiveWaiter(sync_service(), syncer::PASSWORDS).Wait());
    pref_service()->SetBoolean(
        password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
        true);
    pref_service()->SetInteger(
        password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
        1);
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    // Now GmsCore was upgraded and activation can proceed.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion()));
    CreateProfile();

    // The migration is pending.
    EXPECT_EQ(
        pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
        static_cast<int>(kOffAndMigrationPending));

    // Passwords is still in the profile, it was not moved to account even
    // though user was syncing.
    password_manager::PasswordStoreResultsObserver profile_store_observer;
    password_manager::PasswordStoreResultsObserver account_store_observer;
    profile_password_store()->GetAllLogins(profile_store_observer.GetWeakPtr());
    account_password_store()->GetAllLogins(account_store_observer.GetWeakPtr());
    EXPECT_EQ(profile_store_observer.WaitForResults().size(), 1u);
    EXPECT_EQ(account_store_observer.WaitForResults().size(), 0u);
    DestroyProfile();
  }
}

TEST_F(UsesSplitStoresAndUPMForLocalTest,
       SyncingButNoInitialUPMMigrationAndM4Enabled) {
  // Test setup where one password was saved to profile store and user is
  // enrolled into original UPM.
  {
    // Prevent activation before sync is enabled, by faking an outdated GmsCore.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion() - 1));
    CreateProfile();
    profile_password_store()->AddLogin(MakeExampleForm());
    SignInAndEnableSync();
    ASSERT_TRUE(
        SyncDataTypeActiveWaiter(sync_service(), syncer::PASSWORDS).Wait());
    pref_service()->SetInteger(
        password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
        0);
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    // Now GmsCore was upgraded and activation can proceed.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(GetLocalUpmMinGmsVersion()));
    CreateProfile();

    // The migration is pending.
    EXPECT_EQ(
        pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
        static_cast<int>(kOffAndMigrationPending));

    // Passwords is still in the profile, it was not moved to account even
    // though user was syncing.
    password_manager::PasswordStoreResultsObserver profile_store_observer;
    password_manager::PasswordStoreResultsObserver account_store_observer;
    profile_password_store()->GetAllLogins(profile_store_observer.GetWeakPtr());
    account_password_store()->GetAllLogins(account_store_observer.GetWeakPtr());
    EXPECT_EQ(profile_store_observer.WaitForResults().size(), 1u);
    EXPECT_EQ(account_store_observer.WaitForResults().size(), 0u);
    DestroyProfile();
  }
}

struct GetPasswordAccessLossWarningTypeTestCase {
  std::string test_case_desc;
  std::string gms_core_version;
  bool migration_attempted;
  bool local_passwords_migration_failed;
  bool empty_profile_store;
  bool is_auto;
  PasswordAccessLossWarningType expected_result;
};

class GetPasswordAccessLossWarningTypeTest
    : public PasswordManagerAndroidUtilTest,
      public testing::WithParamInterface<
          GetPasswordAccessLossWarningTypeTestCase> {
 protected:
  void SetUp() override {
    GetPasswordAccessLossWarningTypeTestCase test_case = GetParam();

    int use_upm_and_separate_stores = 0;
    if (!test_case.migration_attempted) {
      use_upm_and_separate_stores = static_cast<int>(kOff);
    } else if (test_case.local_passwords_migration_failed) {
      use_upm_and_separate_stores = static_cast<int>(kOffAndMigrationPending);
    } else {
      use_upm_and_separate_stores = static_cast<int>(kOn);
    }
    pref_service()->SetInteger(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        use_upm_and_separate_stores);
    pref_service()->SetBoolean(
        password_manager::prefs::kEmptyProfileStoreLoginDatabase,
        test_case.empty_profile_store);
  }
};

TEST_P(GetPasswordAccessLossWarningTypeTest, GetPasswordAccessLossWarningType) {
  if (base::android::BuildInfo::GetInstance()->is_automotive() !=
      GetParam().is_auto) {
    GTEST_SKIP() << "Automotive tests don't need to run on non-auto devices "
                    "and vice-versa.";
  }

  // This call is needed to set the variable whether the migration is failed.
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      GetParam().gms_core_version);
  PasswordAccessLossWarningType result =
      GetPasswordAccessLossWarningType(pref_service());

  EXPECT_EQ(GetParam().expected_result, result);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GetPasswordAccessLossWarningTypeTest,
    testing::Values(
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"NoGmsNoPwds",
            /*gms_core_version=*/"",
            /*migration_attempted=*/false,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/true,
            /*is_auto=*/false,
            /*expected_result=*/PasswordAccessLossWarningType::kNone),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"NoGmsButPwds",
            /*gms_core_version=*/"",
            /*migration_attempted=*/false,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/false,
            /*is_auto=*/false,
            /*expected_result=*/PasswordAccessLossWarningType::kNoGmsCore),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"NoUpmNoPwds",
            /*gms_core_version=*/"222912000",
            /*migration_attempted=*/false,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/true,
            /*is_auto=*/false,
            /*expected_result=*/PasswordAccessLossWarningType::kNone),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"NoUpmButPwds",
            /*gms_core_version=*/"222912000",
            /*migration_attempted=*/false,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/false,
            /*is_auto=*/false,
            /*expected_result=*/PasswordAccessLossWarningType::kNoUpm),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"AccountGmsNoPwds",
            /*gms_core_version=*/"223012000",
            /*migration_attempted=*/false,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/true,
            /*is_auto=*/false,
            /*expected_result=*/PasswordAccessLossWarningType::kNone),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"AccountGmsLocalPwds",
            /*gms_core_version=*/"223012000",
            /*migration_attempted=*/true,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/false,
            /*is_auto=*/false,
            /*expected_result=*/PasswordAccessLossWarningType::kOnlyAccountUpm),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"MigrationFailed",
            /*gms_core_version=*/"240212000",
            /*migration_attempted=*/true,
            /*local_passwords_migration_failed=*/true,
            /*empty_profile_store=*/false,
            /*is_auto=*/false,
            /*expected_result=*/
            PasswordAccessLossWarningType::kNewGmsCoreMigrationFailed),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"MigrationSucceeded",
            /*gms_core_version=*/"240212000",
            /*migration_attempted=*/true,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/false,
            /*is_auto=*/false,
            /*expected_result=*/PasswordAccessLossWarningType::kNone),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"AccountGmsLocalPwdsAuto",
            /*gms_core_version=*/"241412000",
            /*migration_attempted=*/false,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/false,
            /*is_auto=*/true,
            /*expected_result=*/PasswordAccessLossWarningType::kOnlyAccountUpm),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"MigrationFailedAuto",
            /*gms_core_version=*/"241512000",
            /*migration_attempted=*/true,
            /*local_passwords_migration_failed=*/true,
            /*empty_profile_store=*/false,
            /*is_auto=*/true,
            /*expected_result=*/
            PasswordAccessLossWarningType::kNewGmsCoreMigrationFailed),
        GetPasswordAccessLossWarningTypeTestCase(
            /*test_case_desc=*/"MigrationSucceededAuto",
            /*gms_core_version=*/"241512000",
            /*migration_attempted=*/true,
            /*local_passwords_migration_failed=*/false,
            /*empty_profile_store=*/false,
            /*is_auto=*/true,
            /*expected_result=*/PasswordAccessLossWarningType::kNone)),
    [](const ::testing::TestParamInfo<GetPasswordAccessLossWarningTypeTestCase>&
           info) { return info.param.test_case_desc; });

}  // namespace
}  // namespace password_manager_android_util
