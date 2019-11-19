// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller.h"

#include <memory>

#include "ash/components/fast_ink/fast_ink_points.h"
#include "ash/highlighter/highlighter_controller_test_api.h"
#include "ash/shell.h"
#include "ash/system/palette/mock_palette_tool_delegate.h"
#include "ash/system/palette/palette_tool.h"
#include "ash/system/palette/tools/metalayer_mode.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class TestHighlighterObserver : public HighlighterController::Observer {
 public:
  TestHighlighterObserver() = default;
  ~TestHighlighterObserver() override = default;

  // HighlighterController::Observer:
  void OnHighlighterEnabledChanged(HighlighterEnabledState state) override {
    switch (state) {
      case HighlighterEnabledState::kEnabled:
        ++enabled_count_;
        break;
      case HighlighterEnabledState::kDisabledByUser:
        ++disabled_by_user_count_;
        break;
      case HighlighterEnabledState::kDisabledBySessionAbort:
        ++disabled_by_session_abort_;
        break;
      case HighlighterEnabledState::kDisabledBySessionComplete:
        ++disabled_by_session_complete_;
        break;
    }
  }

  void OnHighlighterSelectionRecognized(const gfx::Rect& rect) override {
    last_recognized_rect_ = rect;
  }

  int enabled_count_ = 0;
  int disabled_by_user_count_ = 0;
  int disabled_by_session_abort_ = 0;
  int disabled_by_session_complete_ = 0;
  gfx::Rect last_recognized_rect_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestHighlighterObserver);
};

class HighlighterControllerTest : public AshTestBase {
 public:
  HighlighterControllerTest() = default;
  ~HighlighterControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = Shell::Get()->highlighter_controller();
    controller_test_api_ =
        std::make_unique<HighlighterControllerTestApi>(controller_);

    palette_tool_delegate_ = std::make_unique<MockPaletteToolDelegate>();
    tool_ = std::make_unique<MetalayerMode>(palette_tool_delegate_.get());
  }

  void TearDown() override {
    tool_.reset();
    // This needs to be called first to reset the controller state before the
    // shell instance gets torn down.
    controller_test_api_.reset();
    AshTestBase::TearDown();
  }

  void UpdateDisplayAndWaitForCompositingEnded(
      const std::string& display_specs) {
    UpdateDisplay(display_specs);
    ui::DrawWaiterForTest::WaitForCompositingEnded(
        Shell::GetPrimaryRootWindow()->GetHost()->compositor());
  }

 protected:
  void TraceRect(const gfx::Rect& rect) {
    ui::test::EventGenerator* event_generator = GetEventGenerator();
    event_generator->MoveTouch(gfx::Point(rect.x(), rect.y()));
    event_generator->PressTouch();
    event_generator->MoveTouch(gfx::Point(rect.right(), rect.y()));
    event_generator->MoveTouch(gfx::Point(rect.right(), rect.bottom()));
    event_generator->MoveTouch(gfx::Point(rect.x(), rect.bottom()));
    event_generator->MoveTouch(gfx::Point(rect.x(), rect.y()));
    event_generator->ReleaseTouch();

    // The the events above will trigger a frame, so wait until a new
    // CompositorFrame is generated before terminating.
    ui::DrawWaiterForTest::WaitForCompositingEnded(
        Shell::GetPrimaryRootWindow()->GetHost()->compositor());
  }

  std::unique_ptr<HighlighterControllerTestApi> controller_test_api_;
  std::unique_ptr<MockPaletteToolDelegate> palette_tool_delegate_;
  std::unique_ptr<PaletteTool> tool_;

  HighlighterController* controller_ = nullptr;  // Not owned.

 private:
  DISALLOW_COPY_AND_ASSIGN(HighlighterControllerTest);
};

}  // namespace

