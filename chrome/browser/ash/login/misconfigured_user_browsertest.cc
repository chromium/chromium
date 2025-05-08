// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/auth/chrome_safe_mode_delegate.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"
#include "chromeos/ash/components/login/auth/authenticator_builder.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/account_id_util.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace ash {

namespace {

constexpr char kMisconfiguredUserPref[] = "incomplete_login_user_account";
constexpr char kMisconfiguredUserPrefV2[] = "incomplete_login_user_account_v2";

constexpr char kNewUserEmail[] = "new-user@gmail.com";
constexpr GaiaId::Literal kNewUserGaiaId("1234");
constexpr char kNewUserPassword[] = "password";

}  // namespace

class MisconfiguredOwnerUserTest : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // Forward account verification to the embedded server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpOnMainThread() override {
    add_auth_factor_waiter_ = std::make_unique<base::test::TestFuture<void>>();
    auto test_api =
        auth::AuthFactorConfig::TestApi(auth::GetAuthFactorConfigForTesting(
            quick_unlock::QuickUnlockFactory::GetDelegate(),
            g_browser_process->local_state()));
    test_api.SetAddKnowledgeFactorCallback(
        add_auth_factor_waiter_->GetCallback());
    test_api.SetSkipUserIntegrityNotification(true);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  void InitiateUserCreation(const std::string& email,
                            const std::string& password) {
    OobeScreenWaiter(UserCreationView::kScreenId).Wait();

    fake_gaia_mixin_.fake_gaia()->MapEmailToGaiaId(email, kNewUserGaiaId);

    auto* context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    context->skip_post_login_screens_for_tests = true;

    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(email, password,
                                  FakeGaiaMixin::kEmptyUserServices);
  }

  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_,
                                   {},
                                   nullptr,
                                   &cryptohome_mixin_};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};
  std::unique_ptr<base::test::TestFuture<void>> add_auth_factor_waiter_;
};

IN_PROC_BROWSER_TEST_F(MisconfiguredOwnerUserTest,
                       PRE_MisconfiguredOwnerUserIsPowerwashed) {
  InitiateUserCreation(kNewUserEmail, kNewUserPassword);
  ASSERT_TRUE(add_auth_factor_waiter_->Wait());
}

IN_PROC_BROWSER_TEST_F(MisconfiguredOwnerUserTest,
                       MisconfiguredOwnerUserIsPowerwashed) {
  int powerwash_call_count =
      FakeSessionManagerClient::Get()->start_device_wipe_call_count();
  EXPECT_EQ(powerwash_call_count, 1);
}

class MisconfiguredUserTest : public MisconfiguredOwnerUserTest {
 protected:
  void InitiateUserCreation(const std::string& email,
                            const std::string& password) {
    EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
    MisconfiguredOwnerUserTest::InitiateUserCreation(email, password);
  }

  void RegisterOwner() {
    // Make sure no users are logged in yet.
    EXPECT_TRUE(user_manager::UserManager::Get()->GetPersistedUsers().empty());

    // The first log-in user will become the owner.
    login_manager_.LoginAsNewRegularUser();

    // Fake as if auth is completed.
    user_manager::UserDirectoryIntegrityManager(
        g_browser_process->local_state())
        .ClearPrefs();
  }
};

IN_PROC_BROWSER_TEST_F(
    MisconfiguredUserTest,
    PRE_PRE_MisconfiguredUserSuccessfullyRemovedFromUserManager) {
  RegisterOwner();

  // Add another user, which will be marked as misconfigured in the next case.
  user_manager::TestHelper::RegisterPersistedUser(
      *g_browser_process->local_state(),
      AccountId::FromUserEmailGaiaId(kNewUserEmail, kNewUserGaiaId));
}

IN_PROC_BROWSER_TEST_F(
    MisconfiguredUserTest,
    PRE_MisconfiguredUserSuccessfullyRemovedFromUserManager) {
  user_manager::UserList users =
      user_manager::UserManager::Get()->GetPersistedUsers();

  ASSERT_TRUE(user_manager::UserManager::Get()->FindUser(
      AccountId::FromUserEmailGaiaId(kNewUserEmail, kNewUserGaiaId)));

  // Mark the new user, which is not an owner, as misconfigured.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(kMisconfiguredUserPref, kNewUserEmail);
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       MisconfiguredUserSuccessfullyRemovedFromUserManager) {
  user_manager::UserList users =
      user_manager::UserManager::Get()->GetPersistedUsers();

  EXPECT_FALSE(user_manager::UserManager::Get()->FindUser(
      AccountId::FromUserEmailGaiaId(kNewUserEmail, kNewUserGaiaId)));
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       PRE_MisconfiguredUserSuccessfullyRecorded) {
  RegisterOwner();
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       MisconfiguredUserSuccessfullyRecorded) {
  InitiateUserCreation(kNewUserEmail, kNewUserPassword);
  ASSERT_TRUE(add_auth_factor_waiter_->Wait());
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state->HasPrefPath(kMisconfiguredUserPrefV2));
  const base::Value::Dict& pref_dict =
      local_state->GetDict(kMisconfiguredUserPrefV2);
  std::optional<AccountId> account_id = user_manager::LoadAccountId(pref_dict);
  ASSERT_TRUE(account_id);
  EXPECT_EQ(account_id->GetUserEmail(), kNewUserEmail);
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       PRE_PRE_MisconfiguredUserSuccessfullyRecordedLegacy) {
  RegisterOwner();
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       PRE_MisconfiguredUserSuccessfullyRecordedLegacy) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(kMisconfiguredUserPref, kNewUserEmail);
  local_state->CommitPendingWrite();
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       MisconfiguredUserSuccessfullyRecordedLegacy) {
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(FakeUserDataAuthClient::Get()
                  ->WasCalled<FakeUserDataAuthClient::Operation::kRemove>());
  ::user_data_auth::RemoveRequest last_remove_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<FakeUserDataAuthClient::Operation::kRemove>();
  EXPECT_EQ(last_remove_request.identifier().account_id(), kNewUserEmail);
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       PRE_PRE_MisconfiguredUserCryptohomeSuccessfullyRemoved) {
  RegisterOwner();
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       PRE_MisconfiguredUserCryptohomeSuccessfullyRemoved) {
  InitiateUserCreation(kNewUserEmail, kNewUserPassword);
  ASSERT_TRUE(add_auth_factor_waiter_->Wait());
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       MisconfiguredUserCryptohomeSuccessfullyRemoved) {
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(FakeUserDataAuthClient::Get()
                  ->WasCalled<FakeUserDataAuthClient::Operation::kRemove>());
  ::user_data_auth::RemoveRequest last_remove_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<FakeUserDataAuthClient::Operation::kRemove>();
  EXPECT_EQ(last_remove_request.identifier().account_id(), kNewUserEmail);
}

}  // namespace ash
