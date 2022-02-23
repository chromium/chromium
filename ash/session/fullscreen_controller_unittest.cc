// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/window_state.h"
#include "base/run_loop.h"
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

class FullscreenControllerTest : public AshTestBase {
 public:
  FullscreenControllerTest() {}

  FullscreenControllerTest(const FullscreenControllerTest&) = delete;
  FullscreenControllerTest& operator=(const FullscreenControllerTest&) = delete;

  ~FullscreenControllerTest() override {}

  // AshTestBase:
  void SetUp() override {
    // Create a shell delegate and set the active URL.
    auto test_shell_delegate = std::make_unique<TestShellDelegate>();
    test_shell_delegate_ = test_shell_delegate.get();
    test_shell_delegate->SetLastCommittedURLForWindow(kActiveUrl);

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
    window_state_ = WindowState::Get(window_.get());
  }

  void SetKeepFullscreenWithoutNotificationAllowList(
      const std::string& pattern) {
    base::Value list(base::Value::Type::LIST);
    list.Append(base::Value(pattern));
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->Set(
        prefs::kKeepFullscreenWithoutNotificationUrlAllowList, list);
  }

 protected:
  std::unique_ptr<aura::Window> window_;

  WindowState* window_state_ = nullptr;

  raw_ptr<TestShellDelegate> test_shell_delegate_ = nullptr;
};

// Test that full screen is exited after session unlock if the allow list pref
// is unset.
TEST_F(FullscreenControllerTest, ExitFullscreenIfUnsetPref) {
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
TEST_F(FullscreenControllerTest, ExitFullscreenIfNonMatchingPref) {
  SetKeepFullscreenWithoutNotificationAllowList(kNonMatchingPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_FALSE(window_state_->IsFullscreen());
}

// Test that full screen is not exited after session unlock if the URL of the
// active window matches a pattern from the allow list.
TEST_F(FullscreenControllerTest, KeepFullscreenIfMatchingPref) {
  // Set up the URL exempt list with one matching and one non-matching pattern.
  base::Value list(base::Value::Type::LIST);
  list.Append(base::Value(kNonMatchingPattern));
  list.Append(base::Value(kMatchingPattern));
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->Set(
      prefs::kKeepFullscreenWithoutNotificationUrlAllowList, list);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_TRUE(window_state_->IsFullscreen());
}

// Test that full screen is not exited after session unlock if the allow list
// includes the wildcard character.
TEST_F(FullscreenControllerTest, KeepFullscreenIfWildcardPref) {
  SetKeepFullscreenWithoutNotificationAllowList(kWildcardPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_TRUE(window_state_->IsFullscreen());
}

// Test that full screen is exited after session unlock if the URL is not
// available.
TEST_F(FullscreenControllerTest, ExitFullscreenIfUnsetUrlUnsetPref) {
  test_shell_delegate_->SetLastCommittedURLForWindow(kEmptyUrl);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_FALSE(window_state_->IsFullscreen());
}

// Test that full screen is not exited after session unlock if the allow list
// includes the wildcard character and the URL is not available.
TEST_F(FullscreenControllerTest, KeepFullscreenIfUnsetUrlWildcardPref) {
  test_shell_delegate_->SetLastCommittedURLForWindow(kEmptyUrl);

  SetKeepFullscreenWithoutNotificationAllowList(kWildcardPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_TRUE(window_state_->IsFullscreen());
}

}  // namespace
}  // namespace ash
