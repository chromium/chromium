// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/lacros_data_migration_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {
constexpr char kLacrosDataMigrationId[] = "lacros-data-migration";
const test::UIPath kSkipButton = {kLacrosDataMigrationId, "cancelButton"};

class FakeMigrator : public BrowserDataMigrator {
 public:
  // BrowserDataMigrator overrides.
  void Migrate() override {}
  void Cancel() override { cancel_called_ = true; }

  bool IsCancelCalled() { return cancel_called_; }

 private:
  bool cancel_called_ = false;
};

class LacrosDataMigrationScreenTest : public OobeBaseTest {
 public:
  LacrosDataMigrationScreenTest() {
    // Adding a user and marking OOBE completed with DeviceStateMixin ensures
    // that chrome://oobe/login is loaded instead of chrome://oobe/oobe and that
    // LoginDisplayHostMojo is created instead of LoginDisplayHostWebUI.
    login_mixin_.AppendRegularUsers(1);
  }
  LacrosDataMigrationScreenTest(const LacrosDataMigrationScreenTest&) = delete;
  LacrosDataMigrationScreenTest& operator=(
      const LacrosDataMigrationScreenTest&) = delete;
  ~LacrosDataMigrationScreenTest() override = default;

  void SetUpOnMainThread() override {
    LoginDisplayHostMojo* login_display_host =
        static_cast<LoginDisplayHostMojo*>(LoginDisplayHost::default_host());
    // Call `StartWizard()` with any screen to ensure that
    // `LoginDisplayHostMojo::EnsureOobeDialogLoaded()` is called but do not
    // show `LacrosDataMigrationScreen` yet because that will start the
    // migration before stubbing certain methonds.
    login_display_host->StartWizard(GaiaView::kScreenId);
    LacrosDataMigrationScreen* lacros_data_migration_screen =
        static_cast<LacrosDataMigrationScreen*>(
            WizardController::default_controller()->GetScreen(
                LacrosDataMigrationScreenView::kScreenId));
    fake_migrator_ = new FakeMigrator();
    lacros_data_migration_screen->SetMigratorForTesting(
        base::WrapUnique(fake_migrator_));
    lacros_data_migration_screen->SetSkipPostShowButtonForTesting(true);
    OobeBaseTest::SetUpOnMainThread();
  }

 protected:
  FakeMigrator* fake_migrator() { return fake_migrator_; }

 private:
  // This is owned by `LacrosDataMigrationScreen`.
  FakeMigrator* fake_migrator_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(LacrosDataMigrationScreenTest, SkipButton) {
  OobeScreenWaiter waiter(LacrosDataMigrationScreenView::kScreenId);
  WizardController::default_controller()->AdvanceToScreen(
      LacrosDataMigrationScreenView::kScreenId);
  waiter.Wait();

  test::OobeJS().ExpectHiddenPath(kSkipButton);

  LacrosDataMigrationScreen* lacros_data_migration_screen =
      static_cast<LacrosDataMigrationScreen*>(
          WizardController::default_controller()->GetScreen(
              LacrosDataMigrationScreenView::kScreenId));
  lacros_data_migration_screen->ShowSkipButton();

  test::OobeJS().ExpectVisiblePath(kSkipButton);

  EXPECT_FALSE(fake_migrator()->IsCancelCalled());
  test::OobeJS().TapOnPath(kSkipButton);

  // Wait for `TapOnPath(kSkipButton)` to call
  // `LacrosDataMigrationScreen::OnUserAction()`.
  test::TestPredicateWaiter(
      base::BindRepeating(&FakeMigrator::IsCancelCalled,
                          base::Unretained(fake_migrator())))
      .Wait();
}
}  // namespace
}  // namespace ash
