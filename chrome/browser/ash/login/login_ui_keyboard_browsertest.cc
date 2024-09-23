// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/input_method_persistence.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_adding_screen_utils.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/language_preferences/language_preferences.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/experiences/login/login_screen_shown_observer.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace ash {

namespace {

namespace em = ::enterprise_management;

constexpr char kTestUser1[] = "test-user1@gmail.com";
constexpr char kTestUser1NonCanonicalDisplayEmail[] = "test-us.e.r1@gmail.com";
constexpr char kTestUser1GaiaId[] = "1111111111";
constexpr char kTestUser2[] = "test-user2@gmail.com";
constexpr char kTestUser2GaiaId[] = "2222222222";
constexpr char kTestUser3[] = "test-user3@gmail.com";
constexpr char kTestUser3GaiaId[] = "3333333333";

void Append_en_US_InputMethod(std::vector<std::string>* out) {
  out->push_back("xkb:us::eng");
  input_method::InputMethodManager::Get()->GetMigratedInputMethodIDs(out);
}

void Append_en_US_InputMethods(std::vector<std::string>* out) {
  out->push_back("xkb:us::eng");
  out->push_back("xkb:us:intl:eng");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  out->push_back("xkb:us:intl_pc:eng");
#endif
  out->push_back("xkb:us:altgr-intl:eng");
  out->push_back("xkb:us:dvorak:eng");
  out->push_back("xkb:us:dvp:eng");
  out->push_back("xkb:us:colemak:eng");
  out->push_back("xkb:us:workman:eng");
  out->push_back("xkb:us:workman-intl:eng");
  input_method::InputMethodManager::Get()->GetMigratedInputMethodIDs(out);
}

}  // anonymous namespace

class LoginUIKeyboardTest : public LoginManagerTest {
 public:
  LoginUIKeyboardTest() : LoginManagerTest() {
    test_users_.push_back(
        AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId));
    test_users_.push_back(
        AccountId::FromUserEmailGaiaId(kTestUser2, kTestUser2GaiaId));
  }
  ~LoginUIKeyboardTest() override {}

  void SetUpOnMainThread() override {
    user_input_methods.push_back("xkb:fr::fra");
    user_input_methods.push_back("xkb:de::ger");

    input_method::InputMethodManager::Get()->GetMigratedInputMethodIDs(
        &user_input_methods);

    LoginManagerTest::SetUpOnMainThread();
  }

  // Should be called from PRE_ test so that local_state is saved to disk, and
  // reloaded in the main test.
  void InitUserLastInputMethod() {
    input_method::SetUserLastInputMethodPreferenceForTesting(
        test_users_[0], user_input_methods[0]);
    input_method::SetUserLastInputMethodPreferenceForTesting(
        test_users_[1], user_input_methods[1]);
  }

 protected:
  std::vector<std::string> user_input_methods;
  std::vector<AccountId> test_users_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LoginUIUserAddingKeyboardTest : public LoginUIKeyboardTest {
 public:
  LoginUIUserAddingKeyboardTest() {
    test_users_.push_back(
        AccountId::FromUserEmailGaiaId(kTestUser3, kTestUser3GaiaId));
  }

 protected:
  void FocusUserPod(const AccountId& account_id) {
    ASSERT_TRUE(LoginScreenTestApi::FocusUser(account_id));
  }
};

