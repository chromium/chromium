// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_controller.h"

#include <memory>

#include "ash/constants/app_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/window_state.h"
#include "base/run_loop.h"
#include "chromeos/ui/wm/fullscreen/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

const GURL kActiveUrl = GURL("https://wwww.test.com");
const GURL kEmptyUrl = GURL::EmptyGURL();

constexpr char kNonMatchingPattern[] = "google.com";
constexpr char kMatchingPattern[] = "test.com";
constexpr char kWildcardPattern[] = "*";

class FullscreenControllerTest : public AshTestBase,
                                 public testing::WithParamInterface<bool> {
 public:
  FullscreenControllerTest() {}

  FullscreenControllerTest(const FullscreenControllerTest&) = delete;
  FullscreenControllerTest& operator=(const FullscreenControllerTest&) = delete;

  ~FullscreenControllerTest() override {}

  // AshTestBase:
  void SetUp() override {
    // Create a test shell delegate which can return fake responses.
    auto test_shell_delegate = std::make_unique<TestShellDelegate>();
    test_shell_delegate_ = test_shell_delegate.get();

    AshTestBase::SetUp(std::move(test_shell_delegate));

    CreateFullscreenWindow();
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  void CreateFullscreenWindow() {
    window_ = CreateTestWindow();
    window_->SetProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
    if (is_lacros_window_) {
      window_->SetProperty(aura::client::kAppType,
                           static_cast<int>(AppType::LACROS));
    }
    window_state_ = WindowState::Get(window_.get());
  }

  void SetKeepFullscreenWithoutNotificationAllowList(
      const std::string& pattern) {
    base::Value::List list;
    list.Append(pattern);
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetList(
        chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList,
        std::move(list));
  }

  void SetUpShellDelegate(bool should_exit_fullscreen, GURL url = kActiveUrl) {
    // The shell delegate will only retrieve the active URL for ash-chrome
    // windows and return the empty URL for lacros-chrome windows.
    if (is_lacros_window_) {
      test_shell_delegate_->SetLastCommittedURLForWindow(kEmptyUrl);
      test_shell_delegate_->SetShouldExitFullscreenBeforeLock(
          should_exit_fullscreen);
    } else {
      test_shell_delegate_->SetLastCommittedURLForWindow(url);
    }
  }

 protected:
  bool is_lacros_window_ = GetParam();

  std::unique_ptr<aura::Window> window_;

  WindowState* window_state_ = nullptr;

  raw_ptr<TestShellDelegate> test_shell_delegate_ = nullptr;
};

// Test that full screen is exited after session unlock if the allow list pref
// is unset.
TEST_P(FullscreenControllerTest, ExitFullscreenIfUnsetPref) {
  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_FALSE(window_state_->IsFullscreen());
}

// Test that full screen is exited after session unlock if the URL of the active
// window does not match any patterns from the allow list.
TEST_P(FullscreenControllerTest, ExitFullscreenIfNonMatchingPref) {
  bool should_exit_fullscreen = true;
  SetUpShellDelegate(should_exit_fullscreen);

  SetKeepFullscreenWithoutNotificationAllowList(kNonMatchingPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_EQ(window_state_->IsFullscreen(), !should_exit_fullscreen);
}

// Test that full screen is not exited after session unlock if the URL of the
// active window matches a pattern from the allow list.
TEST_P(FullscreenControllerTest, KeepFullscreenIfMatchingPref) {
  bool should_exit_fullscreen = false;
  SetUpShellDelegate(should_exit_fullscreen);

  // Set up the URL exempt list with one matching and one non-matching pattern.
  base::Value::List list;
  list.Append(kNonMatchingPattern);
  list.Append(kMatchingPattern);
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetList(
      chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList,
      std::move(list));

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_EQ(window_state_->IsFullscreen(), !should_exit_fullscreen);
}

// Test that full screen is not exited after session unlock if the allow list
// includes the wildcard character.
TEST_P(FullscreenControllerTest, KeepFullscreenIfWildcardPref) {
  bool should_exit_fullscreen = false;
  SetUpShellDelegate(should_exit_fullscreen);

  SetKeepFullscreenWithoutNotificationAllowList(kWildcardPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_EQ(window_state_->IsFullscreen(), !should_exit_fullscreen);
}

// Test that full screen is exited after session unlock if the URL is not
// available.
TEST_P(FullscreenControllerTest, ExitFullscreenIfUnsetUrlUnsetPref) {
  bool should_exit_fullscreen = true;
  SetUpShellDelegate(should_exit_fullscreen, kEmptyUrl);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_EQ(window_state_->IsFullscreen(), !should_exit_fullscreen);
}

// Test that full screen is not exited after session unlock if the allow list
// includes the wildcard character and the URL is not available.
TEST_P(FullscreenControllerTest, KeepFullscreenIfUnsetUrlWildcardPref) {
  bool should_exit_fullscreen = false;
  SetUpShellDelegate(should_exit_fullscreen, kEmptyUrl);

  SetKeepFullscreenWithoutNotificationAllowList(kWildcardPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_EQ(window_state_->IsFullscreen(), !should_exit_fullscreen);
}

INSTANTIATE_TEST_SUITE_P(All, FullscreenControllerTest, testing::Bool());

}  // namespace
}  // namespace ash
