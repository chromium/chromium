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

  void set_use_lower_case_user_id(bool value) {
    use_lower_case_user_id_ = value;
  }

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

  // Adds a user session from a given display email. If `provide_pref_service`
  // is true, eagerly inject a PrefService for this user. `is_new_profile`
  // indicates whether the user has a newly created profile on the device.
  //
  // For convenience `display_email` is used to create an `AccountId`. For
  // testing behavior where `AccountId`s are compared, prefer the method of the
  // same name that takes an `AccountId` created with a valid storage key
  // instead. See the documentation for`AccountId::GetUserEmail` for discussion.
  //
  // Here is how PrefService creation behaves.
  // a) If `pref_service` is provided, the account will use this provided pref
  // service.
  // b) If `pref_service` is `nullptr` and `provide_pref_service_` is
  // set to true, it will automatically create a new pref service for the
  // account.  The pref service for the account should not exist, or it will
  // result in CHECK failure.
  // c) However, if ClearLogin was called before, the sessions is allowed to
  // reuse the existing pref service. This is to allow a test to emulate the
  // situation that logining in a same user will use the pref service updated by
  // previous login.
  void AddUserSession(
      std::string_view display_email,
      user_manager::UserType user_type = user_manager::UserType::kRegular,
      std::unique_ptr<PrefService> pref_service = nullptr,
      bool is_new_profile = false,
      const std::string& given_name = std::string(),
      bool is_account_managed = false);

  // Adds a user session from a given AccountId.
  void AddUserSession(
      const AccountId& account_id,
      std::string_view display_email,
      user_manager::UserType user_type = user_manager::UserType::kRegular,
      std::unique_ptr<PrefService> pref_service = nullptr,
      bool is_new_profile = false,
      const std::string& given_name = std::string(),
      bool is_account_managed = false);

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

  void set_provide_pref_service(bool provide_pref_service) {
    provide_pref_service_ = provide_pref_service;
  }

  int NumberOfLoggedInUsers() const;

 private:
  void DoSwitchUser(const AccountId& account_id, bool switch_user);

  // Notify first session ready if the notification has not sent, there
  // is at least one user session created, and session state is ACTIVE.
  void MaybeNotifyFirstSessionReady();

  // Notify user prefs initialized if user session has started.
  void NotifyUserPrefServiceInitialized(const AccountId& account_id);

  const raw_ptr<SessionControllerImpl, DanglingUntriaged> controller_;
  const raw_ptr<TestPrefServiceProvider> prefs_provider_;

  int fake_session_id_ = 0;
  SessionInfo session_info_;
  bool first_session_ready_fired_ = false;

  // Whether to auto create user prefs for `AddSession` if `pref_service` is not
  // specified.
  bool provide_pref_service_ = true;

  bool use_lower_case_user_id_ = true;
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
