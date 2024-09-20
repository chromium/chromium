// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "content/public/test/browser_test.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {

namespace {

constexpr char kEncryptionMigrationId[] = "encryption-migration";

const test::UIPath kReadyDialog = {kEncryptionMigrationId, "ready-dialog"};
const test::UIPath kMigratingDialog = {kEncryptionMigrationId,
                                       "migrating-dialog"};
const test::UIPath kErrorDialog = {kEncryptionMigrationId, "error-dialog"};
const test::UIPath kInsufficientSpaceDialog = {kEncryptionMigrationId,
                                               "insufficient-space-dialog"};
const test::UIPath kMigrationProgress = {kEncryptionMigrationId,
                                         "migration-progress"};
const test::UIPath kSkipButton = {kEncryptionMigrationId, "skip-button"};
const test::UIPath kRestartButton = {kEncryptionMigrationId, "restart-button"};
const test::UIPath kUpgradeButton = {kEncryptionMigrationId, "upgrade-button"};
const test::UIPath kInsufficientSpaceSkipButton = {
    kEncryptionMigrationId, "insufficient-space-skip-button"};
const test::UIPath kInsufficientSpaceRestartButton = {
    kEncryptionMigrationId, "insufficient-space-restart-button"};

using AuthOp = FakeUserDataAuthClient::Operation;

}  // namespace

// Base class for testing encryption migration during sign-in.
// The test user account will be specified by the base class.
class EncryptionMigrationTestBase
    : public OobeBaseTest,
      public EncryptionMigrationScreen::EncryptionMigrationScreenTestDelegate {
 public:
  explicit EncryptionMigrationTestBase(
      const LoginManagerMixin::TestUserInfo& test_user)
      : test_user_(test_user) {}

  ~EncryptionMigrationTestBase() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    // Enable ARC, so dircrypto encryption is forced.
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
  }
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    FakeUserDataAuthClient::TestApi::Get()->SetHomeEncryptionMethod(
        GetTestCryptohomeId(),
        FakeUserDataAuthClient::HomeEncryptionMethod::kEcryptfs);
    FakeUserDataAuthClient::TestApi::Get()->set_run_default_dircrypto_migration(
        false);

    // Configure encryption migration screen for test.
    EncryptionMigrationScreen::SetEncryptionMigrationScreenTestDelegate(this);
  }
  void TearDownOnMainThread() override {
    EncryptionMigrationScreen::SetEncryptionMigrationScreenTestDelegate(
        nullptr);
    OobeBaseTest::TearDownOnMainThread();
  }

 protected:
  void SetUpDBusClientAndAttemptLogin(bool has_incomplete_migration) {
    const UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(test_user_);
    FakeUserDataAuthClient::TestApi::Get()->SetHomeEncryptionMethod(
        GetTestCryptohomeId(),
        FakeUserDataAuthClient::HomeEncryptionMethod::kEcryptfs);
    FakeUserDataAuthClient::TestApi::Get()->SetEncryptionMigrationIncomplete(
        GetTestCryptohomeId(), has_incomplete_migration);
    login_manager_.AttemptLoginUsingFakeDataAuthClient(user_context);
  }

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

  void MarkUserHasEnterprisePolicy() {
    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetProfileRequiresPolicy(
        test_user_.account_id,
        user_manager::ProfileRequiresPolicy::kPolicyRequired);
  }

  // Runs a successful full migration flow, and tests that UI is updated as
  // expected.
  void RunFullMigrationFlowTest() {
    test::OobeJS().CreateVisibilityWaiter(true, kMigratingDialog)->Wait();

    test::OobeJS().ExpectHiddenPath(kReadyDialog);
    test::OobeJS().ExpectHiddenPath(kErrorDialog);
    test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);

    auto migrate_request =
        FakeUserDataAuthClient::Get()
            ->GetLastRequest<AuthOp::kStartMigrateToDircrypto>();
    EXPECT_EQ(GetTestCryptohomeId(), migrate_request.account_id());
    EXPECT_FALSE(migrate_request.minimal_migration());

    EXPECT_EQ(
        0,
        chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());

    // Simulate successful migration - restart should be requested immediately
    // after success is reported.
    FakeUserDataAuthClient::Get()->NotifyDircryptoMigrationProgress(
        ::user_data_auth::DircryptoMigrationStatus::
            DIRCRYPTO_MIGRATION_INITIALIZING,
        0 /*current*/, 5 /*total*/);
    EXPECT_EQ(
        0,
        chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());

    test::OobeJS().ExpectAttributeEQ("indeterminate", kMigrationProgress, true);

    FakeUserDataAuthClient::Get()->NotifyDircryptoMigrationProgress(
        ::user_data_auth::DircryptoMigrationStatus::
            DIRCRYPTO_MIGRATION_IN_PROGRESS,
        3 /*current*/, 5 /*total*/);
    EXPECT_EQ(
        0,
        chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());

    test::OobeJS().ExpectAttributeEQ("indeterminate", kMigrationProgress,
                                     false);
    test::OobeJS().ExpectAttributeEQ("value * 100", kMigrationProgress, 60);
    test::OobeJS().ExpectAttributeEQ("max", kMigrationProgress, 1);

    FakeUserDataAuthClient::Get()->NotifyDircryptoMigrationProgress(
        ::user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
        5 /*current*/, 5 /*total*/);

    EXPECT_EQ(
        1,
        chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  }

  // Updates the battery percent info reported by the power manager client.
  void SetBatteryPercent(int battery_percent) {
    std::optional<power_manager::PowerSupplyProperties> properties =
        chromeos::FakePowerManagerClient::Get()->GetLastStatus();
    ASSERT_TRUE(properties.has_value());
    properties->set_battery_percent(battery_percent);
    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(
        properties.value());
  }

  void set_free_space(int64_t free_space) { free_space_ = free_space; }

 private:
  // EncryptionMigrationScreen::EncryptionMigrationScreenTestDelegate
  int64_t GetFreeSpace() const override { return free_space_; }

  // Encryption migration requires at least 50 MB - set the default reported
  // free space to an arbitrary amount above that limit.
  int64_t free_space_ = 200 * 1024 * 1024;

  const LoginManagerMixin::TestUserInfo test_user_;
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_,
                                   {test_user_},
                                   nullptr,
                                   &cryptohome_mixin_};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

