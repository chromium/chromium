// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "ash/shell.h"
#include "base/test/task_environment.h"
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
    testing::Test::SetUp();
    ash_test_helper_.SetUp();
  }

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

}  // namespace ash
