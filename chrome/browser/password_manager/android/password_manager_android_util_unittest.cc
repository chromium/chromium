// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
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
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"
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
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager_android_util {
namespace {

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
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendFalseWhenNotSyncingAndSplitStoresOff) {
  EXPECT_FALSE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ false, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendFalseWhenNotSyncingAndSplitStoresMigrationPending) {
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::
              kOffAndMigrationPending));

  EXPECT_FALSE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ false, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendTrueWhenNotSyncingAndSplitStoresOn) {
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_TRUE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ false, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendTrueWhenNotSyncingAndSplitStoresEnabledAndUnenrolled) {
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  pref_service_.SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  EXPECT_TRUE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ false, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendFalseWhenSyncingAndUnenrolled) {
  pref_service_.SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  EXPECT_FALSE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ true, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendTrueWhenSyncingAndSplitStoresDisabled) {
  EXPECT_TRUE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ true, &pref_service_));
}

TEST_F(PasswordManagerAndroidUtilTest,
       CanUseUPMBackendTrueWhenSyncingAndSplitStoresEnabled) {
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_TRUE(
      CanUseUPMBackend(/*is_pwd_sync_enabled = */ true, &pref_service_));
}

// Integration test for UsesSplitStoresAndUPMForLocal(), which emulates restarts
// by creating and destroying TestingProfiles. This doesn't exercise any of the
// Java layers.
// TODO(crbug.com/1257820): Replace with PRE_ AndroidBrowserTests when those
// are supported, preferably using a FakePasswordStoreAndroidBackend.
class UsesSplitStoresAndUPMForLocalTest : public ::testing::Test {
 public:
  UsesSplitStoresAndUPMForLocalTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        syncer::kSyncDeferredStartupTimeoutSeconds, "0");
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        kSkipLocalUpmGmsCoreVersionCheckForTesting);
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
        {{ProfilePasswordStoreFactory::GetInstance(),
          ProfilePasswordStoreFactory::GetDefaultFactoryForTesting()},
         {AccountPasswordStoreFactory::GetInstance(),
          AccountPasswordStoreFactory::GetDefaultFactoryForTesting()},
         {TrustedVaultServiceFactory::GetInstance(),
          TrustedVaultServiceFactory::GetDefaultFactory()},
         // Unretained() is safe because `this` outlives `profile_`.
         {SyncServiceFactory::GetInstance(),
          base::BindRepeating(
              &UsesSplitStoresAndUPMForLocalTest::BuildSyncService,
              base::Unretained(this))}});
    profile_ = builder.Build();

    // `identity_test_env_adaptor_` is initialized lazily with the SyncService,
    // force it to happen now.
    ASSERT_FALSE(identity_test_env_adaptor_);
    sync_service();
    ASSERT_TRUE(identity_test_env_adaptor_);
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

TEST_F(UsesSplitStoresAndUPMForLocalTest,
       SignedOutWithoutPasswords_NewInstall) {
  base::test::ScopedFeatureList enable_local_upm;
  enable_local_upm.InitWithFeatures(
      {password_manager::features::kEnablePasswordsAccountStorage,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
      {});
  CreateProfile();
  EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
  DestroyProfile();
}

TEST_F(UsesSplitStoresAndUPMForLocalTest,
       SignedOutWithoutPasswords_ExistingInstall) {
  {
    base::test::ScopedFeatureList disable_local_upm;
    disable_local_upm.InitWithFeatures(
        {}, {password_manager::features::kEnablePasswordsAccountStorage,
             password_manager::features::
                 kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration});
    CreateProfile();
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    base::test::ScopedFeatureList enable_local_upm;
    enable_local_upm.InitWithFeatures(
        {password_manager::features::kEnablePasswordsAccountStorage,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
        {});
    CreateProfile();
    EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }
}

TEST_F(UsesSplitStoresAndUPMForLocalTest, SignedOutWithPasswords) {
  {
    base::test::ScopedFeatureList disable_local_upm;
    disable_local_upm.InitWithFeatures(
        {}, {password_manager::features::kEnablePasswordsAccountStorage,
             password_manager::features::
                 kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
             password_manager::features::
                 kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
    CreateProfile();
    profile_password_store()->AddLogin(MakeExampleForm());
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    base::test::ScopedFeatureList enable_local_upm;
    enable_local_upm.InitWithFeatures(
        {password_manager::features::kEnablePasswordsAccountStorage,
         password_manager::features::
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
    base::test::ScopedFeatureList enable_local_upm;
    enable_local_upm.InitWithFeatures(
        {password_manager::features::kEnablePasswordsAccountStorage,
         password_manager::features::
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
    ASSERT_FALSE(sync_service()->GetActiveDataTypes().Has(syncer::PASSWORDS));

    // Pretend the migration finished.
    // TODO(crbug.com/1495626): Once the migration is implemented, make this a
    // call to a fake instead of directly setting the pref.
    pref_service()->SetInteger(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
    EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
    ASSERT_TRUE(
        SyncDataTypeActiveWaiter(sync_service(), syncer::PASSWORDS).Wait());

    DestroyProfile();
  }
}

TEST_F(UsesSplitStoresAndUPMForLocalTest, SignedOutWithCustomSettings) {
  {
    base::test::ScopedFeatureList disable_local_upm;
    disable_local_upm.InitWithFeatures(
        {}, {password_manager::features::kEnablePasswordsAccountStorage,
             password_manager::features::
                 kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
             password_manager::features::
                 kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration});
    CreateProfile();
    pref_service()->SetBoolean(
        password_manager::prefs::kCredentialsEnableAutosignin, false);
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    base::test::ScopedFeatureList enable_local_upm;
    enable_local_upm.InitWithFeatures(
        {password_manager::features::kEnablePasswordsAccountStorage,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration},
        {});
    CreateProfile();
    // Disabled until the settings migration finishes.
    EXPECT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }
}

TEST_F(UsesSplitStoresAndUPMForLocalTest, Syncing) {
  {
    base::test::ScopedFeatureList disable_local_upm;
    disable_local_upm.InitWithFeatures(
        {}, {password_manager::features::kEnablePasswordsAccountStorage,
             password_manager::features::
                 kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration});
    CreateProfile();
    profile_password_store()->AddLogin(MakeExampleForm());
    SignInAndEnableSync();
    ASSERT_TRUE(
        SyncDataTypeActiveWaiter(sync_service(), syncer::PASSWORDS).Wait());
    ASSERT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
    DestroyProfile();
  }

  {
    base::test::ScopedFeatureList enable_local_upm;
    enable_local_upm.InitWithFeatures(
        {password_manager::features::kEnablePasswordsAccountStorage,
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration},
        {});
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

}  // namespace
}  // namespace password_manager_android_util
