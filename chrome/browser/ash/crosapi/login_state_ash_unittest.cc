// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/login_state_ash.h"

#include "base/test/test_future.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {
void EvaluateGetSessionStateResult(base::OnceClosure closure,
                                   mojom::GetSessionStateResultPtr expected,
                                   mojom::GetSessionStateResultPtr found) {
  ASSERT_EQ(expected->which(), found->which());
  if (expected->which() == mojom::GetSessionStateResult::Tag::kErrorMessage) {
    ASSERT_EQ(expected->get_error_message(), found->get_error_message());
  } else {
    ASSERT_EQ(expected->get_session_state(), found->get_session_state());
  }
  std::move(closure).Run();
}
}  // namespace

class LoginStateAshTest : public testing::Test {
 public:
  class MockSessionStateChangedEventObserver
      : public mojom::SessionStateChangedEventObserver {
   public:
    MockSessionStateChangedEventObserver() = default;
    ~MockSessionStateChangedEventObserver() override = default;
    MOCK_METHOD1(OnSessionStateChanged, void(mojom::SessionState state));
    mojo::Receiver<mojom::SessionStateChangedEventObserver> receiver_{this};
  };

  LoginStateAshTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~LoginStateAshTest() override = default;

  void SetUp() override {
    // |session_manager::SessionManager| is not initialized by default.
    // This sets up the static instance of |SessionManager| so
    // |session_manager::SessionManager::Get()| will return this particular
    // instance.
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    login_state_ash_ = std::make_unique<LoginStateAsh>();
    login_state_ash_->BindReceiver(
        login_state_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { login_state_ash_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;

  std::unique_ptr<session_manager::SessionManager> session_manager_;
  mojo::Remote<mojom::LoginState> login_state_remote_;
  std::unique_ptr<LoginStateAsh> login_state_ash_;
};

// Test that calling |GetSessionState()| returns the correctly mapped state.
TEST_F(LoginStateAshTest, GetSessionState) {
  const struct {
    const session_manager::SessionState session_state;
    const mojom::SessionState expected;
  } kTestCases[] = {
      {session_manager::SessionState::UNKNOWN, mojom::SessionState::kUnknown},
      {session_manager::SessionState::OOBE, mojom::SessionState::kInOobeScreen},
      {session_manager::SessionState::LOGIN_PRIMARY,
       mojom::SessionState::kInLoginScreen},
      {session_manager::SessionState::LOGGED_IN_NOT_ACTIVE,
       mojom::SessionState::kInLoginScreen},
      {session_manager::SessionState::LOGIN_SECONDARY,
       mojom::SessionState::kInLoginScreen},
      {session_manager::SessionState::ACTIVE, mojom::SessionState::kInSession},
      {session_manager::SessionState::LOCKED,
       mojom::SessionState::kInLockScreen},
      {session_manager::SessionState::RMA, mojom::SessionState::kInRmaScreen},
  };

  for (const auto& test : kTestCases) {
    session_manager_->SetSessionState(test.session_state);

    mojom::GetSessionStateResultPtr expected_result_ptr =
        mojom::GetSessionStateResult::NewSessionState(test.expected);

    base::test::TestFuture<void> future;
    login_state_remote_->GetSessionState(
        base::BindOnce(&EvaluateGetSessionStateResult, future.GetCallback(),
                       std::move(expected_result_ptr)));
    EXPECT_TRUE(future.Wait());
  }
}

// Test that |OnSessionStateChanged| is dispatched when the session state
// changes.
TEST_F(LoginStateAshTest, OnSessionStateChangedIsDispatched) {
  testing::StrictMock<MockSessionStateChangedEventObserver> observer;

  login_state_remote_->AddObserver(
      observer.receiver_.BindNewPipeAndPassRemote());
  login_state_remote_.FlushForTesting();

  EXPECT_CALL(observer, OnSessionStateChanged(mojom::SessionState::kInSession))
      .Times(1);
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  observer.receiver_.FlushForTesting();
}

// Test that |OnSessionStateChanged| is not dispatched when the mapped state is
// the same even when the |session_manager::SessionState| is different.
TEST_F(LoginStateAshTest, OnSessionStateChangedIsNotDispatchedWhenStateIsSame) {
  testing::StrictMock<MockSessionStateChangedEventObserver> observer;

  login_state_remote_->AddObserver(
      observer.receiver_.BindNewPipeAndPassRemote());
  login_state_remote_.FlushForTesting();

  EXPECT_CALL(observer,
              OnSessionStateChanged(mojom::SessionState::kInLoginScreen))
      .Times(1);
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  observer.receiver_.FlushForTesting();

  session_manager_->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  observer.receiver_.FlushForTesting();
}

}  // namespace crosapi
