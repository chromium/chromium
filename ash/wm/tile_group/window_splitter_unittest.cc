// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_splitter.h"

#include <memory>

#include "ash/public/cpp/window_finder.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ui/base/window_state_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using DragType = WindowSplitter::DragType;
using SplitRegion = WindowSplitter::SplitRegion;

constexpr gfx::Rect kTopmostWindowBounds(10, 20, 500, 300);
constexpr gfx::Rect kDraggedWindowBounds(50, 60, 350, 250);

class WindowSplitterTest : public AshTestBase {
 public:
  WindowSplitterTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  WindowSplitterTest(const WindowSplitterTest&) = delete;
  WindowSplitterTest& operator=(const WindowSplitterTest&) = delete;
  ~WindowSplitterTest() override = default;

 protected:
  aura::test::TestWindowDelegate* GetTestDelegate(aura::Window* window) {
    return static_cast<aura::test::TestWindowDelegate*>(window->delegate());
  }

  gfx::Rect GetPhantomWindowTargetBounds(const WindowSplitter& splitter) {
    if (auto* controller = splitter.GetPhantomWindowControllerForTesting()) {
      return controller->GetTargetBoundsInScreenForTesting();
    }
    return gfx::Rect();
  }

  gfx::Rect TopHalf(gfx::Rect bounds) {
    bounds.set_height(bounds.height() / 2);
    return bounds;
  }
  gfx::Rect BottomHalf(gfx::Rect bounds) {
    bounds.Subtract(TopHalf(bounds));
    return bounds;
  }
  gfx::Rect LeftHalf(gfx::Rect bounds) {
    bounds.set_width(bounds.width() / 2);
    return bounds;
  }
  gfx::Rect RightHalf(gfx::Rect bounds) {
    bounds.Subtract(LeftHalf(bounds));
    return bounds;
  }

  void ExpectHistogramWithSplit(const base::HistogramTester& histogram_tester,
                                SplitRegion split_region,
                                int preview_count) {
    histogram_tester.ExpectUniqueSample(kWindowSplittingDragTypeHistogramName,
                                        DragType::kSplit, 1);
    histogram_tester.ExpectUniqueSample(
        kWindowSplittingSplitRegionHistogramName, split_region, 1);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingDragDurationPerSplitHistogramName, 1);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingDragDurationPerNoSplitHistogramName, 0);
    histogram_tester.ExpectUniqueSample(
        kWindowSplittingPreviewsShownCountPerSplitDragHistogramName,
        preview_count, 1);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingPreviewsShownCountPerNoSplitDragHistogramName, 0);
  }

  void ExpectHistogramWithNoSplit(const base::HistogramTester& histogram_tester,
                                  int preview_count) {
    histogram_tester.ExpectUniqueSample(kWindowSplittingDragTypeHistogramName,
                                        DragType::kNoSplit, 1);
    histogram_tester.ExpectTotalCount(kWindowSplittingSplitRegionHistogramName,
                                      0);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingDragDurationPerSplitHistogramName, 0);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingDragDurationPerNoSplitHistogramName, 1);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingPreviewsShownCountPerSplitDragHistogramName, 0);
    histogram_tester.ExpectUniqueSample(
        kWindowSplittingPreviewsShownCountPerNoSplitDragHistogramName,
        preview_count, 1);
  }

  void ExpectHistogramWithIncompleteDragType(
      const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectUniqueSample(kWindowSplittingDragTypeHistogramName,
                                        DragType::kIncomplete, 1);
    histogram_tester.ExpectTotalCount(kWindowSplittingSplitRegionHistogramName,
                                      0);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingDragDurationPerSplitHistogramName, 0);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingDragDurationPerNoSplitHistogramName, 0);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingPreviewsShownCountPerSplitDragHistogramName, 0);
    histogram_tester.ExpectTotalCount(
        kWindowSplittingPreviewsShownCountPerNoSplitDragHistogramName, 0);
  }

  void FastForwardPastDwellDuration() {
    task_environment()->FastForwardBy(WindowSplitter::kDwellActivationDuration +
                                      base::Milliseconds(100));
  }

  void FastForwardPastCancellationDuration() {
    task_environment()->FastForwardBy(
        WindowSplitter::kDwellCancellationDuration + base::Milliseconds(100));
  }

  // Moves the cursor back and forth across the top margin of the given window,
  // at the specified speed in pixels per second.
  // Returns the last location of the cursor, in screen coordinates.
  gfx::PointF MoveCursorAcrossWindowTopMargin(WindowSplitter* splitter,
                                              aura::Window* topmost_window,
                                              float movement_speed) {
    EXPECT_GT(movement_speed, 0);

    // Drag across top margin.
    const gfx::Rect window_bounds_in_screen =
        topmost_window->GetBoundsInScreen();
    gfx::Point left_location = window_bounds_in_screen.origin();
    left_location.Offset(WindowSplitter::kBaseTriggerMargins.left() + 10,
                         WindowSplitter::kBaseTriggerMargins.top() - 5);
    gfx::Point right_location = window_bounds_in_screen.top_right();
    right_location.Offset(WindowSplitter::kBaseTriggerMargins.right() - 10,
                          WindowSplitter::kBaseTriggerMargins.top() - 5);

    // Use duration much longer than dwell activation to check whether phantom
    // window is shown.
    constexpr base::TimeDelta kTotalDuration =
        WindowSplitter::kDwellActivationDuration * 3;
    // Use frequent enough updates for more accurate velocity calculation.
    constexpr int kNumUpdates = kTotalDuration / base::Milliseconds(30);
    constexpr base::TimeDelta kDeltaDuration = kTotalDuration / kNumUpdates;
    float dx = movement_speed * kDeltaDuration.InSecondsF();
    EXPECT_LT(dx, (right_location.x() - left_location.x()) / 2.0);

    gfx::PointF current_location(left_location);
    for (int i = 0; i < kNumUpdates; ++i) {
      // Keep cursor within the top margin of the window.
      // Flip the movement direction if it would go out of bounds.
      const float new_x = current_location.x() + dx;
      if (new_x > right_location.x() || new_x < left_location.x()) {
        dx = -dx;
      }
      current_location.Offset(dx, 0);

      splitter->UpdateDrag(current_location, /*can_split=*/true);
      task_environment()->FastForwardBy(kDeltaDuration);
    }
    return current_location;
  }
};

}  // namespace

