// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class UserSelectionScreenTest : public LoginManagerTest {
 public:
  UserSelectionScreenTest() : LoginManagerTest() {
    login_manager_mixin_.AppendRegularUsers(3);
    login_manager_mixin_.AppendManagedUsers(1);
  }
  ~UserSelectionScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Enable ARC. Otherwise, the banner would not show.
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

  DISALLOW_COPY_AND_ASSIGN(UserSelectionScreenTest);
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

}  // namespace chromeos
