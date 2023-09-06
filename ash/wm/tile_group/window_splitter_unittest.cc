// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_splitter.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"

namespace ash {

namespace {

constexpr gfx::Rect kTopmostWindowBounds(10, 20, 500, 300);
constexpr gfx::Rect kDraggedWindowBounds(50, 60, 350, 250);

class WindowSplitterTest : public AshTestBase {
 public:
  WindowSplitterTest() = default;
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
};

}  // namespace

TEST_F(WindowSplitterTest, CanSplitWindowFromTop) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  gfx::PointF screen_location(kTopmostWindowBounds.top_center());
  screen_location.set_y(screen_location.y() + 5);

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
  screen_location.set_x(screen_location.x() + 5);

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
  screen_location.set_y(screen_location.y() - 5);

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
  screen_location.set_x(screen_location.x() - 5);

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
  screen_location.set_x(screen_location.x() + 5);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);
  EXPECT_FALSE(split_bounds);
}

TEST_F(WindowSplitterTest, NoSplitDraggedWindowUnderMinimumSize) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);
  GetTestDelegate(dragged_window.get())->set_minimum_size(gfx::Size(300, 100));

  gfx::PointF screen_location(kTopmostWindowBounds.left_center());
  screen_location.set_x(screen_location.x() + 5);

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
  screen_location.set_x(screen_location.x() - 5);

  auto split_bounds = WindowSplitter::MaybeSplitWindow(
      topmost_window.get(), dragged_window.get(), screen_location);
  EXPECT_FALSE(split_bounds);
}

TEST_F(WindowSplitterTest, DragSplitWindow) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  WindowSplitter splitter(dragged_window.get());
  EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

  gfx::PointF screen_location(kTopmostWindowBounds.right_center());
  screen_location.set_x(screen_location.x() - 5);

  splitter.UpdateDrag(screen_location, /*can_split=*/true);
  EXPECT_TRUE(RightHalf(kTopmostWindowBounds)
                  .Contains(GetPhantomWindowTargetBounds(splitter)));

  splitter.CompleteDrag(screen_location);
  EXPECT_EQ(topmost_window->GetBoundsInScreen(),
            LeftHalf(kTopmostWindowBounds));
  EXPECT_EQ(dragged_window->GetBoundsInScreen(),
            RightHalf(kTopmostWindowBounds));
}

TEST_F(WindowSplitterTest, DragEnterExitMarginNoSplit) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  WindowSplitter splitter(dragged_window.get());
  EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

  gfx::PointF screen_location(kTopmostWindowBounds.right_center());
  screen_location.set_x(screen_location.x() - 5);

  splitter.UpdateDrag(screen_location, /*can_split=*/true);
  EXPECT_TRUE(RightHalf(kTopmostWindowBounds)
                  .Contains(GetPhantomWindowTargetBounds(splitter)));

  // Use location barely outside window bounds to ensure extended hit region
  // does not activate window splitting.
  screen_location.set_x(kTopmostWindowBounds.right() + 1);

  splitter.UpdateDrag(screen_location, /*can_split=*/true);
  EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

  splitter.CompleteDrag(screen_location);
  EXPECT_EQ(topmost_window->GetBoundsInScreen(), kTopmostWindowBounds);
  EXPECT_EQ(dragged_window->GetBoundsInScreen(), kDraggedWindowBounds);
}

TEST_F(WindowSplitterTest, DragWithCantSplit) {
  auto topmost_window = CreateToplevelTestWindow(kTopmostWindowBounds);
  auto dragged_window = CreateToplevelTestWindow(kDraggedWindowBounds);

  ASSERT_TRUE(topmost_window->IsVisible());

  WindowSplitter splitter(dragged_window.get());
  EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

  gfx::PointF screen_location(kTopmostWindowBounds.top_center());
  screen_location.set_y(screen_location.y() + 5);

  splitter.UpdateDrag(screen_location, /*can_split=*/true);
  EXPECT_TRUE(TopHalf(kTopmostWindowBounds)
                  .Contains(GetPhantomWindowTargetBounds(splitter)));

  // Use same location but with splitting disabled.
  splitter.UpdateDrag(screen_location, /*can_split=*/false);
  EXPECT_TRUE(GetPhantomWindowTargetBounds(splitter).IsEmpty());

  splitter.CompleteDrag(screen_location);
  EXPECT_EQ(topmost_window->GetBoundsInScreen(), kTopmostWindowBounds);
  EXPECT_EQ(dragged_window->GetBoundsInScreen(), kDraggedWindowBounds);
}

}  // namespace ash
