// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_resizer.h"

#include <optional>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

const int kRootHeight = 600;

// Used to test if the OnPaintLayer is called by DragWindowLayerDelegate.
class TestLayerDelegate : public ui::LayerDelegate {
 public:
  TestLayerDelegate() = default;

  TestLayerDelegate(const TestLayerDelegate&) = delete;
  TestLayerDelegate& operator=(const TestLayerDelegate&) = delete;

  ~TestLayerDelegate() override = default;

  int paint_count() const { return paint_count_; }

 private:
  // Paint content for the layer to the specified context.
  void OnPaintLayer(const ui::PaintContext& context) override {
    paint_count_++;
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  int paint_count_ = 0;
};

}  // namespace

class DragWindowResizerTest : public AshTestBase {
 public:
  DragWindowResizerTest() : transient_child_(nullptr) {}

  DragWindowResizerTest(const DragWindowResizerTest&) = delete;
  DragWindowResizerTest& operator=(const DragWindowResizerTest&) = delete;

  ~DragWindowResizerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    UpdateDisplay(base::StringPrintf("800x%d", kRootHeight));

    aura::Window* root = Shell::GetPrimaryRootWindow();
    gfx::Rect root_bounds(root->bounds());
    EXPECT_EQ(kRootHeight, root_bounds.height());
    EXPECT_EQ(800, root_bounds.width());
    window_ = std::make_unique<aura::Window>(&delegate_);
    window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window_.get());
    window_->SetId(1);

    always_on_top_window_ = std::make_unique<aura::Window>(&delegate2_);
    always_on_top_window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    always_on_top_window_->SetProperty(aura::client::kZOrderingKey,
                                       ui::ZOrderLevel::kFloatingWindow);
    always_on_top_window_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(always_on_top_window_.get());
    always_on_top_window_->SetId(2);

    system_modal_window_ = std::make_unique<aura::Window>(&delegate3_);
    system_modal_window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    system_modal_window_->SetProperty(aura::client::kModalKey,
                                      ui::mojom::ModalType::kSystem);
    system_modal_window_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(system_modal_window_.get());
    system_modal_window_->SetId(3);

    transient_child_ = new aura::Window(&delegate4_);
    transient_child_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    transient_child_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(transient_child_);
    transient_child_->SetId(4);

    transient_parent_ = std::make_unique<aura::Window>(&delegate5_);
    transient_parent_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    transient_parent_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(transient_parent_.get());
    ::wm::AddTransientChild(transient_parent_.get(), transient_child_);
    transient_parent_->SetId(5);
  }

  void TearDown() override {
    window_.reset();
    always_on_top_window_.reset();
    system_modal_window_.reset();
    transient_parent_.reset();
    AshTestBase::TearDown();
  }

 protected:
  gfx::PointF CalculateDragPoint(const WindowResizer& resizer,
                                 int delta_x,
                                 int delta_y) const {
    gfx::PointF location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  ShelfLayoutManager* shelf_layout_manager() {
    return Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  }

  static WindowResizer* CreateDragWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return CreateWindowResizer(window, gfx::PointF(point_in_parent),
                               window_component, ::wm::WINDOW_MOVE_SOURCE_MOUSE)
        .release();
  }

  bool TestIfMouseWarpsAt(const gfx::Point& point_in_screen) {
    return AshTestBase::TestIfMouseWarpsAt(GetEventGenerator(),
                                           point_in_screen);
  }

  aura::test::TestWindowDelegate delegate_;
  aura::test::TestWindowDelegate delegate2_;
  aura::test::TestWindowDelegate delegate3_;
  aura::test::TestWindowDelegate delegate4_;
  aura::test::TestWindowDelegate delegate5_;
  aura::test::TestWindowDelegate delegate6_;

  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<aura::Window> always_on_top_window_;
  std::unique_ptr<aura::Window> system_modal_window_;
  raw_ptr<aura::Window, DanglingUntriaged> transient_child_;
  std::unique_ptr<aura::Window> transient_parent_;
};

