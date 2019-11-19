// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/first_run_helper_impl.h"

#include "ash/first_run/desktop_cleaner.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class FirstRunHelperTest : public AshTestBase {
 public:
  FirstRunHelperTest() = default;
  ~FirstRunHelperTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    CheckContainersAreVisible();
    helper_ = FirstRunHelper::Start(base::BindOnce(
        &FirstRunHelperTest::OnCancelled, base::Unretained(this)));
  }

  void TearDown() override {
    helper_.reset();
    CheckContainersAreVisible();
    AshTestBase::TearDown();
  }

  void CheckContainersAreVisible() const {
    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    std::vector<int> containers_to_check =
        DesktopCleaner::GetContainersToHideForTest();
    for (size_t i = 0; i < containers_to_check.size(); ++i) {
      aura::Window* container =
          Shell::GetContainer(root_window, containers_to_check[i]);
      EXPECT_TRUE(container->IsVisible());
    }
  }

  void CheckContainersAreHidden() const {
    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    std::vector<int> containers_to_check =
        DesktopCleaner::GetContainersToHideForTest();
    for (size_t i = 0; i < containers_to_check.size(); ++i) {
      aura::Window* container =
          Shell::GetContainer(root_window, containers_to_check[i]);
      EXPECT_TRUE(!container->IsVisible());
    }
  }

  FirstRunHelper* helper() { return helper_.get(); }

  int cancelled_times() const { return cancelled_times_; }

 private:
  void OnCancelled() { ++cancelled_times_; }

  std::unique_ptr<FirstRunHelper> helper_;
  int cancelled_times_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelperTest);
};

// This test creates helper, checks that containers are hidden and then
// destructs helper.
TEST_F(FirstRunHelperTest, ContainersAreHidden) {
  CheckContainersAreHidden();
}

// Tests that screen lock cancels the tutorial.
TEST_F(FirstRunHelperTest, ScreenLock) {
  GetSessionControllerClient()->LockScreen();
  EXPECT_EQ(cancelled_times(), 1);
}

// Tests that shutdown cancels the tutorial.
TEST_F(FirstRunHelperTest, ChromeTerminating) {
  Shell::Get()->session_controller()->NotifyChromeTerminating();
  EXPECT_EQ(cancelled_times(), 1);
}

}  // namespace ash
