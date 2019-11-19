// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/session_state_changed_event_dispatcher.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_service_manager_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::api::login_state::OnSessionStateChanged::kEventName;

namespace {

bool WasSessionStateChangedEventDispatched(
    const extensions::TestEventRouterObserver* observer,
    extensions::api::login_state::SessionState expected_state) {
  const auto& event_map = observer->events();
  auto iter = event_map.find(kEventName);
  if (iter == event_map.end()) {
    return false;
  }

  const extensions::Event& event = *iter->second;
  CHECK(event.event_args);
  CHECK_EQ(1u, event.event_args->GetList().size());
  std::string session_state = (event.event_args->GetList())[0].GetString();
  return extensions::api::login_state::ParseSessionState(session_state) ==
         expected_state;
}

}  // namespace

namespace extensions {

class SessionStateChangedEventDispatcherUnittest : public testing::Test {
 public:
  SessionStateChangedEventDispatcherUnittest() {}
  ~SessionStateChangedEventDispatcherUnittest() override = default;

  void SetUp() override {
    // |session_manager::SessionManager| is not initialized by default. This
    // sets up the static instance of |SessionManager| so
    // |session_manager::SessionManager::Get()| will return this particular
    // instance.
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ =
        profile_manager_->CreateTestingProfile(chrome::kInitialProfile);
    dispatcher_ =
        std::make_unique<SessionStateChangedEventDispatcher>(testing_profile_);
    event_router_ = std::make_unique<EventRouter>(testing_profile_, nullptr);
    dispatcher_->SetEventRouterForTesting(event_router_.get());
    observer_ = std::make_unique<TestEventRouterObserver>(event_router_.get());
  }

  void TearDown() override {
    observer_.reset();
    event_router_.reset();
    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestServiceManagerContext service_manager_context_;
  TestingProfile* testing_profile_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<SessionStateChangedEventDispatcher> dispatcher_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<EventRouter> event_router_;
  std::unique_ptr<TestEventRouterObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateChangedEventDispatcherUnittest);
};

// Test that |OnSessionStateChanged| is dispatched when the session state
// changes.
TEST_F(SessionStateChangedEventDispatcherUnittest, EventIsDispatched) {
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(WasSessionStateChangedEventDispatched(
      observer_.get(),
      api::login_state::SessionState::SESSION_STATE_IN_SESSION));

  session_manager_->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_TRUE(WasSessionStateChangedEventDispatched(
      observer_.get(),
      api::login_state::SessionState::SESSION_STATE_IN_LOCK_SCREEN));
}

// Test that the event is not dispatched when the mapped
// |api::login_state::SessionState| is the same even when the
// |session_manager::SessionState| is different.
TEST_F(SessionStateChangedEventDispatcherUnittest,
       EventIsNotDispatchedWhenSessionStateIsSame) {
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  EXPECT_TRUE(WasSessionStateChangedEventDispatched(
      observer_.get(),
      api::login_state::SessionState::SESSION_STATE_IN_LOGIN_SCREEN));
  observer_->ClearEvents();

  session_manager_->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_TRUE(observer_->events().empty());
}

}  // namespace extensions