// Verifies a window can be moved from the primary display to another.
TEST_F(DragWindowResizerTest, WindowDragWithMultiDisplays) {
  // Prevent snapping |window_|. We will drag within 25px of a display edge, for
  // the purpose of testing that a minimum visibility of 25px is enforced. When
  // a drag ends at that distance (or up to 32px) from a left or right display
  // edge, the dragged window is snapped. As that would only distract from the
  // purposes of the test, we avoid it by making |window_| unsnappable.
  // TODO: Remove this code after adding functionality where the mouse has to
  // dwell in the snap region before the dragged window can get snapped.
  window_->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorNone);
  ASSERT_FALSE(WindowState::Get(window_.get())->CanSnap());

  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("800x600,400x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Drag the pointer to the right. Once it reaches the right edge of the
    // primary display, it warps to the secondary.
    // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse
    // input without having to call |CursorManager::SetDisplay|.
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
    resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
    resizer->CompleteDrag();
    // The whole window is on the secondary display now. The parent should be
    // changed.
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    EXPECT_EQ("0,10 50x60", window_->bounds().ToString());
  }

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab (0, 0) of the window and move the pointer to (795, 10).
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse
    // input without having to call |CursorManager::SetDisplay|.
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]));
    resizer->Drag(CalculateDragPoint(*resizer, 795, 10), 0);
    // Window should be adjusted for minimum visibility (25px) during the drag.
    EXPECT_EQ("775,10 50x60", window_->bounds().ToString());
    resizer->CompleteDrag();
    // Since the pointer is still on the primary root window, the parent should
    // not be changed.
    // Window origin should be adjusted for minimum visibility (25px).
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    EXPECT_EQ("775,10 50x60", window_->bounds().ToString());
  }

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab the top-right edge of the window and move the pointer to (0, 10)
    // in the secondary root window's coordinates.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(49, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse
    // input without having to call |CursorManager::SetDisplay|.
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
    resizer->Drag(CalculateDragPoint(*resizer, 751, 10), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag();
    // Since the pointer is on the secondary, the parent should be changed
    // even though only small fraction of the window is within the secondary
    // root window's bounds.
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Window origin should be adjusted for minimum visibility (25px).
    int expected_x = -50 + kMinimumOnScreenArea;

    EXPECT_EQ(base::NumberToString(expected_x) + ",10 50x60",
              window_->bounds().ToString());
  }
  // Dropping a window that is larger than the destination work area
  // will shrink to fit to the work area.
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 700, 500),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  const int shelf_inset = 300 - ShelfConfig::Get()->shelf_size();
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab the top-right edge of the window and move the pointer to (0, 10)
    // in the secondary root window's coordinates.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(699, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 101, 10), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Window size should be adjusted to fit to the work area
    EXPECT_EQ(gfx::Size(400, shelf_inset).ToString(),
              window_->bounds().size().ToString());
    gfx::Rect window_bounds_in_screen = window_->GetBoundsInScreen();
    gfx::Rect intersect(window_->GetRootWindow()->GetBoundsInScreen());
    intersect.Intersect(window_bounds_in_screen);

    EXPECT_LE(10, intersect.width());
    EXPECT_LE(10, intersect.height());
    EXPECT_TRUE(window_bounds_in_screen.Contains(gfx::Point(800, 10)));
  }

  // Dropping a window that is larger than the destination work area
  // will shrink to fit to the work area.
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 700, 500),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab the top-left edge of the window and move the pointer to (150, 10)
    // in the secondary root window's coordinates. Make sure the window is
    // shrink in such a way that it keeps the cursor within.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(0, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 799, 10), ui::EF_CONTROL_DOWN);
    resizer->Drag(CalculateDragPoint(*resizer, 850, 10), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Window size should be adjusted to fit to the work area
    EXPECT_EQ(gfx::Size(400, shelf_inset).ToString(),
              window_->bounds().size().ToString());
    gfx::Rect window_bounds_in_screen = window_->GetBoundsInScreen();
    gfx::Rect intersect(window_->GetRootWindow()->GetBoundsInScreen());
    intersect.Intersect(window_bounds_in_screen);
    EXPECT_LE(10, intersect.width());
    EXPECT_LE(10, intersect.height());
    EXPECT_TRUE(window_bounds_in_screen.Contains(gfx::Point(850, 10)));
  }
}

