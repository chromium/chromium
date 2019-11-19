// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/arc/arc_util.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kAffiliatedUserEmail[] = "affiliated-user@example.com";
constexpr char kAffiliatedUserGaiaId[] = "affiliated-user@example.com";
constexpr char kAffiliationID[] = "some-affiliation-id";
constexpr char kAnotherAffiliationID[] = "another-affiliation-id";

struct Params {
  explicit Params(bool _affiliated) : affiliated(_affiliated) {}
  bool affiliated;
};

}  // namespace

class UnaffiliatedArcAllowedTest
    : public DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface<Params> {
 public:
  UnaffiliatedArcAllowedTest()
      : affiliated_account_id_(
            AccountId::FromUserEmailGaiaId(kAffiliatedUserEmail,
                                           kAffiliatedUserGaiaId)) {
    set_exit_when_last_browser_closes(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
    AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    UserPolicyBuilder user_policy;
    DevicePolicyCrosTestHelper test_helper;

    const std::set<std::string> device_affiliation_ids = {kAffiliationID};
    const std::set<std::string> user_affiliation_ids = {
        GetParam().affiliated ? kAffiliationID : kAnotherAffiliationID};

    AffiliationTestHelper affiliation_helper =
        AffiliationTestHelper::CreateForCloud(session_manager_client());
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
        &test_helper, device_affiliation_ids));
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
        &user_policy, affiliated_account_id_, user_affiliation_ids));
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (chromeos::LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }
    arc::ArcSessionManager::Get()->Shutdown();
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

 protected:
  void SetPolicy(bool allowed) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_unaffiliated_arc_allowed()->set_unaffiliated_arc_allowed(
        allowed);
    RefreshPolicyAndWaitUntilDeviceSettingsUpdated();
  }

  void RefreshPolicyAndWaitUntilDeviceSettingsUpdated() {
    base::RunLoop run_loop;
    std::unique_ptr<chromeos::CrosSettings::ObserverSubscription> observer =
        chromeos::CrosSettings::Get()->AddSettingsObserver(
            chromeos::kUnaffiliatedArcAllowed, run_loop.QuitClosure());
    RefreshDevicePolicy();
    run_loop.Run();
  }

  const AccountId affiliated_account_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnaffiliatedArcAllowedTest);
};

IN_PROC_BROWSER_TEST_P(UnaffiliatedArcAllowedTest, PRE_ProfileTest) {
  AffiliationTestHelper::PreLoginUser(affiliated_account_id_);
}

IN_PROC_BROWSER_TEST_P(UnaffiliatedArcAllowedTest, ProfileTest) {
  AffiliationTestHelper::LoginUser(affiliated_account_id_);
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(affiliated_account_id_);
  const Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  const bool affiliated = GetParam().affiliated;

  EXPECT_EQ(affiliated, user->IsAffiliated());
  EXPECT_TRUE(arc::IsArcAllowedForProfile(profile))
      << "Policy UnaffiliatedArcAllowed is unset, "
      << "expected ARC to be allowed for " << (affiliated ? "" : "un")
      << "affiliated users.";
  SetPolicy(false);
  arc::ResetArcAllowedCheckForTesting(profile);
  EXPECT_EQ(affiliated, arc::IsArcAllowedForProfile(profile))
      << "Policy UnaffiliatedArcAllowed is false, "
      << "expected ARC to be " << (affiliated ? "" : "dis") << "allowed "
      << "for " << (affiliated ? "" : "un") << "affiliated users.";
  SetPolicy(true);
  arc::ResetArcAllowedCheckForTesting(profile);
  EXPECT_TRUE(arc::IsArcAllowedForProfile(profile))
      << "Policy UnaffiliatedArcAllowed is true, "
      << "expected ARC to be allowed for " << (affiliated ? "" : "un")
      << "affiliated users.";
}

INSTANTIATE_TEST_SUITE_P(Blub,
                         UnaffiliatedArcAllowedTest,
                         ::testing::Values(Params(true), Params(false)));
}  // namespace policy
