// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Chrome OS sign-in, the shill (network manager) user profile must be
// loaded. In the case that a user-specific network is configured to reuse the
// user's login password, the shill user profile should only be loaded after the
// login password has been saved to SessionManager (b/183084821).
// This means that triggering the shill user profile load depends on user
// policy having been processed (as user policy would mandate whether the login
// password should be reused, and thus only after processing user network policy
// does chrome decide if the password should be saved in SessionManager).
//
// This test case verifies that chrome triggers LoadShillProfile for the
// unmanaged user case and the managed user with/without network policy cases.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/functional/bind.h"
#include "base/functional/bind_internal.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_profile_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_settings.pb.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

namespace em = ::enterprise_management;

using ::testing::ElementsAre;

constexpr char kUnmanagedUser[] = "unmanaged@gmail.com";
constexpr char kUnmanagedGaiaID[] = "33333";
constexpr char kSecondaryUnmanagedUser[] = "secondaryunmanaged@gmail.com";
constexpr char kSecondaryUnmanagedGaiaID[] = "44444";
constexpr char kManagedUser[] = "user@example.com";
constexpr char kManagedGaiaID[] = "55555";

// Implements waiting for the LoadShillProfile call to SessionManagerClient and
// counting how many LoadShillProfile calls were performed.
class LoadShillProfileWaiter {
 public:
  explicit LoadShillProfileWaiter(
      FakeSessionManagerClient* fake_session_manager_client)
      : fake_session_manager_client_(fake_session_manager_client) {
    fake_session_manager_client_->set_on_load_shill_profile_callback(
        base::BindRepeating(&LoadShillProfileWaiter::OnLoadShillProfile,
                            base::Unretained(this)));
  }
  ~LoadShillProfileWaiter() {
    fake_session_manager_client_->set_on_load_shill_profile_callback(
        FakeSessionManagerClient::OnLoadShillProfileCallback());
  }

  void WaitForFirstInvocation() { run_loop_.Run(); }

  const std::vector<cryptohome::AccountIdentifier> invocations() const {
    return invocations_;
  }

 private:
  void OnLoadShillProfile(const cryptohome::AccountIdentifier& cryptohome_id) {
    invocations_.push_back(cryptohome_id);
    run_loop_.Quit();
  }

  const raw_ptr<FakeSessionManagerClient> fake_session_manager_client_;
  base::RunLoop run_loop_;
  std::vector<cryptohome::AccountIdentifier> invocations_;
};

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

class ShillProfileLoadingTest : public LoginManagerTest {
 protected:
  ShillProfileLoadingTest() = default;
  ~ShillProfileLoadingTest() override = default;

  // LoginManagerTest:
  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    SessionManagerClient::InitializeFakeInMemory();
  }

  const LoginManagerMixin::TestUserInfo unmanaged_user_{
      AccountId::FromUserEmailGaiaId(kUnmanagedUser, kUnmanagedGaiaID)};
  const LoginManagerMixin::TestUserInfo secondary_unmanaged_user_{
      AccountId::FromUserEmailGaiaId(kSecondaryUnmanagedUser,
                                     kSecondaryUnmanagedGaiaID)};
  const LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId(kManagedUser, kManagedGaiaID)};

  UserPolicyMixin user_policy_mixin_{&mixin_host_, managed_user_.account_id};
  LoginManagerMixin login_manager_{&mixin_host_,
                                   {managed_user_, unmanaged_user_}};
};

// Verifies that the LoadShillProfile method call is invoked on
// SessionManagerClient for unmanaged users (who can't have network policy).
IN_PROC_BROWSER_TEST_F(ShillProfileLoadingTest, UnmanagedUser) {
  LoadShillProfileWaiter waiter(FakeSessionManagerClient::Get());
  LoginUser(unmanaged_user_.account_id);
  waiter.WaitForFirstInvocation();
  EXPECT_THAT(
      waiter.invocations(),
      ElementsAre(EqualsProto(cryptohome::CreateAccountIdentifierFromAccountId(
          unmanaged_user_.account_id))));

  // Adding a secondary user does not re-trigger loading the shill profile.
  UserAddingScreen::Get()->Start();
  AddUser(secondary_unmanaged_user_.account_id);
  EXPECT_THAT(
      waiter.invocations(),
      ElementsAre(EqualsProto(cryptohome::CreateAccountIdentifierFromAccountId(
          unmanaged_user_.account_id))));
}

