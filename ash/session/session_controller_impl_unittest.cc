// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_controller_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/login_status.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using session_manager::SessionState;

namespace ash {
namespace {

class TestSessionObserver : public SessionObserver {
 public:
  TestSessionObserver() : active_account_id_(EmptyAccountId()) {}

  TestSessionObserver(const TestSessionObserver&) = delete;
  TestSessionObserver& operator=(const TestSessionObserver&) = delete;

  ~TestSessionObserver() override = default;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override {
    active_account_id_ = account_id;
  }

  void OnUserSessionAdded(const AccountId& account_id) override {
    user_session_account_ids_.push_back(account_id);
  }

  void OnFirstSessionStarted() override { first_session_started_ = true; }

  void OnSessionStateChanged(SessionState state) override { state_ = state; }

  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override {
    DCHECK_NE(last_user_pref_service_, pref_service);
    last_user_pref_service_ = pref_service;
    ++user_prefs_changed_count_;
  }

  std::string GetUserSessionEmails() const {
    std::string emails;
    for (const auto& account_id : user_session_account_ids_) {
      emails += account_id.GetUserEmail() + ",";
    }
    return emails;
  }

  SessionState state() const { return state_; }
  const AccountId& active_account_id() const { return active_account_id_; }
  bool first_session_started() const { return first_session_started_; }
  const std::vector<AccountId>& user_session_account_ids() const {
    return user_session_account_ids_;
  }
  PrefService* last_user_pref_service() const {
    return last_user_pref_service_;
  }
  void clear_last_user_pref_service() { last_user_pref_service_ = nullptr; }
  int user_prefs_changed_count() const { return user_prefs_changed_count_; }

 private:
  SessionState state_ = SessionState::UNKNOWN;
  AccountId active_account_id_;
  bool first_session_started_ = false;
  std::vector<AccountId> user_session_account_ids_;
  PrefService* last_user_pref_service_ = nullptr;
  int user_prefs_changed_count_ = 0;
};

void FillDefaultSessionInfo(SessionInfo* info) {
  info->can_lock_screen = true;
  info->should_lock_screen_automatically = true;
  info->is_running_in_app_mode = false;
  info->add_user_session_policy = AddUserSessionPolicy::ALLOWED;
  info->state = SessionState::LOGIN_PRIMARY;
}

class SessionControllerImplTest : public testing::Test {
 public:
  SessionControllerImplTest() = default;

  SessionControllerImplTest(const SessionControllerImplTest&) = delete;
  SessionControllerImplTest& operator=(const SessionControllerImplTest&) =
      delete;

  ~SessionControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    controller_ = std::make_unique<SessionControllerImpl>();
    controller_->AddObserver(&observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&observer_); }

  void SetSessionInfo(const SessionInfo& info) {
    controller_->SetSessionInfo(info);
  }

  void UpdateSession(uint32_t session_id, const std::string& email) {
    UserSession session;
    session.session_id = session_id;
    session.user_info.type = user_manager::USER_TYPE_REGULAR;
    session.user_info.account_id = AccountId::FromUserEmail(email);
    session.user_info.display_name = email;
    session.user_info.display_email = email;
    session.user_info.is_new_profile = false;

    controller_->UpdateUserSession(session);
  }

  std::string GetUserSessionEmails() const {
    std::string emails;
    for (const auto& session : controller_->GetUserSessions()) {
      emails += session->user_info.display_email + ",";
    }
    return emails;
  }

  SessionControllerImpl* controller() { return controller_.get(); }
  const TestSessionObserver* observer() const { return &observer_; }

 private:
  std::unique_ptr<SessionControllerImpl> controller_;
  TestSessionObserver observer_;
};

class SessionControllerImplWithShellTest : public AshTestBase {
 public:
  SessionControllerImplWithShellTest() = default;

  SessionControllerImplWithShellTest(
      const SessionControllerImplWithShellTest&) = delete;
  SessionControllerImplWithShellTest& operator=(
      const SessionControllerImplWithShellTest&) = delete;

  ~SessionControllerImplWithShellTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    controller()->AddObserver(&observer_);
  }

  void TearDown() override {
    controller()->RemoveObserver(&observer_);
    window_.reset();
    AshTestBase::TearDown();
  }

