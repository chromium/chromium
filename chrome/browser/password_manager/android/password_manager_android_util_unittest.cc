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
#include "chrome/common/chrome_switches.h"
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
#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/model_type.h"
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

using password_manager::UsesSplitStoresAndUPMForLocal;
using password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::
    kOffAndMigrationPending;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn;

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
  kFlagDisabled = 5,
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
                           syncer::ModelType model_type)
      : sync_service_(sync_service), model_type_(model_type) {}
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
    if (service->GetActiveDataTypes().Has(model_type_)) {
      observation_.Reset();
      run_loop_.Quit();
    }
  }

  const raw_ptr<syncer::SyncService> sync_service_;
  const syncer::ModelType model_type_;
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
        base::StrCat({syncer::prefs::internal::
                          kSyncDataTypeStatusForSyncToSigninMigrationPrefix,
                      ".",
                      syncer::GetModelTypeLowerCaseRootTag(syncer::PASSWORDS)}),
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

    // Skip the Gms version check, otherwise enabling UPM flags in individual
    // tests won't actually do anything in bots with outdated GmsCore.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSkipLocalUpmGmsCoreVersionCheckForTesting);
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
               ".", syncer::GetModelTypeLowerCaseRootTag(syncer::PASSWORDS)}),
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

  PrefService* pref_service() { return &pref_service_; }

  const base::FilePath& login_db_directory() { return login_db_directory_; }

  const char* min_gms_version_param_name() {
    return base::android::BuildInfo::GetInstance()->is_automotive()
               ? password_manager::features::kLocalUpmMinGmsVersionParamForAuto
               : password_manager::features::kLocalUpmMinGmsVersionParam;
  }

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
  sync_service.SetSignedInWithSyncFeatureOn();
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  EXPECT_FALSE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenSyncingAndSplitStoresDisabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedInWithSyncFeatureOn();

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenSyncingAndSplitStoresEnabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedInWithSyncFeatureOn();
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(kOn));

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenSignedInWithoutSyncAndSplitStoresDisabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedInWithoutSyncFeature();
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(kOff));

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(PasswordManagerAndroidUtilTest,
       ShouldUseUpmWiringTrueWhenSignedInWithoutSyncAndSplitStoresEnabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetSignedInWithoutSyncFeature();
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(kOn));

  EXPECT_TRUE(ShouldUseUpmWiring(&sync_service, pref_service()));
}

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_SignedOutWithNoPasswordsAndDefaultSettings) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  base::test::ScopedFeatureList disable_local_upm;
  disable_local_upm.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, true);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Nothing changed, the flag was disabled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingNoMigration",
      ActivationError::kFlagDisabled, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();

  base::test::ScopedFeatureList enable_local_upm(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The flag was enabled, so the user got activated.
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration},
      {});
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
  base::test::ScopedFeatureList enable_local_upm(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
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

  // User got activated by the NoMigration flag.
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

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_SignedOutWithFreshProfileWithGMSCheckForAuto) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  base::test::ScopedFeatureList enable_local_upm(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSkipLocalUpmGmsCoreVersionCheckForTesting);

  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, true);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The user did not get activated, because the GMS min version condition
  // isn't met.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SignedOutWithPasswords) {
  // The warning is not shown on automotive, so there is a separate test
  // for auto.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  base::test::ScopedFeatureList enable_local_upm_without_migration;
  enable_local_upm_without_migration.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The migration warning was not acknowledged so the migration attempt fails.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
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
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Nothing changed, the WithMigration flag was disabled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kFlagDisabled, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();

  base::test::ScopedFeatureList enable_local_upm_with_migration;
  enable_local_upm_with_migration.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {});
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The flag was enabled, so the migration got marked as pending (but the user
  // is not considered activated).
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

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SignedOutWithPasswordsAuto) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  base::test::ScopedFeatureList enable_local_upm_with_migration;
  enable_local_upm_with_migration.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration},
      {});
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Despite the migration warning not being acknowledged, the migration
  // attempt should proceed.
  ASSERT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::
          kUserAcknowledgedLocalPasswordsMigrationWarning));
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOffAndMigrationPending));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kNone, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);

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
}

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_SignedOutWithPasswordsWithGMSCheckForAuto) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  base::test::ScopedFeatureList enable_local_upm_with_migration;
  enable_local_upm_with_migration.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {});
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSkipLocalUpmGmsCoreVersionCheckForTesting);
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The migration did not get marked as pending, because the GMS min version
  // condition isn't met.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
}

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_SignedOutWithCustomEnableServiceSetting) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  base::test::ScopedFeatureList enable_local_upm_without_migration;
  enable_local_upm_without_migration.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Nothing changed, the WithMigration flag was disabled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kFlagDisabled, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();

  base::test::ScopedFeatureList enable_local_upm_with_migration;
  enable_local_upm_with_migration.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {});
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The flag was enabled, so the migration got marked as pending (but the user
  // is not considered activated yet).
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
  base::test::ScopedFeatureList enable_local_upm_without_migration;
  enable_local_upm_without_migration.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  pref_service()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Nothing changed, the WithMigration flag was disabled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.NonSyncingWithMigration",
      ActivationError::kFlagDisabled, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();

  base::test::ScopedFeatureList enable_local_upm_with_migration;
  enable_local_upm_with_migration.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {});
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // The flag was enabled, so the migration got marked as pending (but the user
  // is not considered activated yet).
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
  base::test::ScopedFeatureList enable_local_upm;
  enable_local_upm.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration},
      {});
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
  // Advanced case: roll back too.
  base::test::ScopedFeatureList disable_local_upm;
  disable_local_upm.InitWithFeatures(
      {}, {password_manager::features::
               kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
           password_manager::features::
               kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});

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
  base::test::ScopedFeatureList disable_local_upm;
  disable_local_upm.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
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

  // Nothing should've happened, the flag was disabled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.Syncing",
      ActivationError::kFlagDisabled, 1);
  histogram_tester->ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                       false, 1);
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
  histogram_tester = std::make_unique<base::HistogramTester>();

  base::test::ScopedFeatureList enable_local_upm(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
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
       SetUsesSplitStoresAndUPMForLocal_SyncingHealthyWithGmsCheckForAuto) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  base::test::ScopedFeatureList disable_local_upm;
  disable_local_upm.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
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

  // Nothing should've happened, the flag was disabled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));

  base::test::ScopedFeatureList enable_local_upm(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSkipLocalUpmGmsCoreVersionCheckForTesting);
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Nothing should have happened, because the min GMS Core version condition
  // isn't met.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
}

