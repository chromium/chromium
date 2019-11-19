// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace chromeos {

class UserAddingScreenTest : public LoginManagerTest,
                             public UserAddingScreen::Observer {
 public:
  UserAddingScreenTest()
      : LoginManagerTest(false, true /* should_initialize_webui */) {
    struct {
      const char* email;
      const char* gaia_id;
    } const kTestUsers[] = {{"test-user1@gmail.com", "1111111111"},
                            {"test-user2@gmail.com", "2222222222"},
                            {"test-user3@gmail.com", "3333333333"}};
    for (size_t i = 0; i < base::size(kTestUsers); ++i) {
      test_users_.emplace_back(AccountId::FromUserEmailGaiaId(
          kTestUsers[i].email, kTestUsers[i].gaia_id));
    }
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

  void CheckScreenIsVisible() {
    auto* login_view = LoginDisplayHost::default_host()->GetWebUILoginView();
    views::View* web_view = login_view->children().front();
    for (views::View* current_view = web_view; current_view;
         current_view = current_view->parent()) {
      EXPECT_TRUE(current_view->GetVisible());
      if (current_view->layer())
        EXPECT_EQ(current_view->layer()->GetCombinedOpacity(), 1.f);
    }
    for (aura::Window* window = web_view->GetWidget()->GetNativeWindow();
         window; window = window->parent()) {
      EXPECT_TRUE(window->IsVisible());
      EXPECT_EQ(window->layer()->GetCombinedOpacity(), 1.f);
    }
  }

  int user_adding_started() { return user_adding_started_; }

  int user_adding_finished() { return user_adding_finished_; }

  std::vector<AccountId> test_users_;

 private:
  int user_adding_started_ = 0;
  int user_adding_finished_ = 0;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool finished_ = false;  // True if OnUserAddingFinished() has been called
                           // before WaitUntilUserAddingFinishedOrCancelled().

  DISALLOW_COPY_AND_ASSIGN(UserAddingScreenTest);
};

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, PRE_CancelAdding) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  RegisterUser(test_users_[2]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, CancelAdding) {
  EXPECT_EQ(3u, user_manager::UserManager::Get()->GetUsers().size());
  EXPECT_EQ(0u, user_manager::UserManager::Get()->GetLoggedInUsers().size());
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());

  LoginUser(test_users_[0]);
  EXPECT_EQ(1u, user_manager::UserManager::Get()->GetLoggedInUsers().size());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());

  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, user_adding_started());
  EXPECT_EQ(session_manager::SessionState::LOGIN_SECONDARY,
            session_manager::SessionManager::Get()->session_state());

  UserAddingScreen::Get()->Cancel();
  WaitUntilUserAddingFinishedOrCancelled();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, user_adding_finished());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());

  EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
  EXPECT_EQ(1u, user_manager::UserManager::Get()->GetLoggedInUsers().size());
  EXPECT_EQ(test_users_[0],
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, PRE_AddingSeveralUsers) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  RegisterUser(test_users_[2]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, AddingSeveralUsers) {
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());

  LoginUser(test_users_[0]);
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  for (int i = 1; i < 3; ++i) {
    UserAddingScreen::Get()->Start();
    content::RunAllPendingInMessageLoop();
    EXPECT_EQ(i, user_adding_started());
    EXPECT_EQ(session_manager::SessionState::LOGIN_SECONDARY,
              session_manager::SessionManager::Get()->session_state());
    AddUser(test_users_[i]);
    WaitUntilUserAddingFinishedOrCancelled();
    content::RunAllPendingInMessageLoop();
    EXPECT_EQ(i, user_adding_finished());
    EXPECT_EQ(session_manager::SessionState::ACTIVE,
              session_manager::SessionManager::Get()->session_state());
    EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
    ASSERT_EQ(unsigned(i + 1), user_manager->GetLoggedInUsers().size());
  }

  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());

  // Now check how unlock policy works for these users.
  PrefService* prefs1 =
      ProfileHelper::Get()
          ->GetProfileByUserUnsafe(user_manager->GetLoggedInUsers()[0])
          ->GetPrefs();
  PrefService* prefs2 =
      ProfileHelper::Get()
          ->GetProfileByUserUnsafe(user_manager->GetLoggedInUsers()[1])
          ->GetPrefs();
  PrefService* prefs3 =
      ProfileHelper::Get()
          ->GetProfileByUserUnsafe(user_manager->GetLoggedInUsers()[2])
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
  ASSERT_EQ(1UL, unlock_users.size());
  EXPECT_EQ(test_users_[0], unlock_users[0]->GetAccountId());

  prefs1->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(1UL, unlock_users.size());
  EXPECT_EQ(test_users_[0], unlock_users[0]->GetAccountId());

  // If all users have unrestricted policy then anyone can perform unlock.
  prefs1->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorUnrestricted);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(3UL, unlock_users.size());
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(test_users_[i], unlock_users[i]->GetAccountId());

  // This preference doesn't affect list of unlock users.
  prefs2->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(3UL, unlock_users.size());
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(test_users_[i], unlock_users[i]->GetAccountId());

  // Now one of the users is unable to unlock.
  SetUserCanLock(user_manager->GetLoggedInUsers()[2], false);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(2UL, unlock_users.size());
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(test_users_[i], unlock_users[i]->GetAccountId());
  SetUserCanLock(user_manager->GetLoggedInUsers()[2], true);

  // Now one of the users has not-allowed policy.
  // In this scenario this user is not allowed in multi-profile session but
  // if that user happened to still be part of multi-profile session it should
  // not be listed on screen lock.
  prefs3->SetString(prefs::kMultiProfileUserBehavior,
                    MultiProfileUserController::kBehaviorNotAllowed);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(2UL, unlock_users.size());
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(test_users_[i], unlock_users[i]->GetAccountId());
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, PRE_ScreenVisibility) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  StartupUtils::MarkOobeCompleted();
}

// http://crbug.com/978267
IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, DISABLED_ScreenVisibility) {
  LoginUser(test_users_[0]);

  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  CheckScreenIsVisible();
  UserAddingScreen::Get()->Cancel();
  WaitUntilUserAddingFinishedOrCancelled();
  content::RunAllPendingInMessageLoop();

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
  content::RunAllPendingInMessageLoop();
  CheckScreenIsVisible();
  UserAddingScreen::Get()->Cancel();
  WaitUntilUserAddingFinishedOrCancelled();
  content::RunAllPendingInMessageLoop();
}

}  // namespace chromeos