IN_PROC_BROWSER_TEST_F(LoginUIUserAddingKeyboardTest, PRE_CheckPODSwitches) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  RegisterUser(test_users_[2]);
  InitUserLastInputMethod();
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUIUserAddingKeyboardTest, CheckPODSwitches) {
  EXPECT_EQ(lock_screen_utils::GetUserLastInputMethodId(test_users_[2]),
            std::string());
  LoginUser(test_users_[2]);
  const std::string logged_user_input_method =
      lock_screen_utils::GetUserLastInputMethodId(test_users_[2]);
  test::ShowUserAddingScreen();

  std::vector<std::string> expected_input_methods;
  expected_input_methods.push_back(user_input_methods[0]);
  // Append just one.
  Append_en_US_InputMethod(&expected_input_methods);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetEnabledInputMethodIds());

  EXPECT_EQ(user_input_methods[0], input_method::InputMethodManager::Get()
                                       ->GetActiveIMEState()
                                       ->GetCurrentInputMethod()
                                       .id());

  FocusUserPod(test_users_[1]);
  EXPECT_EQ(user_input_methods[1], input_method::InputMethodManager::Get()
                                       ->GetActiveIMEState()
                                       ->GetCurrentInputMethod()
                                       .id());

  FocusUserPod(test_users_[0]);
  EXPECT_EQ(user_input_methods[0], input_method::InputMethodManager::Get()
                                       ->GetActiveIMEState()
                                       ->GetCurrentInputMethod()
                                       .id());

  // Check that logged in user settings did not change.
  EXPECT_EQ(lock_screen_utils::GetUserLastInputMethodId(test_users_[2]),
            logged_user_input_method);
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, PRE_CheckPODScreenDefault) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);

  StartupUtils::MarkOobeCompleted();
}

// Check default IME initialization, when there is no IME configuration in
// local_state.
IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, CheckPODScreenDefault) {
  EXPECT_EQ(2, LoginScreenTestApi::GetUsersCount());
  EXPECT_EQ(test_users_[0], LoginScreenTestApi::GetFocusedUser());

  std::vector<std::string> expected_input_methods;
  Append_en_US_InputMethods(&expected_input_methods);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetEnabledInputMethodIds());
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, PRE_CheckPODScreenWithUsers) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);

  InitUserLastInputMethod();

  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, CheckPODScreenWithUsers) {
  EXPECT_EQ(2, LoginScreenTestApi::GetUsersCount());
  EXPECT_EQ(test_users_[0], LoginScreenTestApi::GetFocusedUser());

  EXPECT_EQ(user_input_methods[0], input_method::InputMethodManager::Get()
                                       ->GetActiveIMEState()
                                       ->GetCurrentInputMethod()
                                       .id());

  std::vector<std::string> expected_input_methods;
  Append_en_US_InputMethods(&expected_input_methods);
  // Enabled IM for the first user (active user POD).
  expected_input_methods.push_back(user_input_methods[0]);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetEnabledInputMethodIds());

  EXPECT_TRUE(LoginScreenTestApi::FocusUser(test_users_[1]));

  EXPECT_EQ(user_input_methods[1], input_method::InputMethodManager::Get()
                                       ->GetActiveIMEState()
                                       ->GetCurrentInputMethod()
                                       .id());

  EXPECT_TRUE(LoginScreenTestApi::FocusUser(test_users_[0]));

  EXPECT_EQ(user_input_methods[0], input_method::InputMethodManager::Get()
                                       ->GetActiveIMEState()
                                       ->GetCurrentInputMethod()
                                       .id());
}

class LoginUIKeyboardTestWithUsersAndOwner : public LoginManagerTest {
 public:
  LoginUIKeyboardTestWithUsersAndOwner() = default;
  ~LoginUIKeyboardTestWithUsersAndOwner() override {}

  void SetUp() override {
    LoginManagerTest::SetUp();

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  void SetUpOnMainThread() override {
    user_input_methods.push_back("xkb:fr::fra");
    user_input_methods.push_back("xkb:de::ger");
    user_input_methods.push_back("xkb:pl::pol");

    input_method::InputMethodManager::Get()->GetMigratedInputMethodIDs(
        &user_input_methods);

    GetFakeUserManager().SetOwnerId(
        AccountId::FromUserEmailGaiaId(kTestUser3, kTestUser3GaiaId));

    LoginManagerTest::SetUpOnMainThread();
  }

  // Should be called from PRE_ test so that local_state is saved to disk, and
  // reloaded in the main test.
  void InitUserLastInputMethod() {
    input_method::SetUserLastInputMethodPreferenceForTesting(
        AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId),
        user_input_methods[0]);
    input_method::SetUserLastInputMethodPreferenceForTesting(
        AccountId::FromUserEmailGaiaId(kTestUser2, kTestUser2GaiaId),
        user_input_methods[1]);
    input_method::SetUserLastInputMethodPreferenceForTesting(
        AccountId::FromUserEmailGaiaId(kTestUser3, kTestUser3GaiaId),
        user_input_methods[2]);

    PrefService* local_state = g_browser_process->local_state();
    local_state->SetString(language_prefs::kPreferredKeyboardLayout,
                           user_input_methods[2]);
  }