// Test to ensure the class responsible for drawing the highlighter pointer
// receives points from stylus movements as expected.
TEST_F(HighlighterControllerTest, HighlighterRenderer) {
  // The highlighter pointer mode only works with stylus.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  // When disabled the highlighter pointer should not be showing.
  event_generator->MoveTouch(gfx::Point(1, 1));
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify that by enabling the mode, the highlighter pointer should still not
  // be showing.
  controller_test_api_->SetEnabled(true);
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify moving the stylus 4 times will not display the highlighter pointer.
  event_generator->MoveTouch(gfx::Point(2, 2));
  event_generator->MoveTouch(gfx::Point(3, 3));
  event_generator->MoveTouch(gfx::Point(4, 4));
  event_generator->MoveTouch(gfx::Point(5, 5));
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify pressing the stylus will show the highlighter pointer and add a
  // point but will not activate fading out.
  event_generator->PressTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());
  EXPECT_FALSE(controller_test_api_->IsFadingAway());
  EXPECT_EQ(1, controller_test_api_->points().GetNumberOfPoints());

  // Verify dragging the stylus 2 times will add 2 more points.
  event_generator->MoveTouch(gfx::Point(6, 6));
  event_generator->MoveTouch(gfx::Point(7, 7));
  EXPECT_EQ(3, controller_test_api_->points().GetNumberOfPoints());

  // Verify releasing the stylus still shows the highlighter pointer, which is
  // fading away.
  event_generator->ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());
  EXPECT_TRUE(controller_test_api_->IsFadingAway());

  // Verify that disabling the mode right after the gesture completion does not
  // hide the highlighter pointer immediately but lets it play out the
  // animation.
  controller_test_api_->SetEnabled(false);
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());
  EXPECT_TRUE(controller_test_api_->IsFadingAway());

  // Verify that disabling the mode mid-gesture hides the highlighter pointer
  // immediately.
  controller_test_api_->DestroyPointerView();
  controller_test_api_->SetEnabled(true);
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(6, 6));
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());
  controller_test_api_->SetEnabled(false);
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify that the highlighter pointer does not add points while disabled.
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(8, 8));
  event_generator->ReleaseTouch();
  event_generator->MoveTouch(gfx::Point(9, 9));
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());

  // Verify that the highlighter pointer does not get shown if points are not
  // coming from the stylus, even when enabled.
  event_generator->ExitPenPointerMode();
  controller_test_api_->SetEnabled(true);
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(10, 10));
  event_generator->MoveTouch(gfx::Point(11, 11));
  EXPECT_FALSE(controller_test_api_->IsShowingHighlighter());
  event_generator->ReleaseTouch();
}

// Test to ensure the class responsible for drawing the highlighter pointer
// handles prediction as expected when it receives points from stylus movements.
TEST_F(HighlighterControllerTest, HighlighterPrediction) {
  controller_test_api_->SetEnabled(true);
  // The highlighter pointer mode only works with stylus.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();
  event_generator->PressTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingHighlighter());

  EXPECT_EQ(1, controller_test_api_->points().GetNumberOfPoints());
  // Initial press event shouldn't generate any predicted points as there's no
  // history to use for prediction.
  EXPECT_EQ(0, controller_test_api_->predicted_points().GetNumberOfPoints());

  // Verify dragging the stylus 3 times will add some predicted points.
  event_generator->MoveTouch(gfx::Point(10, 10));
  event_generator->MoveTouch(gfx::Point(20, 20));
  event_generator->MoveTouch(gfx::Point(30, 30));
  EXPECT_NE(0, controller_test_api_->predicted_points().GetNumberOfPoints());
  // Verify predicted points are in the right direction.
  for (const auto& point : controller_test_api_->predicted_points().points()) {
    EXPECT_LT(30, point.location.x());
    EXPECT_LT(30, point.location.y());
  }
}

// Test that stylus gestures are correctly recognized by HighlighterController.
TEST_F(HighlighterControllerTest, HighlighterGestures) {
  controller_test_api_->SetEnabled(true);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  TestHighlighterObserver observer;
  controller_->AddObserver(&observer);

  // A non-horizontal stroke is not recognized
  controller_test_api_->ResetSelection();
  event_generator->MoveTouch(gfx::Point(100, 100));
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(200, 200));
  event_generator->ReleaseTouch();
  EXPECT_FALSE(controller_test_api_->HandleSelectionCalled());

  // An almost horizontal stroke is recognized
  controller_test_api_->ResetSelection();
  event_generator->MoveTouch(gfx::Point(100, 100));
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(300, 102));
  event_generator->ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());

  // Horizontal stroke selection rectangle should:
  //   have the same horizontal center line as the stroke bounding box,
  //   be 4dp wider than the stroke bounding box,
  //   be exactly 14dp high.
  gfx::Rect expected_rect(98, 94, 204, 14);
  EXPECT_EQ(expected_rect, controller_test_api_->selection());
  EXPECT_EQ(expected_rect, observer.last_recognized_rect_);

  // An insufficiently closed C-like shape is not recognized
  controller_test_api_->ResetSelection();
  event_generator->MoveTouch(gfx::Point(100, 0));
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(0, 0));
  event_generator->MoveTouch(gfx::Point(0, 100));
  event_generator->MoveTouch(gfx::Point(100, 100));
  event_generator->ReleaseTouch();
  EXPECT_FALSE(controller_test_api_->HandleSelectionCalled());

  // An almost closed G-like shape is recognized
  controller_test_api_->ResetSelection();
  event_generator->MoveTouch(gfx::Point(200, 0));
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(0, 0));
  event_generator->MoveTouch(gfx::Point(0, 100));
  event_generator->MoveTouch(gfx::Point(200, 100));
  event_generator->MoveTouch(gfx::Point(200, 20));
  event_generator->ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
  expected_rect = gfx::Rect(0, 0, 200, 100);
  EXPECT_EQ(expected_rect, controller_test_api_->selection());
  EXPECT_EQ(expected_rect, observer.last_recognized_rect_);

  // A closed diamond shape is recognized
  controller_test_api_->ResetSelection();
  event_generator->MoveTouch(gfx::Point(100, 50));
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(200, 150));
  event_generator->MoveTouch(gfx::Point(100, 250));
  event_generator->MoveTouch(gfx::Point(0, 150));
  event_generator->MoveTouch(gfx::Point(100, 50));
  event_generator->ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
  expected_rect = gfx::Rect(0, 50, 200, 200);
  EXPECT_EQ(expected_rect, controller_test_api_->selection());
  EXPECT_EQ(expected_rect, observer.last_recognized_rect_);

  controller_->RemoveObserver(&observer);
}

