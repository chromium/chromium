// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_state/session_state_changed_event_dispatcher.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/api/login_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::api::login_state::OnSessionStateChanged::kEventName;

namespace {

const struct {
  const session_manager::SessionState session_state;
  const crosapi::mojom::SessionState mapped_mojo_state;
  const extensions::api::login_state::SessionState expected_event;
} kTestCases[] = {
    {session_manager::SessionState::OOBE,
     crosapi::mojom::SessionState::kInOobeScreen,
     extensions::api::login_state::SessionState::kInOobeScreen},
    {session_manager::SessionState::LOGIN_PRIMARY,
     crosapi::mojom::SessionState::kInLoginScreen,
     extensions::api::login_state::SessionState::kInLoginScreen},
    {session_manager::SessionState::ACTIVE,
     crosapi::mojom::SessionState::kInSession,
     extensions::api::login_state::SessionState::kInSession},
    {session_manager::SessionState::LOCKED,
     crosapi::mojom::SessionState::kInLockScreen,
     extensions::api::login_state::SessionState::kInLockScreen},
    {session_manager::SessionState::UNKNOWN,
     crosapi::mojom::SessionState::kUnknown,
     extensions::api::login_state::SessionState::kUnknown},
};

bool WasSessionStateChangedEventDispatched(
    const extensions::TestEventRouterObserver* observer,
    extensions::api::login_state::SessionState expected_state) {
  const auto& event_map = observer->events();
  auto iter = event_map.find(kEventName);
  if (iter == event_map.end()) {
    return false;
  }

  const extensions::Event& event = *iter->second;
  CHECK_EQ(1u, event.event_args.size());
  std::string session_state = event.event_args[0].GetString();
  return extensions::api::login_state::ParseSessionState(session_state) ==
         expected_state;
}

}  // namespace

namespace extensions {

class SessionStateChangedEventDispatcherAshUnittest : public testing::Test {
 public:
  // A mock around the event dispatcher for tracking callbacks.
  class MockSessionStateChangedEventDispatcher
      : public SessionStateChangedEventDispatcher {
   public:
    explicit MockSessionStateChangedEventDispatcher(
        content::BrowserContext* context)
        : SessionStateChangedEventDispatcher(context) {}
    ~MockSessionStateChangedEventDispatcher() override = default;
    MOCK_METHOD1(OnSessionStateChanged,
                 void(crosapi::mojom::SessionState state));
  };

  SessionStateChangedEventDispatcherAshUnittest() {}

  SessionStateChangedEventDispatcherAshUnittest(
      const SessionStateChangedEventDispatcherAshUnittest&) = delete;
  SessionStateChangedEventDispatcherAshUnittest& operator=(
      const SessionStateChangedEventDispatcherAshUnittest&) = delete;

  ~SessionStateChangedEventDispatcherAshUnittest() override = default;

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

    crosapi::IdleServiceAsh::DisableForTesting();
    ash::LoginState::Initialize();
    manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();

    dispatcher_ =
        std::make_unique<SessionStateChangedEventDispatcher>(testing_profile_);
    event_router_ = std::make_unique<EventRouter>(testing_profile_, nullptr);
    dispatcher_->SetEventRouterForTesting(event_router_.get());
    observer_ = std::make_unique<TestEventRouterObserver>(event_router_.get());
  }

  void TearDown() override {
    observer_.reset();
    event_router_.reset();
    manager_.reset();
    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
    ash::LoginState::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<crosapi::CrosapiManager> manager_;
  std::unique_ptr<SessionStateChangedEventDispatcher> dispatcher_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<EventRouter> event_router_;
  std::unique_ptr<TestEventRouterObserver> observer_;
};

TEST_F(SessionStateChangedEventDispatcherAshUnittest,
       OnSessionStateChangedDispatchesEvent) {
  for (const auto& test : kTestCases) {
    dispatcher_->OnSessionStateChanged(test.mapped_mojo_state);
    EXPECT_TRUE(WasSessionStateChangedEventDispatched(observer_.get(),
                                                      test.expected_event));
  }
}

TEST_F(SessionStateChangedEventDispatcherAshUnittest,
       ObservesSessionStateChangedEvent) {
  // The observer is fired every time the session state changes (to a new mapped
  // state).
  for (const auto& test : kTestCases) {
    testing::StrictMock<MockSessionStateChangedEventDispatcher> mock_dispatcher(
        testing_profile_);
    base::RunLoop run_loop;

    EXPECT_CALL(mock_dispatcher, OnSessionStateChanged(test.mapped_mojo_state))
        .Times(1)
        .WillOnce(testing::Invoke([&]() { run_loop.Quit(); }));

    session_manager_->SetSessionState(test.session_state);
    run_loop.Run();
  }
}

}  // namespace extensions