  ash::FakeChromeUserManager& GetFakeUserManager() {
    return CHECK_DEREF(static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get()));
  }

  void CheckGaiaKeyboard();

 protected:
  std::vector<std::string> user_input_methods;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

void LoginUIKeyboardTestWithUsersAndOwner::CheckGaiaKeyboard() {
  std::vector<std::string> expected_input_methods;
  // kPreferredKeyboardLayout is now set to last focused POD.
  expected_input_methods.push_back(user_input_methods[0]);
  // Owner input method.
  expected_input_methods.push_back(user_input_methods[2]);
  // Locale default input methods (the first one also is hardware IM).
  Append_en_US_InputMethods(&expected_input_methods);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetEnabledInputMethodIds());
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTestWithUsersAndOwner,
                       PRE_CheckPODScreenKeyboard) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId));
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser2, kTestUser2GaiaId));
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser3, kTestUser3GaiaId));

  InitUserLastInputMethod();

  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTestWithUsersAndOwner,
                       CheckPODScreenKeyboard) {
  EXPECT_EQ(3, LoginScreenTestApi::GetUsersCount());

  std::vector<std::string> expected_input_methods;
  // Owner input method.
  expected_input_methods.push_back(user_input_methods[2]);
  // Locale default input methods (the first one also is hardware IM).
  Append_en_US_InputMethods(&expected_input_methods);
  // Enabled IM for the first user (active user POD).
  expected_input_methods.push_back(user_input_methods[0]);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetEnabledInputMethodIds());

  // Switch to Gaia.
  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  CheckGaiaKeyboard();

  // Switch back.
  test::ExecuteOobeJS("$('user-creation').cancel()");
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetEnabledInputMethodIds());
}

class LoginUIKeyboardPolicy : public LoginManagerTest {
 protected:
  policy::DevicePolicyBuilder* device_policy() {
    return policy_helper_.device_policy();
  }

  void SetAllowedInputMethod(const std::string& method) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_login_screen_input_methods()->add_login_screen_input_methods(
        method);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {kDeviceLoginScreenInputMethods});
  }
  LoginManagerMixin login_manager_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardPolicy, RestrictInputMethods) {
  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();
  ASSERT_TRUE(imm);

  // Check that input methods are default when policy is not set.
  ASSERT_EQ(imm->GetActiveIMEState()->GetAllowedInputMethodIds().size(), 0U);
  std::vector<std::string> expected_input_methods;
  Append_en_US_InputMethods(&expected_input_methods);
  EXPECT_EQ(input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetEnabledInputMethodIds(),
            expected_input_methods);

  std::vector<std::string> allowed_input_method{"xkb:de::ger"};
  SetAllowedInputMethod(allowed_input_method.front());
  ASSERT_EQ(imm->GetActiveIMEState()->GetAllowedInputMethodIds().size(), 1U);
  ASSERT_EQ(imm->GetActiveIMEState()->GetNumEnabledInputMethods(), 1U);

  input_method::InputMethodManager::Get()->GetMigratedInputMethodIDs(
      &allowed_input_method);
  ASSERT_EQ(imm->GetActiveIMEState()->GetCurrentInputMethod().id(),
            allowed_input_method.front());

  // The policy method stored to language_prefs::kPreferredKeyboardLayout. So
  // it will be there after the policy is gone.
  expected_input_methods.insert(
      expected_input_methods.begin(),
      imm->GetActiveIMEState()->GetEnabledInputMethodIds()[0]);

  // Remove the policy again
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_login_screen_input_methods()
      ->clear_login_screen_input_methods();
  policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
      {kDeviceLoginScreenInputMethods});

  ASSERT_EQ(imm->GetActiveIMEState()->GetAllowedInputMethodIds().size(), 0U);
  ASSERT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetEnabledInputMethodIds());
}