TEST_F(HighlighterControllerTest, HighlighterGesturesScaled) {
  controller_test_api_->SetEnabled(true);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  const gfx::Rect original_px(200, 100, 400, 300);

  constexpr float display_scales[] = {1.f, 1.5f, 2.0f};
  constexpr float ui_scales[] = {0.5f,  0.67f, 1.0f,  1.25f,
                                 1.33f, 1.5f,  1.67f, 2.0f};

  for (size_t i = 0; i < sizeof(display_scales) / sizeof(float); ++i) {
    const float display_scale = display_scales[i];
    for (size_t j = 0; j < sizeof(ui_scales) / sizeof(float); ++j) {
      const float ui_scale = ui_scales[j];

      std::string display_spec =
          base::StringPrintf("1500x1000*%.2f@%.2f", display_scale, ui_scale);
      SCOPED_TRACE(display_spec);
      UpdateDisplayAndWaitForCompositingEnded(display_spec);

      controller_test_api_->ResetSelection();
      TraceRect(original_px);
      EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());

      const float combined_scale = display_scale * ui_scale;

      const gfx::Rect selection_dp = controller_test_api_->selection();
      const gfx::Rect selection_px = gfx::ToEnclosingRect(
          gfx::ScaleRect(gfx::RectF(selection_dp), combined_scale));
      EXPECT_TRUE(selection_px.Contains(original_px));

      gfx::Rect inflated_px(original_px);
      // Allow for rounding errors within 1dp.
      const int error_margin = static_cast<int>(std::ceil(combined_scale));
      inflated_px.Inset(-error_margin, -error_margin);
      EXPECT_TRUE(inflated_px.Contains(selection_px));
    }
  }
}

// Test that stylus gesture recognition correctly handles display rotation
TEST_F(HighlighterControllerTest, HighlighterGesturesRotated) {
  controller_test_api_->SetEnabled(true);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  const gfx::Rect trace(200, 100, 400, 300);

  // No rotation
  UpdateDisplayAndWaitForCompositingEnded("1500x1000");
  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
  EXPECT_EQ("200,100 400x300", controller_test_api_->selection().ToString());

  // Rotate to 90 degrees
  UpdateDisplayAndWaitForCompositingEnded("1500x1000/r");
  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
  EXPECT_EQ("100,900 300x400", controller_test_api_->selection().ToString());

  // Rotate to 180 degrees
  UpdateDisplayAndWaitForCompositingEnded("1500x1000/u");
  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
  EXPECT_EQ("900,600 400x300", controller_test_api_->selection().ToString());

  // Rotate to 270 degrees
  UpdateDisplayAndWaitForCompositingEnded("1500x1000/l");
  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
  EXPECT_EQ("600,200 300x400", controller_test_api_->selection().ToString());
}

