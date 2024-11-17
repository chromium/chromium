// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/site_isolation/about_flags.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace policy {
namespace {

namespace em = ::enterprise_management;

struct Params {
  Params(std::string login_screen_isolate_origins,
         std::string user_policy_isolate_origins,
         bool user_policy_site_per_process,
         std::vector<std::string> user_flag_internal_names,
         bool ephemeral_users,
         bool expected_request_restart,
         std::vector<std::string> expected_switches_for_user,
         std::vector<std::string> expected_isolated_origins = {})
      : login_screen_isolate_origins(login_screen_isolate_origins),
        user_policy_isolate_origins(user_policy_isolate_origins),
        user_policy_site_per_process(user_policy_site_per_process),
        user_flag_internal_names(std::move(user_flag_internal_names)),
        ephemeral_users(ephemeral_users),
        expected_request_restart(expected_request_restart),
        expected_switches_for_user(expected_switches_for_user),
        expected_isolated_origins(expected_isolated_origins) {}

  friend std::ostream& operator<<(std::ostream& os, const Params& p) {
    os << "{" << std::endl
       << "  login_screen_isolate_origins: " << p.login_screen_isolate_origins
       << std::endl
       << "  user_policy_site_per_process: " << p.user_policy_site_per_process
       << std::endl
       << "  user_policy_isolate_origins: " << p.user_policy_isolate_origins
       << std::endl
       << "  user_flag_internal_names: "
       << base::JoinString(p.user_flag_internal_names, ", ") << std::endl
       << "  ephemeral_users: " << p.ephemeral_users << std::endl
       << "  expected_request_restart: " << p.expected_request_restart
       << std::endl
       << "  expected_switches_for_user: "
       << base::JoinString(p.expected_switches_for_user, ", ") << std::endl
       << "  expected_isolated_origins: "
       << base::JoinString(p.expected_isolated_origins, ", ") << std::endl
       << "}";
    return os;
  }

  // If non-empty, --isolate-origins=|login_screen_isolate_origins| will be
  // passed to the login manager chrome instance between policy flag sentinels.
  // Note: On Chrome OS, login_manager evaluates device policy and does this.
  std::string login_screen_isolate_origins;

  // If non-empty, the IsolateOrigins user policy will be simulated to be set
  // |user_policy_isolate_origins|.
  std::string user_policy_isolate_origins;

  // If true, the SitePerProcess user policy will be simulated to be set to
  // true.
  bool user_policy_site_per_process;

  std::vector<std::string> user_flag_internal_names;

  // If true, ephemeral users are enabled.
  bool ephemeral_users;

  // If true, the test case will expect that AttemptRestart has been called by
  // UserSessionManager.
  bool expected_request_restart;

  // When a restart was requested, the test case verifies that the flags passed
  // to |SessionManagerClient::SetFlagsForUser| match
  // |expected_switches_for_user|.
  std::vector<std::string> expected_switches_for_user;