  void CreateFullscreenWindow() {
    window_ = CreateTestWindow();
    window_->SetProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
    window_state_ = WindowState::Get(window_.get());
  }

  SessionControllerImpl* controller() {
    return Shell::Get()->session_controller();
  }
  const TestSessionObserver* observer() const { return &observer_; }

 protected:
  WindowState* window_state_ = nullptr;

 private:
  TestSessionObserver observer_;
  std::unique_ptr<aura::Window> window_;
};

// Tests that the simple session info is reflected properly.
TEST_F(SessionControllerImplTest, SimpleSessionInfo) {
  SessionInfo info;
  FillDefaultSessionInfo(&info);
  SetSessionInfo(info);
  UpdateSession(1u, "user1@test.com");

  EXPECT_TRUE(controller()->CanLockScreen());
  EXPECT_TRUE(controller()->ShouldLockScreenAutomatically());
  EXPECT_FALSE(controller()->IsRunningInAppMode());

  info.can_lock_screen = false;
  SetSessionInfo(info);
  EXPECT_FALSE(controller()->CanLockScreen());
  EXPECT_TRUE(controller()->ShouldLockScreenAutomatically());
  EXPECT_FALSE(controller()->IsRunningInAppMode());

  info.should_lock_screen_automatically = false;
  SetSessionInfo(info);
  EXPECT_FALSE(controller()->CanLockScreen());
  EXPECT_FALSE(controller()->ShouldLockScreenAutomatically());
  EXPECT_FALSE(controller()->IsRunningInAppMode());

  info.is_running_in_app_mode = true;
  SetSessionInfo(info);
  EXPECT_FALSE(controller()->CanLockScreen());
  EXPECT_FALSE(controller()->ShouldLockScreenAutomatically());
  EXPECT_TRUE(controller()->IsRunningInAppMode());
}

TEST_F(SessionControllerImplTest, OnFirstSessionStarted) {
  // Simulate chrome starting a user session.
  SessionInfo info;
  FillDefaultSessionInfo(&info);
  SetSessionInfo(info);
  UpdateSession(1u, "user1@test.com");
  controller()->SetUserSessionOrder({1u});

  // Observer is notified.
  EXPECT_TRUE(observer()->first_session_started());
}

// Tests that the CanLockScreen is only true with an active user session.
TEST_F(SessionControllerImplTest, CanLockScreen) {
  SessionInfo info;
  FillDefaultSessionInfo(&info);
  ASSERT_TRUE(info.can_lock_screen);  // Check can_lock_screen default to true.
  SetSessionInfo(info);

  // Cannot lock screen when there is no active user session.
  EXPECT_FALSE(controller()->IsActiveUserSessionStarted());
  EXPECT_FALSE(controller()->CanLockScreen());

  UpdateSession(1u, "user1@test.com");
  EXPECT_TRUE(controller()->IsActiveUserSessionStarted());
  EXPECT_TRUE(controller()->CanLockScreen());
}

// Tests that AddUserSessionPolicy is set properly.
TEST_F(SessionControllerImplTest, AddUserPolicy) {
  const AddUserSessionPolicy kTestCases[] = {
      AddUserSessionPolicy::ALLOWED,
      AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
      AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
      AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED,
  };

  SessionInfo info;
  FillDefaultSessionInfo(&info);
  for (const auto& policy : kTestCases) {
    info.add_user_session_policy = policy;
    SetSessionInfo(info);
    EXPECT_EQ(policy, controller()->GetAddUserPolicy())
        << "Test case policy=" << static_cast<int>(policy);
  }
}

