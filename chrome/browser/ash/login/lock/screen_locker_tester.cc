// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/screen_locker_tester.h"

#include <cstdint>
#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
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
      run_loop_.reset();
    }
    ASSERT_TRUE(login_attempted_);
  }

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override { LoginAttempted(); }
  void OnAuthSuccess(const UserContext& credentials) override {
    auth_succeeded_ = true;
    LoginAttempted();
  }

  bool auth_succeeded() const { return auth_succeeded_; }

 private:
  void LoginAttempted() {
    login_attempted_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  bool login_attempted_ = false;
  bool auth_succeeded_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

ScreenLockerTester::ScreenLockerTester() = default;

ScreenLockerTester::~ScreenLockerTester() = default;

void ScreenLockerTester::Lock() {
  CHECK_EQ(CHECK_DEREF(session_manager::SessionManager::Get()).session_state(),
           session_manager::SessionState::ACTIVE);

  ScreenLocker::Show();
  WaitForLock();
  base::RunLoop().RunUntilIdle();
}

void ScreenLockerTester::WaitForLock() {
  SessionStateWaiter(session_manager::SessionState::LOCKED).Wait();
  ASSERT_TRUE(IsLocked());
}

void ScreenLockerTester::WaitForUnlock() {
  SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();
  ASSERT_TRUE(!IsLocked());
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
  LoginAttemptObserver login_observer;
  LoginScreenTestApi::SubmitPassword(account_id, password,
                                     true /*check_if_submittable*/);
  login_observer.WaitForAttempt();
  if (login_observer.auth_succeeded()) {
    WaitForUnlock();
  }
}

void ScreenLockerTester::ForceSubmitPassword(const AccountId& account_id,
                                             const std::string& password) {
  LoginAttemptObserver login_observer;
  LoginScreenTestApi::SubmitPassword(account_id, password,
                                     false /*check_if_submittable*/);
  login_observer.WaitForAttempt();
  if (login_observer.auth_succeeded()) {
    WaitForUnlock();
  }
}

}  // namespace ash