// TODO: crbug.com/40265507 - Clean up when M4 feature flag is removed.
TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SyncingButUnenrolled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList enable_local_upm;
  enable_local_upm.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore});
  SetPasswordSyncEnabledPref(true);
  pref_service()->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      1);
  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  ASSERT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  ASSERT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Nothing should've happened, the user was unenrolled.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.Syncing",
      ActivationError::kUnenrolled, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
}

// TODO: crbug.com/40265507 - Clean up when M4 feature flag is removed.
TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_SyncingButInitialMigrationNotFinished) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList enable_local_upm;
  enable_local_upm.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore});
  SetPasswordSyncEnabledPref(true);
  ASSERT_EQ(pref_service()->GetInteger(
                password_manager::prefs::
                    kCurrentMigrationVersionToGoogleMobileServices),
            0);
  ASSERT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  ASSERT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  ASSERT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Nothing should've happened, the initial UPM migration wasn't finished.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationError.Syncing",
      ActivationError::kInitialUpmMigrationMissing, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LocalUpmActivated",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LocalUpmActivationStatus", kOff, 1);
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_StaysActivatedIfEnabledSyncLater) {
  // Set up a user that got activated while being signed out and later enabled
  // sync, but didn't have kCurrentMigrationVersionToGoogleMobileServices set
  // (that pref is part of a migration logic that's no longer triggered when
  // the local UPM is activated).
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList enable_local_upm(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
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
       SetUsesSplitStoresAndUPMForLocal_OnlyNoMigrationFlagDeactivates) {
  // Set up a user that required a migration and got successfully activated in
  // the past.
  base::test::ScopedFeatureList disable_with_migration_flag;
  disable_with_migration_flag.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
  pref_service()->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(kOn));
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Disabling only the WithMigration flag does nothing.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOn));

  base::test::ScopedFeatureList disable_both_flags;
  disable_both_flags.InitWithFeatures(
      {}, {password_manager::features::
               kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
           password_manager::features::
               kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Disabling both flags deactivates the user.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_DeactivatingSyncUserMovesDBFile) {
  // Set up a healthy syncing user that got previously activated.
  base::test::ScopedFeatureList disable_local_upm;
  disable_local_upm.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
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

  SetUsesSplitStoresAndUPMForLocal(pref_service(), login_db_directory());

  // Disabling the flag undoes the process, including the file move.
  EXPECT_EQ(pref_service()->GetInteger(kPasswordsUseUPMLocalAndSeparateStores),
            static_cast<int>(kOff));
  EXPECT_TRUE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForProfileFileName)));
  EXPECT_FALSE(base::PathExists(login_db_directory().Append(
      password_manager::kLoginDataForAccountFileName)));
}

