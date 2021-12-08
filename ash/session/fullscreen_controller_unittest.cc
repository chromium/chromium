// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/fullscreen_notification_bubble.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/window_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

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

    fullscreen_controller_ =
        Shell::Get()->session_controller()->fullscreen_controller();

    GetSessionControllerClient()->LockScreen();
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  void CreateFullscreenWindow() {
    window_ = CreateTestWindow();
    window_->SetProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
  }

  void SetFullscreenNotificationExemptList(const std::string& pattern) {
    base::Value list(base::Value::Type::LIST);
    list.Append(base::Value(pattern));
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->Set(
        prefs::kFullscreenNotificationUrlExemptList, list);
  }

  bool IsNotificationVisible() const {
    return fullscreen_controller_->bubble_for_test() &&
           fullscreen_controller_->bubble_for_test()->widget_for_test() &&
           fullscreen_controller_->bubble_for_test()
               ->widget_for_test()
               ->IsVisible();
  }

 protected:
  std::unique_ptr<aura::Window> window_;

  FullscreenController* fullscreen_controller_ = nullptr;

  raw_ptr<TestShellDelegate> test_shell_delegate_ = nullptr;
};

// Test that the notification is not shown on session lock.
TEST_F(FullscreenControllerTest, NotShowingOnLock) {
  EXPECT_FALSE(IsNotificationVisible());
}

// Test that the notification is shown on session unlock if the exempt list pref
// is unset.
TEST_F(FullscreenControllerTest, UnsetPref_ShowingOnUnlock) {
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(IsNotificationVisible());
}

// Test that the notification is shown on session unlock if the URL of the
// active window does not match any patterns from the exempt list.
TEST_F(FullscreenControllerTest, NonMatchingPref_ShowingOnUnlock) {
  SetFullscreenNotificationExemptList(kNonMatchingPattern);

  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(IsNotificationVisible());
}

// Test that the notification is not shown on session unlock if the URL of the
// active window matches a pattern from the exempt list.
TEST_F(FullscreenControllerTest, MatchingPref_NotShowingOnUnlock) {
  // Set up the URL exempt list with one matching and one non-matching pattern.
  base::Value list(base::Value::Type::LIST);
  list.Append(base::Value(kNonMatchingPattern));
  list.Append(base::Value(kMatchingPattern));
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->Set(
      prefs::kFullscreenNotificationUrlExemptList, list);

  GetSessionControllerClient()->UnlockScreen();
  EXPECT_FALSE(IsNotificationVisible());
}

// Test that the notification is not shown on session unlock if the exempt list
// includes the wildcard character.
TEST_F(FullscreenControllerTest, WildcardPref_NotShowingOnUnlock) {
  SetFullscreenNotificationExemptList(kWildcardPattern);

  GetSessionControllerClient()->UnlockScreen();
  EXPECT_FALSE(IsNotificationVisible());
}

// Test that the notification is shown on session unlock if the exempt list pref
// is unset.
TEST_F(FullscreenControllerTest, EmptyUrlUnsetPref_ShowingOnUnlock) {
  test_shell_delegate_->SetLastCommittedURLForWindow(kEmptyUrl);

  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(IsNotificationVisible());
}

// Test that the notification is not shown on session unlock if the exempt list
// includes the wildcard character.
TEST_F(FullscreenControllerTest, EmptyUrlWildcardPref_NotShowingOnUnlock) {
  test_shell_delegate_->SetLastCommittedURLForWindow(kEmptyUrl);

  SetFullscreenNotificationExemptList(kWildcardPattern);

  GetSessionControllerClient()->UnlockScreen();
  EXPECT_FALSE(IsNotificationVisible());
}

}  // namespace
}  // namespace ash