// Tests that session state can be set and reflected properly.
TEST_F(SessionControllerImplWithShellTest, SessionState) {
  const struct {
    SessionState state;
    bool expected_is_screen_locked;
    bool expected_is_user_session_blocked;
  } kTestCases[] = {
      {SessionState::OOBE, false, true},
      {SessionState::LOGIN_PRIMARY, false, true},
      {SessionState::LOGGED_IN_NOT_ACTIVE, false, false},
      {SessionState::ACTIVE, false, false},
      {SessionState::LOCKED, true, true},
      {SessionState::LOGIN_SECONDARY, false, true},
  };

  SessionInfo info;
  FillDefaultSessionInfo(&info);
  for (const auto& test_case : kTestCases) {
    info.state = test_case.state;
    controller()->SetSessionInfo(info);

    EXPECT_EQ(test_case.state, controller()->GetSessionState())
        << "Test case state=" << static_cast<int>(test_case.state);
    EXPECT_EQ(observer()->state(), controller()->GetSessionState())
        << "Test case state=" << static_cast<int>(test_case.state);
    EXPECT_EQ(test_case.expected_is_screen_locked,
              controller()->IsScreenLocked())
        << "Test case state=" << static_cast<int>(test_case.state);
    EXPECT_EQ(test_case.expected_is_user_session_blocked,
              controller()->IsUserSessionBlocked())
        << "Test case state=" << static_cast<int>(test_case.state);
  }
}

// Tests that LoginStatus is computed correctly for most session states.
TEST_F(SessionControllerImplTest, GetLoginStatus) {
  const struct {
    SessionState state;
    LoginStatus expected_status;
  } kTestCases[] = {
      {SessionState::UNKNOWN, LoginStatus::NOT_LOGGED_IN},
      {SessionState::OOBE, LoginStatus::NOT_LOGGED_IN},
      {SessionState::LOGIN_PRIMARY, LoginStatus::NOT_LOGGED_IN},
      {SessionState::LOGGED_IN_NOT_ACTIVE, LoginStatus::NOT_LOGGED_IN},
      {SessionState::LOCKED, LoginStatus::LOCKED},
      // TODO(jamescook): Add LOGIN_SECONDARY if we added a status for it.
  };

  SessionInfo info;
  FillDefaultSessionInfo(&info);
  for (const auto& test_case : kTestCases) {
    info.state = test_case.state;
    SetSessionInfo(info);
    EXPECT_EQ(test_case.expected_status, controller()->login_status())
        << "Test case state=" << static_cast<int>(test_case.state);
  }
}

// Tests that LoginStatus is computed correctly for active sessions.
TEST_F(SessionControllerImplTest, GetLoginStateForActiveSession) {
  // Simulate an active user session.
  SessionInfo info;
  FillDefaultSessionInfo(&info);
  info.state = SessionState::ACTIVE;
  SetSessionInfo(info);

  const struct {
    user_manager::UserType user_type;
    LoginStatus expected_status;
  } kTestCases[] = {
      {user_manager::USER_TYPE_REGULAR, LoginStatus::USER},
      {user_manager::USER_TYPE_GUEST, LoginStatus::GUEST},
      {user_manager::USER_TYPE_PUBLIC_ACCOUNT, LoginStatus::PUBLIC},
      {user_manager::USER_TYPE_KIOSK_APP, LoginStatus::KIOSK_APP},
      {user_manager::USER_TYPE_CHILD, LoginStatus::CHILD},
      {user_manager::USER_TYPE_ARC_KIOSK_APP, LoginStatus::KIOSK_APP},
      {user_manager::USER_TYPE_WEB_KIOSK_APP, LoginStatus::KIOSK_APP}
      // TODO(jamescook): Add USER_TYPE_ACTIVE_DIRECTORY if we add a status for
      // it.
  };

  for (const auto& test_case : kTestCases) {
    UserSession session;
    session.session_id = 1u;
    session.user_info.type = test_case.user_type;
    session.user_info.account_id = AccountId::FromUserEmail("user1@test.com");
    session.user_info.display_name = "User 1";
    session.user_info.display_email = "user1@test.com";
    controller()->UpdateUserSession(session);

    EXPECT_EQ(test_case.expected_status, controller()->login_status())
        << "Test case user_type=" << static_cast<int>(test_case.user_type);
  }
}

