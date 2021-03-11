// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "components/user_manager/known_user.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kLoginOnlineShortDelay =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kLoginOnlineLongDelay =
    base::TimeDelta::FromSeconds(20);

}  // namespace

class UserSelectionScreenTest : public LoginManagerTest {
 public:
  UserSelectionScreenTest() : LoginManagerTest() {
    login_manager_mixin_.AppendRegularUsers(3);
    login_manager_mixin_.AppendManagedUsers(1);
  }
  UserSelectionScreenTest(const UserSelectionScreenTest&) = delete;
  UserSelectionScreenTest& operator=(const UserSelectionScreenTest&) = delete;
  ~UserSelectionScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Enable ARC. Otherwise, the banner would not show.
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Test that a banner shows up for known-unmanaged users that need dircrypto
// migration. Also test that no banner shows up for users that may be managed.
IN_PROC_BROWSER_TEST_F(UserSelectionScreenTest, ShowDircryptoMigrationBanner) {
  const auto& users = login_manager_mixin_.users();
  // No banner for the first user since default is no migration.
  EXPECT_FALSE(ash::LoginScreenTestApi::IsWarningBubbleShown());

  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  FakeCryptohomeClient::Get()->SetEcryptfsUserHome(
      cryptohome::CreateAccountIdentifierFromAccountId(users[1].account_id),
      true);

  // Focus the 2nd user pod (consumer).
  ASSERT_TRUE(ash::LoginScreenTestApi::FocusUser(users[1].account_id));

  // Wait for FakeCryptohomeClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should be shown for the 2nd user (consumer).
    return ash::LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();
  histogram_tester->ExpectBucketCount("Ash.Login.Login.MigrationBanner", true,
                                      1);

  FakeCryptohomeClient::Get()->SetEcryptfsUserHome(
      cryptohome::CreateAccountIdentifierFromAccountId(users[2].account_id),
      false);
  histogram_tester = std::make_unique<base::HistogramTester>();
  // Focus the 3rd user pod (consumer).
  ASSERT_TRUE(ash::LoginScreenTestApi::FocusUser(users[2].account_id));

  // Wait for FakeCryptohomeClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should be shown for the 3rd user (consumer).
    return !ash::LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();
  histogram_tester->ExpectBucketCount("Ash.Login.Login.MigrationBanner", false,
                                      1);

  FakeCryptohomeClient::Get()->SetEcryptfsUserHome(
      cryptohome::CreateAccountIdentifierFromAccountId(users[3].account_id),
      true);
  histogram_tester = std::make_unique<base::HistogramTester>();

  // Focus to the 4th user pod (enterprise).
  ASSERT_TRUE(ash::LoginScreenTestApi::FocusUser(users[3].account_id));

  // Wait for FakeCryptohomeClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should not be shown for the enterprise user.
    return !ash::LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();

  // Not recorded for enterprise.
  histogram_tester->ExpectUniqueSample("Ash.Login.Login.MigrationBanner", false,
                                       0);
}

class UserSelectionScreenEnforceOnlineTest : public LoginManagerTest,
                                             public LocalStateMixin::Delegate {
 public:
  UserSelectionScreenEnforceOnlineTest() : LoginManagerTest() {
    login_manager_mixin_.AppendManagedUsers(2);
  }
  ~UserSelectionScreenEnforceOnlineTest() override = default;
  UserSelectionScreenEnforceOnlineTest(
      const UserSelectionScreenEnforceOnlineTest&) = delete;
  UserSelectionScreenEnforceOnlineTest& operator=(
      const UserSelectionScreenEnforceOnlineTest&) = delete;

  // chromeos::LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    const auto& users = login_manager_mixin_.users();
    const base::Time now = base::DefaultClock::GetInstance()->Now();

    // User with expired offline login timeout.
    user_manager::known_user::SetLastOnlineSignin(users[0].account_id,
                                                  now - kLoginOnlineLongDelay);
    user_manager::known_user::SetOfflineSigninLimit(users[0].account_id,
                                                    kLoginOnlineShortDelay);

    // User withoin offline login timeout.
    user_manager::known_user::SetLastOnlineSignin(users[1].account_id, now);
    user_manager::known_user::SetOfflineSigninLimit(users[1].account_id,
                                                    kLoginOnlineShortDelay);
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  chromeos::LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(UserSelectionScreenEnforceOnlineTest,
                       IsOnlineLoginEnforced) {
  const auto& users = login_manager_mixin_.users();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(
      ash::LoginScreenTestApi::IsForcedOnlineSignin(users[0].account_id));
  EXPECT_FALSE(
      ash::LoginScreenTestApi::IsForcedOnlineSignin(users[1].account_id));
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(users[0].account_id));
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

}  // namespace chromeos