// Test that a stroke interrupted close to the screen edge is treated as
// contiguous.
TEST_F(HighlighterControllerTest, InterruptedStroke) {
  controller_test_api_->SetEnabled(true);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  UpdateDisplayAndWaitForCompositingEnded("1500x1000");

  // An interrupted stroke close to the screen edge should be recognized as a
  // contiguous stroke.
  controller_test_api_->ResetSelection();
  event_generator->MoveTouch(gfx::Point(300, 100));
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(0, 100));
  event_generator->ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->IsWaitingToResumeStroke());
  EXPECT_FALSE(controller_test_api_->HandleSelectionCalled());
  EXPECT_FALSE(controller_test_api_->IsFadingAway());

  event_generator->MoveTouch(gfx::Point(0, 200));
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(300, 200));
  event_generator->ReleaseTouch();
  EXPECT_FALSE(controller_test_api_->IsWaitingToResumeStroke());
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
  EXPECT_EQ("0,100 300x100", controller_test_api_->selection().ToString());

  // Repeat the same gesture, but simulate a timeout after the gap. This should
  // force the gesture completion.
  controller_test_api_->ResetSelection();
  event_generator->MoveTouch(gfx::Point(300, 100));
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(0, 100));
  event_generator->ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->IsWaitingToResumeStroke());
  EXPECT_FALSE(controller_test_api_->HandleSelectionCalled());
  EXPECT_FALSE(controller_test_api_->IsFadingAway());

  controller_test_api_->SimulateInterruptedStrokeTimeout();
  EXPECT_FALSE(controller_test_api_->IsWaitingToResumeStroke());
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
  EXPECT_TRUE(controller_test_api_->IsFadingAway());
}

// Test that the selection is never crossing the screen bounds.
TEST_F(HighlighterControllerTest, SelectionInsideScreen) {
  controller_test_api_->SetEnabled(true);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  constexpr float display_scales[] = {1.f, 1.5f, 2.0f};

  for (size_t i = 0; i < sizeof(display_scales) / sizeof(float); ++i) {
    // 2nd display is for offscreen test.
    std::string display_spec = base::StringPrintf(
        "1000x1000*%.2f,500x1000*%.2f", display_scales[i], display_scales[i]);
    SCOPED_TRACE(display_spec);
    UpdateDisplayAndWaitForCompositingEnded(display_spec);

    const gfx::Rect screen(0, 0, 1000, 1000);

    // Rectangle completely offscreen.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(-100, -100, 10, 10));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_FALSE(controller_test_api_->HandleSelectionCalled());

    // Rectangle crossing the left edge.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(-100, 100, 200, 200));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Rectangle crossing the top edge.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(100, -100, 200, 200));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Rectangle crossing the right edge.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(900, 100, 200, 200));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Rectangle crossing the bottom edge.
    controller_test_api_->ResetSelection();
    TraceRect(gfx::Rect(100, 900, 200, 200));
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Vertical stroke completely offscreen.
    controller_test_api_->ResetSelection();
    event_generator->MoveTouch(gfx::Point(1100, 100));
    event_generator->PressTouch();
    event_generator->MoveTouch(gfx::Point(1100, 500));
    event_generator->ReleaseTouch();
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_FALSE(controller_test_api_->HandleSelectionCalled());

    // Horizontal stroke along the top edge of the screen.
    controller_test_api_->ResetSelection();
    event_generator->MoveTouch(gfx::Point(0, 0));
    event_generator->PressTouch();
    event_generator->MoveTouch(gfx::Point(1000, 0));
    event_generator->ReleaseTouch();
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));

    // Horizontal stroke along the bottom edge of the screen.
    controller_test_api_->ResetSelection();
    event_generator->MoveTouch(gfx::Point(0, 999));
    event_generator->PressTouch();
    event_generator->MoveTouch(gfx::Point(1000, 999));
    event_generator->ReleaseTouch();
    controller_test_api_->SimulateInterruptedStrokeTimeout();
    EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
    EXPECT_TRUE(screen.Contains(controller_test_api_->selection()));
  }
}

// Test that a detached client does not receive notifications.
TEST_F(HighlighterControllerTest, DetachedClient) {
  controller_test_api_->SetEnabled(true);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  UpdateDisplayAndWaitForCompositingEnded("1500x1000");
  const gfx::Rect trace(200, 100, 400, 300);

  // Detach the client, no notifications should reach it.
  controller_test_api_->DetachClient();

  controller_test_api_->ResetEnabledState();
  controller_test_api_->SetEnabled(false);
  EXPECT_FALSE(controller_test_api_->HandleEnabledStateChangedCalled());
  controller_test_api_->SetEnabled(true);
  EXPECT_FALSE(controller_test_api_->HandleEnabledStateChangedCalled());

  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_FALSE(controller_test_api_->HandleSelectionCalled());

  // Attach the client again, notifications should be delivered normally.
  controller_test_api_->AttachClient();

  controller_test_api_->ResetEnabledState();
  controller_test_api_->SetEnabled(false);
  EXPECT_TRUE(controller_test_api_->HandleEnabledStateChangedCalled());
  controller_test_api_->SetEnabled(true);
  EXPECT_TRUE(controller_test_api_->HandleEnabledStateChangedCalled());

  controller_test_api_->ResetSelection();
  TraceRect(trace);
  EXPECT_TRUE(controller_test_api_->HandleSelectionCalled());
}