// Verifies that dragging the active window to another display makes the new
// root window the active root window.
TEST_F(DragWindowResizerTest, WindowDragWithMultiDisplaysActiveRoot) {
  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(&delegate);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindowInPrimaryRootWindow(window.get());
  window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                            display::Screen::GetScreen()->GetPrimaryDisplay());
  window->Show();
  EXPECT_TRUE(wm::CanActivateWindow(window.get()));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ(root_windows[0], Shell::GetRootWindowForNewWindows());
  {
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Drag the pointer to the right. Once it reaches the right edge of the
    // primary display, it warps to the secondary.
    // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse
    // input without having to call |CursorManager::SetDisplay|.
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
    resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
    resizer->CompleteDrag();
    // The whole window is on the secondary display now. The parent should be
    // changed.
    EXPECT_EQ(root_windows[1], window->GetRootWindow());
    EXPECT_EQ(root_windows[1], Shell::GetRootWindowForNewWindows());
  }
}

// Verifies a window can be moved from the secondary display to primary.
TEST_F(DragWindowResizerTest, WindowDragWithMultiDisplaysRightToLeft) {
  // See comment on similar code at beginning of |WindowDragWithMultiDisplays|.
  // TODO: Same TODO as with that other comment.
  window_->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorNone);
  ASSERT_FALSE(WindowState::Get(window_.get())->CanSnap());

  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(
      gfx::Rect(800, 00, 50, 60),
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  EXPECT_EQ(root_windows[1], window_->GetRootWindow());
  {
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Move the mouse near the right edge, (798, 0), of the primary display.
    resizer->Drag(CalculateDragPoint(*resizer, -2, 0), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    // Window origin should be adjusted for minimum visibility (25px).
    EXPECT_EQ("775,0 50x60", window_->bounds().ToString());
  }
}

// Verifies the drag window is shown correctly.
TEST_F(DragWindowResizerTest, DragWindowController) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  TestLayerDelegate delegate;
  window_->layer()->set_delegate(&delegate);
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  {
    // Hold the center of the window so that the window doesn't stick to the
    // edge when dragging around the edge of the display.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(25, 30), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    DragWindowResizer* drag_resizer = DragWindowResizer::instance_;
    ASSERT_TRUE(drag_resizer);
    EXPECT_FALSE(drag_resizer->drag_window_controller_.get());

    // The pointer is inside the primary root. The drag window controller
    // should be NULL.
    ASSERT_EQ(display::Screen::GetScreen()
                  ->GetDisplayNearestWindow(root_windows[0])
                  .id(),
              Shell::Get()->cursor_manager()->GetDisplay().id());
    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    DragWindowController* controller =
        drag_resizer->drag_window_controller_.get();
    EXPECT_EQ(0, controller->GetDragWindowsCountForTest());

    // The window spans both root windows.
    resizer->Drag(CalculateDragPoint(*resizer, 773, 10), 0);
    EXPECT_EQ(1, controller->GetDragWindowsCountForTest());
    const aura::Window* drag_window = controller->GetDragWindowForTest(0);
    ASSERT_TRUE(drag_window);

    const ui::Layer* drag_layer = drag_window->layer();
    ASSERT_TRUE(drag_layer);
    // Check if |resizer->layer_| is properly set to the drag widget.
    EXPECT_FALSE(drag_layer->children().empty());

    // The paint request on a drag window should reach the original delegate.
    controller->RequestLayerPaintForTest();
    EXPECT_EQ(1, delegate.paint_count());

    // Invalidating the delegate on the original layer should prevent
    // calling the OnPaintLayer on the original delegate from new delegate.
    window_->layer()->set_delegate(nullptr);
    controller->RequestLayerPaintForTest();
    EXPECT_EQ(1, delegate.paint_count());

    // |window_| should be opaque since the pointer is still on the primary
    // root window. The drag window should be semi-transparent.
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
    EXPECT_GT(1.0f, drag_layer->opacity());

    // Enter the pointer to the secondary display.
    // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse
    // input without having to call |CursorManager::SetDisplay|.
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
    resizer->Drag(CalculateDragPoint(*resizer, 775, 10), 0);
    EXPECT_EQ(1, controller->GetDragWindowsCountForTest());
    // |window_| should be transparent, and the drag window should be opaque.
    EXPECT_GT(1.0f, window_->layer()->opacity());
    EXPECT_FLOAT_EQ(1.0f, drag_layer->opacity());

    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  }

  // Do the same test with RevertDrag().
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  {
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    DragWindowResizer* drag_resizer = DragWindowResizer::instance_;
    DragWindowController* controller =
        drag_resizer->drag_window_controller_.get();
    ASSERT_TRUE(drag_resizer);
    EXPECT_FALSE(controller);

    resizer->Drag(CalculateDragPoint(*resizer, 0, 610), 0);
    resizer->RevertDrag();
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  }
}