// Verifies that the LoadShillProfile method call is invoked on
// SessionManagerClient for managed users who don't have a user network policy
// set.
IN_PROC_BROWSER_TEST_F(ShillProfileLoadingTest,
                       ManagedUserWithoutPasswordPlaceholder) {
  user_policy_mixin_.RequestPolicyUpdate();

  LoadShillProfileWaiter waiter(FakeSessionManagerClient::Get());
  LoginUser(managed_user_.account_id);
  waiter.WaitForFirstInvocation();
  EXPECT_THAT(
      waiter.invocations(),
      ElementsAre(EqualsProto(cryptohome::CreateAccountIdentifierFromAccountId(
          managed_user_.account_id))));

  // Also check that the login password has not been saved in session_manager
  // because it was not required by policy.
  EXPECT_EQ(FakeSessionManagerClient::Get()->login_password(), "");
}

// Verifies that the LoadShillProfile method call is invoked on
// SessionManagerClient for managed users who have a user network policy
// set that mandates re-using the login password.
IN_PROC_BROWSER_TEST_F(ShillProfileLoadingTest,
                       ManagedUserWithPasswordPlaceholder) {
  const char kUserONC[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{user-policy-for-wifi}",
          "Name": "DeviceLevelWifi",
          "Type": "WiFi",
          "WiFi": {
            "Security": "WPA-EAP",
            "SSID": "TestTest",
            "EAP": {
              "Outer": "PEAP",
              "Inner": "MSCHAPv2",
              "Identity": "my_identity",
              "Password": "${PASSWORD}"
            }
          }
        }
      ]
    })";

  {
    auto policy_update = user_policy_mixin_.RequestPolicyUpdate();
    policy_update->policy_payload()
        ->mutable_opennetworkconfiguration()
        ->mutable_policy_options()
        ->set_mode(em::PolicyOptions::MANDATORY);
    policy_update->policy_payload()
        ->mutable_opennetworkconfiguration()
        ->set_value(kUserONC);
  }

  LoadShillProfileWaiter waiter(FakeSessionManagerClient::Get());
  LoginUser(managed_user_.account_id);
  waiter.WaitForFirstInvocation();
  EXPECT_THAT(
      waiter.invocations(),
      ElementsAre(EqualsProto(cryptohome::CreateAccountIdentifierFromAccountId(
          managed_user_.account_id))));

  // Also check that the login password has been saved in session_manager
  // because it was configured to be reused for network authentication by
  // policy.
  EXPECT_EQ(FakeSessionManagerClient::Get()->login_password(),
            LoginManagerTest::kPassword);
}

class ShillProfileLoadingGuestLoginTest : public ShillProfileLoadingTest {
 protected:
  ShillProfileLoadingGuestLoginTest() {
    login_manager_.set_session_restore_enabled();
  }

  ~ShillProfileLoadingGuestLoginTest() override = default;

  // ShillProfileLoadingTest:
  void SetUpInProcessBrowserTestFixture() override {
    ShillProfileLoadingTest::SetUpInProcessBrowserTestFixture();
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
  }
};

IN_PROC_BROWSER_TEST_F(ShillProfileLoadingGuestLoginTest, GuestLogin) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  LoadShillProfileWaiter load_shill_profile_waiter(
      FakeSessionManagerClient::Get());

  // The ToS screen is shown to guest users regardless of EULA being accepted
  // previously by the device owner.
  StartupUtils::MarkEulaAccepted();
  ASSERT_TRUE(LoginScreenTestApi::ClickGuestButton());

  // Accept guest ToS consent screen.
  ash::test::WaitForGuestTosScreen();
  ash::test::TapGuestTosAccept();

  restart_job_waiter.Run();

  // Before restarting, chrome is supposed to have triggered loading the shill
  // profile for the guest.
  EXPECT_THAT(
      load_shill_profile_waiter.invocations(),
      ElementsAre(EqualsProto(cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::GuestAccountId()))));
}

}  // namespace ash
