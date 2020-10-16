// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace chromeos {

namespace {

constexpr char kEncryptionMigrationId[] = "encryption-migration";

const test::UIPath kReadyDialog = {kEncryptionMigrationId, "ready-dialog"};
const test::UIPath kMigratingDialog = {kEncryptionMigrationId,
                                       "migrating-dialog"};
const test::UIPath kErrorDialog = {kEncryptionMigrationId, "error-dialog"};
const test::UIPath kInsufficientSpaceDialog = {kEncryptionMigrationId,
                                               "insufficient-space-dialog"};
const test::UIPath kMinimalMigrationDialog = {kEncryptionMigrationId,
                                              "minimal-migration-dialog"};
const test::UIPath kMigrationProgress = {kEncryptionMigrationId,
                                         "migration-progress"};
const test::UIPath kSkipButton = {kEncryptionMigrationId, "skip-button"};
const test::UIPath kRestartButton = {kEncryptionMigrationId, "restart-button"};
const test::UIPath kUpgradeButton = {kEncryptionMigrationId, "upgrade-button"};
const test::UIPath kInsufficientSpaceSkipButton = {
    kEncryptionMigrationId, "insufficient-space-skip-button"};
const test::UIPath kInsufficientSpaceRestartButton = {
    kEncryptionMigrationId, "insufficient-space-restart-button"};

}  // namespace

class EncryptionMigrationTest : public OobeBaseTest {
 public:
  EncryptionMigrationTest() = default;
  ~EncryptionMigrationTest() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    // Enable ARC, so dircrypto encryption is forced.
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
  }
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    FakeCryptohomeClient::Get()->set_run_default_dircrypto_migration(false);

    // Configure encryption migration screen for test.
    EncryptionMigrationScreen* screen = EncryptionMigrationScreen::Get(
        WizardController::default_controller()->screen_manager());
    screen->set_tick_clock_for_testing(&tick_clock_);
    screen->set_free_disk_space_fetcher_for_testing(base::BindRepeating(
        &EncryptionMigrationTest::GetFreeSpace, base::Unretained(this)));
  }

 protected:
  void SetUpStubAuthenticatorAndAttemptLogin(bool has_incomplete_migration) {
    const UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(test_user_);

    auto authenticator_builder =
        std::make_unique<StubAuthenticatorBuilder>(user_context);
    authenticator_builder->SetUpOldEncryption(has_incomplete_migration);

    login_manager_.AttemptLoginUsingAuthenticator(
        user_context, std::move(authenticator_builder));
  }

  void WaitForActiveSession() { login_manager_.WaitForActiveSession(); }

  cryptohome::AccountIdentifier GetTestCryptohomeId() const {
    return cryptohome::CreateAccountIdentifierFromAccountId(
        test_user_.account_id);
  }

  void SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction action) {
    std::unique_ptr<ScopedUserPolicyUpdate> updater =
        user_policy_mixin_.RequestPolicyUpdate();
    updater->policy_payload()->mutable_ecryptfsmigrationstrategy()->set_value(
        static_cast<int>(action));
  }

  // Runs a successful full migration flow, and tests that UI is updated as
  // expected.
  void RunFullMigrationFlowTest() {
    test::OobeJS().CreateVisibilityWaiter(true, kMigratingDialog)->Wait();

    test::OobeJS().ExpectHiddenPath(kReadyDialog);
    test::OobeJS().ExpectHiddenPath(kErrorDialog);
    test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
    test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

    EXPECT_EQ(
        GetTestCryptohomeId(),
        FakeCryptohomeClient::Get()->get_id_for_disk_migrated_to_dircrypto());
    EXPECT_FALSE(FakeCryptohomeClient::Get()->minimal_migration());

    EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

    // Simulate successful migration - restart should be requested immediately
    // after success is reported.
    FakeCryptohomeClient::Get()->NotifyDircryptoMigrationProgress(
        cryptohome::DIRCRYPTO_MIGRATION_INITIALIZING, 0 /*current*/,
        5 /*total*/);
    EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

    test::OobeJS().ExpectAttributeEQ("indeterminate", kMigrationProgress, true);

    FakeCryptohomeClient::Get()->NotifyDircryptoMigrationProgress(
        cryptohome::DIRCRYPTO_MIGRATION_IN_PROGRESS, 3 /*current*/,
        5 /*total*/);
    EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

    test::OobeJS().ExpectAttributeEQ("indeterminate", kMigrationProgress,
                                     false);
    test::OobeJS().ExpectAttributeEQ("value * 100", kMigrationProgress, 60);
    test::OobeJS().ExpectAttributeEQ("max", kMigrationProgress, 1);

    FakeCryptohomeClient::Get()->NotifyDircryptoMigrationProgress(
        cryptohome::DIRCRYPTO_MIGRATION_SUCCESS, 5 /*current*/, 5 /*total*/);

    EXPECT_EQ(1, FakePowerManagerClient::Get()->num_request_restart_calls());
  }

  // Updates the battery percent info reported by the power manager client.
  void SetBatteryPercent(int battery_percent) {
    base::Optional<power_manager::PowerSupplyProperties> properties =
        FakePowerManagerClient::Get()->GetLastStatus();
    ASSERT_TRUE(properties.has_value());
    properties->set_battery_percent(battery_percent);
    FakePowerManagerClient::Get()->UpdatePowerProperties(properties.value());
  }

  void set_free_space(int64_t free_space) { free_space_ = free_space; }

  void AdvanceTime(base::TimeDelta delta) { tick_clock_.Advance(delta); }

 private:
  int64_t GetFreeSpace() const { return free_space_; }

  // Encryption migration requires at least 50 MB - set the default reported
  // free space to an arbitrary amount above that limit.
  int64_t free_space_ = 200 * 1024 * 1024;

  base::SimpleTestTickClock tick_clock_;

  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@gmail.com", "user")};
  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, SkipWithNoPolicySet) {
  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  test::OobeJS().ExpectVisiblePath(kSkipButton);
  test::OobeJS().ExpectVisiblePath(kUpgradeButton);

  // Click skip - this should start the user session.
  test::OobeJS().TapOnPath(kSkipButton);

  WaitForActiveSession();

  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigrateWithNoUserPolicySet) {
  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  test::OobeJS().ExpectVisiblePath(kSkipButton);
  test::OobeJS().ExpectVisiblePath(kUpgradeButton);

  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());

  test::OobeJS().TapOnPath(kUpgradeButton);

  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       ResumeMigrationWithNoUserPolicySet) {
  SetUpStubAuthenticatorAndAttemptLogin(true /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  // Migration is expected to continue immediately.
  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigratePolicy) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  // With kMigrate policy, the migration should start immediately.
  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       ResumeMigrationWithMigratePolicy) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(true /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  RunFullMigrationFlowTest();
}

