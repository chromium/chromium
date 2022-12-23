// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/note_action_launch_button.h"

#include <memory>
#include <vector>

#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "ash/shell.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The note action button bubble sizes:
constexpr int kLargeButtonRadiusDp = 56;
constexpr int kSmallButtonRadiusDp = 48;

constexpr float kSqrt2 = 1.4142;

}  // namespace

class NoteActionLaunchButtonTest : public LoginTestBase {
 public:
  NoteActionLaunchButtonTest() = default;

  NoteActionLaunchButtonTest(const NoteActionLaunchButtonTest&) = delete;
  NoteActionLaunchButtonTest& operator=(const NoteActionLaunchButtonTest&) =
      delete;

  ~NoteActionLaunchButtonTest() override = default;

  void SetUp() override {
    set_start_session(true);
    LoginTestBase::SetUp();

    Shell::Get()->tray_action()->SetClient(
        tray_action_client_.CreateRemoteAndBind(),
        mojom::TrayActionState::kAvailable);
  }

  TestTrayActionClient* tray_action_client() { return &tray_action_client_; }

  void PerformClick(const gfx::Point& point) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(point.x(), point.y());
    generator->ClickLeftButton();

    Shell::Get()->tray_action()->FlushMojoForTesting();
  }

  void GestureFling(const gfx::Point& start, const gfx::Point& end) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->GestureScrollSequence(start, end, base::Milliseconds(10), 2);

    Shell::Get()->tray_action()->FlushMojoForTesting();
  }

 private:
  TestTrayActionClient tray_action_client_;
};