// Tests that user sessions can be set and updated.
TEST_F(SessionControllerImplTest, UserSessions) {
  EXPECT_FALSE(controller()->IsActiveUserSessionStarted());

  UpdateSession(1u, "user1@test.com");
  EXPECT_TRUE(controller()->IsActiveUserSessionStarted());
  EXPECT_EQ("user1@test.com,", GetUserSessionEmails());
  EXPECT_EQ(GetUserSessionEmails(), observer()->GetUserSessionEmails());
  EXPECT_EQ("user1@test.com",
            controller()->GetPrimaryUserSession()->user_info.display_email);

  UpdateSession(2u, "user2@test.com");
  EXPECT_TRUE(controller()->IsActiveUserSessionStarted());
  EXPECT_EQ("user1@test.com,user2@test.com,", GetUserSessionEmails());
  EXPECT_EQ(GetUserSessionEmails(), observer()->GetUserSessionEmails());
  EXPECT_EQ("user1@test.com",
            controller()->GetPrimaryUserSession()->user_info.display_email);

  UpdateSession(1u, "user1_changed@test.com");
  EXPECT_EQ("user1_changed@test.com,user2@test.com,", GetUserSessionEmails());
  // TODO(xiyuan): Verify observer gets the updated user session info.
}

// Tests that user sessions can be ordered.
TEST_F(SessionControllerImplTest, ActiveSession) {
  UpdateSession(1u, "user1@test.com");
  UpdateSession(2u, "user2@test.com");
  EXPECT_EQ("user1@test.com",
            controller()->GetPrimaryUserSession()->user_info.display_email);

  std::vector<uint32_t> order = {1u, 2u};
  controller()->SetUserSessionOrder(order);
  EXPECT_EQ("user1@test.com,user2@test.com,", GetUserSessionEmails());
  EXPECT_EQ("user1@test.com", observer()->active_account_id().GetUserEmail());
  EXPECT_EQ("user1@test.com",
            controller()->GetPrimaryUserSession()->user_info.display_email);

  order = {2u, 1u};
  controller()->SetUserSessionOrder(order);
  EXPECT_EQ("user2@test.com,user1@test.com,", GetUserSessionEmails());
  EXPECT_EQ("user2@test.com", observer()->active_account_id().GetUserEmail());
  EXPECT_EQ("user1@test.com",
            controller()->GetPrimaryUserSession()->user_info.display_email);

  order = {1u, 2u};
  controller()->SetUserSessionOrder(order);
  EXPECT_EQ("user1@test.com,user2@test.com,", GetUserSessionEmails());
  EXPECT_EQ("user1@test.com", observer()->active_account_id().GetUserEmail());
  EXPECT_EQ("user1@test.com",
            controller()->GetPrimaryUserSession()->user_info.display_email);
}

// Tests that user session is unblocked with a running unlock animation so that
// focus rules can find a correct activatable window after screen lock is
// dismissed.
TEST_F(SessionControllerImplWithShellTest,
       UserSessionUnblockedWithRunningUnlockAnimation) {
  SessionInfo info;
  FillDefaultSessionInfo(&info);

  // LOCKED means blocked user session.
  info.state = SessionState::LOCKED;
  controller()->SetSessionInfo(info);
  EXPECT_TRUE(controller()->IsUserSessionBlocked());

  const struct {
    SessionState state;
    bool expect_blocked_after_unlock_animation;
  } kTestCases[] = {
      {SessionState::LOCKED, false},
      {SessionState::OOBE, true},
      {SessionState::LOGIN_PRIMARY, true},
      {SessionState::LOGGED_IN_NOT_ACTIVE, false},
      {SessionState::ACTIVE, false},
      {SessionState::LOGIN_SECONDARY, true},
  };
  for (const auto& test_case : kTestCases) {
    info.state = test_case.state;
    controller()->SetSessionInfo(info);

    // Mark a running unlock animation.
    base::RunLoop run_loop;
    controller()->RunUnlockAnimation(base::BindLambdaForTesting(
        [&run_loop](bool aborted) { run_loop.Quit(); }));
    run_loop.Run();
    EXPECT_EQ(test_case.expect_blocked_after_unlock_animation,
              controller()->IsUserSessionBlocked())
        << "Test case state=" << static_cast<int>(test_case.state);
  }
}

TEST_F(SessionControllerImplTest, IsUserChild) {
  UserSession session;
  session.session_id = 1u;
  session.user_info.type = user_manager::USER_TYPE_CHILD;
  controller()->UpdateUserSession(session);

  EXPECT_TRUE(controller()->IsUserChild());
}

using SessionControllerImplPrefsTest = NoSessionAshTestBase;