TEST_F(DragWindowResizerTest, DragWindowControllerAcrossThreeDisplays) {
  UpdateDisplay("400x600,400x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Layout so that all three displays touch each other.
  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(3u, list.size());
  ASSERT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(), list[0]);
  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0],
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[0],
                              display::DisplayPlacement::BOTTOM, 0);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());
  // Sanity check.
  ASSERT_EQ(gfx::Rect(0, 000, 400, 600),
            display_manager()->GetDisplayForId(list[0]).bounds());
  ASSERT_EQ(gfx::Rect(400, 0, 400, 600),
            display_manager()->GetDisplayForId(list[1]).bounds());
  ASSERT_EQ(gfx::Rect(0, 600, 800, 600),
            display_manager()->GetDisplayForId(list[2]).bounds());

  // Create a window on 2nd display.
  window_->SetBoundsInScreen(gfx::Rect(400, 0, 100, 100),
                             display_manager()->GetDisplayForId(list[1]));
  ASSERT_EQ(root_windows[1], window_->GetRootWindow());

  // Hold the center of the window so that the window doesn't stick to the edge
  // when dragging around the edge of the display.
  std::unique_ptr<WindowResizer> resizer(
      CreateDragWindowResizer(window_.get(), gfx::Point(50, 50), HTCAPTION));
  ASSERT_TRUE(resizer.get());
  DragWindowResizer* drag_resizer = DragWindowResizer::instance_;
  ASSERT_TRUE(drag_resizer);
  EXPECT_FALSE(drag_resizer->drag_window_controller_.get());
  // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse input
  // without having to call |CursorManager::SetDisplay|.
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  resizer->Drag(CalculateDragPoint(*resizer, -50, 0), 0);
  DragWindowController* controller =
      drag_resizer->drag_window_controller_.get();
  ASSERT_TRUE(controller);
  ASSERT_EQ(1, controller->GetDragWindowsCountForTest());
  const aura::Window* drag_window0 = controller->GetDragWindowForTest(0);
  ASSERT_TRUE(drag_window0);
  const ui::Layer* drag_layer0 = drag_window0->layer();
  EXPECT_EQ(root_windows[0], drag_window0->GetRootWindow());

  // |window_| should be opaque since the pointer is still on the primary
  // root window. The drag window should be semi-transparent.
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  EXPECT_GT(1.0f, drag_layer0->opacity());

  // The window spans across 3 displays, dragging to 3rd display.
  resizer->Drag(CalculateDragPoint(*resizer, -50, 549), 0);
  ASSERT_EQ(2, controller->GetDragWindowsCountForTest());
  drag_window0 = controller->GetDragWindowForTest(0);
  const aura::Window* drag_window1 = controller->GetDragWindowForTest(1);
  drag_layer0 = drag_window0->layer();
  const ui::Layer* drag_layer1 = drag_window1->layer();
  EXPECT_EQ(root_windows[0], drag_window0->GetRootWindow());
  EXPECT_EQ(root_windows[2], drag_window1->GetRootWindow());

  // |window_| should be opaque since the pointer is still on the 2nd
  // root window. The drag window should be semi-transparent.
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  EXPECT_GT(1.0f, drag_layer0->opacity());
  EXPECT_GT(1.0f, drag_layer1->opacity());

  // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse input
  // without having to call |CursorManager::SetDisplay|.
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]));
  resizer->Drag(CalculateDragPoint(*resizer, -51, 549), 0);
  ASSERT_EQ(2, controller->GetDragWindowsCountForTest());
  drag_window0 = controller->GetDragWindowForTest(0);
  drag_window1 = controller->GetDragWindowForTest(1);
  drag_layer0 = drag_window0->layer();
  drag_layer1 = drag_window1->layer();
  EXPECT_EQ(root_windows[0], drag_window0->GetRootWindow());
  EXPECT_EQ(root_windows[2], drag_window1->GetRootWindow());

  // |window_| should be transparent since the pointer is still on the primary
  // root window. The drag window should be semi-transparent.
  EXPECT_GT(1.0f, window_->layer()->opacity());
  EXPECT_FLOAT_EQ(1.0f, drag_layer0->opacity());
  EXPECT_GT(1.0f, drag_layer1->opacity());

  // Enter the pointer to the 3rd. Since it's bottom, the window snaps and
  // no drag windwos are created.
  // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse input
  // without having to call |CursorManager::SetDisplay|.
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[2]));
  resizer->Drag(CalculateDragPoint(*resizer, -51, 551), 0);
  ASSERT_EQ(1, controller->GetDragWindowsCountForTest());
  drag_window0 = controller->GetDragWindowForTest(0);
  drag_layer0 = drag_window0->layer();
  EXPECT_EQ(root_windows[2], drag_window0->GetRootWindow());

  // |window_| should be transparent, and the drag window should be opaque.
  EXPECT_FLOAT_EQ(0.0f, window_->layer()->opacity());
  EXPECT_FLOAT_EQ(1.0f, drag_layer0->opacity());

  resizer->CompleteDrag();
  EXPECT_EQ(root_windows[2], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
}