// The "ask user" mode should be available to consumer users only - is set as a
// policy value, it should be treated the same as migrate.
IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, AskUserPolicy) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kAskUser);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  // Verify that ready dialog is not shown, and that the migration started
  // without ask user for confirmation.
  test::OobeJS().CreateVisibilityWaiter(true, kMigratingDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kReadyDialog);

  EXPECT_EQ(
      GetTestCryptohomeId(),
      FakeCryptohomeClient::Get()->get_id_for_disk_migrated_to_dircrypto());
  EXPECT_FALSE(FakeCryptohomeClient::Get()->minimal_migration());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MinimalMigration) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMinimalMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kMinimalMigrationDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

  EXPECT_EQ(
      GetTestCryptohomeId(),
      FakeCryptohomeClient::Get()->get_id_for_disk_migrated_to_dircrypto());
  EXPECT_TRUE(FakeCryptohomeClient::Get()->minimal_migration());

  // Simulate migration success - restart should be requested immediately after.
  FakeCryptohomeClient::Get()->NotifyDircryptoMigrationProgress(
      cryptohome::DIRCRYPTO_MIGRATION_SUCCESS, 5 /*current*/, 5 /*total*/);
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

  WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MinimalMigrationWithTimeout) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMinimalMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kMinimalMigrationDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

  EXPECT_EQ(
      GetTestCryptohomeId(),
      FakeCryptohomeClient::Get()->get_id_for_disk_migrated_to_dircrypto());
  EXPECT_TRUE(FakeCryptohomeClient::Get()->minimal_migration());

  // Simulate time passage during migration - enough for the user to get asked
  // to reauthenticate upon migration completion..
  AdvanceTime(base::TimeDelta::FromMinutes(3));

  FakeCryptohomeClient::Get()->NotifyDircryptoMigrationProgress(
      cryptohome::DIRCRYPTO_MIGRATION_SUCCESS, 5 /*current*/, 5 /*total*/);
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       PRE_MinimalMigrationPolicyWithIncompleteFullMigration) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kMigratingDialog))
      ->Wait();
}

