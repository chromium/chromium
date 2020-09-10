// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/login_screen_shown_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kLoginFormatJS[] = R"({
    var pod = $('pod-row').getPodWithUsername_('%s');
    $('pod-row').focusPod(pod, true);
    pod.passwordElement.value = '%s';
    pod.passwordElement.dispatchEvent(
        new Event('input', { bubbles: true, cancelable: true }));
    pod.submitButton.click();
  })";

}  // namespace

class UserAddingScreenTest : public LoginManagerTest,
                             public UserAddingScreen::Observer {
 public:
  UserAddingScreenTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(3);
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    UserAddingScreen::Get()->AddObserver(this);
  }

  void WaitUntilUserAddingFinishedOrCancelled() {
    if (finished_)
      return;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void OnUserAddingFinished() override {
    ++user_adding_finished_;
    finished_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void OnUserAddingStarted() override {
    ++user_adding_started_;
    finished_ = false;
  }

  void SetUserCanLock(user_manager::User* user, bool can_lock) {
    user->set_can_lock(can_lock);
  }

  void UILoginUser(const AccountId& account_id, const std::string& password) {
    // Wait for pods to load.
    test::OobeJS().CreateWaiter("$(\'pod-row\').pods.length > 0")->Wait();

    SetExpectedCredentials(CreateUserContext(account_id, password));
    SessionStateWaiter waiter;

    const std::string login_js = base::StringPrintf(
        kLoginFormatJS, account_id.Serialize().c_str(), password.c_str());
    test::ExecuteOobeJS(login_js);

    waiter.Wait();
  }

  int user_adding_started() { return user_adding_started_; }

  int user_adding_finished() { return user_adding_finished_; }

  std::vector<AccountId> users_in_session_order_;
  LoginManagerMixin login_mixin_{&mixin_host_};

 private:
  int user_adding_started_ = 0;
  int user_adding_finished_ = 0;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool finished_ = false;  // True if OnUserAddingFinished() has been called
                           // before WaitUntilUserAddingFinishedOrCancelled().

  DISALLOW_COPY_AND_ASSIGN(UserAddingScreenTest);
};

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, CancelAdding) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(users.size(), user_manager::UserManager::Get()->GetUsers().size());
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  base::HistogramTester histogram_tester;
  UserAddingScreen::Get()->Start();
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();

  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);

  EXPECT_TRUE(ash::LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(ash::LoginScreenTestApi::ClickCancelButton());
  WaitUntilUserAddingFinishedOrCancelled();

  histogram_tester.ExpectTotalCount("ChromeOS.UserAddingScreen.LoadTime", 1);

  EXPECT_EQ(user_adding_finished(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
            users[0].account_id);
}

class UserAddingScreenViewBasedTest : public UserAddingScreenTest,
                                      public LoginScreenShownObserver {
 public:
  UserAddingScreenViewBasedTest() : UserAddingScreenTest() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kViewBasedMultiprofileLogin}, {});
  }

  // LoginScreenShownObserver:
  void OnLoginScreenShown() override {
    LoginScreenClient::Get()->RemoveLoginScreenShownObserver(this);
    login_screen_shown_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitUntilLoginScreenShown() {
    if (login_screen_shown_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  bool login_screen_shown_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAddingScreenViewBasedTest, CancelAdding) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(users.size(), user_manager::UserManager::Get()->GetUsers().size());
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  base::HistogramTester histogram_tester;
  UserAddingScreen::Get()->Start();
  LoginScreenClient::Get()->AddLoginScreenShownObserver(this);
  WaitUntilLoginScreenShown();

  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);

  EXPECT_TRUE(ash::LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(ash::LoginScreenTestApi::ClickCancelButton());
  WaitUntilUserAddingFinishedOrCancelled();

  histogram_tester.ExpectTotalCount(
      "ChromeOS.UserAddingScreen.LoadTimeViewsBased", 1);

  EXPECT_EQ(user_adding_finished(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
            users[0].account_id);
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, UILogin) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  base::HistogramTester histogram_tester;
  UserAddingScreen::Get()->Start();
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();
  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(ash::LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  UILoginUser(users.back().account_id, kPassword);

  EXPECT_EQ(user_adding_finished(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
  ASSERT_EQ(user_manager->GetLoggedInUsers().size(), 2u);

  histogram_tester.ExpectTotalCount("ChromeOS.UserAddingScreen.LoadTime", 1);

  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenViewBasedTest, UILogin) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  base::HistogramTester histogram_tester;
  UserAddingScreen::Get()->Start();
  LoginScreenClient::Get()->AddLoginScreenShownObserver(this);
  WaitUntilLoginScreenShown();

  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(ash::LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  SetExpectedCredentials(CreateUserContext(users.back().account_id, kPassword));
  ash::LoginScreenTestApi::SubmitPassword(users.back().account_id, kPassword,
                                          true /* check_if_submittable */);

  WaitUntilUserAddingFinishedOrCancelled();
  EXPECT_EQ(user_adding_finished(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
  ASSERT_EQ(user_manager->GetLoggedInUsers().size(), 2u);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.UserAddingScreen.LoadTimeViewsBased", 1);

  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, AddingSeveralUsers) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  users_in_session_order_.push_back(users[0].account_id);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const int n = users.size();
  for (int i = 1; i < n; ++i) {
    UserAddingScreen::Get()->Start();
    EXPECT_EQ(user_adding_started(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::LOGIN_SECONDARY);

    AddUser(users[i].account_id);
    users_in_session_order_.push_back(users[i].account_id);
    WaitUntilUserAddingFinishedOrCancelled();

    EXPECT_EQ(user_adding_finished(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
    EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
    ASSERT_EQ(user_manager->GetLoggedInUsers().size(),
              static_cast<size_t>(i + 1));
  }

  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  // Now check how unlock policy works for these users.
  PrefService* prefs1 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[0])
          ->GetPrefs();
  PrefService* prefs2 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[1])
          ->GetPrefs();
  PrefService* prefs3 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[2])
          ->GetPrefs();
  ASSERT_TRUE(prefs1 != nullptr);
  ASSERT_TRUE(prefs2 != nullptr);
  ASSERT_TRUE(prefs3 != nullptr);
  prefs1->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);
  prefs2->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);
  prefs3->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);

  // One of the users has the primary-only policy.
  // List of unlock users doesn't depend on kEnableLockScreen preference.
  prefs1->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);
  prefs1->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorPrimaryOnly);
  prefs2->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorUnrestricted);
  prefs3->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorUnrestricted);
  user_manager::UserList unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 1u);
  EXPECT_EQ(users[0].account_id, unlock_users[0]->GetAccountId());

  prefs1->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 1u);
  EXPECT_EQ(users[0].account_id, unlock_users[0]->GetAccountId());

  // If all users have unrestricted policy then anyone can perform unlock.
  prefs1->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorUnrestricted);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 3u);
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());

  // This preference doesn't affect list of unlock users.
  prefs2->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 3u);
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());

  // Now one of the users is unable to unlock.
  SetUserCanLock(user_manager->GetLoggedInUsers()[2], false);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 2u);
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());
  SetUserCanLock(user_manager->GetLoggedInUsers()[2], true);

  // Now one of the users has not-allowed policy.
  // In this scenario this user is not allowed in multi-profile session but
  // if that user happened to still be part of multi-profile session it should
  // not be listed on screen lock.
  prefs3->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorNotAllowed);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 2u);
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenViewBasedTest, AddingSeveralUsers) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  users_in_session_order_.push_back(users[0].account_id);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const int n = users.size();
  for (int i = 1; i < n; ++i) {
    UserAddingScreen::Get()->Start();
    LoginScreenClient::Get()->AddLoginScreenShownObserver(this);
    WaitUntilLoginScreenShown();

    EXPECT_EQ(user_adding_started(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::LOGIN_SECONDARY);

    AddUser(users[i].account_id);
    users_in_session_order_.push_back(users[i].account_id);
    WaitUntilUserAddingFinishedOrCancelled();

    EXPECT_EQ(user_adding_finished(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
    EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
    ASSERT_EQ(user_manager->GetLoggedInUsers().size(),
              static_cast<size_t>(i + 1));
  }

  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  // Now check how unlock policy works for these users.
  PrefService* prefs1 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[0])
          ->GetPrefs();
  PrefService* prefs2 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[1])
          ->GetPrefs();
  PrefService* prefs3 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[2])
          ->GetPrefs();
  ASSERT_TRUE(prefs1 != nullptr);
  ASSERT_TRUE(prefs2 != nullptr);
  ASSERT_TRUE(prefs3 != nullptr);
  prefs1->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);
  prefs2->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);
  prefs3->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);

  // One of the users has the primary-only policy.
  // List of unlock users doesn't depend on kEnableLockScreen preference.
  prefs1->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);
  prefs1->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorPrimaryOnly);
  prefs2->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorUnrestricted);
  prefs3->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorUnrestricted);
  user_manager::UserList unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 1u);
  EXPECT_EQ(users[0].account_id, unlock_users[0]->GetAccountId());

  prefs1->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 1u);
  EXPECT_EQ(users[0].account_id, unlock_users[0]->GetAccountId());

  // If all users have unrestricted policy then anyone can perform unlock.
  prefs1->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorUnrestricted);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 3u);
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());

  // This preference doesn't affect list of unlock users.
  prefs2->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 3u);
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());

  // Now one of the users is unable to unlock.
  SetUserCanLock(user_manager->GetLoggedInUsers()[2], false);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 2u);
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());
  SetUserCanLock(user_manager->GetLoggedInUsers()[2], true);

  // Now one of the users has not-allowed policy.
  // In this scenario this user is not allowed in multi-profile session but
  // if that user happened to still be part of multi-profile session it should
  // not be listed on screen lock.
  prefs3->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorNotAllowed);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 2u);
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, ScreenVisibilityAfterLock) {
  const auto& users = login_mixin_.users();
  LoginUser(users[0].account_id);

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    ScreenLocker::Show();
    observer.Wait();
  }

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    ScreenLocker::Hide();
    observer.Wait();
  }

  UserAddingScreen::Get()->Start();
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();
  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);

  EXPECT_TRUE(ash::LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(ash::LoginScreenTestApi::ClickCancelButton());

  WaitUntilUserAddingFinishedOrCancelled();
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenViewBasedTest,
                       ScreenVisibilityAfterLock) {
  const auto& users = login_mixin_.users();
  LoginUser(users[0].account_id);

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    ScreenLocker::Show();
    observer.Wait();
  }

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    ScreenLocker::Hide();
    observer.Wait();
  }

  UserAddingScreen::Get()->Start();
  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);

  EXPECT_TRUE(ash::LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(ash::LoginScreenTestApi::ClickCancelButton());

  WaitUntilUserAddingFinishedOrCancelled();
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenViewBasedTest, InfoBubbleVisible) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(users.size(), user_manager::UserManager::Get()->GetUsers().size());
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  EXPECT_FALSE(ash::LoginScreenTestApi::IsUserAddingScreenBubbleShown());

  LoginUser(users[0].account_id);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Check if the user adding screen bubble is still shown after adding other
  // users, as a re-layout is executed each time.
  const int n = users.size();
  for (int i = 1; i < n; ++i) {
    UserAddingScreen::Get()->Start();
    EXPECT_EQ(user_adding_started(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::LOGIN_SECONDARY);

    EXPECT_TRUE(ash::LoginScreenTestApi::IsUserAddingScreenBubbleShown());

    AddUser(users[i].account_id);
    users_in_session_order_.push_back(users[i].account_id);
    WaitUntilUserAddingFinishedOrCancelled();

    EXPECT_EQ(user_adding_finished(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
    EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
    ASSERT_EQ(user_manager->GetLoggedInUsers().size(),
              static_cast<size_t>(i + 1));
  }

  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

}  // namespace chromeos