TEST_F(DragWindowResizerTest, DragWindowControllerWithCustomShadowBounds) {
  UpdateDisplay("400x600,400x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Layout so that all three displays touch each other.
  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(3u, list.size());
  ASSERT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(), list[0]);
  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0],
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[0],
                              display::DisplayPlacement::BOTTOM, 0);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());
  const display::Display& display0 =
      display_manager()->GetDisplayForId(list[0]);
  const display::Display& display1 =
      display_manager()->GetDisplayForId(list[1]);
  const display::Display& display2 =
      display_manager()->GetDisplayForId(list[2]);
  // Sanity check.
  ASSERT_EQ(gfx::Rect(0, 000, 400, 600), display0.bounds());
  ASSERT_EQ(gfx::Rect(400, 0, 400, 600), display1.bounds());
  ASSERT_EQ(gfx::Rect(0, 600, 800, 600), display2.bounds());

  const gfx::Rect shadow_bounds(10, 10, 200, 200);
  const auto update_controller_and_check_root_and_shadow =
      [&shadow_bounds](DragWindowController* controller,
                       aura::Window* root_window) {
        controller->Update();
        ASSERT_EQ(1, controller->GetDragWindowsCountForTest());
        EXPECT_EQ(root_window,
                  controller->GetDragWindowForTest(0)->GetRootWindow());
        const ui::Shadow* shadow = controller->GetDragWindowShadowForTest(0);
        ASSERT_TRUE(shadow);
        ASSERT_TRUE(shadow->layer());
        EXPECT_TRUE(shadow->layer()->visible());
        EXPECT_EQ(shadow_bounds, shadow->content_bounds());
      };
  // Test mouse dragging.
  {
    wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
    // Start on |display1|.
    cursor_manager->SetDisplay(display1);
    window_->SetBoundsInScreen(gfx::Rect(420, 20, 100, 100), display1);
    DragWindowController controller(window_.get(), /*is_touch_dragging=*/false,
                                    std::make_optional(shadow_bounds));
    // Move to |display0|.
    cursor_manager->SetDisplay(display0);
    window_->SetBoundsInScreen(gfx::Rect(20, 20, 100, 100), display0);
    update_controller_and_check_root_and_shadow(&controller, root_windows[0]);
    // Move to |display2|.
    cursor_manager->SetDisplay(display2);
    window_->SetBoundsInScreen(gfx::Rect(20, 620, 100, 100), display2);
    update_controller_and_check_root_and_shadow(&controller, root_windows[2]);
  }
  // Test touch dragging.
  {
    // Start on |display0|.
    window_->SetBoundsInScreen(gfx::Rect(20, 20, 100, 100), display0);
    DragWindowController controller(window_.get(), /*is_touch_dragging=*/true,
                                    std::make_optional(shadow_bounds));
    // Move the window so some is visible on |display1|.
    window_->SetBoundsInScreen(gfx::Rect(380, 20, 100, 100), display0);
    update_controller_and_check_root_and_shadow(&controller, root_windows[1]);
    // Move the window so some is visible on |display2|.
    window_->SetBoundsInScreen(gfx::Rect(20, 580, 100, 100), display0);
    update_controller_and_check_root_and_shadow(&controller, root_windows[2]);
  }
}