TEST_F(WindowSplitterTest, CanSplitWindowFromTop) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  gfx::PointF screen_location(kTopmostWindowBounds.top_center());
  screen_location.Offset(0, 5);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);

  ASSERT_TRUE(split_bounds);
  EXPECT_EQ(split_bounds->topmost_window_bounds,
            BottomHalf(kTopmostWindowBounds));
  EXPECT_EQ(split_bounds->dragged_window_bounds, TopHalf(kTopmostWindowBounds));
}

TEST_F(WindowSplitterTest, CanSplitWindowFromLeft) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  gfx::PointF screen_location(kTopmostWindowBounds.left_center());
  screen_location.Offset(5, 0);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);

  ASSERT_TRUE(split_bounds);
  EXPECT_EQ(split_bounds->topmost_window_bounds,
            RightHalf(kTopmostWindowBounds));
  EXPECT_EQ(split_bounds->dragged_window_bounds,
            LeftHalf(kTopmostWindowBounds));
}

TEST_F(WindowSplitterTest, CanSplitWindowFromBottom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  gfx::PointF screen_location(kTopmostWindowBounds.bottom_center());
  screen_location.Offset(0, -5);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);

  ASSERT_TRUE(split_bounds);
  EXPECT_EQ(split_bounds->topmost_window_bounds, TopHalf(kTopmostWindowBounds));
  EXPECT_EQ(split_bounds->dragged_window_bounds,
            BottomHalf(kTopmostWindowBounds));
}

TEST_F(WindowSplitterTest, CanSplitWindowFromRight) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  gfx::PointF screen_location(kTopmostWindowBounds.right_center());
  screen_location.Offset(-5, 0);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);

  ASSERT_TRUE(split_bounds);
  EXPECT_EQ(split_bounds->topmost_window_bounds,
            LeftHalf(kTopmostWindowBounds));
  EXPECT_EQ(split_bounds->dragged_window_bounds,
            RightHalf(kTopmostWindowBounds));
}

TEST_F(WindowSplitterTest, NoSplitWindowNearCenter) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(),
      gfx::PointF(kTopmostWindowBounds.CenterPoint()));
  EXPECT_FALSE(split_bounds);
}

TEST_F(WindowSplitterTest, NoSplitTopmostWindowUnderMinimumSize) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  GetTestDelegate(topmost_window.get())->set_minimum_size(gfx::Size(300, 100));
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  gfx::PointF screen_location(kTopmostWindowBounds.left_center());
  screen_location.Offset(5, 0);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);
  EXPECT_FALSE(split_bounds);
}

TEST_F(WindowSplitterTest, NoSplitDraggedWindowUnderMinimumSize) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);
  GetTestDelegate(dragged_window.get())->set_minimum_size(gfx::Size(300, 100));

  gfx::PointF screen_location(kTopmostWindowBounds.left_center());
  screen_location.Offset(5, 0);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);
  EXPECT_FALSE(split_bounds);
}

