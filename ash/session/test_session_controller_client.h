// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_TEST_SESSION_CONTROLLER_CLIENT_H_
#define ASH_SESSION_TEST_SESSION_CONTROLLER_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "ash/public/cpp/session/session_controller_client.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/test/login_info.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/token.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"

namespace views {
class Widget;
}

class AccountId;
class PrefService;

namespace ash {

enum class AddUserSessionPolicy;
class SessionControllerImpl;
class TestPrefServiceProvider;

// Implement SessionControllerClient to simulate chrome behavior
// in tests. This breaks the ash/chrome dependency to allow testing ash code in
// isolation. Note that tests that have an instance of SessionControllerClient
// should NOT use this, i.e. tests that run BrowserMain to have chrome's
// SessionControllerClient created, e.g. InProcessBrowserTest based tests. On
// the other hand, tests code in chrome can use this class as long as it does
// not run BrowserMain, e.g. testing::Test based test.
class TestSessionControllerClient final : public SessionControllerClient {
 public:
  TestSessionControllerClient(SessionControllerImpl* controller,
                              TestPrefServiceProvider* prefs_provider,
                              bool create_signin_pref_service);

  TestSessionControllerClient(const TestSessionControllerClient&) = delete;
  TestSessionControllerClient& operator=(const TestSessionControllerClient&) =
      delete;

  ~TestSessionControllerClient() override;

  // Initialize using existing info in |controller| and set as its client.
  void InitializeAndSetClient();

  // Sets up the default state of SessionController.
  void Reset();

  int attempt_restart_chrome_count() const {
    return attempt_restart_chrome_count_;
  }
  int request_hide_lock_screen_count() const {
    return request_hide_lock_screen_count_;
  }
  int request_sign_out_count() const { return request_sign_out_count_; }
  int request_restart_for_update_count() const {
    return request_restart_for_update_count_;
  }

  // Helpers to set SessionController state.
  void SetCanLockScreen(bool can_lock);
  void SetShouldLockScreenAutomatically(bool should_lock);
  void SetAddUserSessionPolicy(AddUserSessionPolicy policy);
  void SetSessionState(session_manager::SessionState state);
  void SetIsRunningInAppMode(bool app_mode);
  void SetIsDemoSession();

  // Adds a user session from a given LoginInfo, `acount_id' and `pref_service`.
  // For convenience, `LoginInfo.user_email` can be used to create an AccountId,
  // in which case, the `account_id` should be std::nulopt.  For testing
  // behavior where |AccountId|s are compared, prefer the method of the same
  // name that takes an |AccountId| created with a valid storage key
  // instead. See the documentation for|AccountId::GetUserEmail| for discussion.
  //
  // If `pref_service1 is provided, the new session will use it, or it will
  // create a new PrefService. However, if `pref_service_must_exist_` is set to
  // true, the PrefService for the account must already exist, or it will result
  // in CHCEK failure.
  AccountId AddUserSession(LoginInfo login_info,
                           std::optional<AccountId> account_id = std::nullopt,
                           std::unique_ptr<PrefService> pref_service = nullptr);

  // Synchronously lock screen by requesting screen lock and waiting for the
  // request to complete.
  void LockScreen();

  // Simulates screen unlocking. It is virtual so that test cases can override
  // it. The default implementation sets the session state of SessionController
  // to be ACTIVE.
  void UnlockScreen();

  // Spins message loop to finish pending lock screen request if any.
  void FlushForTest();

  // Use |pref_service| for sign-in profile pref service.
  void SetSigninScreenPrefService(std::unique_ptr<PrefService> pref_service);

  // Use |pref_service| for the user identified by |account_id|.
  void SetUnownedUserPrefService(const AccountId& account_id,
                                 raw_ptr<PrefService> unowned_pref_service);

  // ash::SessionControllerClient:
  void RequestLockScreen() override;
  void RequestHideLockScreen() override;
  void RequestSignOut() override;
  void RequestRestartForUpdate() override;
  void AttemptRestartChrome() override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void CycleActiveUser(CycleUserDirection direction) override;
  void ShowMultiProfileLogin() override;
  void EmitAshInitialized() override;
  PrefService* GetSigninScreenPrefService() override;
  PrefService* GetUserPrefService(const AccountId& account_id) override;
  base::FilePath GetProfilePath(const AccountId& account_id) override;
  std::tuple<bool, bool> IsEligibleForSeaPen(
      const AccountId& account_id) override;
  std::optional<int> GetExistingUsersCount() const override;

  // By default `LockScreen()` only changes the session state but no UI views
  // will be created.  If your tests requires the lock screen to be created,
  // please set this to true.
  void set_show_lock_screen_views(bool should_show) {
    should_show_lock_screen_ = should_show;
  }

  void set_is_eligible_for_background_replace(
      const std::tuple<bool, bool>& is_eligible_for_background_replace) {
    is_eligible_for_background_replace_ = is_eligible_for_background_replace;
  }

  void set_existing_users_count(int existing_users_count) {
    existing_users_count_ = existing_users_count;
  }

  void set_pref_service_must_exist(bool pref_service_must_exist) {
    pref_service_must_exist_ = pref_service_must_exist;
  }

  int NumberOfLoggedInUsers() const;

 private:
  void DoSwitchUser(const AccountId& account_id, bool switch_user);

  // Notify first session ready if the notification has not sent, there
  // is at least one user session created, and session state is ACTIVE.
  void MaybeNotifyFirstSessionReady();

  const raw_ptr<SessionControllerImpl, DanglingUntriaged> controller_;
  const raw_ptr<TestPrefServiceProvider> prefs_provider_;

  int fake_session_id_ = 0;
  SessionInfo session_info_;
  bool first_session_ready_fired_ = false;

  // If true, pref service must exist when adding a session, or fail with CHECK.
  bool pref_service_must_exist_ = false;

  int request_hide_lock_screen_count_ = 0;
  int request_sign_out_count_ = 0;
  int request_restart_for_update_count_ = 0;
  int attempt_restart_chrome_count_ = 0;

  bool should_show_lock_screen_ = false;

  std::tuple<bool, bool> is_eligible_for_background_replace_ = {true, true};

  int existing_users_count_ = 0;

  bool reuse_pref_service_ = false;

  std::unique_ptr<views::Widget> multi_profile_login_widget_;

  base::WeakPtrFactory<TestSessionControllerClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SESSION_TEST_SESSION_CONTROLLER_CLIENT_H_