// Verifies if the resizer sets and resets
// MouseCursorEventFilter::mouse_warp_mode_ as expected.
TEST_F(DragWindowResizerTest, WarpMousePointer) {
  MouseCursorEventFilter* event_filter = Shell::Get()->mouse_cursor_filter();
  ASSERT_TRUE(event_filter);
  window_->SetBounds(gfx::Rect(0, 0, 50, 60));

  EXPECT_TRUE(event_filter->mouse_warp_enabled_);
  {
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTCAPTION));
    // While dragging a window, warp should be allowed.
    EXPECT_TRUE(event_filter->mouse_warp_enabled_);
    resizer->CompleteDrag();
  }
  EXPECT_TRUE(event_filter->mouse_warp_enabled_);

  {
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_TRUE(event_filter->mouse_warp_enabled_);
    resizer->RevertDrag();
  }
  EXPECT_TRUE(event_filter->mouse_warp_enabled_);

  {
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTRIGHT));
    // While resizing a window, warp should NOT be allowed.
    EXPECT_FALSE(event_filter->mouse_warp_enabled_);
    resizer->CompleteDrag();
  }
  EXPECT_TRUE(event_filter->mouse_warp_enabled_);

  {
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTRIGHT));
    EXPECT_FALSE(event_filter->mouse_warp_enabled_);
    resizer->RevertDrag();
  }
  EXPECT_TRUE(event_filter->mouse_warp_enabled_);
}

// Verifies cursor's device scale factor is updated whe a window is moved across
// root windows with different device scale factors (http://crbug.com/154183).
TEST_F(DragWindowResizerTest, CursorDeviceScaleFactor) {
  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("600x400,1000x800*2");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  const display::Display display0 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]);
  const display::Display display1 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);

  auto* cursor_manager = Shell::Get()->cursor_manager();
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
  // Move window from the root window with 1.0 device scale factor to the root
  // window with 2.0 device scale factor.
  {
    window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60), display0);
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_EQ(1.0f,
              cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                  ->scale_factor);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    TestIfMouseWarpsAt(gfx::Point(699, 200));
    EXPECT_EQ(2.0f,
              cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                  ->scale_factor);
    resizer->CompleteDrag();
    EXPECT_EQ(2.0f,
              cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                  ->scale_factor);
  }

  // Move window from the root window with 2.0 device scale factor to the root
  // window with 1.0 device scale factor.
  {
    window_->SetBoundsInScreen(gfx::Rect(600, 0, 50, 60), display1);
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_EQ(2.0f,
              cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                  ->scale_factor);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, -200, 200), 0);
    TestIfMouseWarpsAt(gfx::Point(400, 200));
    EXPECT_EQ(1.0f,
              cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                  ->scale_factor);
    resizer->CompleteDrag();
    EXPECT_EQ(1.0f,
              cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                  ->scale_factor);
  }
}

// Verifies several kinds of windows can be moved across displays.
TEST_F(DragWindowResizerTest, MoveWindowAcrossDisplays) {
  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("400x300,400x300");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  // Normal window can be moved across display.
  {
    aura::Window* window = window_.get();
    window->SetBoundsInScreen(
        gfx::Rect(0, 0, 50, 60),
        display::Screen::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(399, 200)));
    EXPECT_EQ("401,200",
              aura::Env::GetInstance()->last_mouse_location().ToString());
    resizer->CompleteDrag();
  }

  // Always on top window can be moved across display.
  {
    aura::Window* window = always_on_top_window_.get();
    window->SetBoundsInScreen(
        gfx::Rect(0, 0, 50, 60),
        display::Screen::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(399, 200)));
    EXPECT_EQ("401,200",
              aura::Env::GetInstance()->last_mouse_location().ToString());
    resizer->CompleteDrag();
  }

  // System modal window can be moved across display.
  {
    aura::Window* window = system_modal_window_.get();
    window->SetBoundsInScreen(
        gfx::Rect(0, 0, 50, 60),
        display::Screen::GetScreen()->GetPrimaryDisplay());
    aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(0, 0));
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(399, 200)));
    EXPECT_EQ("401,200",
              aura::Env::GetInstance()->last_mouse_location().ToString());
    resizer->CompleteDrag();
  }

  // Transient window cannot be moved across display.
  {
    aura::Window* window = transient_child_;
    window->SetBoundsInScreen(
        gfx::Rect(0, 0, 50, 60),
        display::Screen::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(399, 200)));
    EXPECT_EQ("399,200",
              aura::Env::GetInstance()->last_mouse_location().ToString());
    resizer->CompleteDrag();
  }

  // The parent of transient window can be moved across display.
  {
    aura::Window* window = transient_parent_.get();
    window->SetBoundsInScreen(
        gfx::Rect(0, 0, 50, 60),
        display::Screen::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(399, 200)));
    EXPECT_EQ("401,200",
              aura::Env::GetInstance()->last_mouse_location().ToString());
    resizer->CompleteDrag();
  }
}

