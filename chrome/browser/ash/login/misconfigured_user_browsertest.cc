// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/auth/chrome_safe_mode_delegate.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"
#include "chromeos/ash/components/login/auth/authenticator_builder.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kMisconfiguredUserPref[] = "incomplete_login_user_account";
constexpr char kNewUserGaiaId[] = "1234";
constexpr char kNewUserPassword[] = "password";
constexpr char kNewUserEmail[] = "new-user@gmail.com";

class FakeAuthSessionAuthenticator : public AuthSessionAuthenticator {
 public:
  FakeAuthSessionAuthenticator(
      AuthStatusConsumer* consumer,
      std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
      base::RepeatingCallback<void(const AccountId&)> user_recorder,
      PrefService* local_state,
      base::OnceClosure on_record_auth_factor_added)
      : AuthSessionAuthenticator(consumer,
                                 std::move(safe_mode_delegate),
                                 user_recorder,
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

class FakeAuthenticatorBuilder : public AuthenticatorBuilder {
 public:
  explicit FakeAuthenticatorBuilder(
      base::OnceClosure on_record_auth_factor_added)
      : on_record_auth_factor_added_(std::move(on_record_auth_factor_added)) {}

  FakeAuthenticatorBuilder(const FakeAuthenticatorBuilder&) = delete;
  FakeAuthenticatorBuilder& operator=(const FakeAuthenticatorBuilder&) = delete;

  ~FakeAuthenticatorBuilder() override = default;

  // Makes UserSessionManager initialize a fake AuthSessionAuthenticator.
  // Also passes a callback to the fake that's called when attempting to record
  // the addition of an auth factor.
  scoped_refptr<Authenticator> Create(AuthStatusConsumer* consumer) override {
    return new FakeAuthSessionAuthenticator(
        consumer, std::make_unique<ChromeSafeModeDelegate>(),
        /*user_recorder=*/base::DoNothing(), g_browser_process->local_state(),
        std::move(on_record_auth_factor_added_));
  }

 private:
  base::OnceClosure on_record_auth_factor_added_;
};

}  // namespace

class MisconfiguredOwnerUserTest : public LoginManagerTest {
 public:
  void SetUpOnMainThread() override {
    add_auth_factor_waiter_ = std::make_unique<base::test::TestFuture<void>>();
    user_session_manager_test_api_ =
        std::make_unique<test::UserSessionManagerTestApi>(
            UserSessionManager::GetInstance());
    user_session_manager_test_api_->InjectAuthenticatorBuilder(
        std::make_unique<FakeAuthenticatorBuilder>(
            add_auth_factor_waiter_->GetCallback()));
    LoginManagerTest::SetUpOnMainThread();
  }

 protected:
  void InitiateUserCreation(const std::string& email,
                            const std::string& password) {
    OobeScreenWaiter(UserCreationView::kScreenId).Wait();

    fake_gaia_mixin_.fake_gaia()->MapEmailToGaiaId(email, kNewUserGaiaId);

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
  std::unique_ptr<test::UserSessionManagerTestApi>
      user_session_manager_test_api_;
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
 public:
  MisconfiguredUserTest() {
    login_manager_.AppendRegularUsers(1);
    test_account_id_ = login_manager_.users().front().account_id;
  }

 protected:
  void InitiateUserCreation(const std::string& email,
                            const std::string& password) {
    EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
    MisconfiguredOwnerUserTest::InitiateUserCreation(email, password);
  }
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
  std::string incomplete_user = local_state->GetString(kMisconfiguredUserPref);
  EXPECT_EQ(incomplete_user, kNewUserEmail);
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       PRE_MisconfiguredUserCryptohomeSuccessfullyRemoved) {
  InitiateUserCreation(kNewUserEmail, kNewUserPassword);
  ASSERT_TRUE(add_auth_factor_waiter_->Wait());
}

IN_PROC_BROWSER_TEST_F(MisconfiguredUserTest,
                       MisconfiguredUserCryptohomeSuccessfullyRemoved) {
  base::RunLoop().RunUntilIdle();
  ::user_data_auth::RemoveRequest last_remove_request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<FakeUserDataAuthClient::Operation::kRemove>();
  EXPECT_EQ(last_remove_request.identifier().account_id(), kNewUserEmail);
}

}  // namespace ash
