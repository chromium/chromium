// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_welcome_bubble.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@palettewelcome.com";
constexpr char kUser2Email[] = "user2@palettewelcome.com";
constexpr char kGuestEmail[] = "guest@palettewelcome.com";
constexpr char kPublicAccountEmail[] = "public@palettewelcome.com";

}  // namespace

class PaletteWelcomeBubbleTest : public AshTestBase {
 public:
  PaletteWelcomeBubbleTest() = default;
  ~PaletteWelcomeBubbleTest() override = default;

  PrefService* user1_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  PrefService* user2_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser2Email));
  }

  void ShowBubble() { welcome_bubble_->Show(); }
  void HideBubble() { welcome_bubble_->Hide(); }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    welcome_bubble_ = std::make_unique<PaletteWelcomeBubble>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->palette_tray());
    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  void TearDown() override {
    welcome_bubble_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<PaletteWelcomeBubble> welcome_bubble_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaletteWelcomeBubbleTest);
};

// Test the basic Show/Hide functions work.
TEST_F(PaletteWelcomeBubbleTest, Basic) {
  EXPECT_FALSE(welcome_bubble_->GetBubbleViewForTesting());

  ShowBubble();
  EXPECT_TRUE(welcome_bubble_->GetBubbleViewForTesting());

  HideBubble();
  EXPECT_FALSE(welcome_bubble_->GetBubbleViewForTesting());

  // Verify that the pref changes after the bubble is hidden.
  EXPECT_TRUE(
      user1_pref_service()->GetBoolean(prefs::kShownPaletteWelcomeBubble));
}

// Verify if the bubble has been show before, it will not be shown again.
TEST_F(PaletteWelcomeBubbleTest, ShowIfNeeded) {
  user1_pref_service()->SetBoolean(prefs::kShownPaletteWelcomeBubble, true);

  welcome_bubble_->ShowIfNeeded();
  EXPECT_FALSE(welcome_bubble_->GetBubbleViewForTesting());
}

// Verify that tapping on the screen outside of the welcome bubble closes the
// bubble.
TEST_F(PaletteWelcomeBubbleTest, TapOutsideOfBubble) {
  ShowBubble();
  ASSERT_TRUE(welcome_bubble_->GetBubbleViewForTesting());
  auto bounds = welcome_bubble_->GetBubbleViewForTesting()->GetBoundsInScreen();
  ASSERT_FALSE(bounds.IsEmpty());

  // The bubble remains open if a tap occurs on the bubble.
  GetEventGenerator()->set_current_screen_location(bounds.CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(welcome_bubble_->GetBubbleViewForTesting());

  // Tap anywhere outside the bubble.
  ASSERT_FALSE(bounds.Contains(gfx::Point()));
  GetEventGenerator()->set_current_screen_location(gfx::Point());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(welcome_bubble_->GetBubbleViewForTesting());
}

// Verify that a second user sees the bubble even after a first user has seen it
// already.
TEST_F(PaletteWelcomeBubbleTest, BubbleShownForSecondUser) {
  // Show the bubble for the first user, and verify that the pref is set when
  // the bubble is hidden.
  ShowBubble();
  EXPECT_TRUE(welcome_bubble_->GetBubbleViewForTesting());
  HideBubble();
  ASSERT_TRUE(
      user1_pref_service()->GetBoolean(prefs::kShownPaletteWelcomeBubble));

  // Switch to the second user, and verify that the bubble will get shown.
  GetSessionControllerClient()->SwitchActiveUser(
      AccountId::FromUserEmail(kUser2Email));
  EXPECT_FALSE(
      user2_pref_service()->GetBoolean(prefs::kShownPaletteWelcomeBubble));
  welcome_bubble_->ShowIfNeeded();
  EXPECT_TRUE(welcome_bubble_->GetBubbleViewForTesting());
}

TEST_F(PaletteWelcomeBubbleTest, BubbleNotShownInactiveSession) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  welcome_bubble_->ShowIfNeeded();
  EXPECT_FALSE(welcome_bubble_->GetBubbleViewForTesting());
}

using PaletteWelcomeBubbleEmphemeralAccountTest = AshTestBase;

TEST_F(PaletteWelcomeBubbleEmphemeralAccountTest, BubbleNotShownForGuest) {
  auto welcome_bubble = std::make_unique<PaletteWelcomeBubble>(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->palette_tray());
  GetSessionControllerClient()->AddUserSession(kGuestEmail,
                                               user_manager::USER_TYPE_GUEST);
  GetSessionControllerClient()->SwitchActiveUser(
      AccountId::FromUserEmail(kGuestEmail));
  welcome_bubble->ShowIfNeeded();
  EXPECT_FALSE(welcome_bubble->GetBubbleViewForTesting());
}

TEST_F(PaletteWelcomeBubbleEmphemeralAccountTest,
       BubbleNotShownForPublicAccount) {
  auto welcome_bubble = std::make_unique<PaletteWelcomeBubble>(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->palette_tray());
  GetSessionControllerClient()->AddUserSession(
      kPublicAccountEmail, user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  GetSessionControllerClient()->SwitchActiveUser(
      AccountId::FromUserEmail(kPublicAccountEmail));
  welcome_bubble->ShowIfNeeded();
  EXPECT_FALSE(welcome_bubble->GetBubbleViewForTesting());
}

}  // namespace ash