// Test for encryption migration during sign-in for regular users.
class EncryptionMigrationTest : public EncryptionMigrationTestBase {
 public:
  EncryptionMigrationTest()
      : EncryptionMigrationTestBase(LoginManagerMixin::TestUserInfo{
            AccountId::FromUserEmailGaiaId("user@gmail.com", "user")}) {}
  ~EncryptionMigrationTest() override = default;

  EncryptionMigrationTest(const EncryptionMigrationTest& other) = delete;
  EncryptionMigrationTest& operator=(const EncryptionMigrationTest& other) =
      delete;
};

// Test for encryption migration during sign-in for child users.
class EncryptionMigrationChildUserTest : public EncryptionMigrationTestBase {
 public:
  EncryptionMigrationChildUserTest()
      : EncryptionMigrationTestBase(LoginManagerMixin::TestUserInfo{
            AccountId::FromUserEmailGaiaId("userchild@gmail.com", "userchild"),
            test::kDefaultAuthSetup, user_manager::UserType::kChild}) {}
  ~EncryptionMigrationChildUserTest() override = default;

  EncryptionMigrationChildUserTest(
      const EncryptionMigrationChildUserTest& other) = delete;
  EncryptionMigrationChildUserTest& operator=(
      const EncryptionMigrationChildUserTest& other) = delete;
};

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, SkipWithNoPolicySet) {
  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);

  test::OobeJS().ExpectVisiblePath(kSkipButton);
  test::OobeJS().ExpectVisiblePath(kUpgradeButton);

  // Click skip - this should start the user session.
  test::OobeJS().TapOnPath(kSkipButton);

  WaitForActiveSession();

  EXPECT_FALSE(FakeUserDataAuthClient::Get()
                   ->WasCalled<AuthOp::kStartMigrateToDircrypto>());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigrateWithNoUserPolicySet) {
  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);

  test::OobeJS().ExpectVisiblePath(kSkipButton);
  test::OobeJS().ExpectVisiblePath(kUpgradeButton);

  EXPECT_FALSE(FakeUserDataAuthClient::Get()
                   ->WasCalled<AuthOp::kStartMigrateToDircrypto>());

  test::OobeJS().TapOnPath(kUpgradeButton);

  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       ResumeMigrationWithNoUserPolicySet) {
  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/true);
  encryption_migration_screen_waiter.Wait();

  // Migration is expected to continue immediately.
  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigratePolicy) {
  MarkUserHasEnterprisePolicy();

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  // With kMigrate policy, the migration should start immediately.
  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       ResumeMigrationWithMigratePolicy) {
  MarkUserHasEnterprisePolicy();

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/true);
  encryption_migration_screen_waiter.Wait();

  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationChildUserTest, MigrateForChildUser) {
  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  // With kMigrate policy, the migration should start immediately.
  RunFullMigrationFlowTest();
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       InsufficientSpaceWithNoUserPolicy) {
  set_free_space(5 * 1000 * 1000);

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kInsufficientSpaceDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceRestartButton);
  test::OobeJS().ExpectVisiblePath(kInsufficientSpaceSkipButton);
  test::OobeJS().TapOnPath(kInsufficientSpaceSkipButton);

  WaitForActiveSession();
  EXPECT_FALSE(FakeUserDataAuthClient::Get()
                   ->WasCalled<AuthOp::kStartMigrateToDircrypto>());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigrateWithInsuficientSpace) {
  set_free_space(5 * 1000 * 1000);
  MarkUserHasEnterprisePolicy();

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kInsufficientSpaceDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  test::OobeJS().ExpectVisiblePath(kInsufficientSpaceRestartButton);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceSkipButton);

  test::TapOnPathAndWaitForOobeToBeDestroyed(kInsufficientSpaceRestartButton);

  EXPECT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_FALSE(FakeUserDataAuthClient::Get()
                   ->WasCalled<AuthOp::kStartMigrateToDircrypto>());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, InsufficientSpaceOnResume) {
  set_free_space(5 * 1000 * 1000);
  MarkUserHasEnterprisePolicy();

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/true);
  encryption_migration_screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kInsufficientSpaceDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  test::OobeJS().ExpectVisiblePath(kInsufficientSpaceRestartButton);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceSkipButton);

  test::TapOnPathAndWaitForOobeToBeDestroyed(kInsufficientSpaceRestartButton);

  EXPECT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_FALSE(FakeUserDataAuthClient::Get()
                   ->WasCalled<AuthOp::kStartMigrateToDircrypto>());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, MigrationFailure) {
  MarkUserHasEnterprisePolicy();

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kMigratingDialog))
      ->Wait();

  EXPECT_EQ(GetTestCryptohomeId(),
            FakeUserDataAuthClient::Get()
                ->GetLastRequest<AuthOp::kStartMigrateToDircrypto>()
                .account_id());
  FakeUserDataAuthClient::Get()->NotifyDircryptoMigrationProgress(
      ::user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_FAILED,
      5 /*current*/, 5 /*total*/);

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());

  test::OobeJS().CreateVisibilityWaiter(true, kErrorDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kReadyDialog);
  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);

  test::OobeJS().ExpectVisiblePath(kRestartButton);
  test::TapOnPathAndWaitForOobeToBeDestroyed(kRestartButton);

  EXPECT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest, LowBattery) {
  SetBatteryPercent(5);
  MarkUserHasEnterprisePolicy();

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  test::OobeJS().ExpectVisiblePath(kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().ExpectVisiblePath(kUpgradeButton);
  test::OobeJS().ExpectDisabledPath(kUpgradeButton);

  test::OobeJS().TapOnPath(kSkipButton);

  WaitForActiveSession();
  EXPECT_FALSE(FakeUserDataAuthClient::Get()
                   ->WasCalled<AuthOp::kStartMigrateToDircrypto>());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       CannotSkipWithLowBatteryOnMigrationResume) {
  SetBatteryPercent(5);
  MarkUserHasEnterprisePolicy();

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/true);
  encryption_migration_screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  test::OobeJS().ExpectPathDisplayed(false, kSkipButton);
  test::OobeJS().ExpectPathDisplayed(false, kUpgradeButton);

  EXPECT_FALSE(FakeUserDataAuthClient::Get()
                   ->WasCalled<AuthOp::kStartMigrateToDircrypto>());
}

IN_PROC_BROWSER_TEST_F(EncryptionMigrationTest,
                       StartMigrationWhenEnoughBattery) {
  SetBatteryPercent(5);
  MarkUserHasEnterprisePolicy();

  OobeScreenWaiter encryption_migration_screen_waiter(
      EncryptionMigrationScreenView::kScreenId);
  SetUpDBusClientAndAttemptLogin(/*has_incomplete_migration=*/false);
  encryption_migration_screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kReadyDialog)->Wait();

  test::OobeJS().ExpectHiddenPath(kMigratingDialog);
  test::OobeJS().ExpectHiddenPath(kInsufficientSpaceDialog);
  test::OobeJS().ExpectHiddenPath(kErrorDialog);

  EXPECT_FALSE(FakeUserDataAuthClient::Get()
                   ->WasCalled<AuthOp::kStartMigrateToDircrypto>());

  SetBatteryPercent(60);

  RunFullMigrationFlowTest();
}

// TODO(b/271142350): Add test coverage for EncryptionMigrationScreen obtaining
// the wake lock.

}  // namespace ash
