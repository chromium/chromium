// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <tuple>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::full_restore::RestoreOption;
using session_manager::SessionState;

namespace ash {
namespace {

enum class StartMode {
  // Simulates ash starts from the login screen.
  kLogin,
  // Simulates ash starts with a signed-in user, e.g. restart to apply flags.
  kRestart,
};

constexpr char kTestUser[] = "test_user@gmail.com";
constexpr char kTestGaiaId[] = "12345";

// DeferredTaskStartWaiter waits for post login deferred task to start.
class DeferredTaskStartWaiter : public session_manager::SessionManagerObserver {
 public:
  DeferredTaskStartWaiter() {
    observaation_.Observe(session_manager::SessionManager::Get());
  }

  void Wait() {
    if (IsStarted()) {
      return;
    }

    run_loop_.emplace();
    run_loop_->Run();
  }

  bool IsStarted() const {
    return session_manager::SessionManager::Get()
        ->IsUserSessionStartUpTaskCompleted();
  }

  // session_manager::SessionManagerObserver:
  void OnUserSessionStartUpTaskCompleted() override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

 private:
  std::optional<base::RunLoop> run_loop_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      observaation_{this};
};

using TestParams = std::tuple<StartMode, RestoreOption>;
class PostLoginDeferredTaskTest
    : public LoginManagerTest,
      public testing::WithParamInterface<TestParams> {
 public:
  PostLoginDeferredTaskTest() { set_should_launch_browser(true); }

  // LoginManagerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    switch (start_mode()) {
      case StartMode::kLogin: {
        LoginManagerTest::SetUpCommandLine(command_line);
        command_line->AppendSwitch(switches::kOobeSkipPostLogin);
        break;
      }
      case StartMode::kRestart: {
        const cryptohome::AccountIdentifier cryptohome_id =
            cryptohome::CreateAccountIdentifierFromAccountId(account_id());

        command_line->AppendSwitchASCII(switches::kLoginUser,
                                        cryptohome_id.account_id());
        command_line->AppendSwitchASCII(
            switches::kLoginProfile,
            UserDataAuthClient::GetStubSanitizedUsername(cryptohome_id));
      }
    }
  }

  StartMode start_mode() const { return std::get<0>(GetParam()); }

  RestoreOption restore_option() const { return std::get<1>(GetParam()); }

  AccountId account_id() const {
    return AccountId::FromUserEmailGaiaId(kTestUser, kTestGaiaId);
  }

  SessionState session_state() const {
    return session_manager::SessionManager::Get()->session_state();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PostLoginDeferredTaskTest,
    testing::Combine(testing::Values(StartMode::kLogin, StartMode::kRestart),
                     testing::Values(RestoreOption::kAlways,
                                     RestoreOption::kAskEveryTime,
                                     RestoreOption::kDoNotRestore)),
    [](const testing::TestParamInfo<TestParams>& params) {
      std::ostringstream oss;
      switch (std::get<0>(params.param)) {
        case StartMode::kLogin:
          oss << "Login";
          break;
        case StartMode::kRestart:
          oss << "Restart";
          break;
      }
      oss << "_";
      switch (std::get<1>(params.param)) {
        case RestoreOption::kAlways:
          oss << "AutoRestore";
          break;
        case RestoreOption::kAskEveryTime:
          oss << "ManualRestore";
          break;
        case RestoreOption::kDoNotRestore:
          oss << "NoRestore";
          break;
      }
      return oss.str();
    });

// Registers user to simulate existing user login.
IN_PROC_BROWSER_TEST_P(PostLoginDeferredTaskTest, PRE_PRE_Basic) {
  if (start_mode() == StartMode::kLogin) {
    EXPECT_EQ(SessionState::OOBE, session_state());
    StartupUtils::MarkOobeCompleted();
  }

  // This needs to happen before UserManager is created.
  RegisterUser(account_id());
}

// PRE test to sets up the previous user session.
IN_PROC_BROWSER_TEST_P(PostLoginDeferredTaskTest, PRE_Basic) {
  if (start_mode() == StartMode::kLogin) {
    EXPECT_EQ(SessionState::LOGIN_PRIMARY, session_state());
    LoginUser(account_id());
  }

  EXPECT_EQ(SessionState::ACTIVE, session_state());

  // Creates a browser window and loads page other than new tab page.
  Profile* user_profile = ProfileManager::GetActiveUserProfile();
  Browser* browser = CreateBrowser(user_profile);
  ASSERT_NE(browser, nullptr);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser, GURL("http://www.google.com")));

  auto* prefs = user_profile->GetPrefs();
  prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                    static_cast<int>(restore_option()));

  DeferredTaskStartWaiter().Wait();
}

// Verifies that post login deferred tasks would run.
IN_PROC_BROWSER_TEST_P(PostLoginDeferredTaskTest, Basic) {
  if (start_mode() == StartMode::kLogin) {
    EXPECT_EQ(SessionState::LOGIN_PRIMARY, session_state());
    LoginUser(account_id());
  }

  EXPECT_EQ(SessionState::ACTIVE, session_state());

  DeferredTaskStartWaiter().Wait();
}

}  // namespace
}  // namespace ash