// Tests that attempted full migration is continued, even if the migration mode
// changes to minimal in mean time.
IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       MinimalMigrationPolicyWithIncompleteFullMigration) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMinimalMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(true /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, PRE_ResumeMinimalMigration) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMinimalMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kMinimalMigrationDialog))
      ->Wait();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, ResumeMinimalMigration) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(true /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kMinimalMigrationDialog)->Wait();

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

  EXPECT_EQ(
      GetTestCryptohomeId(),
      FakeCryptohomeClient::Get()->get_id_for_disk_migrated_to_dircrypto());
  EXPECT_TRUE(FakeCryptohomeClient::Get()->minimal_migration());

  // Simulate migration success - restart should be requested immediately after.
  FakeCryptohomeClient::Get()->NotifyDircryptoMigrationProgress(
      cryptohome::DIRCRYPTO_MIGRATION_SUCCESS, 5 /*current*/, 5 /*total*/);
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

  WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigrationDisallowedByPolicy) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kDisallowMigration);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  WaitForActiveSession();
  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, WipeMigrationActionPolicy) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kWipe);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);

  // Wipe is expected to wipe the cryptohome, and force online login.
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();

  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       InsufficientSpaceWithNoUserPolicy) {
  set_free_space(5 * 1000 * 1000);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kInsufficientSpaceDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceRestartButton);
  test::OobeJS().ExpectVisiblePath(kInsufficientSpaceSkipButton);
  test::OobeJS().TapOnPath(kInsufficientSpaceSkipButton);

  WaitForActiveSession();
  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigrateWithInsuficientSpace) {
  set_free_space(5 * 1000 * 1000);
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kInsufficientSpaceDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  test::OobeJS().ExpectVisiblePath(kInsufficientSpaceRestartButton);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceSkipButton);

  test::OobeJS().TapOnPath(kInsufficientSpaceRestartButton);

  EXPECT_EQ(1, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, InsufficientSpaceOnResume) {
  set_free_space(5 * 1000 * 1000);
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(true /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kInsufficientSpaceDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  test::OobeJS().ExpectVisiblePath(kInsufficientSpaceRestartButton);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceSkipButton);

  test::OobeJS().TapOnPath(kInsufficientSpaceRestartButton);

  EXPECT_EQ(1, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigrationFailure) {
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kMigratingDialog))
      ->Wait();

  EXPECT_EQ(
      GetTestCryptohomeId(),
      FakeCryptohomeClient::Get()->get_id_for_disk_migrated_to_dircrypto());
  FakeCryptohomeClient::Get()->NotifyDircryptoMigrationProgress(
      cryptohome::DIRCRYPTO_MIGRATION_FAILED, 5 /*current*/, 5 /*total*/);

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());

  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  test::OobeJS().ExpectVisiblePath(kRestartButton);
  test::OobeJS().TapOnPath(kRestartButton);

  EXPECT_EQ(1, FakePowerManagerClient::Get()->num_request_restart_calls());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, LowBattery) {
  SetBatteryPercent(5);
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  test::OobeJS().ExpectVisiblePath(kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().ExpectVisiblePath(kUpgradeButton);
  test::OobeJS().ExpectDisabledPath(kUpgradeButton);

  test::OobeJS().TapOnPath(kSkipButton);

  WaitForActiveSession();
  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       CannotSkipWithLowBatteryOnMigrationResume) {
  SetBatteryPercent(5);
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(true /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  test::OobeJS().ExpectPathDisplayed(false, kSkipButton);
  test::OobeJS().ExpectPathDisplayed(false, kUpgradeButton);

  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       StartMigrationWhenEnoughBattery) {
  SetBatteryPercent(5);
  SetUpEncryptionMigrationActionPolicy(
      arc::policy_util::EcryptfsMigrationAction::kMigrate);

  SetUpStubAuthenticatorAndAttemptLogin(false /* has_incomplete_migration */);
  OobeScreenWaiter(EncryptionMigrationScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kMinimalMigrationDialog);

  EXPECT_FALSE(FakeCryptohomeClient::Get()
                   ->get_id_for_disk_migrated_to_dircrypto()
                   .has_account_id());

  SetBatteryPercent(60);

  RunFullMigrationFlowTest();
}

}  // namespace chromeos
