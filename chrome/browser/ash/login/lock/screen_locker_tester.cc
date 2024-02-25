// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/screen_locker_tester.h"

#include <cstdint>
#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
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

  LoginAttemptObserver(const LoginAttemptObserver&) = delete;
  LoginAttemptObserver& operator=(const LoginAttemptObserver&) = delete;

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
};

}  // namespace

ScreenLockerTester::ScreenLockerTester() {
  DCHECK(session_manager::SessionManager::Get());
  session_manager_observation_.Observe(session_manager::SessionManager::Get());
}

ScreenLockerTester::~ScreenLockerTester() = default;

void ScreenLockerTester::OnSessionStateChanged() {
  if (IsLocked() && !on_lock_callback_.is_null()) {
    std::move(on_lock_callback_).Run();
  }
  if (!IsLocked() && !on_unlock_callback_.is_null()) {
    std::move(on_unlock_callback_).Run();
  }
}

void ScreenLockerTester::Lock() {
  ScreenLocker::Show();
  WaitForLock();
  base::RunLoop().RunUntilIdle();
}

void ScreenLockerTester::WaitForLock() {
  if (!IsLocked()) {
    base::RunLoop run_loop;
    on_lock_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  ASSERT_TRUE(IsLocked());
  ASSERT_EQ(session_manager::SessionState::LOCKED,
            session_manager::SessionManager::Get()->session_state());
}

void ScreenLockerTester::WaitForUnlock() {
  if (IsLocked()) {
    base::RunLoop run_loop;
    on_unlock_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  ASSERT_TRUE(!IsLocked());
  ASSERT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());
}

void ScreenLockerTester::SetUnlockPassword(const AccountId& account_id,
                                           const std::string& password) {
  UserContext user_context(user_manager::UserType::kRegular, account_id);
  user_context.SetKey(Key(password));

  auto* locker = ScreenLocker::default_screen_locker();
  CHECK(locker);
  locker->SetAuthenticatorsForTesting(
      base::MakeRefCounted<StubAuthenticator>(locker, user_context));
}

bool ScreenLockerTester::IsLocked() {
  return IsScreenLockerLocked() && LoginScreenTestApi::IsLockShown();
}

bool ScreenLockerTester::IsLockRestartButtonShown() {
  return IsScreenLockerLocked() && LoginScreenTestApi::IsRestartButtonShown();
}

bool ScreenLockerTester::IsLockShutdownButtonShown() {
  return IsScreenLockerLocked() && LoginScreenTestApi::IsShutdownButtonShown();
}

void ScreenLockerTester::UnlockWithPassword(const AccountId& account_id,
                                            const std::string& password) {
  LoginScreenTestApi::SubmitPassword(account_id, password,
                                     true /*check_if_submittable*/);
  base::RunLoop().RunUntilIdle();
}

void ScreenLockerTester::ForceSubmitPassword(const AccountId& account_id,
                                             const std::string& password) {
  LoginScreenTestApi::SubmitPassword(account_id, password,
                                     false /*check_if_submittable*/);
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
