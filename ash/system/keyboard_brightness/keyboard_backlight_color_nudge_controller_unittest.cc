// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_color_nudge_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kUser1[] = "user1@test.com";
const AccountId account_id_1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);

}  // namespace

class KeyboardBacklightColorNudgeControllerTest : public AshTestBase {
 public:
  KeyboardBacklightColorNudgeControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  KeyboardBacklightColorNudgeControllerTest(
      const KeyboardBacklightColorNudgeControllerTest&) = delete;
  KeyboardBacklightColorNudgeControllerTest& operator=(
      const KeyboardBacklightColorNudgeControllerTest&) = delete;

  ~KeyboardBacklightColorNudgeControllerTest() override = default;

 protected:
  PrefService* pref_service() {
    return Shell::Get()->session_controller()->GetActivePrefService();
  }

  bool can_show_nudge() {
    return contextual_tooltip::ShouldShowNudge(
        pref_service(),
        contextual_tooltip::TooltipType::kKeyboardBacklightColor, nullptr);
  }

  KeyboardBacklightColorNudgeController controller_;
};

TEST_F(KeyboardBacklightColorNudgeControllerTest, ShowEducationNudge) {
  // Create a dummy anchor view for the bubble.
  views::View anchor_view;
  anchor_view.SetBounds(200, 200, 10, 10);

  SimulateUserLogin(account_id_1);
  EXPECT_TRUE(can_show_nudge());
  controller_.MaybeShowEducationNudge(&anchor_view);

  EXPECT_FALSE(can_show_nudge());

  // Fast forward to the next 1 day.
  task_environment()->FastForwardBy(base::Days(1));
  EXPECT_TRUE(can_show_nudge());
}

TEST_F(KeyboardBacklightColorNudgeControllerTest,
       WontShowNudgeAfterUserSelectsColor) {
  SimulateUserLogin(account_id_1);
  EXPECT_TRUE(can_show_nudge());

  controller_.SetUserPerformedAction();

  EXPECT_FALSE(can_show_nudge());

  // Fast forward to the next 1 day. Still can't show nudge.
  task_environment()->FastForwardBy(base::Days(1));
  EXPECT_FALSE(can_show_nudge());
}

}  // namespace ash
