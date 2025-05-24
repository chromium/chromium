// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_access_loss_warning_startup_launcher.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
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

password_manager::PasswordForm CreateTestBlocklistedPasswordForm() {
  password_manager::PasswordForm form;
  form.url = GURL("https://test.com");
  form.signon_realm = form.url.spec();
  form.blocked_by_user = true;
  return form;
}
}  // namespace

class PasswordAccessLossWarningStartupLauncherTest : public testing::Test {
 public:
  PasswordAccessLossWarningStartupLauncherTest() {
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    warning_startup_launcher_ =
        std::make_unique<PasswordAccessLossWarningStartupLauncher>(
            show_access_loss_warning_callback().Get());
  }

  ~PasswordAccessLossWarningStartupLauncherTest() override {
    store_->ShutdownOnUIThread();
  }

  PasswordAccessLossWarningStartupLauncher* warning_startup_launcher() {
    return warning_startup_launcher_.get();
  }

  base::MockCallback<
      PasswordAccessLossWarningStartupLauncher::ShowAccessLossWarningCallback>&
  show_access_loss_warning_callback() {
    return show_access_loss_warning_callback_;
  }

  password_manager::PasswordStoreInterface* store() { return store_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<password_manager::TestPasswordStore> store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  base::MockCallback<
      PasswordAccessLossWarningStartupLauncher::ShowAccessLossWarningCallback>
      show_access_loss_warning_callback_;
  std::unique_ptr<PasswordAccessLossWarningStartupLauncher>
      warning_startup_launcher_;
};

TEST_F(PasswordAccessLossWarningStartupLauncherTest,
       DoesntTriggerWarningIfNoPasswords) {
  EXPECT_CALL(show_access_loss_warning_callback(), Run).Times(0);
  warning_startup_launcher()->FetchPasswordsAndShowWarning(store());
  RunUntilIdle();
}

TEST_F(PasswordAccessLossWarningStartupLauncherTest,
       TriggersWarningIfItHasPasswords) {
  store()->AddLogin(CreateTestPasswordForm());
  RunUntilIdle();
  EXPECT_CALL(show_access_loss_warning_callback(), Run).Times(1);
  warning_startup_launcher()->FetchPasswordsAndShowWarning(store());
  RunUntilIdle();
}

TEST_F(PasswordAccessLossWarningStartupLauncherTest,
       TriggersWarningIfItHasBlocklistedPasswords) {
  store()->AddLogin(CreateTestBlocklistedPasswordForm());
  RunUntilIdle();
  EXPECT_CALL(show_access_loss_warning_callback(), Run).Times(1);
  warning_startup_launcher()->FetchPasswordsAndShowWarning(store());
  RunUntilIdle();
}