TEST_F(WindowSplitterTest, NoSplitWindowOutsideWorkArea) {
  gfx::Rect topmost_window_bounds = kTopmostWindowBounds;
  topmost_window_bounds.set_x(-10);
  auto topmost_window = CreateToplevelTestWindow(topmost_window_bounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  gfx::PointF screen_location(topmost_window_bounds.right_center());
  screen_location.Offset(-5, 0);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);
  EXPECT_FALSE(split_bounds);
}

TEST_F(WindowSplitterTest, DragSplitWindow) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.right_center());
    screen_location.Offset(-5, 0);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(RightHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(),
              LeftHalf(kTopmostWindowBounds));
    EXPECT_EQ(dragged_window->GetBoundsInScreen(),
              RightHalf(kTopmostWindowBounds));
  }

  ExpectHistogramWithSplit(histogram_tester, SplitRegion::kRight,
                           /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragSplitWindowShowPreviewMultipleTimes) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.right_center());
    screen_location.Offset(-5, 0);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());
    FastForwardPastDwellDuration();
    EXPECT_TRUE(RightHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    screen_location = gfx::PointF(kTopmostWindowBounds.CenterPoint());
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());
    FastForwardPastDwellDuration();
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    screen_location = gfx::PointF(kTopmostWindowBounds.left_center());
    screen_location.Offset(5, 0);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());
    FastForwardPastDwellDuration();
    EXPECT_TRUE(LeftHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    screen_location = gfx::PointF(kTopmostWindowBounds.bottom_center());
    screen_location.Offset(0, -5);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());
    FastForwardPastDwellDuration();
    EXPECT_TRUE(BottomHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    // Update drag again at the same place to ensure no metric double counting.
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(BottomHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(),
              TopHalf(kTopmostWindowBounds));
    EXPECT_EQ(dragged_window->GetBoundsInScreen(),
              BottomHalf(kTopmostWindowBounds));
  }

  ExpectHistogramWithSplit(histogram_tester, SplitRegion::kBottom,
                           /*preview_count=*/3);
}

TEST_F(WindowSplitterTest, DragDwellCancelPhantomWindow) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.right_center());
    screen_location.Offset(-5, 0);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(RightHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    FastForwardPastCancellationDuration();
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(), kTopmostWindowBounds);
    EXPECT_EQ(dragged_window->GetBoundsInScreen(), kDraggedWindowBounds);
  }

  ExpectHistogramWithNoSplit(histogram_tester,
                             /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragEnterExitMarginNoSplitBeforePhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.right_center());
    screen_location.Offset(-5, 0);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    // Use location barely outside window bounds to ensure extended hit region
    // does not activate window splitting.
    screen_location.set_x(kTopmostWindowBounds.right() + 1);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    // Should still not have phantom window after dwelling.
    FastForwardPastDwellDuration();
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(), kTopmostWindowBounds);
    EXPECT_EQ(dragged_window->GetBoundsInScreen(), kDraggedWindowBounds);
  }

  ExpectHistogramWithNoSplit(histogram_tester,
                             /*preview_count=*/0);
}

TEST_F(WindowSplitterTest, DragEnterExitMarginNoSplitAfterPhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.right_center());
    screen_location.Offset(-5, 0);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());
    FastForwardPastDwellDuration();
    EXPECT_TRUE(RightHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    // Use location barely outside window bounds to ensure extended hit region
    // does not activate window splitting.
    screen_location.set_x(kTopmostWindowBounds.right() + 1);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    // Should still not have phantom window after dwelling.
    FastForwardPastDwellDuration();
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(), kTopmostWindowBounds);
    EXPECT_EQ(dragged_window->GetBoundsInScreen(), kDraggedWindowBounds);
  }

  ExpectHistogramWithNoSplit(histogram_tester,
                             /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragLowVelocityShowPhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF last_location = MoveCursorAcrossWindowTopMargin(
        &splitter, topmost_window.get(),
        WindowSplitter::kDwellMaxVelocityPixelsPerSec - 10.0);

    // Should already be showing phantom window by now.
    EXPECT_TRUE(TopHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));
    splitter.CompleteDrag(last_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(),
              BottomHalf(kTopmostWindowBounds));
    EXPECT_EQ(dragged_window->GetBoundsInScreen(),
              TopHalf(kTopmostWindowBounds));
  }

  ExpectHistogramWithSplit(histogram_tester, SplitRegion::kTop,
                           /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragHighVelocityNoShowPhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF last_location = MoveCursorAcrossWindowTopMargin(
        &splitter, topmost_window.get(),
        WindowSplitter::kDwellMaxVelocityPixelsPerSec + 30.0);

    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());
    splitter.CompleteDrag(last_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(), kTopmostWindowBounds);
    EXPECT_EQ(dragged_window->GetBoundsInScreen(), kDraggedWindowBounds);
  }

  ExpectHistogramWithNoSplit(histogram_tester, /*preview_count=*/0);
}

