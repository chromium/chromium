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
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"
#include "chromeos/ash/components/login/auth/authenticator_builder.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/account_id_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kMisconfiguredUserPref[] = "incomplete_login_user_account";
constexpr char kMisconfiguredUserPrefV2[] = "incomplete_login_user_account_v2";
constexpr char kNewUserGaiaId[] = "1234";
constexpr char kNewUserPassword[] = "password";
constexpr char kNewUserEmail[] = "new-user@gmail.com";

// TODO(b/315827147): After Local Passwords are launched it would be
// possible to rewrite this test in a way without callback injection,
// as getting right timing could be performed via Screens interaction.
class FakeAuthSessionAuthenticator : public AuthSessionAuthenticator {
 public:
  FakeAuthSessionAuthenticator(
      AuthStatusConsumer* consumer,
      std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
      base::RepeatingCallback<void(const AccountId&)> user_recorder,
      PrefService* local_state,
      bool new_user_can_become_owner,
      base::OnceClosure on_record_auth_factor_added)
      : AuthSessionAuthenticator(consumer,
                                 std::move(safe_mode_delegate),
                                 user_recorder,
                                 new_user_can_become_owner,
                                 local_state) {
    on_record_auth_factor_added_ = std::move(on_record_auth_factor_added);
  }

  FakeAuthSessionAuthenticator(const FakeAuthSessionAuthenticator&) = delete;
  FakeAuthSessionAuthenticator& operator=(const FakeAuthSessionAuthenticator&) =
      delete;

 protected:
  ~FakeAuthSessionAuthenticator() override = default;

 private:
  // AuthSessionAuthenticator
  void RecordFirstAuthFactorAdded(std::unique_ptr<UserContext> context,
                                  AuthOperationCallback callback) override {
    if (on_record_auth_factor_added_) {
      std::move(on_record_auth_factor_added_).Run();
    }
  }

  base::OnceClosure on_record_auth_factor_added_;
};

}  // namespace

class MisconfiguredOwnerUserTest : public LoginManagerTest {
 public:
  explicit MisconfiguredOwnerUserTest(bool new_user_can_become_owner = true)
      : new_user_can_become_owner_(new_user_can_become_owner) {}

  void SetUpOnMainThread() override {
    add_auth_factor_waiter_ = std::make_unique<base::test::TestFuture<void>>();
    auto test_api =
        auth::AuthFactorConfig::TestApi(auth::GetAuthFactorConfigForTesting(
            quick_unlock::QuickUnlockFactory::GetDelegate(),
            g_browser_process->local_state()));
    test_api.SetAddKnowledgeFactorCallback(
        add_auth_factor_waiter_->GetCallback());
    test_api.SetSkipUserIntegrityNotification(true);

    LoginManagerTest::SetUpOnMainThread();
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

  AccountId test_account_id_;
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_,
                                   {},
                                   nullptr,
                                   &cryptohome_mixin_};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};
  std::unique_ptr<base::test::TestFuture<void>> add_auth_factor_waiter_;
  bool new_user_can_become_owner_;
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
 public:
  MisconfiguredUserTest() : MisconfiguredOwnerUserTest(false) {
    login_manager_.AppendRegularUsers(1);
    test_account_id_ = login_manager_.users().front().account_id;
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kDeviceOwner, base::Value(test_account_id_.GetUserEmail()));
  }

 protected:
  void InitiateUserCreation(const std::string& email,
                            const std::string& password) {
    EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
    MisconfiguredOwnerUserTest::InitiateUserCreation(email, password);
  }

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(
    MisconfiguredUserTest,
    PRE_MisconfiguredUserSuccessfullyRemovedFromUserManager) {
  user_manager::UserList users = user_manager::UserManager::Get()->GetUsers();

  auto result =
      std::find_if(begin(users), end(users), [this](user_manager::User* user) {
        return user->GetAccountId().GetUserEmail() ==
               test_account_id_.GetUserEmail();
      });

  EXPECT_NE(result, end(users));

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(kMisconfiguredUserPref,
                         test_account_id_.GetUserEmail());
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       MisconfiguredUserSuccessfullyRemovedFromUserManager) {
  user_manager::UserList users = user_manager::UserManager::Get()->GetUsers();

  auto result =
      std::find_if(begin(users), end(users), [this](user_manager::User* user) {
        return user->GetAccountId().GetUserEmail() ==
               test_account_id_.GetUserEmail();
      });

  EXPECT_EQ(result, end(users));
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