// Regression test for animation / window drag race condition that can result
// in windows finishing a drag in an invisible state (see crbug.com/1430664).
TEST_F(DragWindowResizerTest, DragWindowControllerLatchesTargetOpacity) {
  // We must test with at least two displays as the DragWindowController is only
  // created for two or more displays (see DragWindowResizer::Drag).
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  TestLayerDelegate delegate;
  window_->layer()->set_delegate(&delegate);
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());

  // Setup the animator such that opacity transitions take non-zero time.
  constexpr auto kTransitionDuration = base::Seconds(3);
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);
  window_->layer()->SetAnimator(test_controller.animator());
  test_controller.StartThreadedAnimationsIfNeeded();

  // The window's layer opacity will initially be set to 1.
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->GetTargetOpacity());

  {
    // Set the target opacity to 0 and run the animation until this is reflected
    // on the layer itself. Opacity animation uses threaded animation.
    // Explicitly stop the animation because threaded animation may have started
    // a bit later. `kTransitionDuration` may not be quite enough to reach the
    // end.
    window_->layer()->SetOpacity(0);
    EXPECT_FLOAT_EQ(0, window_->layer()->GetTargetOpacity());
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
    test_controller.Step(kTransitionDuration);
    window_->layer()->GetAnimator()->StopAnimating();
    EXPECT_FLOAT_EQ(0, window_->layer()->GetTargetOpacity());
    EXPECT_FLOAT_EQ(0, window_->layer()->opacity());

    // Hold the center of the window so that the window doesn't stick to the
    // edge when dragging around the edge of the display.
    std::unique_ptr<WindowResizer> resizer(
        CreateDragWindowResizer(window_.get(), gfx::Point(25, 30), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    DragWindowResizer* drag_resizer = DragWindowResizer::instance_;
    ASSERT_TRUE(drag_resizer);
    EXPECT_FALSE(drag_resizer->drag_window_controller_.get());

    // Set up the drag by setting the dragged window's target opacity to 1. This
    // should not be immediately reflected in the layer's reported opacity.
    window_->layer()->SetOpacity(1.0f);
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->GetTargetOpacity());
    EXPECT_FLOAT_EQ(0, window_->layer()->opacity());

    // Start the drag. Although the layer's reported opacity is 0 the controller
    // should latch the target opacity of 1. The drag will begin by setting the
    // target opacity to 1.
    ASSERT_EQ(display::Screen::GetScreen()
                  ->GetDisplayNearestWindow(root_windows[0])
                  .id(),
              Shell::Get()->cursor_manager()->GetDisplay().id());
    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    DragWindowController* controller =
        drag_resizer->drag_window_controller_.get();
    EXPECT_FLOAT_EQ(1.0f, controller->old_opacity_for_testing());
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->GetTargetOpacity());

    // Simulate animation to a target opacity of 0 after the drag begins, which
    // may occur during the window drag session.
    window_->layer()->SetOpacity(0);
    test_controller.Step(kTransitionDuration);
    window_->layer()->GetAnimator()->StopAnimating();
    EXPECT_FLOAT_EQ(0, window_->layer()->GetTargetOpacity());
    EXPECT_FLOAT_EQ(0, window_->layer()->opacity());
  }

  // Regardless of the state of the layer opacity animation that occurred during
  // the drag session, the window's layer should be restored to the target
  // opacity of 1 after the drag session ends.
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->GetTargetOpacity());
  test_controller.Step(kTransitionDuration);
  window_->layer()->GetAnimator()->StopAnimating();
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->GetTargetOpacity());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
}

}  // namespace ash
