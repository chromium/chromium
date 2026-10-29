// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Tests for AshTestHelper. Who will watch the watchers? And who will test
// the tests?
class AshTestHelperTest : public testing::Test {
 public:
  AshTestHelperTest() = default;

  AshTestHelperTest(const AshTestHelperTest&) = delete;
  AshTestHelperTest& operator=(const AshTestHelperTest&) = delete;

  ~AshTestHelperTest() override = default;

  void SetUp() override {
    ash_test_helper_.SetUp();
  }

  void TearDown() override { ash_test_helper_.TearDown(); }

  AshTestHelper* ash_test_helper() { return &ash_test_helper_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  AshTestHelper ash_test_helper_;
};

// Ensure that we have initialized enough of Ash to create and show a window.
TEST_F(AshTestHelperTest, AshTestHelper) {
  // Check initial state.
  ASSERT_TRUE(Shell::HasInstance());
  EXPECT_TRUE(Shell::Get()->shell_delegate());
  EXPECT_TRUE(ash_test_helper()->GetContext());
  EXPECT_EQ(1, Shell::Get()->session_controller()->NumberOfLoggedInUsers());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            Shell::Get()->session_controller()->GetSessionState());

  // Enough state is initialized to create a window.
  using views::Widget;
  std::unique_ptr<Widget> w1(new Widget);
  Widget::InitParams params(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params.context = ash_test_helper()->GetContext();
  w1->Init(std::move(params));
  w1->Show();
  EXPECT_TRUE(w1->IsActive());
  EXPECT_TRUE(w1->IsVisible());
}

TEST(AshTestHelperInitParamsTest, EmptyUsers) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};
  AshTestHelper ash_test_helper;

  AshTestHelper::InitParams init_params;
  init_params.users.clear();
  ash_test_helper.SetUp(std::move(init_params));

  EXPECT_EQ(0, Shell::Get()->session_controller()->NumberOfLoggedInUsers());
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            Shell::Get()->session_controller()->GetSessionState());

  ash_test_helper.TearDown();
}

TEST(AshTestHelperInitParamsTest, MultipleUsers) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};
  AshTestHelper ash_test_helper;

  const AccountId kUser1 = AccountId::FromUserEmail("user1@example.com");
  const AccountId kUser2 = AccountId::FromUserEmail("user2@example.com");

  AshTestHelper::InitParams init_params;
  init_params.users.clear();
  init_params.users.push_back({.account_id = kUser1});
  init_params.users.push_back({.account_id = kUser2});
  ash_test_helper.SetUp(std::move(init_params));

  auto* session_controller = Shell::Get()->session_controller();
  EXPECT_EQ(2, session_controller->NumberOfLoggedInUsers());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_controller->GetSessionState());
  ASSERT_TRUE(session_controller->GetPrimaryUserSession());
  EXPECT_EQ(kUser1,
            session_controller->GetPrimaryUserSession()->user_info.account_id);
  EXPECT_EQ(kUser2, session_controller->GetActiveAccountId());

  ash_test_helper.TearDown();
}

}  // namespace ash