TEST_F(WindowSplitterTest, DragHighVelocityThenStopShowsPhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF last_location = MoveCursorAcrossWindowTopMargin(
        &splitter, topmost_window.get(),
        WindowSplitter::kDwellMaxVelocityPixelsPerSec + 30.0);

    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(TopHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    splitter.CompleteDrag(last_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(),
              BottomHalf(kTopmostWindowBounds));
    EXPECT_EQ(dragged_window->GetBoundsInScreen(),
              TopHalf(kTopmostWindowBounds));
  }

  ExpectHistogramWithSplit(histogram_tester, SplitRegion::kTop,
                           /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragWithCantSplit) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.top_center());
    screen_location.Offset(0, 5);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());
    FastForwardPastDwellDuration();
    EXPECT_TRUE(TopHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    // Use same location but with splitting disabled.
    splitter.UpdateDrag(screen_location, /*can_split=*/false);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    // Should still not have phantom window after dwelling.
    FastForwardPastDwellDuration();
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(), kTopmostWindowBounds);
    EXPECT_EQ(dragged_window->GetBoundsInScreen(), kDraggedWindowBounds);
  }

  ExpectHistogramWithNoSplit(histogram_tester, /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragDraggedWindowDestroyedBeforePhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.top_center());
    screen_location.Offset(0, 5);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    // Dragged window got destroyed during drag!
    dragged_window.reset();

    // Callback should handle dragged window disappearing.
    FastForwardPastDwellDuration();
  }

  ExpectHistogramWithIncompleteDragType(histogram_tester);
}

TEST_F(WindowSplitterTest, DragDraggedWindowDestroyedAfterPhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.top_center());
    screen_location.Offset(0, 5);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(TopHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    // Dragged window got destroyed during drag!
    dragged_window.reset();
  }

  ExpectHistogramWithIncompleteDragType(histogram_tester);
}

TEST_F(WindowSplitterTest, DragTopmostWindowDestroyedBeforePhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.top_center());
    screen_location.Offset(0, 5);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    // Top most window got destroyed during drag!
    topmost_window.reset();

    // Callback should handle top most window disappearing.
    FastForwardPastDwellDuration();
  }

  ExpectHistogramWithIncompleteDragType(histogram_tester);
}

TEST_F(WindowSplitterTest, DragTopmostWindowDestroyedAfterPhantom) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(kTopmostWindowBounds.top_center());
    screen_location.Offset(0, 5);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(TopHalf(kTopmostWindowBounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    // Top most window got destroyed during drag!
    topmost_window.reset();
  }

  ExpectHistogramWithIncompleteDragType(histogram_tester);
}