  // List of origins that should be isolated (via policy or via cmdline flag).
  std::vector<std::string> expected_isolated_origins;
};

// Defines the test cases that will be executed.
const Params kTestCases[] = {
    // 0. No site isolation in device or user policy - no restart expected.
    Params(std::string() /* login_screen_isolate_origins */,
           std::string() /* user_policy_isolate_origins */,
           false /* user_policy_site_per_process */,
           {} /* user_flag_internal_names */,
           false /* ephemeral_users */,
           false /* expected_request_restart */,
           {} /* expected_switches_for_user */),

    // 1. SitePerProcess opt-out through about://flags - restart expected.
    Params(
        std::string() /* login_screen_isolate_origins */,
        std::string() /* user_policy_isolate_origins */,
        false /* user_policy_site_per_process */,
        /* user_flag_internal_names */
        {about_flags::SiteIsolationTrialOptOutChoiceEnabled()},
        false /* ephemeral_users */,
        true /* expected_request_restart */,
        {"--disable-site-isolation-trials"} /* expected_switches_for_user */),

    // 2. SitePerProcess forced through user policy - opt-out through
    // about://flags entry expected to be ignored.
    Params(std::string() /* login_screen_isolate_origins */,
           std::string() /* user_policy_isolate_origins */,
           true /* user_policy_site_per_process */,
           /* user_flag_internal_names */
           {about_flags::SiteIsolationTrialOptOutChoiceEnabled()},
           false /* ephemeral_users */,
           false /* expected_request_restart */,
           {} /* expected_switches_for_user */),

    // 3. IsolateOrigins in user policy only - no restart expected, because
    //    IsolateOrigins from the user policy should be picked up by
    //    SiteIsolationPrefsObserver (without requiring injection of the
    //    --isolate-origins cmdline switch).
    Params(std::string() /* login_screen_isolate_origins */,
           "https://example.com" /* user_policy_isolate_origins */,
           false /* user_policy_site_per_process */,
           {} /* user_flag_internal_names */,
           false /* ephemeral_users */,
           false /* expected_request_restart */,
           {} /* expected_switches_for_user */,
           {"https://example.com"} /* expected_isolated_origins */)};

class SiteIsolationFlagHandlingTest
    : public ash::OobeBaseTest,
      public ::testing::WithParamInterface<Params> {
 public:
  SiteIsolationFlagHandlingTest(const SiteIsolationFlagHandlingTest&) = delete;
  SiteIsolationFlagHandlingTest& operator=(
      const SiteIsolationFlagHandlingTest&) = delete;

 protected:
  SiteIsolationFlagHandlingTest()
      : account_id_(AccountId::FromUserEmailGaiaId("username@examle.com",
                                                   "1111111111")) {}

  void SetUpInProcessBrowserTestFixture() override {
    ash::SessionManagerClient::InitializeFakeInMemory();

    // Mark that chrome restart can be requested.
    // Note that AttemptRestart() is mocked out in UserSessionManager through
    // |SetAttemptRestartClosureInTests| (set up in SetUpOnMainThread).
    ash::FakeSessionManagerClient::Get()->set_supports_browser_restart(true);

    std::unique_ptr<ash::ScopedDevicePolicyUpdate> update =
        device_state_.RequestDevicePolicyUpdate();
    update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(GetParam().ephemeral_users);
    update.reset();

    std::unique_ptr<ash::ScopedUserPolicyUpdate> user_policy_update =
        user_policy_.RequestPolicyUpdate();
    if (GetParam().user_policy_site_per_process) {
      user_policy_update->policy_payload()
          ->mutable_siteperprocess()
          ->mutable_policy_options()
          ->set_mode(em::PolicyOptions::MANDATORY);
      user_policy_update->policy_payload()->mutable_siteperprocess()->set_value(
          true);
    }

    if (!GetParam().user_policy_isolate_origins.empty()) {
      user_policy_update->policy_payload()
          ->mutable_isolateorigins()
          ->mutable_policy_options()
          ->set_mode(em::PolicyOptions::MANDATORY);
      user_policy_update->policy_payload()->mutable_isolateorigins()->set_value(
          GetParam().user_policy_isolate_origins);
    }
    user_policy_update.reset();

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLogin(account_id_.GetUserEmail(),
                                     account_id_.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);

    OobeBaseTest::SetUpOnMainThread();

    // Mock out chrome restart.
    ash::test::UserSessionManagerTestApi session_manager_test_api(
        ash::UserSessionManager::GetInstance());
    session_manager_test_api.SetAttemptRestartClosureInTests(
        base::BindRepeating(
            &SiteIsolationFlagHandlingTest::AttemptRestartCalled,
            base::Unretained(this)));

    // Observe for user session start.
    user_session_started_observer_ =
        std::make_unique<ash::SessionStateWaiter>();
  }

  bool HasAttemptRestartBeenCalled() const { return attempt_restart_called_; }

  // Called when chrome requests a restarted.
  void AttemptRestartCalled() {
    user_session_started_observer_.reset();
    attempt_restart_called_ = true;
  }

  void LogIn() {
    // Start user sign-in. We can't use |LoginPolicyTestBase::LogIn|, because
    // it waits for a user session start unconditionally, which will not happen
    // if chrome requests a restart to set user-session flags.
    login_manager_.SkipPostLoginScreens();
    OobeBaseTest::WaitForSigninScreen();
    login_manager_.LoginWithDefaultContext(user_);

    // Wait for either the user session to start, or for restart to be requested
    // (whichever happens first).
    user_session_started_observer_->Wait();
  }

  const AccountId account_id_;

  // This will be set to |true| when chrome has requested a restart.
  bool attempt_restart_called_ = false;

  // This is important because ephemeral users only work on enrolled machines.
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::UserPolicyMixin user_policy_{&mixin_host_, account_id_};

  const ash::LoginManagerMixin::TestUserInfo user_{account_id_};
  ash::LoginManagerMixin login_manager_{&mixin_host_, {user_}};

  FakeGaiaMixin fake_gaia_{&mixin_host_};

  // Observes for user session start.
  std::unique_ptr<ash::SessionStateWaiter> user_session_started_observer_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(SiteIsolationFlagHandlingTest, PRE_FlagHandlingTest) {
  LogIn();

  if (!GetParam().user_flag_internal_names.empty()) {
    Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetActiveUser());
    ASSERT_TRUE(profile);
    flags_ui::PrefServiceFlagsStorage flags_storage(profile->GetPrefs());
    std::set<std::string> flags_to_set;
    for (const std::string& flag_to_set : GetParam().user_flag_internal_names)
      flags_to_set.insert(flag_to_set);
    EXPECT_TRUE(flags_storage.SetFlags(flags_to_set));
    flags_storage.CommitPendingWrites();
  }
}

IN_PROC_BROWSER_TEST_P(SiteIsolationFlagHandlingTest, FlagHandlingTest) {
  // Skip tests where expected_request_restart is true.
  // See crbug.com/990817 for more details.
  if (GetParam().expected_request_restart)
    return;

  // Log in and wait for either the user session to start, or for the restart
  // to be requested (whichever happens first).
  LogIn();

  EXPECT_EQ(GetParam().expected_request_restart, HasAttemptRestartBeenCalled());

  // Verify that expected origins are isolated...
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  for (const std::string& origin_str : GetParam().expected_isolated_origins) {
    url::Origin origin = url::Origin::Create(GURL(origin_str));
    EXPECT_TRUE(policy->IsGloballyIsolatedOriginForTesting(origin));
  }

  if (!HasAttemptRestartBeenCalled())
    return;

  // Also verify flags if chrome was restarted.
  std::vector<std::string> switches_for_user;
  bool has_switches_for_user =
      ash::FakeSessionManagerClient::Get()->GetFlagsForUser(
          cryptohome::CreateAccountIdentifierFromAccountId(account_id_),
          &switches_for_user);
  EXPECT_TRUE(has_switches_for_user);

  // Remove flag sentinels. Keep whatever is between those sentinels, to
  // verify that we don't pass additional parameters in there.
  std::erase_if(switches_for_user, [](const std::string& flag) {
    return flag == "--flag-switches-begin" || flag == "--flag-switches-end";
  });
  EXPECT_EQ(GetParam().expected_switches_for_user, switches_for_user);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SiteIsolationFlagHandlingTest,
                         ::testing::ValuesIn(kTestCases));

}  // namespace policy
