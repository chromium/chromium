// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "ash/components/arc/test/arc_util_test_support.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

struct Params {
  explicit Params(bool _affiliated) : affiliated(_affiliated) {}
  bool affiliated;
};

}  // namespace

class UnaffiliatedArcAllowedTest
    : public DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface<Params> {
 public:
  UnaffiliatedArcAllowedTest() {
    set_exit_when_last_browser_closes(false);
    affiliation_mixin_.set_affiliated(GetParam().affiliated);
    cryptohome_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    cryptohome_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
  }

  UnaffiliatedArcAllowedTest(const UnaffiliatedArcAllowedTest&) = delete;
  UnaffiliatedArcAllowedTest& operator=(const UnaffiliatedArcAllowedTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
    AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (ash::LoginDisplayHost::default_host()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
    base::CallbackListSubscription subscription =
        ash::CrosSettings::Get()->AddSettingsObserver(
            ash::kUnaffiliatedArcAllowed, run_loop.QuitClosure());
    RefreshDevicePolicy();
    run_loop.Run();
  }

  AffiliationMixin affiliation_mixin_{&mixin_host_, policy_helper()};

 private:
  ash::CryptohomeMixin cryptohome_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(UnaffiliatedArcAllowedTest, PRE_ProfileTest) {
  AffiliationTestHelper::PreLoginUser(affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_P(UnaffiliatedArcAllowedTest, ProfileTest) {
  AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
      affiliation_mixin_.account_id());
  const Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
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