// Test enabling/disabling metalayer mode by selecting/deselecting on palette
// tool and calling UpdateEnabledState notify observers properly.
TEST_F(HighlighterControllerTest, UpdateEnabledState) {
  TestHighlighterObserver observer;
  controller_->AddObserver(&observer);

  // Assert initial state.
  ASSERT_EQ(0, observer.enabled_count_);
  ASSERT_EQ(0, observer.disabled_by_user_count_);
  ASSERT_EQ(0, observer.disabled_by_session_abort_);
  ASSERT_EQ(0, observer.disabled_by_session_complete_);

  // Test enabling.
  tool_->OnEnable();
  EXPECT_EQ(1, observer.enabled_count_);
  EXPECT_EQ(0, observer.disabled_by_user_count_);
  EXPECT_EQ(0, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);

  // Test disabling by user.
  tool_->OnDisable();
  EXPECT_EQ(1, observer.enabled_count_);
  EXPECT_EQ(1, observer.disabled_by_user_count_);
  EXPECT_EQ(0, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);

  // Test disabling by session abort.
  tool_->OnEnable();
  EXPECT_EQ(2, observer.enabled_count_);
  EXPECT_EQ(1, observer.disabled_by_user_count_);
  EXPECT_EQ(0, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);
  controller_->UpdateEnabledState(
      HighlighterEnabledState::kDisabledBySessionAbort);
  EXPECT_EQ(2, observer.enabled_count_);
  EXPECT_EQ(1, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);

  // Test disabling by session complete.
  tool_->OnEnable();
  EXPECT_EQ(3, observer.enabled_count_);
  EXPECT_EQ(1, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);
  controller_->UpdateEnabledState(
      HighlighterEnabledState::kDisabledBySessionComplete);
  EXPECT_EQ(3, observer.enabled_count_);
  EXPECT_EQ(1, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(1, observer.disabled_by_session_complete_);

  controller_->RemoveObserver(&observer);
}

// Test aborting a metalayer session and notifying observers properly.
TEST_F(HighlighterControllerTest, AbortSession) {
  TestHighlighterObserver observer;
  controller_->AddObserver(&observer);

  // Assert initial state.
  ASSERT_EQ(0, observer.enabled_count_);
  ASSERT_EQ(0, observer.disabled_by_user_count_);
  ASSERT_EQ(0, observer.disabled_by_session_abort_);
  ASSERT_EQ(0, observer.disabled_by_session_complete_);

  // Start metalayer session.
  tool_->OnEnable();
  EXPECT_EQ(1, observer.enabled_count_);
  EXPECT_EQ(0, observer.disabled_by_user_count_);
  EXPECT_EQ(0, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);

  // Abort metalayer session.
  controller_->AbortSession();
  EXPECT_EQ(1, observer.enabled_count_);
  EXPECT_EQ(0, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);

  // Assert no-op when aborting an aborted session.
  controller_->AbortSession();
  EXPECT_EQ(1, observer.enabled_count_);
  EXPECT_EQ(0, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);

  // Assert no-op when aborting a completed session.
  tool_->OnEnable();
  EXPECT_EQ(2, observer.enabled_count_);
  EXPECT_EQ(0, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(0, observer.disabled_by_session_complete_);
  controller_->UpdateEnabledState(
      HighlighterEnabledState::kDisabledBySessionComplete);
  EXPECT_EQ(2, observer.enabled_count_);
  EXPECT_EQ(0, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(1, observer.disabled_by_session_complete_);
  controller_->AbortSession();
  EXPECT_EQ(2, observer.enabled_count_);
  EXPECT_EQ(0, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(1, observer.disabled_by_session_complete_);

  // Assert no-op when aborting a disabled session.
  tool_->OnEnable();
  EXPECT_EQ(3, observer.enabled_count_);
  EXPECT_EQ(0, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(1, observer.disabled_by_session_complete_);
  tool_->OnDisable();
  EXPECT_EQ(3, observer.enabled_count_);
  EXPECT_EQ(1, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(1, observer.disabled_by_session_complete_);
  controller_->AbortSession();
  EXPECT_EQ(3, observer.enabled_count_);
  EXPECT_EQ(1, observer.disabled_by_user_count_);
  EXPECT_EQ(1, observer.disabled_by_session_abort_);
  EXPECT_EQ(1, observer.disabled_by_session_complete_);
}

}  // namespace ash
