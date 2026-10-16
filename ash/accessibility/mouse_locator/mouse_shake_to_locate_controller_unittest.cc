// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/mouse_locator/mouse_shake_to_locate_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

constexpr int kLeftX = 100;
constexpr int kRightX = 140;
constexpr int kY = 100;
constexpr base::TimeDelta kReversalInterval = base::Milliseconds(100);

}  // namespace

// Runs through the production path: with the feature and preference enabled,
// `Shell` creates the `MouseShakeToLocateController`, which receives mouse
// moves as an accessibility event handler.
class MouseShakeToLocateControllerTest : public AshTestBase {
 public:
  MouseShakeToLocateControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(::features::kAccessibilityShakeToLocate);
  }

  void SetUp() override {
    AshTestBase::SetUp();
    SetShakeToLocatePrefEnabled(true);
    GetEventGenerator()->MoveMouseTo(gfx::Point(kLeftX, kY));
  }

 protected:
  void SetShakeToLocatePrefEnabled(bool enabled) {
    Shell::Get()->accessibility_controller()->GetActiveUserPrefs()->SetBoolean(
        prefs::kAccessibilityShakeToLocateEnabled, enabled);
  }

  // Moves the mouse back and forth so that the detector confirms
  // `reversal_count` reversals, one every `kReversalInterval`.
  void Shake(int reversal_count) {
    auto* generator = GetEventGenerator();
    // The first move anchors the stroke; each following move is a reversal.
    for (int i = 0; i <= reversal_count; ++i) {
      task_environment()->FastForwardBy(kReversalInterval);
      generator->MoveMouseTo(gfx::Point(at_right_ ? kLeftX : kRightX, kY));
      at_right_ = !at_right_;
    }
  }

  ui::CursorSize GetCursorSize() const {
    return Shell::Get()->cursor_manager()->GetCursorSize();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  bool at_right_ = false;
};

TEST_F(MouseShakeToLocateControllerTest, ShakeEnlargesThenRestoresCursor) {
  const int original_large_size =
      Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip();

  Shake(9);
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);

  Shake(1);
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kLarge);

  // Hold (250ms) and shrink (100ms), then the cursor is restored.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);
  EXPECT_EQ(Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip(),
            original_large_size);

  // Shaking again after the detector resets re-enlarges the cursor (the first
  // move re-anchors and the second sets the initial stroke direction).
  Shake(11);
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kLarge);
}

// Each shake that enlarges the cursor is recorded once, not once per reversal.
TEST_F(MouseShakeToLocateControllerTest, RecordsCursorEnlargedUserAction) {
  constexpr char kAction[] = "Accessibility.CrosCursorShakeToLocate.Enlarged";
  base::UserActionTester user_action_tester;

  Shake(9);
  EXPECT_EQ(user_action_tester.GetActionCount(kAction), 0);

  Shake(1);
  EXPECT_EQ(user_action_tester.GetActionCount(kAction), 1);

  Shake(5);
  EXPECT_EQ(user_action_tester.GetActionCount(kAction), 1);
}

TEST_F(MouseShakeToLocateControllerTest, ContinuedShakeKeepsCursorEnlarged) {
  Shake(10);
  ASSERT_EQ(GetCursorSize(), ui::CursorSize::kLarge);

  // Keep shaking at 10 reversals/s for another 2s.
  Shake(20);
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kLarge);

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);
}

// A user who already uses a large cursor gets their exact size back.
TEST_F(MouseShakeToLocateControllerTest, RestoresUserLargeCursorSize) {
  constexpr int kUserLargeCursorSize = 48;
  Shell::Get()->cursor_manager()->SetCursorSize(ui::CursorSize::kLarge);
  Shell::Get()->SetLargeCursorSizeInDip(kUserLargeCursorSize);

  Shake(10);
  EXPECT_GT(Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip(),
            kUserLargeCursorSize);

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kLarge);
  EXPECT_EQ(Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip(),
            kUserLargeCursorSize);
}

// Cancelling mid-shake restores the cursor, and the detector's later
// `OnMouseShakeEnded()` is ignored.
TEST_F(MouseShakeToLocateControllerTest, CancelEffectDuringShake) {
  const int original_large_size =
      Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip();
  Shake(10);
  ASSERT_EQ(GetCursorSize(), ui::CursorSize::kLarge);

  Shell::Get()->mouse_shake_to_locate_controller()->CancelEffect();
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);
  EXPECT_EQ(Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip(),
            original_large_size);

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);
}

// Disabling the preference cancels an active shake effect and stops observing
// future shakes until re-enabled.
TEST_F(MouseShakeToLocateControllerTest, DisablingPrefCancelsAndIgnoresShake) {
  Shake(10);
  ASSERT_EQ(GetCursorSize(), ui::CursorSize::kLarge);

  SetShakeToLocatePrefEnabled(false);
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);

  Shake(15);
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);

  SetShakeToLocatePrefEnabled(true);
  Shake(11);
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kLarge);
}

// Synthesized mouse moves do not count toward a shake.
TEST_F(MouseShakeToLocateControllerTest, SynthesizedMovesAreIgnored) {
  GetEventGenerator()->set_flags(ui::EF_IS_SYNTHESIZED);
  Shake(10);
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);
}

// Tearing down `Shell` while the shrink animation is in progress cleanly stops
// the animation and restores the cursor without crashing.
TEST_F(MouseShakeToLocateControllerTest, DestroyShellDuringShrinkAnimation) {
  Shake(10);
  ASSERT_EQ(GetCursorSize(), ui::CursorSize::kLarge);

  // Advance to the end of the 250ms hold window so the 100ms shrink animation
  // is running when `AshTestBase::TearDown()` destroys `Shell`.
  task_environment()->FastForwardBy(base::Milliseconds(250));
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kLarge);
}

// Shaking across the boundary between two displays enlarges the cursor, keeps
// it enlarged as the cursor continues crossing between displays, and restores
// the original cursor size once shaking stops.
TEST_F(MouseShakeToLocateControllerTest, ShakeAcrossDisplayBoundary) {
  UpdateDisplay("500x400,500x400");
  const int original_large_size =
      Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip();

  constexpr int kPrimaryDisplayX = 480;
  constexpr int kSecondaryDisplayX = 520;
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::Point(kPrimaryDisplayX, kY));

  // Shake back and forth across the x=500 display boundary to trigger the
  // enlarged cursor, then keep crossing the boundary while enlarged.
  bool on_secondary = false;
  for (int i = 0; i <= 10; ++i) {
    task_environment()->FastForwardBy(kReversalInterval);
    generator->MoveMouseTo(
        gfx::Point(on_secondary ? kPrimaryDisplayX : kSecondaryDisplayX, kY));
    on_secondary = !on_secondary;
  }
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kLarge);

  for (int i = 0; i < 10; ++i) {
    task_environment()->FastForwardBy(kReversalInterval);
    generator->MoveMouseTo(
        gfx::Point(on_secondary ? kPrimaryDisplayX : kSecondaryDisplayX, kY));
    on_secondary = !on_secondary;
    EXPECT_EQ(GetCursorSize(), ui::CursorSize::kLarge);
  }

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(GetCursorSize(), ui::CursorSize::kNormal);
  EXPECT_EQ(Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip(),
            original_large_size);
}

}  // namespace ash
