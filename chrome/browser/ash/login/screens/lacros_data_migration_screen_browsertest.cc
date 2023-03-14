// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/lacros_data_migration_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {
constexpr char kLacrosDataMigrationId[] = "lacros-data-migration";
const test::UIPath kSkipButton = {kLacrosDataMigrationId, "skipButton"};
const test::UIPath kUpdating = {kLacrosDataMigrationId, "updating"};
const test::UIPath kLowBattery = {kLacrosDataMigrationId, "lowBattery"};
const test::UIPath kProgressDialog = {kLacrosDataMigrationId, "progressDialog"};
const test::UIPath kErrorDialog = {kLacrosDataMigrationId, "errorDialog"};
const test::UIPath kLowDiskSpace = {kLacrosDataMigrationId,
                                    "lowDiskSpaceError"};
const test::UIPath kGenericError = {kLacrosDataMigrationId, "genericError"};
const test::UIPath kCancelButton = {kLacrosDataMigrationId, "cancelButton"};
const test::UIPath kGotoFilesButton = {kLacrosDataMigrationId,
                                       "gotoFilesButton"};

class FakeMigrator : public BrowserDataMigrator {
 public:
  // BrowserDataMigrator overrides.
  void Migrate(crosapi::browser_util::MigrationMode mode,
               MigrateCallback callback) override {
    callback_ = std::move(callback);
  }
  void Cancel() override { cancel_called_ = true; }

  bool IsCancelCalled() { return cancel_called_; }

  void MaybeRunCallback(const BrowserDataMigrator::Result& result) {
    if (!callback_.is_null())
      std::move(callback_).Run(result);
  }

 private:
  bool cancel_called_ = false;
  MigrateCallback callback_;
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
    lacros_data_migration_screen->SetAttemptRestartForTesting(
        base::BindRepeating(
            &LacrosDataMigrationScreenTest::OnAttemptRestartCalled,
            base::Unretained(this)));
    lacros_data_migration_screen->SetSkipPostShowButtonForTesting(true);
    OobeBaseTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kBrowserDataMigrationMode,
        browser_data_migrator_util::kMoveSwitchValue);
  }

  bool is_attempt_restart_called() const { return is_attempt_restart_called_; }

 protected:
  FakeMigrator* fake_migrator() { return fake_migrator_; }
  chromeos::FakePowerManagerClient* power_manager_client() {
    return static_cast<chromeos::FakePowerManagerClient*>(
        chromeos::PowerManagerClient::Get());
  }
  void OnAttemptRestartCalled() { is_attempt_restart_called_ = true; }

 private:
  // This is owned by `LacrosDataMigrationScreen`.
  FakeMigrator* fake_migrator_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  LoginManagerMixin login_mixin_{&mixin_host_};
  bool is_attempt_restart_called_ = false;
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

  test::OobeJS().CreateVisibilityWaiter(true, kSkipButton)->Wait();

  EXPECT_FALSE(fake_migrator()->IsCancelCalled());
  test::OobeJS().TapOnPath(kSkipButton);

  // Wait for `TapOnPath(kSkipButton)` to call
  // `LacrosDataMigrationScreen::OnUserActionDeprecated()`.
  test::TestPredicateWaiter(
      base::BindRepeating(&FakeMigrator::IsCancelCalled,
                          base::Unretained(fake_migrator())))
      .Wait();
}

IN_PROC_BROWSER_TEST_F(LacrosDataMigrationScreenTest, LowBattery) {
  power_manager::PowerSupplyProperties power_props;
  power_props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  power_props.set_battery_percent(100);
  power_manager_client()->UpdatePowerProperties(power_props);

  OobeScreenWaiter waiter(LacrosDataMigrationScreenView::kScreenId);
  WizardController::default_controller()->AdvanceToScreen(
      LacrosDataMigrationScreenView::kScreenId);
  waiter.Wait();

  // By default, low-battery screen should be hidden.
  test::OobeJS().CreateVisibilityWaiter(true, kUpdating)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kLowBattery)->Wait();

  // If the battery is low, and the charger is not connected,
  // the low battery warning should be shown.
  power_props.Clear();
  power_props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_props.set_battery_percent(30);
  power_manager_client()->UpdatePowerProperties(power_props);

  test::OobeJS().CreateVisibilityWaiter(false, kUpdating)->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLowBattery)->Wait();

  // If power is enough, even if the charger is not connected,
  // the low battery warning should be hidden.
  power_props.Clear();
  power_props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_props.set_battery_percent(100);
  power_manager_client()->UpdatePowerProperties(power_props);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdating)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kLowBattery)->Wait();

  // Similarly, even if the battery is lower, if the charger is connected,
  // the low battery warning should be hidden, still.
  // To confirm the state transition, first switching to low-battery state.
  power_props.Clear();
  power_props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_props.set_battery_percent(30);
  power_manager_client()->UpdatePowerProperties(power_props);
  test::OobeJS().CreateVisibilityWaiter(false, kUpdating)->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLowBattery)->Wait();

  // Then, set to the charging and low-battery state.
  power_props.Clear();
  power_props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  power_props.set_battery_percent(30);
  power_manager_client()->UpdatePowerProperties(power_props);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdating)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kLowBattery)->Wait();
}

