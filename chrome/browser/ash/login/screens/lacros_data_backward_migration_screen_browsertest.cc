// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_backward_migration_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/ash/login/lacros_data_backward_migration_screen_handler.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {
constexpr char kUserIdHash[] = "abcdefg";

constexpr char kLacrosDataBackwardMigrationId[] =
    "lacros-data-backward-migration";
const test::UIPath kProgressDialog = {kLacrosDataBackwardMigrationId,
                                      "progressDialog"};
const test::UIPath kErrorDialog = {kLacrosDataBackwardMigrationId,
                                   "errorDialog"};

class FakeBackMigrator : public BrowserDataBackMigratorBase {
 public:
  void Migrate(BackMigrationProgressCallback progress_callback,
               BackMigrationFinishedCallback finished_callback) override {
    finished_callback_ = std::move(finished_callback);
  }

  void CancelMigration(
      BackMigrationCanceledCallback canceled_callback) override {}

  void MaybeRunFinishedCallback(const BrowserDataBackMigrator::Result& result) {
    if (!finished_callback_.is_null()) {
      std::move(finished_callback_).Run(result);
    }
  }

 private:
  BackMigrationFinishedCallback finished_callback_;
};

class LacrosDataBackwardMigrationScreenTest : public OobeBaseTest {
 public:
  LacrosDataBackwardMigrationScreenTest() {
    // Adding a user and marking OOBE completed with DeviceStateMixin ensures
    // that chrome://oobe/login is loaded instead of chrome://oobe/oobe and that
    // LoginDisplayHostMojo is created instead of LoginDisplayHostWebUI.
    login_mixin_.AppendRegularUsers(1);
  }
  LacrosDataBackwardMigrationScreenTest(
      const LacrosDataBackwardMigrationScreenTest&) = delete;
  LacrosDataBackwardMigrationScreenTest& operator=(
      const LacrosDataBackwardMigrationScreenTest&) = delete;
  ~LacrosDataBackwardMigrationScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    fake_back_migrator_ = new FakeBackMigrator();
    LacrosDataBackwardMigrationScreen::SetMigratorForTesting(
        fake_back_migrator_);

    command_line->AppendSwitchASCII(
        switches::kBrowserDataBackwardMigrationForUser, kUserIdHash);
    command_line->AppendSwitchASCII(
        crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch,
        GetLacrosDataBackwardMigrationModeName(
            crosapi::browser_util::LacrosDataBackwardMigrationMode::kKeepAll));
    OobeBaseTest::SetUpCommandLine(command_line);
  }

 protected:
  FakeBackMigrator* fake_back_migrator() { return fake_back_migrator_; }

 private:
  raw_ptr<FakeBackMigrator, ExperimentalAsh> fake_back_migrator_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(LacrosDataBackwardMigrationScreenTest, FailureScreen) {
  OobeScreenWaiter waiter(LacrosDataBackwardMigrationScreenView::kScreenId);
  waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kProgressDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  fake_back_migrator()->MaybeRunFinishedCallback(
      {BrowserDataBackMigrator::Result::kFailed});

  test::OobeJS().CreateVisibilityWaiter(false, kProgressDialog)->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();
}

}  // namespace
}  // namespace ash
