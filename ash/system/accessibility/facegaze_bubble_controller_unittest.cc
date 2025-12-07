// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/facegaze_bubble_controller.h"

#include <string_view>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/facegaze_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

class FaceGazeBubbleControllerTest : public AshTestBase {
 public:
  FaceGazeBubbleControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FaceGazeBubbleControllerTest() override = default;
  FaceGazeBubbleControllerTest(const FaceGazeBubbleControllerTest&) = delete;
  FaceGazeBubbleControllerTest& operator=(const FaceGazeBubbleControllerTest&) =
      delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->face_gaze().SetEnabled(true);
  }

  FaceGazeBubbleController* GetController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetFaceGazeBubbleControllerForTest();
  }

  void Update(const std::u16string& text, bool is_warning) {
    GetController()->UpdateBubble(text, is_warning);
  }

  FaceGazeBubbleView* GetView() {
    return GetController()->facegaze_bubble_view_;
  }

  const raw_ptr<FaceGazeBubbleCloseView> GetCloseView() {
    return GetView()->GetCloseViewForTesting();
  }

  bool IsVisible() { return GetController()->widget_->IsVisible(); }

  std::u16string_view GetBubbleText() { return GetView()->GetTextForTesting(); }

  bool IsShowTimerRunning() { return GetController()->show_timer_.IsRunning(); }
};

TEST_F(FaceGazeBubbleControllerTest, LabelText) {
  EXPECT_FALSE(GetView());
  Update(u"Testing", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_EQ(u"Testing", GetBubbleText());

  Update(u"", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_EQ(u"", GetBubbleText());
}

TEST_F(FaceGazeBubbleControllerTest, UpdateColor) {
  EXPECT_FALSE(GetView());
  Update(u"Default", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_EQ(cros_tokens::kCrosSysSystemBaseElevatedOpaque,
            GetView()->background_color());

  Update(u"Warning", /*is_warning=*/true);
  EXPECT_TRUE(GetView());
  EXPECT_EQ(cros_tokens::kCrosSysWarningContainer,
            GetView()->background_color());

  Update(u"Default", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_EQ(cros_tokens::kCrosSysSystemBaseElevatedOpaque,
            GetView()->background_color());
}

TEST_F(FaceGazeBubbleControllerTest, AccessibleProperties) {
  Update(u"", /*is_warning=*/false);
  ui::AXNodeData data;
  GetView()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kGenericContainer);
}

TEST_F(FaceGazeBubbleControllerTest, Hide) {
  Update(u"Testing", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsVisible());

  // Simulate mouse hover event to hide the view.
  GetEventGenerator()->MoveMouseTo(
      GetView()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsVisible());
}

TEST_F(FaceGazeBubbleControllerTest, ShowAfterHide) {
  Update(u"Testing", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsVisible());

  // Simulate mouse hover event to hide the view.
  GetEventGenerator()->MoveMouseTo(
      GetView()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsVisible());

  // The view remains hidden while the timer is running.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsVisible());

  // Move mouse away from the view to ensure it's not hidden again.
  GetEventGenerator()->MoveMouseTo(-100, -100);

  // Ensure the view is shown again after a timeout has elapsed.
  task_environment()->FastForwardBy(base::Milliseconds(600));
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsVisible());
}

TEST_F(FaceGazeBubbleControllerTest, RepeatedlyHidden) {
  Update(u"Testing", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsVisible());

  // Simulate mouse hover event to hide the view.
  GetEventGenerator()->MoveMouseTo(
      GetView()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsVisible());

  // The view remains hidden while the timer is running.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsVisible());

  // The view gets hidden again because it still overlaps with the mouse
  // location.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsVisible());
}

TEST_F(FaceGazeBubbleControllerTest, UpdateWhileHidden) {
  Update(u"Testing", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsVisible());

  // Simulate mouse hover event to hide the view.
  GetEventGenerator()->MoveMouseTo(
      GetView()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsVisible());

  // The view should still be hidden, even if updated, because the timer is
  // running.
  EXPECT_TRUE(IsShowTimerRunning());
  Update(u"Hello world", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsVisible());
}

TEST_F(FaceGazeBubbleControllerTest, CloseButton) {
  Update(u"Face control active", /*is_warning=*/false);
  EXPECT_TRUE(GetCloseView());
}

TEST_F(FaceGazeBubbleControllerTest, HoverCloseButton) {
  Update(u"Testing", /*is_warning=*/false);
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsVisible());

  // Ensure that the bubble remains visible if the close button is hovered.
  GetEventGenerator()->MoveMouseTo(
      GetCloseView()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsVisible());
}

}  // namespace ash