class LoginUIDevicePolicyUserAdding : public LoginUIKeyboardPolicy {
 public:
  LoginUIDevicePolicyUserAdding() {
    // Need at least two to run user adding screen.
    login_manager_.AppendRegularUsers(2);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LoginUIDevicePolicyUserAdding, PolicyNotHonored) {
  const AccountId primary_account_id = login_manager_.users()[0].account_id;
  LoginUser(primary_account_id);

  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();
  auto user_ime_state = input_manager->GetActiveIMEState();

  std::vector<std::string> allowed_input_method{"xkb:de::ger"};
  SetAllowedInputMethod(allowed_input_method.front());
  input_method::InputMethodManager::Get()->GetMigratedInputMethodIDs(
      &allowed_input_method);

  test::ShowUserAddingScreen();

  auto user_adding_ime_state = input_manager->GetActiveIMEState();
  EXPECT_NE(user_ime_state, user_adding_ime_state);

  std::vector<std::string> default_input_methods;
  Append_en_US_InputMethods(&default_input_methods);
  // Input methods should be default because the other user (which is focused)
  // does not have saved last input method.
  EXPECT_EQ(user_adding_ime_state->GetEnabledInputMethodIds(),
            default_input_methods);

  EXPECT_EQ(user_adding_ime_state->GetAllowedInputMethodIds().size(), 0u);
  EXPECT_FALSE(base::Contains(user_adding_ime_state->GetEnabledInputMethodIds(),
                              allowed_input_method.front()));
}

class FirstLoginKeyboardTest : public LoginManagerTest {
 public:
  FirstLoginKeyboardTest() = default;
  ~FirstLoginKeyboardTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  }

 protected:
  AccountId test_user_{
      AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId)};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

// Tests that user input method correctly propagated after session start or
// session unlock.
IN_PROC_BROWSER_TEST_F(FirstLoginKeyboardTest,
                       UsersLastInputMethodPersistsOnLoginOrUnlock) {
  EXPECT_TRUE(lock_screen_utils::GetUserLastInputMethodId(test_user_).empty());

  // Non canonical display email (typed) should not affect input method storage.
  LoginDisplayHost::default_host()->SetDisplayEmail(
      kTestUser1NonCanonicalDisplayEmail);
  LoginUser(test_user_);

  // Last input method should be stored.
  EXPECT_FALSE(lock_screen_utils::GetUserLastInputMethodId(test_user_).empty());

  ScreenLockerTester locker_tester;
  locker_tester.Lock();

  // Clear user input method.
  input_method::SetUserLastInputMethodPreferenceForTesting(test_user_,
                                                           std::string());
  EXPECT_TRUE(lock_screen_utils::GetUserLastInputMethodId(test_user_).empty());

  locker_tester.UnlockWithPassword(test_user_, "password");
  locker_tester.WaitForUnlock();

  // Last input method should be stored.
  EXPECT_FALSE(lock_screen_utils::GetUserLastInputMethodId(test_user_).empty());
}

class EphemeralUserKeyboardTest : public LoginManagerTest {
 protected:
  // LoginManagerTest:
  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<ScopedDevicePolicyUpdate> update =
        device_state_.RequestDevicePolicyUpdate();
    update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(true);
    update.reset();
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  LoginManagerMixin login_manager_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Check that ephemeral users have last input method set.
IN_PROC_BROWSER_TEST_F(EphemeralUserKeyboardTest, PersistToProfile) {
  login_manager_.SkipPostLoginScreens();
  login_manager_.LoginAsNewRegularUser();
  login_manager_.WaitForActiveSession();

  const AccountId& account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // Should be empty because known_user does not persist data for ephemeral
  // users.
  EXPECT_FALSE(known_user.GetUserLastInputMethodId(account_id));

  std::vector<std::string> expected_input_method;
  Append_en_US_InputMethod(&expected_input_method);
  EXPECT_EQ(lock_screen_utils::GetUserLastInputMethodId(account_id),
            expected_input_method[0]);
}

}  // namespace ash