IN_PROC_BROWSER_TEST_F(LacrosDataMigrationScreenTest, OutOfDiskError) {
  OobeScreenWaiter waiter(LacrosDataMigrationScreenView::kScreenId);
  WizardController::default_controller()->AdvanceToScreen(
      LacrosDataMigrationScreenView::kScreenId);
  waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kProgressDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  fake_migrator()->MaybeRunCallback(
      {BrowserDataMigrator::ResultKind::kFailed, 12345});

  test::OobeJS().CreateVisibilityWaiter(false, kProgressDialog)->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();
  test::OobeJS().ExpectVisiblePath(kLowDiskSpace);
  test::OobeJS().ExpectHiddenPath(kGenericError);
  test::OobeJS().ExpectVisiblePath(kCancelButton);
  test::OobeJS().ExpectVisiblePath(kGotoFilesButton);
}

IN_PROC_BROWSER_TEST_F(LacrosDataMigrationScreenTest, GenericError) {
  OobeScreenWaiter waiter(LacrosDataMigrationScreenView::kScreenId);
  WizardController::default_controller()->AdvanceToScreen(
      LacrosDataMigrationScreenView::kScreenId);
  waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kProgressDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  fake_migrator()->MaybeRunCallback({BrowserDataMigrator::ResultKind::kFailed});

  test::OobeJS().CreateVisibilityWaiter(false, kProgressDialog)->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kLowDiskSpace);
  test::OobeJS().ExpectVisiblePath(kGenericError);
  test::OobeJS().ExpectVisiblePath(kCancelButton);
  test::OobeJS().ExpectHiddenPath(kGotoFilesButton);
}

IN_PROC_BROWSER_TEST_F(LacrosDataMigrationScreenTest, OnCancel) {
  OobeScreenWaiter waiter(LacrosDataMigrationScreenView::kScreenId);
  WizardController::default_controller()->AdvanceToScreen(
      LacrosDataMigrationScreenView::kScreenId);
  waiter.Wait();

  fake_migrator()->MaybeRunCallback(
      {BrowserDataMigrator::ResultKind::kFailed, 12345});
  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();

  EXPECT_FALSE(is_attempt_restart_called());
  test::OobeJS().TapOnPath(kCancelButton);
  test::TestPredicateWaiter(
      base::BindRepeating(
          &LacrosDataMigrationScreenTest::is_attempt_restart_called,
          base::Unretained(this)))
      .Wait();
}

IN_PROC_BROWSER_TEST_F(LacrosDataMigrationScreenTest, OnGotoFiles) {
  const std::string user_id = "user-abcde";
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kBrowserDataMigrationForUser, user_id);

  OobeScreenWaiter waiter(LacrosDataMigrationScreenView::kScreenId);
  WizardController::default_controller()->AdvanceToScreen(
      LacrosDataMigrationScreenView::kScreenId);
  waiter.Wait();

  fake_migrator()->MaybeRunCallback(
      {BrowserDataMigrator::ResultKind::kFailed, 12345});
  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();

  EXPECT_FALSE(is_attempt_restart_called());
  EXPECT_FALSE(crosapi::browser_util::WasGotoFilesClicked(
      g_browser_process->local_state(), user_id));
  test::OobeJS().TapOnPath(kGotoFilesButton);
  test::TestPredicateWaiter(
      base::BindRepeating(
          &LacrosDataMigrationScreenTest::is_attempt_restart_called,
          base::Unretained(this)))
      .Wait();
  EXPECT_TRUE(crosapi::browser_util::WasGotoFilesClicked(
      g_browser_process->local_state(), user_id));
}

}  // namespace
}  // namespace ash
