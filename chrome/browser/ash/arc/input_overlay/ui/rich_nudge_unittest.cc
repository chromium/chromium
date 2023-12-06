// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/rich_nudge.h"

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/ui/target_view.h"

namespace arc::input_overlay {

class RichNudgeTest : public OverlayViewTestBase {
 public:
  RichNudgeTest() = default;
  ~RichNudgeTest() override = default;

  RichNudge* GetRichNudge() {
    DCHECK(controller_);
    return controller_->GetRichNudge();
  }

  bool IsRichNudgeOnTop(auto* rich_nudge) {
    DCHECK(rich_nudge);
    return rich_nudge->on_top_;
  }
};

TEST_F(RichNudgeTest, TestRichNudgeEnterExistButtonPlaceMode) {
  // Enter into the button placement mode and rich nudge shows up.
  PressAddButton();
  // Test rich nudge showing up in the button placement mode.
  auto* rich_nudge = GetRichNudge();
  EXPECT_TRUE(rich_nudge);
  EXPECT_TRUE(IsRichNudgeOnTop(rich_nudge));

  // Exit button placement mode and rich nudge closes.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  rich_nudge = GetRichNudge();
  EXPECT_FALSE(rich_nudge);
}

TEST_F(RichNudgeTest, TestRichNudgePosition) {
  // Test rich nudge flipping its position when the target view center circle
  // intersects on it.
  PressAddButton();
  auto* rich_nudge = GetRichNudge();
  EXPECT_TRUE(rich_nudge);
  EXPECT_TRUE(IsRichNudgeOnTop(rich_nudge));

  auto* target_view = GetTargetView();
  EXPECT_TRUE(target_view);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(target_view->GetBoundsInScreen().CenterPoint());

  // Simulate moving mouse to the top of the window.
  const int window_height = target_view->size().height();
  for (int i = 0; i < window_height / 2; i++) {
    event_generator->MoveMouseBy(/*x=*/0, /*y=*/-1);
  }
  rich_nudge = GetRichNudge();
  EXPECT_TRUE(rich_nudge);
  EXPECT_FALSE(IsRichNudgeOnTop(rich_nudge));

  // Simulate moving mouse to the bottom of the window.
  for (int i = 0; i < window_height; i++) {
    event_generator->MoveMouseBy(/*x=*/0, /*y=*/1);
  }
  rich_nudge = GetRichNudge();
  EXPECT_TRUE(rich_nudge);
  EXPECT_TRUE(IsRichNudgeOnTop(rich_nudge));
}

}  // namespace arc::input_overlay
