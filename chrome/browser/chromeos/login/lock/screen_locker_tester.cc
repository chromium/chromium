// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"

#include <cstdint>
#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/fake_extended_authenticator.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/stub_authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

bool IsScreenLockerLocked() {
  return ScreenLocker::default_screen_locker() &&
         ScreenLocker::default_screen_locker()->locked();
}

// This class is used to observe state of the global ScreenLocker instance,
// which can go away as a result of a successful authentication. As such,
// it needs to directly reference the global ScreenLocker.
class LoginAttemptObserver : public AuthStatusConsumer {
 public:
  LoginAttemptObserver() : AuthStatusConsumer() {
    ScreenLocker::default_screen_locker()->SetLoginStatusConsumer(this);
  }
  ~LoginAttemptObserver() override {
    if (ScreenLocker::default_screen_locker())
      ScreenLocker::default_screen_locker()->SetLoginStatusConsumer(nullptr);
  }

  void WaitForAttempt() {
    if (!login_attempted_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.release();
    }
    ASSERT_TRUE(login_attempted_);
  }

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override { LoginAttempted(); }
  void OnAuthSuccess(const UserContext& credentials) override {
    LoginAttempted();
  }

 private:
  void LoginAttempted() {
    login_attempted_ = true;
    if (run_loop_)
      run_loop_->QuitWhenIdle();
  }

  bool login_attempted_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(LoginAttemptObserver);
};

}  // namespace

ScreenLockerTester::ScreenLockerTester() = default;

ScreenLockerTester::~ScreenLockerTester() = default;

void ScreenLockerTester::Lock() {
  content::WindowedNotificationObserver lock_state_observer(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());
  ScreenLocker::Show();
  if (!IsLocked())
    lock_state_observer.Wait();
  ASSERT_TRUE(IsLocked());
  ASSERT_EQ(session_manager::SessionState::LOCKED,
            session_manager::SessionManager::Get()->session_state());
  base::RunLoop().RunUntilIdle();
}

void ScreenLockerTester::SetUnlockPassword(const AccountId& account_id,
                                           const std::string& password) {
  UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                           account_id);
  user_context.SetKey(Key(password));

  auto* locker = ScreenLocker::default_screen_locker();
  locker->SetAuthenticatorsForTesting(
      base::MakeRefCounted<StubAuthenticator>(locker, user_context),
      base::MakeRefCounted<FakeExtendedAuthenticator>(locker, user_context));
}

bool ScreenLockerTester::IsLocked() {
  return IsScreenLockerLocked() && ash::LoginScreenTestApi::IsLockShown();
}

bool ScreenLockerTester::IsLockRestartButtonShown() {
  return IsScreenLockerLocked() &&
         ash::LoginScreenTestApi::IsRestartButtonShown();
}

bool ScreenLockerTester::IsLockShutdownButtonShown() {
  return IsScreenLockerLocked() &&
         ash::LoginScreenTestApi::IsShutdownButtonShown();
}

bool ScreenLockerTester::IsAuthErrorBubbleShown() {
  return IsScreenLockerLocked() &&
         ash::LoginScreenTestApi::IsAuthErrorBubbleShown();
}

void ScreenLockerTester::UnlockWithPassword(const AccountId& account_id,
                                            const std::string& password) {
  ash::LoginScreenTestApi::SubmitPassword(account_id, password);
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
