
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_migration_warning_startup_launcher.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::MockPasswordStoreInterface;
using testing::_;

namespace {
password_manager::PasswordForm CreateTestPasswordForm() {
  password_manager::PasswordForm form;
  form.url = GURL("https://test.com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username";
  form.password_value = u"password";
  return form;
}
}  // namespace

class PasswordMigrationWarningStartupLauncherTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordMigrationWarningStartupLauncherTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>();
    profile()->GetPrefs()->SetInteger(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
    test_store_->Init(profile()->GetPrefs(),
                      /*affiliated_match_helper=*/nullptr);
    warning_startup_launcher_ =
        std::make_unique<PasswordMigrationWarningStartupLauncher>(
            web_contents(), profile(), show_migration_warning_callback_.Get());
  }

  ~PasswordMigrationWarningStartupLauncherTest() override = default;

  void TearDown() override {
    test_store_->ShutdownOnUIThread();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PasswordMigrationWarningStartupLauncher* warning_startup_launcher() {
    return warning_startup_launcher_.get();
  }

  base::MockCallback<
      PasswordMigrationWarningStartupLauncher::ShowMigrationWarningCallback>&
  show_migration_warning_callback() {
    return show_migration_warning_callback_;
  }

  password_manager::PasswordStoreInterface* store() {
    return test_store_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning};
  scoped_refptr<password_manager::TestPasswordStore> test_store_;
  base::MockCallback<
      PasswordMigrationWarningStartupLauncher::ShowMigrationWarningCallback>
      show_migration_warning_callback_;
  std::unique_ptr<PasswordMigrationWarningStartupLauncher>
      warning_startup_launcher_;
};

TEST_F(PasswordMigrationWarningStartupLauncherTest,
       DoesntTriggerWarningIfNoPasswords) {
  EXPECT_CALL(show_migration_warning_callback(), Run).Times(0);
  warning_startup_launcher()->MaybeFetchPasswordsAndShowWarning(store());
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordMigrationWarningStartupLauncherTest,
       TriggersWarningIfItHasPasswords) {
  store()->AddLogin(CreateTestPasswordForm());
  task_environment()->RunUntilIdle();
  EXPECT_CALL(show_migration_warning_callback(),
              Run(_, _,
                  password_manager::metrics_util::
                      PasswordMigrationWarningTriggers::kChromeStartup))
      .Times(1);
  warning_startup_launcher()->MaybeFetchPasswordsAndShowWarning(store());
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordMigrationWarningStartupLauncherTest, DoesntFetchIfAlreadyShown) {
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kLocalPasswordMigrationWarningShownAtStartup,
      true);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kLocalPasswordMigrationWarningPrefsVersion, 1);

  scoped_refptr<MockPasswordStoreInterface> mock_store(
      new MockPasswordStoreInterface());

  PasswordMigrationWarningStartupLauncher launcher{
      web_contents(), profile(), show_migration_warning_callback().Get()};
  EXPECT_CALL(*mock_store, GetAutofillableLogins).Times(0);
  launcher.MaybeFetchPasswordsAndShowWarning(mock_store.get());
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordMigrationWarningStartupLauncherTest,
       DoesntReshowIfAlreadyShownAndResetCounterIncreased) {
  std::vector<base::test::FeatureRefAndParams> features_and_params = {
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsMigrationWarning,
       {{password_manager::features::kLocalPasswordMigrationWarningPrefsVersion
             .name,
         "0"}}}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(features_and_params, {});
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kLocalPasswordMigrationWarningShownAtStartup,
      true);
  profile()->GetPrefs()->SetTime(
      password_manager::prefs::kLocalPasswordsMigrationWarningShownTimestamp,
      base::Time::Now());
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kLocalPasswordMigrationWarningPrefsVersion, 0);

  EXPECT_CALL(show_migration_warning_callback(), Run).Times(0);
  store()->AddLogin(CreateTestPasswordForm());
  task_environment()->RunUntilIdle();
  warning_startup_launcher()->MaybeFetchPasswordsAndShowWarning(store());
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordMigrationWarningStartupLauncherTest,
       ReshowsIfAlreadyShownAndResetCounterIncreased) {
  std::vector<base::test::FeatureRefAndParams> features_and_params = {
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsMigrationWarning,
       {{password_manager::features::kLocalPasswordMigrationWarningPrefsVersion
             .name,
         "1"}}}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(features_and_params, {});
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kLocalPasswordMigrationWarningShownAtStartup,
      true);
  profile()->GetPrefs()->SetTime(
      password_manager::prefs::kLocalPasswordsMigrationWarningShownTimestamp,
      base::Time::Now());
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kLocalPasswordMigrationWarningPrefsVersion, 0);

  EXPECT_CALL(show_migration_warning_callback(), Run).Times(1);
  store()->AddLogin(CreateTestPasswordForm());
  task_environment()->RunUntilIdle();
  warning_startup_launcher()->MaybeFetchPasswordsAndShowWarning(store());
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordMigrationWarningStartupLauncherTest, DoesntFetchIfCantShow) {
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      password_manager::prefs::kLocalPasswordMigrationWarningShownAtStartup));

  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning,
      true);
  scoped_refptr<MockPasswordStoreInterface> mock_store(
      new MockPasswordStoreInterface());

  PasswordMigrationWarningStartupLauncher launcher{
      web_contents(), profile(), show_migration_warning_callback().Get()};
  EXPECT_CALL(*mock_store, GetAutofillableLogins).Times(0);
  launcher.MaybeFetchPasswordsAndShowWarning(mock_store.get());
  task_environment()->RunUntilIdle();
}