// Verifies that SessionObserver is notified for PrefService changes.
TEST_F(SessionControllerImplPrefsTest, Observer) {
  constexpr char kUser1[] = "user1@test.com";
  constexpr char kUser2[] = "user2@test.com";
  const AccountId kUserAccount1 = AccountId::FromUserEmail(kUser1);
  const AccountId kUserAccount2 = AccountId::FromUserEmail(kUser2);

  TestSessionObserver observer;
  SessionControllerImpl* controller = Shell::Get()->session_controller();
  controller->AddObserver(&observer);

  // Setup 2 users.
  TestSessionControllerClient* session = GetSessionControllerClient();
  // Disable auto-provision of PrefService for each user.
  constexpr bool kProvidePrefService = false;
  session->AddUserSession(kUser1, user_manager::USER_TYPE_REGULAR,
                          kProvidePrefService);
  session->AddUserSession(kUser2, user_manager::USER_TYPE_REGULAR,
                          kProvidePrefService);

  // The observer is not notified because the PrefService for kUser1 is not yet
  // ready.
  session->SwitchActiveUser(kUserAccount1);
  EXPECT_EQ(nullptr, observer.last_user_pref_service());

  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(pref_service->registry(), true /* for_test */);
  session->SetUserPrefService(kUserAccount1, std::move(pref_service));
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount1),
            observer.last_user_pref_service());
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount1),
            controller->GetLastActiveUserPrefService());

  observer.clear_last_user_pref_service();

  // Switching to a user for which prefs are not ready does not notify and
  // GetLastActiveUserPrefService() returns the old PrefService.
  session->SwitchActiveUser(kUserAccount2);
  EXPECT_EQ(nullptr, observer.last_user_pref_service());
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount1),
            controller->GetLastActiveUserPrefService());

  session->SwitchActiveUser(kUserAccount1);
  EXPECT_EQ(nullptr, observer.last_user_pref_service());
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount1),
            controller->GetLastActiveUserPrefService());

  // There should be no notification about a PrefService for an inactive user
  // becoming initialized.
  pref_service = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(pref_service->registry(), true /* for_test */);
  session->SetUserPrefService(kUserAccount2, std::move(pref_service));
  EXPECT_EQ(nullptr, observer.last_user_pref_service());

  session->SwitchActiveUser(kUserAccount2);
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount2),
            observer.last_user_pref_service());
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount2),
            controller->GetLastActiveUserPrefService());

  controller->RemoveObserver(&observer);
}

// Verifies that SessionObserver is notified only once for the same user prefs.
TEST_F(SessionControllerImplPrefsTest, NotifyOnce) {
  constexpr char kUser1[] = "user1@test.com";
  constexpr char kUser2[] = "user2@test.com";
  const AccountId kUserAccount1 = AccountId::FromUserEmail(kUser1);
  const AccountId kUserAccount2 = AccountId::FromUserEmail(kUser2);

  TestSessionObserver observer;
  SessionControllerImpl* controller = Shell::Get()->session_controller();
  controller->AddObserver(&observer);
  ASSERT_EQ(0, observer.user_prefs_changed_count());

  SimulateUserLogin(kUser1);
  EXPECT_EQ(1, observer.user_prefs_changed_count());
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount1),
            observer.last_user_pref_service());

  SimulateUserLogin(kUser2);
  EXPECT_EQ(2, observer.user_prefs_changed_count());
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount2),
            observer.last_user_pref_service());

  GetSessionControllerClient()->SwitchActiveUser(kUserAccount1);
  EXPECT_EQ(3, observer.user_prefs_changed_count());
  EXPECT_EQ(controller->GetUserPrefServiceForUser(kUserAccount1),
            observer.last_user_pref_service());

  controller->RemoveObserver(&observer);
}

TEST_F(SessionControllerImplTest, GetUserType) {
  // Child accounts
  UserSession session;
  session.session_id = 1u;
  session.user_info.type = user_manager::USER_TYPE_CHILD;
  controller()->UpdateUserSession(session);
  EXPECT_EQ(user_manager::USER_TYPE_CHILD, controller()->GetUserType());

  // Regular accounts
  session = UserSession();
  session.session_id = 1u;
  session.user_info.type = user_manager::USER_TYPE_REGULAR;
  controller()->UpdateUserSession(session);
  EXPECT_EQ(user_manager::USER_TYPE_REGULAR, controller()->GetUserType());
}