// Verifies that note action button is not visible if lock screen note taking
// is not enabled.
TEST_F(NoteActionLaunchButtonTest, VisibilityActionNotAvailable) {
  auto note_action_button = std::make_unique<NoteActionLaunchButton>(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(note_action_button->GetVisible());
}

// Verifies that note action button is shown and enabled if lock screen note
// taking is available.
TEST_F(NoteActionLaunchButtonTest, VisibilityActionAvailable) {
  auto note_action_button = std::make_unique<NoteActionLaunchButton>(
      mojom::TrayActionState::kAvailable);
  NoteActionLaunchButton::TestApi test_api(note_action_button.get());

  EXPECT_TRUE(note_action_button->GetVisible());
  EXPECT_TRUE(note_action_button->GetEnabled());

  EXPECT_TRUE(test_api.ActionButtonView()->GetVisible());
  EXPECT_TRUE(test_api.ActionButtonView()->GetEnabled());
  EXPECT_TRUE(test_api.BackgroundView()->GetVisible());
}

// Tests that clicking Enter while lock screen action button is focused requests
// a new note action.
TEST_F(NoteActionLaunchButtonTest, KeyboardTest) {
  auto* note_action_button =
      new NoteActionLaunchButton(mojom::TrayActionState::kAvailable);
  std::unique_ptr<views::Widget> widget =
      CreateWidgetWithContent(login_views_utils::WrapViewForPreferredSize(
                                  base::WrapUnique(note_action_button))
                                  .release());
  NoteActionLaunchButton::TestApi test_api(note_action_button);

  note_action_button->RequestFocus();
  // Focusing the whole note action launch button view should give the image
  // button sub-view the focus.
  EXPECT_TRUE(test_api.ActionButtonView()->HasFocus());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  Shell::Get()->tray_action()->FlushMojoForTesting();

  EXPECT_EQ(std::vector<mojom::LockScreenNoteOrigin>(
                {mojom::LockScreenNoteOrigin::kLockScreenButtonKeyboard}),
            tray_action_client()->note_origins());
}

// The button hit area is expected to be a circle centered in the top right
// corner of the view with kSmallButtonRadiusDp (and clipped but the view
// bounds). The test verifies clicking the button within the button's hit area
// requests a new note action.
TEST_F(NoteActionLaunchButtonTest, ClickTest) {
  auto* note_action_button =
      new NoteActionLaunchButton(mojom::TrayActionState::kAvailable);
  std::unique_ptr<views::Widget> widget =
      CreateWidgetWithContent(login_views_utils::WrapViewForPreferredSize(
                                  base::WrapUnique(note_action_button))
                                  .release());

  const gfx::Size action_size = note_action_button->GetPreferredSize();
  EXPECT_EQ(gfx::Size(kLargeButtonRadiusDp, kLargeButtonRadiusDp), action_size);

  const gfx::Rect view_bounds = note_action_button->GetBoundsInScreen();
  ASSERT_EQ(gfx::Rect(gfx::Point(), action_size), view_bounds);
  const std::vector<mojom::LockScreenNoteOrigin> expected_actions = {
      mojom::LockScreenNoteOrigin::kLockScreenButtonTap};

  // Point near the center of the view, inside the actionable area:
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-kSmallButtonRadiusDp / kSqrt2 + 2,
                             kSmallButtonRadiusDp / kSqrt2 - 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the center of the view, outside the actionable area:
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-kSmallButtonRadiusDp / kSqrt2 - 2,
                             kSmallButtonRadiusDp / kSqrt2 + 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the top right corner:
  PerformClick(view_bounds.top_right() + gfx::Vector2d(-2, 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom left corner:
  PerformClick(view_bounds.bottom_left() + gfx::Vector2d(2, -2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the origin:
  PerformClick(view_bounds.origin() + gfx::Vector2d(2, 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the origin of the actionable area bounds (inside the bounds):
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-kSmallButtonRadiusDp + 2, 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the origin of the actionable area bounds (outside the bounds):
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-kSmallButtonRadiusDp - 2, 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom right corner:
  PerformClick(view_bounds.bottom_right() + gfx::Vector2d(0, -2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom right corner of the actionable area bounds (inside
  // the bounds):
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-2, kSmallButtonRadiusDp - 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom right corner of the actionable area bounds (outside
  // the bounds):
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-2, kSmallButtonRadiusDp + 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom edge:
  PerformClick(view_bounds.bottom_left() +
               gfx::Vector2d(kSmallButtonRadiusDp / 2, -1));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the top edge:
  PerformClick(view_bounds.origin() +
               gfx::Vector2d(kSmallButtonRadiusDp / 2, 1));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the left edge:
  PerformClick(view_bounds.origin() +
               gfx::Vector2d(1, kSmallButtonRadiusDp / 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the right edge:
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-1, kSmallButtonRadiusDp / 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point in the center of the actionable area:
  PerformClick(
      view_bounds.top_right() +
      gfx::Vector2d(-kSmallButtonRadiusDp / 2, kSmallButtonRadiusDp / 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point outside the view bounds:
  PerformClick(view_bounds.top_right() + gfx::Vector2d(2, 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();
}

// Tests tap gesture in and outside of the note action launch button.
TEST_F(NoteActionLaunchButtonTest, TapTest) {
  auto* note_action_button =
      new NoteActionLaunchButton(mojom::TrayActionState::kAvailable);
  std::unique_ptr<views::Widget> widget =
      CreateWidgetWithContent(login_views_utils::WrapViewForPreferredSize(
                                  base::WrapUnique(note_action_button))
                                  .release());

  const gfx::Size action_size = note_action_button->GetPreferredSize();
  EXPECT_EQ(gfx::Size(kLargeButtonRadiusDp, kLargeButtonRadiusDp), action_size);

  const gfx::Rect view_bounds = note_action_button->GetBoundsInScreen();
  ASSERT_EQ(gfx::Rect(gfx::Point(), action_size), view_bounds);
  const std::vector<mojom::LockScreenNoteOrigin> expected_actions = {
      mojom::LockScreenNoteOrigin::kLockScreenButtonTap};

  ui::test::EventGenerator* generator = GetEventGenerator();
  // Tap in actionable area of the button requests action:
  generator->GestureTapAt(view_bounds.top_right() +
                          gfx::Vector2d(-kSmallButtonRadiusDp / kSqrt2 + 2,
                                        kSmallButtonRadiusDp / kSqrt2 - 2));
  Shell::Get()->tray_action()->FlushMojoForTesting();
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Tap in non-actionable area of the button does not request action:
  generator->GestureTapAt(view_bounds.top_right() +
                          gfx::Vector2d(-kSmallButtonRadiusDp / kSqrt2 - 2,
                                        kSmallButtonRadiusDp / kSqrt2 + 2));
  Shell::Get()->tray_action()->FlushMojoForTesting();
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();
}

// Tests a number of fling gestures that interact with the note action button.
// Verifies that only a fling from the button's actionable area to bottom right
// direction generate an action request.
TEST_F(NoteActionLaunchButtonTest, FlingGesture) {
  auto* note_action_button =
      new NoteActionLaunchButton(mojom::TrayActionState::kAvailable);
  std::unique_ptr<views::Widget> widget =
      CreateWidgetWithContent(login_views_utils::WrapViewForPreferredSize(
                                  base::WrapUnique(note_action_button))
                                  .release());

  const gfx::Size action_size = note_action_button->GetPreferredSize();
  EXPECT_EQ(gfx::Size(kLargeButtonRadiusDp, kLargeButtonRadiusDp), action_size);

  // Offset note action button closer to the center of the test widget, to give
  // extra space for performing gestures.
  gfx::Rect view_bounds = note_action_button->GetBoundsInScreen();
  view_bounds.Offset(200, 200);
  note_action_button->SetBoundsRect(view_bounds);

  ASSERT_EQ(gfx::Rect(gfx::Point(200, 200), action_size),
            note_action_button->GetBoundsInScreen());

  const std::vector<mojom::LockScreenNoteOrigin> expected_actions = {
      mojom::LockScreenNoteOrigin::kLockScreenButtonSwipe};

  // Point in the center of the note action element's actionable area:
  gfx::Point start =
      view_bounds.top_right() +
      gfx::Vector2d(-kSmallButtonRadiusDp / 2, kSmallButtonRadiusDp / 2);

  // Fling from the center of the actionable area to bottom left:
  GestureFling(start, view_bounds.bottom_left() + gfx::Vector2d(-50, 50));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Fling from the center of the actionable area to bottom right:
  GestureFling(start, view_bounds.bottom_right() + gfx::Vector2d(0, 50));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());

  // Fling from the center of the actionable area to top left:
  GestureFling(start, view_bounds.origin() + gfx::Vector2d(-50, 0));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());

  // Fling accross the button:
  GestureFling(view_bounds.top_right() + gfx::Vector2d(25, -25),
               view_bounds.bottom_left() + gfx::Vector2d(-25, 25));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());

  // Fling from non-actionable area of the button:
  GestureFling(view_bounds.top_right() +
                   gfx::Vector2d(-kSmallButtonRadiusDp / kSqrt2 - 2,
                                 kSmallButtonRadiusDp / kSqrt2 + 2),
               view_bounds.bottom_left() + gfx::Vector2d(-25, 25));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
}

// Generates multi-finger fling in the direction that would be accepted for
// single finger fling, and verifies no action is requested.
TEST_F(NoteActionLaunchButtonTest, MultiFingerFling) {
  auto* note_action_button =
      new NoteActionLaunchButton(mojom::TrayActionState::kAvailable);
  std::unique_ptr<views::Widget> widget =
      CreateWidgetWithContent(login_views_utils::WrapViewForPreferredSize(
                                  base::WrapUnique(note_action_button))
                                  .release());

  const gfx::Size action_size = note_action_button->GetPreferredSize();
  EXPECT_EQ(gfx::Size(kLargeButtonRadiusDp, kLargeButtonRadiusDp), action_size);

  // Offset note action button closer to the center of the test widget, to give
  // extra space for performing gestures:
  gfx::Rect view_bounds = note_action_button->GetBoundsInScreen();
  view_bounds.Offset(200, 200);
  note_action_button->SetBoundsRect(view_bounds);

  ASSERT_EQ(gfx::Rect(gfx::Point(200, 200), action_size),
            note_action_button->GetBoundsInScreen());

  const int kTouchPoints = 3;
  const gfx::Point start_points[kTouchPoints] = {
      view_bounds.top_right() + gfx::Vector2d(-2, 2),
      view_bounds.top_right() + gfx::Vector2d(-20, 15),
      view_bounds.top_right() + gfx::Vector2d(-35, 40)};
  const gfx::Vector2d deltas[kTouchPoints] = {gfx::Vector2d(-100, 100),
                                              gfx::Vector2d(-100, 100),
                                              gfx::Vector2d(-100, 100)};
  int delays_adding_fingers_ms[kTouchPoints] = {0, 4, 8};
  int delays_releasing_fingers_ms[kTouchPoints] = {20, 16, 18};

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureMultiFingerScrollWithDelays(
      kTouchPoints, start_points, deltas, delays_adding_fingers_ms,
      delays_releasing_fingers_ms, 4 /* event_separaation_time_ms*/,
      5 /*steps*/);

  Shell::Get()->tray_action()->FlushMojoForTesting();
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
}
}  // namespace ash