TEST_F(PasswordManagerAndroidUtilTest,
       SetUsesSplitStoresAndUPMForLocal_OldGmsCoreVersionIsNotActivated) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList enable_local_upm_with_high_min_version;
  enable_local_upm_with_high_min_version.InitAndEnableFeatureWithParameters(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
      {{min_gms_version_param_name(),
        base::ToString(std::numeric_limits<int>::max())}});
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSkipLocalUpmGmsCoreVersionCheckForTesting);
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
       SetUsesSplitStoresAndUPMForLocal_BumpingMinGmsCoreVersionDeactivates) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList enable_local_upm_with_high_min_version;
  enable_local_upm_with_high_min_version.InitAndEnableFeatureWithParameters(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
      {{min_gms_version_param_name(),
        base::ToString(std::numeric_limits<int>::max())}});
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSkipLocalUpmGmsCoreVersionCheckForTesting);
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

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_DisablingMigrationFlagCancelsMigration) {
  // In the past the WithMigration flag was enabled and the migration got
  // scheduled, but never finished. The flag has since been disabled.
  base::test::ScopedFeatureList disable_with_migration_flag;
  disable_with_migration_flag.InitWithFeatures(
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
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

TEST_F(
    PasswordManagerAndroidUtilTest,
    SetUsesSplitStoresAndUPMForLocal_BumpingMinGmsCoreVersionCancelsMigration) {
  // In the past the WithMigration flag was enabled and the migration got
  // scheduled, but never finished. The min GmsCore version has since been
  // bumped.
  base::test::ScopedFeatureList enable_local_upm_with_high_min_version;
  enable_local_upm_with_high_min_version.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{password_manager::features::
            kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
        {}},
       {password_manager::features::
            kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
        {{min_gms_version_param_name(),
          base::ToString(std::numeric_limits<int>::max())}}}},
      /*disabled_features=*/{});
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSkipLocalUpmGmsCoreVersionCheckForTesting);
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
    // Skip the Gms version check, otherwise enabling UPM flags in individual
    // tests won't actually do anything in bots with outdated GmsCore.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSkipLocalUpmGmsCoreVersionCheckForTesting);
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
        {{TrustedVaultServiceFactory::GetInstance(),
          TrustedVaultServiceFactory::GetDefaultFactory()},
         // Unretained() is safe because `this` outlives `profile_`.
         {SyncServiceFactory::GetInstance(),
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
            profile->GetPath()));
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
                profile->GetPath()),
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
    base::test::ScopedFeatureList disable_local_upm;
    disable_local_upm.InitAndDisableFeature(
        password_manager::features::
            kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
    CreateProfile();
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    base::test::ScopedFeatureList enable_local_upm(
        password_manager::features::
            kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
    CreateProfile();
    EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }
}

TEST_F(UsesSplitStoresAndUPMForLocalTest, SignedOutWithPasswords) {
  {
    // Set up a signed-out user, with saved passwords, who already acknowledged
    // the migration warning.
    base::test::ScopedFeatureList disable_local_upm;
    disable_local_upm.InitWithFeatures(
        {}, {password_manager::features::
                 kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
             password_manager::features::
                 kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
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
    base::test::ScopedFeatureList enable_local_upm_no_migration;
    enable_local_upm_no_migration.InitWithFeatures(
        {password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
        {password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
    CreateProfile();
    // Should be false because the user had existing passwords and the
    // "WithMigration" flag is disabled.
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    base::test::ScopedFeatureList enable_local_upm_with_migration;
    enable_local_upm_with_migration.InitWithFeatures(
        {password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration},
        {});
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
    base::test::ScopedFeatureList disable_local_upm;
    disable_local_upm.InitAndDisableFeature(
        password_manager::features::
            kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
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
    base::test::ScopedFeatureList enable_local_upm(
        password_manager::features::
            kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
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
    base::test::ScopedFeatureList disable_upm;
    disable_upm.InitWithFeatures(
        {},
        {password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
         password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore});
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
    base::test::ScopedFeatureList enable_upm;
    enable_upm.InitWithFeatures(
        {password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
         password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore},
        {});
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
    base::test::ScopedFeatureList disable_upm;
    disable_upm.InitWithFeatures(
        {},
        {password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
         password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore});
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
    base::test::ScopedFeatureList enable_upm;
    enable_upm.InitWithFeatures(
        {password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
         password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore},
        {});
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

}  // namespace
}  // namespace password_manager_android_util