TEST_F(SessionControllerImplTest, IsUserPrimary) {
  controller()->ClearUserSessionsForTest();

  // The first added user is a primary user
  UserSession session;
  session.session_id = 1u;
  session.user_info.type = user_manager::USER_TYPE_REGULAR;
  controller()->UpdateUserSession(session);
  EXPECT_TRUE(controller()->IsUserPrimary());

  // The users added thereafter are not primary users
  session = UserSession();
  session.session_id = 2u;
  session.user_info.type = user_manager::USER_TYPE_REGULAR;
  controller()->UpdateUserSession(session);
  // Simulates user switching by changing the order of session_ids.
  controller()->SetUserSessionOrder({2u, 1u});
  EXPECT_FALSE(controller()->IsUserPrimary());
}

TEST_F(SessionControllerImplTest, IsUserFirstLogin) {
  UserSession session;
  session.session_id = 1u;
  session.user_info.type = user_manager::USER_TYPE_REGULAR;
  controller()->UpdateUserSession(session);
  EXPECT_FALSE(controller()->IsUserFirstLogin());

  // user_info->is_new_profile being true means the user is first time login.
  session = UserSession();
  session.session_id = 1u;
  session.user_info.type = user_manager::USER_TYPE_REGULAR;
  session.user_info.is_new_profile = true;
  controller()->UpdateUserSession(session);
  EXPECT_TRUE(controller()->IsUserFirstLogin());
}

class CanSwitchUserTest : public AshTestBase {
 public:
  // The action type to perform / check for upon user switching.
  enum ActionType {
    NO_DIALOG,       // No dialog should be shown.
    ACCEPT_DIALOG,   // A dialog should be shown and we should accept it.
    DECLINE_DIALOG,  // A dialog should be shown and we do not accept it.
  };
  CanSwitchUserTest() = default;

  CanSwitchUserTest(const CanSwitchUserTest&) = delete;
  CanSwitchUserTest& operator=(const CanSwitchUserTest&) = delete;

  ~CanSwitchUserTest() override = default;

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    AshTestBase::TearDown();
  }

  // Accessing the capture session functionality.
  // Simulates a screen capture session start.
  void StartCaptureSession() {
    Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStart(
        base::BindRepeating(&CanSwitchUserTest::StopCaptureCallback,
                            base::Unretained(this)),
        base::RepeatingClosure(), std::u16string());
  }

  // The callback which gets called when the screen capture gets stopped.
  void StopCaptureSession() {
    Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();
  }

  // Simulates a screen capture session stop.
  void StopCaptureCallback() { stop_capture_callback_hit_count_++; }

  // Accessing the share session functionality.
  // Simulate a Screen share session start.
  void StartShareSession() {
    Shell::Get()->system_tray_notifier()->NotifyScreenShareStart(
        base::BindRepeating(&CanSwitchUserTest::StopShareCallback,
                            base::Unretained(this)),
        std::u16string());
  }

  // Simulates a screen share session stop.
  void StopShareSession() {
    Shell::Get()->system_tray_notifier()->NotifyScreenShareStop();
  }

  // The callback which gets called when the screen share gets stopped.
  void StopShareCallback() { stop_share_callback_hit_count_++; }

  // Issuing a switch user call which might or might not create a dialog.
  // The passed |action| type parameter defines the outcome (which will be
  // checked) and the action the user will choose.
  void SwitchUser(ActionType action) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&CloseMessageBox, action));
    Shell::Get()->session_controller()->CanSwitchActiveUser(base::BindOnce(
        &CanSwitchUserTest::SwitchCallback, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Called when the user will get actually switched.
  void SwitchCallback(bool switch_user) {
    if (switch_user)
      switch_callback_hit_count_++;
  }

  // Various counter accessors.
  int stop_capture_callback_hit_count() const {
    return stop_capture_callback_hit_count_;
  }
  int stop_share_callback_hit_count() const {
    return stop_share_callback_hit_count_;
  }
  int switch_callback_hit_count() const { return switch_callback_hit_count_; }

 private:
  static void CloseMessageBox(ActionType action) {
    aura::Window* active_window = window_util::GetActiveWindow();
    views::DialogDelegate* dialog =
        active_window ? views::Widget::GetWidgetForNativeWindow(active_window)
                            ->widget_delegate()
                            ->AsDialogDelegate()
                      : nullptr;

    switch (action) {
      case NO_DIALOG:
        EXPECT_FALSE(dialog);
        return;
      case ACCEPT_DIALOG:
        ASSERT_TRUE(dialog);
        EXPECT_TRUE(dialog->Accept());
        return;
      case DECLINE_DIALOG:
        ASSERT_TRUE(dialog);
        EXPECT_TRUE(dialog->Close());
        return;
    }
  }

  // Various counters to query for.
  int stop_capture_callback_hit_count_ = 0;
  int stop_share_callback_hit_count_ = 0;
  int switch_callback_hit_count_ = 0;
};

// Test that when there is no screen operation going on the user switch will be
// performed as planned.
TEST_F(CanSwitchUserTest, NoLock) {
  EXPECT_EQ(0, switch_callback_hit_count());
  SwitchUser(CanSwitchUserTest::NO_DIALOG);
  EXPECT_EQ(1, switch_callback_hit_count());
}

// Test that with a screen capture operation going on, the user will need to
// confirm. Declining will neither change the running state or switch users.
TEST_F(CanSwitchUserTest, CaptureActiveDeclined) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartCaptureSession();
  SwitchUser(CanSwitchUserTest::DECLINE_DIALOG);
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
  StopCaptureSession();
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
}

