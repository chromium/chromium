// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

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
  ~AshTestHelperTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    ash_test_helper_ = std::make_unique<AshTestHelper>();
    AshTestHelper::InitParams init_params;
    ash_test_helper_->SetUp(std::move(init_params));
  }

  void TearDown() override {
    ash_test_helper_->TearDown();
    testing::Test::TearDown();
  }

  AshTestHelper* ash_test_helper() { return ash_test_helper_.get(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  std::unique_ptr<AshTestHelper> ash_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshTestHelperTest);
};

// Ensure that we have initialized enough of Ash to create and show a window.
TEST_F(AshTestHelperTest, AshTestHelper) {
  // Check initial state.
  EXPECT_TRUE(ash_test_helper()->test_shell_delegate());
  EXPECT_TRUE(ash_test_helper()->CurrentContext());

  // Enough state is initialized to create a window.
  using views::Widget;
  std::unique_ptr<Widget> w1(new Widget);
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = ash_test_helper()->CurrentContext();
  w1->Init(std::move(params));
  w1->Show();
  EXPECT_TRUE(w1->IsActive());
  EXPECT_TRUE(w1->IsVisible());
}

}  // namespace ash