TEST_F(WindowSplitterTest, SplitMaximizedWindow) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  auto* topmost_window_state = WindowState::Get(topmost_window.get());
  topmost_window_state->Maximize();
  ASSERT_EQ(topmost_window_state->GetStateType(),
            chromeos::WindowStateType::kMaximized);
  const auto topmost_window_bounds = topmost_window->GetBoundsInScreen();
  EXPECT_EQ(topmost_window_bounds, GetPrimaryDisplay().work_area());

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(topmost_window_bounds.top_center());
    screen_location.Offset(0, 5);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(TopHalf(topmost_window_bounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(),
              BottomHalf(topmost_window_bounds));
    EXPECT_EQ(dragged_window->GetBoundsInScreen(),
              TopHalf(topmost_window_bounds));
    EXPECT_TRUE(chromeos::IsNormalWindowStateType(
        topmost_window_state->GetStateType()));
  }

  ExpectHistogramWithSplit(histogram_tester, SplitRegion::kTop,
                           /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, SplitSnappedWindow) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  const WindowSnapWMEvent event(
      WM_EVENT_SNAP_SECONDARY, WindowSnapActionSource::kDragWindowToEdgeToSnap);
  auto* topmost_window_state = WindowState::Get(topmost_window.get());
  topmost_window_state->OnWMEvent(&event);
  ASSERT_TRUE(
      chromeos::IsSnappedWindowStateType(topmost_window_state->GetStateType()));
  const auto topmost_window_bounds = topmost_window->GetBoundsInScreen();
  EXPECT_EQ(topmost_window_bounds, RightHalf(GetPrimaryDisplay().work_area()));

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(topmost_window_bounds.bottom_center());
    screen_location.Offset(0, -5);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(BottomHalf(topmost_window_bounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(),
              TopHalf(topmost_window_bounds));
    EXPECT_EQ(dragged_window->GetBoundsInScreen(),
              BottomHalf(topmost_window_bounds));
    EXPECT_TRUE(chromeos::IsNormalWindowStateType(
        topmost_window_state->GetStateType()));
  }

  ExpectHistogramWithSplit(histogram_tester, SplitRegion::kBottom,
                           /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragWithinExtendedDisplay) {
  UpdateDisplay("0+0-1200x800,1200+0-1600x1200@1.25");
  const gfx::Rect topmost_window_bounds(1300, 20, 500, 400);
  const gfx::Rect dragged_window_bounds(1400, 120, 300, 200);
  auto topmost_window = CreateToplevelTestWindow(topmost_window_bounds);
  auto dragged_window = CreateToplevelTestWindow(dragged_window_bounds);

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(topmost_window_bounds.right_center());
    screen_location.Offset(-5, 0);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(RightHalf(topmost_window_bounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(),
              LeftHalf(topmost_window_bounds));
    EXPECT_EQ(dragged_window->GetBoundsInScreen(),
              RightHalf(topmost_window_bounds));
  }

  ExpectHistogramWithSplit(histogram_tester, SplitRegion::kRight,
                           /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragAcrossExtendedDisplay) {
  UpdateDisplay("0+0-1200x800,1200+0-1600x1200@1.25");
  const gfx::Rect topmost_window_bounds(1300, 20, 500, 400);
  auto topmost_window = CreateToplevelTestWindow(topmost_window_bounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  base::HistogramTester histogram_tester;

  {
    // Nested scope used to exercise metrics update on splitter destruction.
    WindowSplitter splitter(dragged_window.get());
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    gfx::PointF screen_location(topmost_window_bounds.right_center());
    screen_location.Offset(-5, 0);
    splitter.UpdateDrag(screen_location, /*can_split=*/true);
    EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

    FastForwardPastDwellDuration();
    EXPECT_TRUE(RightHalf(topmost_window_bounds)
                    .Contains(GetPhantomWindowTargetBounds(splitter)));

    splitter.CompleteDrag(screen_location);
    EXPECT_EQ(topmost_window->GetBoundsInScreen(),
              LeftHalf(topmost_window_bounds));
    EXPECT_EQ(dragged_window->GetBoundsInScreen(),
              RightHalf(topmost_window_bounds));
  }

  ExpectHistogramWithSplit(histogram_tester, SplitRegion::kRight,
                           /*preview_count=*/1);
}

TEST_F(WindowSplitterTest, DragSplitWindowBringToTop) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);
  auto occluding_window = CreateToplevelTestWindow(gfx::Rect(80, 10, 500, 350));

  constexpr gfx::Point left_point(100, 40);
  constexpr gfx::Point right_point(400, 200);

  EXPECT_EQ(GetTopmostWindowAtPoint(left_point, {}), occluding_window.get());
  EXPECT_EQ(GetTopmostWindowAtPoint(right_point, {}), occluding_window.get());

  WindowSplitter splitter(dragged_window.get());
  gfx::PointF screen_location(kTopmostWindowBounds.left_center());
  screen_location.Offset(5, 0);
  splitter.UpdateDrag(screen_location, /*can_split=*/true);
  FastForwardPastDwellDuration();
  EXPECT_TRUE(LeftHalf(kTopmostWindowBounds)
                  .Contains(GetPhantomWindowTargetBounds(splitter)));

  splitter.CompleteDrag(screen_location);
  EXPECT_EQ(topmost_window->GetBoundsInScreen(),
            RightHalf(kTopmostWindowBounds));
  EXPECT_EQ(dragged_window->GetBoundsInScreen(),
            LeftHalf(kTopmostWindowBounds));
  EXPECT_EQ(GetTopmostWindowAtPoint(left_point, {}), dragged_window.get());
  EXPECT_EQ(GetTopmostWindowAtPoint(right_point, {}), topmost_window.get());
  EXPECT_THAT(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk),
      testing::ElementsAre(dragged_window.get(), topmost_window.get(),
                           occluding_window.get()));
  EXPECT_TRUE(wm::IsActiveWindow(dragged_window.get()));
  EXPECT_EQ(
      aura::client::GetFocusClient(dragged_window.get())->GetFocusedWindow(),
      dragged_window.get());
}

}  // namespace ash