// Test that with a screen share operation going on, the user will need to
// confirm. Declining will neither change the running state or switch users.
TEST_F(CanSwitchUserTest, ShareActiveDeclined) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartShareSession();
  SwitchUser(CanSwitchUserTest::DECLINE_DIALOG);
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
  StopShareSession();
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
}

// Test that with both operations going on, the user will need to confirm.
// Declining will neither change the running state or switch users.
TEST_F(CanSwitchUserTest, BothActiveDeclined) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartShareSession();
  StartCaptureSession();
  SwitchUser(CanSwitchUserTest::DECLINE_DIALOG);
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
  StopShareSession();
  StopCaptureSession();
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
}

// Test that with a screen capture operation going on, the user will need to
// confirm. Accepting will change to stopped state and switch users.
TEST_F(CanSwitchUserTest, CaptureActiveAccepted) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartCaptureSession();
  SwitchUser(CanSwitchUserTest::ACCEPT_DIALOG);
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
  // Another stop should have no effect.
  StopCaptureSession();
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
}

// Test that with a screen share operation going on, the user will need to
// confirm. Accepting will change to stopped state and switch users.
TEST_F(CanSwitchUserTest, ShareActiveAccepted) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartShareSession();
  SwitchUser(CanSwitchUserTest::ACCEPT_DIALOG);
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
  // Another stop should have no effect.
  StopShareSession();
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
}

// Test that with both operations going on, the user will need to confirm.
// Accepting will change to stopped state and switch users.
TEST_F(CanSwitchUserTest, BothActiveAccepted) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartShareSession();
  StartCaptureSession();
  SwitchUser(CanSwitchUserTest::ACCEPT_DIALOG);
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
  // Another stop should have no effect.
  StopShareSession();
  StopCaptureSession();
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
}

using SessionControllerImplUnblockTest = NoSessionAshTestBase;

TEST_F(SessionControllerImplUnblockTest, ActiveWindowAfterUnblocking) {
  EXPECT_TRUE(Shell::Get()->session_controller()->IsUserSessionBlocked());
  auto widget = CreateTestWidget();
  // |widget| should not be active as it is blocked by SessionControllerImpl.
  EXPECT_FALSE(widget->IsActive());
  SimulateUserLogin("user@test.com");
  EXPECT_FALSE(Shell::Get()->session_controller()->IsUserSessionBlocked());

  // |widget| should now be active as SessionControllerImpl no longer is
  // blocking windows from becoming active.
  EXPECT_TRUE(widget->IsActive());
}

TEST_F(SessionControllerImplWithShellTest, ExitFullscreenBeforeLock) {
  CreateFullscreenWindow();
  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());

  EXPECT_FALSE(window_state_->IsFullscreen());
}

}  // namespace
}  // namespace ash
