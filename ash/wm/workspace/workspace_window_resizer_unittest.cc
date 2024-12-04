// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_constants.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

using chromeos::WindowStateType;

constexpr int kRootHeight = 600;

gfx::PointF CalculateDragPoint(const WindowResizer& resizer,
                               int delta_x,
                               int delta_y) {
  gfx::PointF location = resizer.GetInitialLocation();
  location.set_x(location.x() + delta_x);
  location.set_y(location.y() + delta_y);
  return location;
}

void AllowSnap(aura::Window* window) {
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize |
                          aura::client::kResizeBehaviorCanMaximize);
}

}  // namespace

class WorkspaceWindowResizerTest : public AshTestBase {
 public:
  WorkspaceWindowResizerTest() = default;
  WorkspaceWindowResizerTest(const WorkspaceWindowResizerTest&) = delete;
  WorkspaceWindowResizerTest& operator=(const WorkspaceWindowResizerTest&) =
      delete;
  ~WorkspaceWindowResizerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay(base::StringPrintf("800x%d", kRootHeight));
    // Ignore the touch slop region.
    ui::GestureConfiguration::GetInstance()
        ->set_max_touch_move_in_pixels_for_click(0);

    aura::Window* root = Shell::GetPrimaryRootWindow();
    gfx::Rect root_bounds(root->bounds());
    EXPECT_EQ(800, root_bounds.width());
    WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
        root, gfx::Rect(), gfx::Insets(), gfx::Insets());
    window_ = std::make_unique<aura::Window>(&delegate_);
    window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window_.get());
    window_->SetId(1);

    window2_ = std::make_unique<aura::Window>(&delegate2_);
    window2_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window2_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window2_.get());
    window2_->SetId(2);

    window3_ = std::make_unique<aura::Window>(&delegate3_);
    window3_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window3_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window3_.get());
    window3_->SetId(3);

    window4_ = std::make_unique<aura::Window>(&delegate4_);
    window4_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window4_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window4_.get());
    window4_->SetId(4);
  }

  void TearDown() override {
    window_.reset();
    window2_.reset();
    window3_.reset();
    window4_.reset();
    touch_resize_window_.reset();
    AshTestBase::TearDown();
  }

  // Returns a string identifying the z-order of each of the known child windows
  // of |parent|.  The returned string constains the id of the known windows and
  // is ordered from topmost to bottomost windows.
  std::vector<int> WindowOrderAsIntVector(aura::Window* parent) const {
    std::vector<int> result;
    const aura::Window::Windows& windows = parent->children();
    for (aura::Window* window : base::Reversed(windows)) {
      if (window == window_.get() || window == window2_.get() ||
          window == window3_.get()) {
        result.push_back(window->GetId());
      }
    }
    return result;
  }

 protected:
  std::unique_ptr<WindowResizer> CreateResizerForTest(
      aura::Window* window,
      const gfx::Point& point_in_parent = gfx::Point(),
      int window_component = HTCAPTION) {
    auto resizer =
        CreateWindowResizer(window, gfx::PointF(point_in_parent),
                            window_component, wm::WINDOW_MOVE_SOURCE_MOUSE);
    workspace_resizer_ = WorkspaceWindowResizer::GetInstanceForTest();
    return resizer;
  }

  std::unique_ptr<WorkspaceWindowResizer> CreateWorkspaceResizerForTest(
      aura::Window* window,
      int window_component,
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
          attached_windows) {
    return CreateWorkspaceResizerForTest(window, gfx::Point(), window_component,
                                         wm::WINDOW_MOVE_SOURCE_MOUSE,
                                         attached_windows);
  }

  std::unique_ptr<WorkspaceWindowResizer> CreateWorkspaceResizerForTest(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component,
      wm::WindowMoveSource source,
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
          attached_windows) {
    WindowState* window_state = WindowState::Get(window);
    window_state->CreateDragDetails(gfx::PointF(point_in_parent),
                                    window_component, source);
    return WorkspaceWindowResizer::Create(window_state, attached_windows);
  }

  void DragToMaximize(aura::Window* window) {
    window->SetBounds(gfx::Rect(200, 200));
    std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window);
    resizer->Drag(gfx::PointF(400.f, 400.f), 0);
    resizer->Drag(gfx::PointF(400.f, 2.f), 0);
    DwellCountdownTimerFireNow();
    resizer->CompleteDrag();
    ASSERT_TRUE(WindowState::Get(window)->IsMaximized());
    ASSERT_TRUE(WindowState::Get(window)->HasRestoreBounds());
    resizer.reset();
  }

  void DragToRestore(aura::Window* window) {
    std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window);
    resizer->Drag(gfx::PointF(200.f, 200.f), 0);
    resizer->CompleteDrag();
    ASSERT_FALSE(WindowState::Get(window)->IsMaximized());
    resizer.reset();
  }

  PhantomWindowController* snap_phantom_window_controller() const {
    return workspace_resizer_->snap_phantom_window_controller_.get();
  }

  void InitTouchResizeWindow(const gfx::Rect& bounds, int window_component) {
    touch_resize_delegate_.set_window_component(window_component);
    touch_resize_window_.reset(CreateTestWindowInShellWithDelegate(
        &touch_resize_delegate_, 0, bounds));
  }

  bool IsDwellCountdownTimerRunning() {
    return workspace_resizer_->dwell_countdown_timer_.IsRunning();
  }

  void DwellCountdownTimerFireNow() {
    workspace_resizer_->dwell_countdown_timer_.FireNow();
  }

  void DragToMaximizeBehaviorCheckCountdownTimerFireNow(aura::Window* window) {
    WindowState::Get(window)->drag_to_maximize_mis_trigger_timer_.FireNow();
  }

  aura::test::TestWindowDelegate delegate_;
  aura::test::TestWindowDelegate delegate2_;
  aura::test::TestWindowDelegate delegate3_;
  aura::test::TestWindowDelegate delegate4_;
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<aura::Window> window2_;
  std::unique_ptr<aura::Window> window3_;
  std::unique_ptr<aura::Window> window4_;

  aura::test::TestWindowDelegate touch_resize_delegate_;
  std::unique_ptr<aura::Window> touch_resize_window_;

  raw_ptr<WorkspaceWindowResizer, DanglingUntriaged> workspace_resizer_ =
      nullptr;
};

// Assertions around attached window resize dragging from the right with 2
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_2) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTRIGHT, {window2_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Push off the screen, w2 should be resized to its min.
  delegate2_.set_minimum_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  EXPECT_EQ("0,300 780x300", window_->bounds().ToString());
  EXPECT_EQ("780,200 20x200", window2_->bounds().ToString());

  // Move back to 100 and verify w2 gets its original size.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  resizer->RevertDrag();
  EXPECT_EQ("0,300 400x300", window_->bounds().ToString());
  EXPECT_EQ("400,200 100x200", window2_->bounds().ToString());
}

// Assertions around collapsing and expanding.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_Compress) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTRIGHT, {window2_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the left, which should expand w2 and collapse w1.
  resizer->Drag(CalculateDragPoint(*resizer, -100, 10), 0);
  EXPECT_EQ("0,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,200 200x200", window2_->bounds().ToString());

  // Collapse all the way to w1's min.
  delegate_.set_minimum_size(gfx::Size(25, 25));
  resizer->Drag(CalculateDragPoint(*resizer, -800, 25), 0);
  EXPECT_EQ("0,300 25x300", window_->bounds().ToString());
  EXPECT_EQ("25,200 475x200", window2_->bounds().ToString());

  // But should keep minimum visible width;
  resizer->Drag(CalculateDragPoint(*resizer, -800, 20), 0);
  EXPECT_EQ("0,300 25x300", window_->bounds().ToString());
  EXPECT_EQ("25,200 475x200", window2_->bounds().ToString());

  // Move 100 to the left.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Back to -100.
  resizer->Drag(CalculateDragPoint(*resizer, -100, 20), 0);
  EXPECT_EQ("0,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,200 200x200", window2_->bounds().ToString());
}

// Assertions around attached window resize dragging from the right with 3
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_3) {
  window_->SetBounds(gfx::Rect(100, 300, 200, 300));
  window2_->SetBounds(gfx::Rect(300, 300, 150, 200));
  window3_->SetBounds(gfx::Rect(450, 300, 100, 200));
  delegate2_.set_minimum_size(gfx::Size(52, 50));
  delegate3_.set_minimum_size(gfx::Size(38, 50));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTRIGHT,
                                    {window2_.get(), window3_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);
  EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("400,300 150x200", window2_->bounds().ToString());
  EXPECT_EQ("550,300 100x200", window3_->bounds().ToString());

  // Move it 300, things should compress.
  resizer->Drag(CalculateDragPoint(*resizer, 300, -10), 0);
  EXPECT_EQ("100,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("600,300 120x200", window2_->bounds().ToString());
  EXPECT_EQ("720,300 80x200", window3_->bounds().ToString());

  // Move it so much the last two end up at their min.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 50), 0);
  EXPECT_EQ("100,300 610x300", window_->bounds().ToString());
  EXPECT_EQ("710,300 52x200", window2_->bounds().ToString());
  EXPECT_EQ("762,300 38x200", window3_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->RevertDrag();
  EXPECT_EQ("100,300 200x300", window_->bounds().ToString());
  EXPECT_EQ("300,300 150x200", window2_->bounds().ToString());
  EXPECT_EQ("450,300 100x200", window3_->bounds().ToString());
}

// Assertions around attached window resizing (collapsing and expanding) with
// 3 windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_3_Compress) {
  window_->SetBounds(gfx::Rect(100, 300, 200, 300));
  window2_->SetBounds(gfx::Rect(300, 300, 200, 200));
  window3_->SetBounds(gfx::Rect(450, 300, 100, 200));
  delegate2_.set_minimum_size(gfx::Size(52, 50));
  delegate3_.set_minimum_size(gfx::Size(38, 50));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTRIGHT,
                                    {window2_.get(), window3_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it -100 to the right, which should collapse w1 and expand w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -100, -10), 0);
  EXPECT_EQ("100,300 100x300", window_->bounds().ToString());
  EXPECT_EQ("200,300 266x200", window2_->bounds().ToString());
  EXPECT_EQ("466,300 134x200", window3_->bounds().ToString());

  // Move it 100 to the right.
  resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);
  EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("400,300 200x200", window2_->bounds().ToString());
  EXPECT_EQ("600,300 100x200", window3_->bounds().ToString());

  // 100 to the left again.
  resizer->Drag(CalculateDragPoint(*resizer, -100, -10), 0);
  EXPECT_EQ("100,300 100x300", window_->bounds().ToString());
  EXPECT_EQ("200,300 266x200", window2_->bounds().ToString());
  EXPECT_EQ("466,300 134x200", window3_->bounds().ToString());
}

// Assertions around collapsing and expanding from the bottom.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_Compress) {
  window_->SetBounds(gfx::Rect(0, 100, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 400, 100, 200));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTBOTTOM, {window2_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it up 100, which should expand w2 and collapse w1.
  resizer->Drag(CalculateDragPoint(*resizer, 10, -100), 0);
  EXPECT_EQ("0,100 400x200", window_->bounds().ToString());
  EXPECT_EQ("400,300 100x300", window2_->bounds().ToString());

  // Collapse all the way to w1's min.
  delegate_.set_minimum_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 20, -800), 0);
  EXPECT_EQ("0,100 400x20", window_->bounds().ToString());
  EXPECT_EQ("400,120 100x480", window2_->bounds().ToString());

  // Move 100 down.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,100 400x400", window_->bounds().ToString());
  EXPECT_EQ("400,500 100x100", window2_->bounds().ToString());

  // Back to -100.
  resizer->Drag(CalculateDragPoint(*resizer, 20, -100), 0);
  EXPECT_EQ("0,100 400x200", window_->bounds().ToString());
  EXPECT_EQ("400,300 100x300", window2_->bounds().ToString());
}

// Assertions around attached window resize dragging from the bottom with 2
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_2) {
  window_->SetBounds(gfx::Rect(0, 50, 400, 200));
  window2_->SetBounds(gfx::Rect(0, 250, 200, 100));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTBOTTOM, {window2_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the bottom, which should expand w1 and push w2.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,50 400x300", window_->bounds().ToString());
  EXPECT_EQ("0,350 200x100", window2_->bounds().ToString());

  // Push off the screen, w2 should be resized to its min.
  delegate2_.set_minimum_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 50, 820), 0);
  EXPECT_EQ("0,50 400x530", window_->bounds().ToString());
  EXPECT_EQ("0,580 200x20", window2_->bounds().ToString());

  // Move back to 100 and verify w2 gets its original size.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,50 400x300", window_->bounds().ToString());
  EXPECT_EQ("0,350 200x100", window2_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  resizer->RevertDrag();
  EXPECT_EQ("0,50 400x200", window_->bounds().ToString());
  EXPECT_EQ("0,250 200x100", window2_->bounds().ToString());
}

// Assertions around attached window resize dragging from the bottom with 3
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_3) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  window_->SetBounds(gfx::Rect(300, 100, 300, 200));
  window2_->SetBounds(gfx::Rect(300, 300, 200, 150));
  window3_->SetBounds(gfx::Rect(300, 450, 200, 100));
  delegate2_.set_minimum_size(gfx::Size(50, 52));
  delegate3_.set_minimum_size(gfx::Size(50, 38));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTBOTTOM,
                                    {window2_.get(), window3_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 100 down, which should expand w1 and push w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 100), 0);
  EXPECT_EQ("300,100 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,400 200x150", window2_->bounds().ToString());
  EXPECT_EQ("300,550 200x100", window3_->bounds().ToString());

  // Move it 296 things should compress.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 296), 0);
  EXPECT_EQ("300,100 300x496", window_->bounds().ToString());
  EXPECT_EQ("300,596 200x123", window2_->bounds().ToString());
  EXPECT_EQ("300,719 200x81", window3_->bounds().ToString());

  // Move it so much everything ends up at its min.
  resizer->Drag(CalculateDragPoint(*resizer, 50, 798), 0);
  EXPECT_EQ("300,100 300x610", window_->bounds().ToString());
  EXPECT_EQ("300,710 200x52", window2_->bounds().ToString());
  EXPECT_EQ("300,762 200x38", window3_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->RevertDrag();
  EXPECT_EQ("300,100 300x200", window_->bounds().ToString());
  EXPECT_EQ("300,300 200x150", window2_->bounds().ToString());
  EXPECT_EQ("300,450 200x100", window3_->bounds().ToString());
}

// Assertions around attached window resizing (collapsing and expanding) with
// 3 windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_3_Compress) {
  window_->SetBounds(gfx::Rect(0, 0, 200, 200));
  window2_->SetBounds(gfx::Rect(10, 200, 200, 200));
  window3_->SetBounds(gfx::Rect(20, 400, 100, 100));
  delegate2_.set_minimum_size(gfx::Size(52, 50));
  delegate3_.set_minimum_size(gfx::Size(38, 50));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTBOTTOM,
                                    {window2_.get(), window3_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 100 up, which should collapse w1 and expand w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -10, -100), 0);
  EXPECT_EQ("0,0 200x100", window_->bounds().ToString());
  EXPECT_EQ("10,100 200x266", window2_->bounds().ToString());
  EXPECT_EQ("20,366 100x134", window3_->bounds().ToString());

  // Move it 100 down.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,0 200x300", window_->bounds().ToString());
  EXPECT_EQ("10,300 200x200", window2_->bounds().ToString());
  EXPECT_EQ("20,500 100x100", window3_->bounds().ToString());

  // 100 up again.
  resizer->Drag(CalculateDragPoint(*resizer, -10, -100), 0);
  EXPECT_EQ("0,0 200x100", window_->bounds().ToString());
  EXPECT_EQ("10,100 200x266", window2_->bounds().ToString());
  EXPECT_EQ("20,366 100x134", window3_->bounds().ToString());
}

// Tests that touch-dragging a window does not lock the mouse cursor
// and therefore shows the cursor on a mousemove.
TEST_F(WorkspaceWindowResizerTest, MouseMoveWithTouchDrag) {
  // Shell hides the cursor by default; show it for this tests.
  Shell::Get()->cursor_manager()->ShowCursor();

  window_->SetBounds(gfx::Rect(0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  Shell* shell = Shell::Get();
  ui::test::EventGenerator generator(window_->GetRootWindow());

  // The cursor should not be locked initially.
  EXPECT_FALSE(shell->cursor_manager()->IsCursorLocked());

  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows;
  windows.push_back(window2_.get());
  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), gfx::Point(), HTRIGHT,
                                    wm::WINDOW_MOVE_SOURCE_TOUCH, windows);
  ASSERT_TRUE(resizer.get());

  // Creating a WorkspaceWindowResizer should not lock the cursor.
  EXPECT_FALSE(shell->cursor_manager()->IsCursorLocked());

  // The cursor should be hidden after touching the screen and
  // starting a drag.
  EXPECT_TRUE(shell->cursor_manager()->IsCursorVisible());
  generator.PressTouch();
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_FALSE(shell->cursor_manager()->IsCursorVisible());
  EXPECT_FALSE(shell->cursor_manager()->IsCursorLocked());

  // Moving the mouse should show the cursor.
  generator.MoveMouseBy(1, 1);
  EXPECT_TRUE(shell->cursor_manager()->IsCursorVisible());
  EXPECT_FALSE(shell->cursor_manager()->IsCursorLocked());

  resizer->RevertDrag();
}

// Assertions around dragging near the left/right edge of the display.
TEST_F(WorkspaceWindowResizerTest, Edge) {
  // Resize host window to force insets update.
  UpdateDisplay("800x700");
  // TODO(varkha): Insets are reset after every drag because of
  // http://crbug.com/292238.
  window_->SetBounds(gfx::Rect(20, 30, 400, 60));
  AllowSnap(window_.get());
  WindowState* window_state = WindowState::Get(window_.get());

  {
    gfx::Rect expected_bounds_in_parent(GetDefaultSnappedWindowBoundsInParent(
        window_.get(), SnapViewType::kPrimary));

    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get());
    ASSERT_TRUE(resizer.get());
    // Test tolerance of 32px for snapping.
    resizer->Drag(CalculateDragPoint(*resizer, 32, 10), 0);
    resizer->CompleteDrag();

    EXPECT_EQ(expected_bounds_in_parent, window_->bounds());
    ASSERT_TRUE(window_state->HasRestoreBounds());
    EXPECT_EQ(gfx::Rect(20, 30, 400, 60),
              window_state->GetRestoreBoundsInScreen());
  }
  // Try the same with the right side.
  {
    gfx::Rect expected_bounds_in_parent(GetDefaultSnappedWindowBoundsInParent(
        window_.get(), SnapViewType::kSecondary));

    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get());
    ASSERT_TRUE(resizer.get());
    // Test tolerance of 32px for snapping (measured from 799, not from 800).
    resizer->Drag(CalculateDragPoint(*resizer, 767, 10), 0);
    resizer->CompleteDrag();
    EXPECT_EQ(expected_bounds_in_parent, window_->bounds());
    ASSERT_TRUE(window_state->HasRestoreBounds());
    EXPECT_EQ(gfx::Rect(20, 30, 400, 60),
              window_state->GetRestoreBoundsInScreen());
  }
}

// Check that non resizable windows will not get resized.
TEST_F(WorkspaceWindowResizerTest, NonResizableWindows) {
  window_->SetBounds(gfx::Rect(20, 30, 50, 60));
  window_->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorNone);

  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -20, 0), 0);
  resizer->CompleteDrag();
  EXPECT_EQ("0,30 50x60", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MultiDisplaySnapPhantom) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             display::Screen::GetScreen()->GetPrimaryDisplay());

  // Make the window snappable.
  AllowSnap(window_.get());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  {
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get());
    ASSERT_TRUE(resizer.get());
    EXPECT_FALSE(snap_phantom_window_controller());

    // The pointer is on the edge but not shared. The snap phantom window
    // controller should be non-NULL.
    resizer->Drag(CalculateDragPoint(*resizer, 799, 0), 0);
    EXPECT_TRUE(snap_phantom_window_controller());

    // Move the cursor across the edge. Now the snap phantom window controller
    // should still be non-null.
    resizer->Drag(CalculateDragPoint(*resizer, 800, 0), 0);
    EXPECT_TRUE(snap_phantom_window_controller());
  }
}

// Verifies that dragging a snapped window unsnaps it.
TEST_F(WorkspaceWindowResizerTest, DragSnapped) {
  WindowState* window_state = WindowState::Get(window_.get());

  const gfx::Rect kInitialBounds(100, 100, 100, 100);
  window_->SetBounds(kInitialBounds);
  window_->Show();
  AllowSnap(window_.get());

  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  gfx::Rect snapped_bounds = window_->bounds();
  EXPECT_NE(snapped_bounds.ToString(), kInitialBounds.ToString());
  EXPECT_EQ(kInitialBounds, window_state->GetRestoreBoundsInParent());

  // Dragging a side snapped window should unsnap it.
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  resizer->Drag(CalculateDragPoint(*resizer, 33, 0), 0);
  resizer->CompleteDrag();
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
  EXPECT_EQ("33,0 100x100", window_->bounds().ToString());
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verifies the behavior of resizing a side snapped window.
TEST_F(WorkspaceWindowResizerTest, ResizeSnapped) {
  WindowState* window_state = WindowState::Get(window_.get());
  AllowSnap(window_.get());

  const gfx::Rect kInitialBounds(100, 100, 100, 100);
  window_->SetBounds(kInitialBounds);
  window_->Show();

  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  gfx::Rect snapped_bounds = window_->bounds();
  EXPECT_NE(snapped_bounds.ToString(), kInitialBounds.ToString());
  EXPECT_EQ(kInitialBounds, window_state->GetRestoreBoundsInParent());

  {
    // 1) Resizing a side snapped window to make it wider should not unsnap the
    // window.
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTRIGHT);
    resizer->Drag(CalculateDragPoint(*resizer, 10, 0), 0);
    resizer->CompleteDrag();
    EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
    snapped_bounds.Inset(gfx::Insets::TLBR(0, 0, 0, -10));
    EXPECT_EQ(snapped_bounds.ToString(), window_->bounds().ToString());
    EXPECT_EQ(kInitialBounds, window_state->GetRestoreBoundsInParent());
  }

  {
    // 2) Resizing a side snapped window vertically and then undoing the change
    // should not unsnap.
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOM);
    resizer->Drag(CalculateDragPoint(*resizer, 0, -30), 0);
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    resizer->CompleteDrag();
    EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
    EXPECT_EQ(snapped_bounds.ToString(), window_->bounds().ToString());
    EXPECT_EQ(kInitialBounds, window_state->GetRestoreBoundsInParent());
  }

  {
    // 3) Resizing a side snapped window vertically and then not undoing the
    // change should unsnap.
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOM);
    resizer->Drag(CalculateDragPoint(*resizer, 0, -10), 0);
    resizer->CompleteDrag();
    EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
    gfx::Rect expected_bounds(snapped_bounds);
    expected_bounds.Inset(gfx::Insets::TLBR(0, 0, 10, 0));
    EXPECT_EQ(expected_bounds.ToString(), window_->bounds().ToString());
    EXPECT_FALSE(window_state->HasRestoreBounds());
  }
}

// Verifies the behavior of resizing and restoring a side snapped window.
TEST_F(WorkspaceWindowResizerTest, ResizeRestoreSnappedWindow) {
  WindowState* window_state = WindowState::Get(window_.get());
  AllowSnap(window_.get());

  const gfx::Rect kInitialBounds(100, 100, 100, 100);
  window_->SetBounds(kInitialBounds);
  window_->Show();

  // Snap the window to the left.
  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);
  gfx::Rect snapped_bounds = window_->bounds();

  // Resize the snapped window to make it wider.
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTRIGHT);
  resizer->Drag(CalculateDragPoint(*resizer, 10, 0), 0);
  resizer->CompleteDrag();
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  snapped_bounds.Inset(gfx::Insets::TLBR(0, 0, 0, -10));
  EXPECT_EQ(snapped_bounds.ToString(), window_->bounds().ToString());
  EXPECT_EQ(kInitialBounds, window_state->GetRestoreBoundsInParent());

  // Minimize then restore the window and expect the resized snapped bounds to
  // be restored.
  window_state->Minimize();
  window_state->Restore();
  EXPECT_EQ(snapped_bounds.ToString(), window_->bounds().ToString());
}

// Verifies windows are correctly restacked when reordering multiple windows.
TEST_F(WorkspaceWindowResizerTest, RestackAttached) {
  window_->SetBounds(gfx::Rect(0, 0, 200, 300));
  window2_->SetBounds(gfx::Rect(200, 0, 100, 200));
  window3_->SetBounds(gfx::Rect(300, 0, 100, 100));

  {
    std::unique_ptr<WorkspaceWindowResizer> resizer =
        CreateWorkspaceResizerForTest(window_.get(), HTRIGHT, {window2_.get()});
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the right, which should expand w1 and push w2 and w3.
    resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);

    // 2 should be topmost since it's initially the highest in the stack.
    const std::vector<int> expected_order = {2, 1, 3};
    EXPECT_EQ(expected_order, WindowOrderAsIntVector(window_->parent()));
  }

  {
    std::unique_ptr<WorkspaceWindowResizer> resizer =
        CreateWorkspaceResizerForTest(window2_.get(), HTRIGHT,
                                      {window3_.get()});
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the right, which should expand w1 and push w2 and w3.
    resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);

    // 2 should be topmost since it's initially the highest in the stack.
    const std::vector<int> expected_order = {2, 3, 1};
    EXPECT_EQ(expected_order, WindowOrderAsIntVector(window_->parent()));
  }
}

// Makes sure we don't allow dragging below the work area.
TEST_F(WorkspaceWindowResizerTest, DontDragOffBottom) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 10, 0),
      gfx::Insets::TLBR(0, 0, 10, 0));

  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  window_->SetBounds(gfx::Rect(100, 200, 300, 400));
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600), 0);
  int expected_y =
      kRootHeight - WorkspaceWindowResizer::kMinOnscreenHeight - 10;
  EXPECT_EQ(gfx::Rect(100, expected_y, 300, 400), window_->bounds());
}

// Makes sure we don't allow dragging on the work area with multidisplay.
TEST_F(WorkspaceWindowResizerTest, DontDragOffBottomWithMultiDisplay) {
  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 10, 0),
      gfx::Insets::TLBR(0, 0, 10, 0));

  // Positions the secondary display at the bottom the primary display.
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::BOTTOM, 0));

  {
    window_->SetBounds(gfx::Rect(100, 200, 300, 20));
    DCHECK_LT(window_->bounds().height(),
              WorkspaceWindowResizer::kMinOnscreenHeight);
    // Drag down avoiding dragging along the edge as that would side-snap.
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(10, 0), HTCAPTION);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 400), 0);
    int expected_y = kRootHeight - window_->bounds().height() - 10;
    // When the mouse cursor is in the primary display, the window cannot move
    // on non-work area but can get all the way towards the bottom,
    // restricted only by the window height.
    EXPECT_EQ(gfx::Rect(100, expected_y, 300, 20), window_->bounds());
    // Revert the drag in order to not remember the restore bounds.
    resizer->RevertDrag();
  }

  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 10, 0),
      gfx::Insets::TLBR(0, 0, 10, 0));
  {
    window_->SetBounds(gfx::Rect(100, 200, 300, 400));
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(10, 0), HTCAPTION);
    ASSERT_TRUE(resizer.get());
    // Drag down avoiding dragging along the edge as that would side-snap.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 400), 0);
    int expected_y =
        kRootHeight - WorkspaceWindowResizer::kMinOnscreenHeight - 10;
    // When the mouse cursor is in the primary display, the window cannot move
    // on non-work area with kMinOnscreenHeight margin.
    EXPECT_EQ(gfx::Rect(100, expected_y, 300, 400), window_->bounds());
    resizer->CompleteDrag();
  }

  {
    window_->SetBounds(gfx::Rect(100, 200, 300, 400));
    std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(
        window_.get(), window_->bounds().origin(), HTCAPTION);
    ASSERT_TRUE(resizer.get());
    // Drag down avoiding getting stuck against the shelf on the bottom screen.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 500), 0);
    // The window can move to the secondary display beyond non-work area of
    // the primary display.
    EXPECT_EQ("100,700 300x400", window_->bounds().ToString());
    resizer->CompleteDrag();
  }
}

// Makes sure we don't allow dragging off the top of the work area.
TEST_F(WorkspaceWindowResizerTest, DontDragOffTop) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(10, 0, 0, 0),
      gfx::Insets::TLBR(10, 0, 0, 0));

  window_->SetBounds(gfx::Rect(100, 200, 300, 400));
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, -600), 0);
  EXPECT_EQ("100,10 300x400", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, ResizeBottomOutsideWorkArea) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 50, 0),
      gfx::Insets::TLBR(0, 0, 50, 0));

  window_->SetBounds(gfx::Rect(100, 200, 300, 380));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTTOP);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 8, 0), 0);
  EXPECT_EQ("100,200 300x380", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, ResizeWindowOutsideLeftWorkArea) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 50, 0),
      gfx::Insets::TLBR(0, 0, 50, 0));
  int left = screen_util::GetDisplayWorkAreaBoundsInParent(window_.get()).x();
  int pixels_to_left_border = 50;
  int window_width = 300;
  int window_x = left - window_width + pixels_to_left_border;
  window_->SetBounds(gfx::Rect(window_x, 100, window_width, 380));
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(
      window_.get(), gfx::Point(pixels_to_left_border, 0), HTRIGHT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -window_width, 0), 0);
  EXPECT_EQ(gfx::Rect(window_x, 100, kMinimumOnScreenArea - window_x, 380),
            window_->bounds());
}

TEST_F(WorkspaceWindowResizerTest, ResizeWindowOutsideRightWorkArea) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 50, 0),
      gfx::Insets::TLBR(0, 0, 50, 0));
  int right =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get()).right();
  int pixels_to_right_border = 50;
  int window_width = 300;
  int window_x = right - pixels_to_right_border;
  window_->SetBounds(gfx::Rect(window_x, 100, window_width, 380));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(window_x, 0), HTLEFT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, window_width, 0), 0);
  EXPECT_EQ(
      gfx::Rect(right - kMinimumOnScreenArea, 100,
                window_width - pixels_to_right_border + kMinimumOnScreenArea,
                380),
      window_->bounds());
}

TEST_F(WorkspaceWindowResizerTest, ResizeWindowOutsideBottomWorkArea) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 50, 0),
      gfx::Insets::TLBR(0, 0, 50, 0));
  int bottom =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get()).bottom();
  int delta_to_bottom = 50;
  int height = 380;
  window_->SetBounds(gfx::Rect(100, bottom - delta_to_bottom, 300, height));
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(
      window_.get(), gfx::Point(0, bottom - delta_to_bottom), HTTOP);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, bottom), 0);
  EXPECT_EQ(gfx::Rect(100, bottom - kMinimumOnScreenArea, 300,
                      height - (delta_to_bottom - kMinimumOnScreenArea)),
            window_->bounds());
}

// Verifies that 'outside' check of the resizer take into account the extended
// desktop in case of repositions.
TEST_F(WorkspaceWindowResizerTest, DragWindowOutsideRightToSecondaryDisplay) {
  // Only primary display.  Changes the window position to fit within the
  // display.
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 50, 0),
      gfx::Insets::TLBR(0, 0, 50, 0));
  int right =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get()).right();
  int pixels_to_right_border = 50;
  int window_width = 300;
  int window_x = right - pixels_to_right_border;
  window_->SetBounds(gfx::Rect(window_x, 100, window_width, 380));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(window_x, 0), HTCAPTION);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, window_width, 0), 0);
  EXPECT_EQ(gfx::Rect(right - kMinimumOnScreenArea, 100, window_width, 380),
            window_->bounds());

  // With secondary display.  Operation itself is same but doesn't change
  // the position because the window is still within the secondary display.
  UpdateDisplay("1000x600,600x400");
  root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets::TLBR(0, 0, 50, 0),
      gfx::Insets::TLBR(0, 0, 50, 0));
  window_->SetBounds(gfx::Rect(window_x, 100, window_width, 380));
  resizer->Drag(CalculateDragPoint(*resizer, window_width, 0), 0);
  EXPECT_EQ(gfx::Rect(window_x + window_width, 100, window_width, 380),
            window_->bounds());
}

// Verifies snapping to edges works.
TEST_F(WorkspaceWindowResizerTest, SnapToEdge) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  // Click 50px to the right so that the mouse pointer does not leave the
  // workspace ensuring sticky behavior.
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(
      window_.get(), window_->bounds().origin() + gfx::Vector2d(50, 0),
      HTCAPTION);
  ASSERT_TRUE(resizer.get());
  int distance_to_left = window_->bounds().x();
  int distance_to_right =
      800 - window_->bounds().width() - window_->bounds().x();
  int distance_to_bottom =
      600 - window_->bounds().height() - window_->bounds().y();
  int distance_to_top = window_->bounds().y();

  // Test left side.
  // Move to an x-coordinate of 15, which should not snap.
  resizer->Drag(CalculateDragPoint(*resizer, 15 - distance_to_left, 0), 0);
  // An x-coordinate of 7 should snap.
  resizer->Drag(CalculateDragPoint(*resizer, 7 - distance_to_left, 0), 0);
  EXPECT_EQ("0,112 320x160", window_->bounds().ToString());
  // Move to -15, should still snap to 0.
  resizer->Drag(CalculateDragPoint(*resizer, -15 - distance_to_left, 0), 0);
  EXPECT_EQ("0,112 320x160", window_->bounds().ToString());
  // At -32 should move past snap points.
  resizer->Drag(CalculateDragPoint(*resizer, -32 - distance_to_left, 0), 0);
  EXPECT_EQ("-32,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, -33 - distance_to_left, 0), 0);
  EXPECT_EQ("-33,112 320x160", window_->bounds().ToString());

  // Right side should similarly snap.
  resizer->Drag(CalculateDragPoint(*resizer, distance_to_right - 15, 0), 0);
  EXPECT_EQ("465,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, distance_to_right - 7, 0), 0);
  EXPECT_EQ("480,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, distance_to_right + 15, 0), 0);
  EXPECT_EQ("480,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, distance_to_right + 32, 0), 0);
  EXPECT_EQ("512,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, distance_to_right + 33, 0), 0);
  EXPECT_EQ("513,112 320x160", window_->bounds().ToString());

  // And the bottom should snap too.
  resizer->Drag(CalculateDragPoint(*resizer, 0, distance_to_bottom - 7), 0);
  EXPECT_EQ(gfx::Rect(96, 440, 320, 160), window_->bounds());
  resizer->Drag(CalculateDragPoint(*resizer, 0, distance_to_bottom + 15), 0);
  EXPECT_EQ(gfx::Rect(96, 440, 320, 160), window_->bounds());
  resizer->Drag(CalculateDragPoint(*resizer, 0, distance_to_bottom - 2 + 32),
                0);
  EXPECT_EQ("96,470 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, distance_to_bottom - 2 + 33),
                0);
  EXPECT_EQ("96,471 320x160", window_->bounds().ToString());

  // And the top should snap too.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -distance_to_top + 20), 0);
  EXPECT_EQ("96,20 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, -distance_to_top + 7), 0);
  EXPECT_EQ("96,0 320x160", window_->bounds().ToString());

  // And bottom/left should snap too.
  resizer->Drag(CalculateDragPoint(*resizer, 7 - distance_to_left,
                                   distance_to_bottom - 7),
                0);
  EXPECT_EQ(gfx::Rect(0, 440, 320, 160), window_->bounds());
  resizer->Drag(CalculateDragPoint(*resizer, -15 - distance_to_left,
                                   distance_to_bottom + 15),
                0);
  EXPECT_EQ(gfx::Rect(0, 440, 320, 160), window_->bounds());
  // should move past snap points.
  resizer->Drag(CalculateDragPoint(*resizer, -32 - distance_to_left,
                                   distance_to_bottom - 2 + 32),
                0);
  EXPECT_EQ("-32,470 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, -33 - distance_to_left,
                                   distance_to_bottom - 2 + 33),
                0);
  EXPECT_EQ("-33,471 320x160", window_->bounds().ToString());

  // No need to test dragging < 0 as we force that to 0.
}

// Verifies a resize snap when dragging TOPLEFT.
TEST_F(WorkspaceWindowResizerTest, SnapToWorkArea_TOPLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTTOPLEFT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -98, -199), 0);
  EXPECT_EQ("0,0 120x230", window_->bounds().ToString());
}

// Verifies a resize snap when dragging TOPRIGHT.
TEST_F(WorkspaceWindowResizerTest, SnapToWorkArea_TOPRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get()));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTTOPRIGHT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, work_area.right() - 120 - 1, -199),
                0);
  EXPECT_EQ(100, window_->bounds().x());
  EXPECT_EQ(work_area.y(), window_->bounds().y());
  EXPECT_EQ(work_area.right() - 100, window_->bounds().width());
  EXPECT_EQ(230, window_->bounds().height());
}

// Verifies a resize snap when dragging BOTTOMRIGHT.
TEST_F(WorkspaceWindowResizerTest, SnapToWorkArea_BOTTOMRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get()));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMRIGHT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, work_area.right() - 120 - 1,
                                   work_area.bottom() - 220 - 2),
                0);
  EXPECT_EQ(100, window_->bounds().x());
  EXPECT_EQ(200, window_->bounds().y());
  EXPECT_EQ(work_area.right() - 100, window_->bounds().width());
  EXPECT_EQ(work_area.bottom() - 200, window_->bounds().height());
}

// Verifies a resize snap when dragging BOTTOMLEFT.
TEST_F(WorkspaceWindowResizerTest, SnapToWorkArea_BOTTOMLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get()));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMLEFT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, -98, work_area.bottom() - 220 - 2),
                0);
  EXPECT_EQ(0, window_->bounds().x());
  EXPECT_EQ(200, window_->bounds().y());
  EXPECT_EQ(120, window_->bounds().width());
  EXPECT_EQ(work_area.bottom() - 200, window_->bounds().height());
}

// Verifies window sticks to both window and work area.
TEST_F(WorkspaceWindowResizerTest, StickToBothEdgeAndWindow) {
  window_->SetBounds(gfx::Rect(10, 10, 20, 50));
  window_->Show();
  window2_->SetBounds(gfx::Rect(150, 160, 25, 1000));
  window2_->Show();

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(10, 10), HTCAPTION);
  ASSERT_TRUE(resizer.get());

  // Move |window| one pixel to the left of |window2|. Should snap to right.
  resizer->Drag(CalculateDragPoint(*resizer, 119, 145), 0);
  gfx::Rect expected(130, 160, 20, 50);
  EXPECT_EQ(expected.ToString(), window_->bounds().ToString());

  gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get()));

  // The initial y position of |window_|.
  int initial_y = 10;
  // The drag position where the window is exactly attached to the bottom.
  int attach_y = work_area.bottom() - window_->bounds().height() - initial_y;

  // Dragging 10px above should not attach to the bottom.
  resizer->Drag(CalculateDragPoint(*resizer, 119, attach_y - 10), 0);
  expected.set_y(attach_y + initial_y - 10);
  EXPECT_EQ(expected.ToString(), window_->bounds().ToString());

  // Stick to the work area.
  resizer->Drag(CalculateDragPoint(*resizer, 119, attach_y - 1), 0);
  expected.set_y(attach_y + initial_y);
  EXPECT_EQ(expected.ToString(), window_->bounds().ToString());

  resizer->Drag(CalculateDragPoint(*resizer, 119, attach_y), 0);
  expected.set_y(attach_y + initial_y);
  EXPECT_EQ(expected.ToString(), window_->bounds().ToString());

  resizer->Drag(CalculateDragPoint(*resizer, 119, attach_y + 1), 0);
  expected.set_y(attach_y + initial_y);
  EXPECT_EQ(expected.ToString(), window_->bounds().ToString());

  // Moving down further should move the window.
  resizer->Drag(CalculateDragPoint(*resizer, 119, attach_y + 18), 0);
  expected.set_y(attach_y + initial_y + 18);
  EXPECT_EQ(expected.ToString(), window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, CtrlDragResizeToExactPosition) {
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMRIGHT);
  ASSERT_TRUE(resizer.get());
  // Resize the right bottom to add 10 in width, 12 in height.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 12), ui::EF_CONTROL_DOWN);
  // Both bottom and right sides to resize to exact size requested.
  EXPECT_EQ("96,112 330x172", window_->bounds().ToString());
}

// Verifies that a dragged, non-snapped window will clear restore bounds.
TEST_F(WorkspaceWindowResizerTest, RestoreClearedOnResize) {
  window_->SetBounds(gfx::Rect(10, 10, 100, 100));
  WindowState* window_state = WindowState::Get(window_.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(50, 50, 50, 50));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMRIGHT);
  ASSERT_TRUE(resizer.get());
  // Drag the window to new position by adding (20, 30) to original point,
  // the original restore bound should be cleared.
  resizer->Drag(CalculateDragPoint(*resizer, 20, 30), 0);
  resizer->CompleteDrag();
  EXPECT_EQ("10,10 120x130", window_->bounds().ToString());
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verifies that a dragged window will restore to its pre-maximized size.
TEST_F(WorkspaceWindowResizerTest, RestoreToPreMaximizeCoordinates) {
  window_->SetBounds(gfx::Rect(0, 0, 1000, 1000));
  WindowState* window_state = WindowState::Get(window_.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(96, 112, 320, 160));
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  ASSERT_TRUE(resizer.get());
  // Drag the window to new position by adding (33, 33) to original point,
  // the window should get restored.
  resizer->Drag(CalculateDragPoint(*resizer, 33, 33), 0);
  resizer->CompleteDrag();
  EXPECT_EQ("33,33 320x160", window_->bounds().ToString());
  // The restore rectangle should get cleared as well.
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verifies that a dragged window will restore to its pre-maximized size.
TEST_F(WorkspaceWindowResizerTest, RevertResizeOperation) {
  const gfx::Rect initial_bounds(0, 0, 200, 400);
  window_->SetBounds(initial_bounds);

  WindowState* window_state = WindowState::Get(window_.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(96, 112, 320, 160));
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  ASSERT_TRUE(resizer.get());
  // Drag the window to new poistion by adding (180, 16) to original point,
  // the window should get restored.
  resizer->Drag(CalculateDragPoint(*resizer, 180, 16), 0);
  resizer->RevertDrag();
  EXPECT_EQ(initial_bounds.ToString(), window_->bounds().ToString());
  EXPECT_EQ(gfx::Rect(96, 112, 320, 160),
            window_state->GetRestoreBoundsInScreen());
}

// Check that only usable sizes get returned by the resizer.
TEST_F(WorkspaceWindowResizerTest, MagneticallyAttach) {
  window_->SetBounds(gfx::Rect(10, 10, 20, 30));
  window2_->SetBounds(gfx::Rect(150, 160, 25, 20));
  window2_->Show();

  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  ASSERT_TRUE(resizer.get());
  // Move |window| one pixel to the left of |window2|. Should snap to right and
  // top.
  resizer->Drag(CalculateDragPoint(*resizer, 119, 145), 0);
  EXPECT_EQ("130,160 20x30", window_->bounds().ToString());

  // Move |window| one pixel to the right of |window2|. Should snap to left and
  // top.
  resizer->Drag(CalculateDragPoint(*resizer, 164, 145), 0);
  EXPECT_EQ("175,160 20x30", window_->bounds().ToString());

  // Move |window| one pixel above |window2|. Should snap to top and left.
  resizer->Drag(CalculateDragPoint(*resizer, 142, 119), 0);
  EXPECT_EQ("150,130 20x30", window_->bounds().ToString());

  // Move |window| one pixel above the bottom of |window2|. Should snap to
  // bottom and left.
  resizer->Drag(CalculateDragPoint(*resizer, 142, 169), 0);
  EXPECT_EQ("150,180 20x30", window_->bounds().ToString());
}

// The following variants verify magnetic snapping during resize when dragging a
// particular edge.
TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_TOP) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->SetBounds(gfx::Rect(99, 179, 10, 20));
  window2_->Show();

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTTOP);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
  EXPECT_EQ("100,199 20x31", window_->bounds().ToString());
}

// Resize window to the top edge of display should not trigger maximize nor
// the maximize dwell timer (crbug.com/1251859)
TEST_F(WorkspaceWindowResizerTest, ResizeTopShouldNotTriggerMaximize) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  AllowSnap(window_.get());
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTTOP);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 50, -195), 0);
  resizer->Drag(CalculateDragPoint(*resizer, 50, -200), 0);
  ASSERT_TRUE(WindowState::Get(window_.get())->IsNormalStateType());
  EXPECT_EQ("100,0 20x230", window_->bounds().ToString());
  EXPECT_FALSE(snap_phantom_window_controller());
  EXPECT_FALSE(IsDwellCountdownTimerRunning());
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_TOPLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->SetBounds(gfx::Rect(99, 179, 10, 20));
  window2_->Show();

  {
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTTOPLEFT);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("99,199 21x31", window_->bounds().ToString());
    resizer->RevertDrag();
  }

  {
    window2_->SetBounds(gfx::Rect(88, 201, 10, 20));
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTTOPLEFT);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("98,201 22x29", window_->bounds().ToString());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_TOPRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->Show();

  {
    window2_->SetBounds(gfx::Rect(111, 179, 10, 20));
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTTOPRIGHT);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("100,199 21x31", window_->bounds().ToString());
    resizer->RevertDrag();
  }

  {
    window2_->SetBounds(gfx::Rect(121, 199, 10, 20));
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTTOPRIGHT);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("100,199 21x31", window_->bounds().ToString());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_RIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->SetBounds(gfx::Rect(121, 199, 10, 20));
  window2_->Show();

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTRIGHT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
  EXPECT_EQ("100,200 21x30", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_BOTTOMRIGHT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->Show();

  {
    window2_->SetBounds(gfx::Rect(122, 212, 10, 20));
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMRIGHT);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("100,200 22x32", window_->bounds().ToString());
    resizer->RevertDrag();
  }

  {
    window2_->SetBounds(gfx::Rect(111, 233, 10, 20));
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMRIGHT);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("100,200 21x33", window_->bounds().ToString());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_BOTTOM) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->SetBounds(gfx::Rect(111, 233, 10, 20));
  window2_->Show();

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOM);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
  EXPECT_EQ("100,200 20x33", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_BOTTOMLEFT) {
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->Show();

  {
    window2_->SetBounds(gfx::Rect(99, 231, 10, 20));
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMLEFT);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("99,200 21x31", window_->bounds().ToString());
    resizer->RevertDrag();
  }

  {
    window2_->SetBounds(gfx::Rect(89, 209, 10, 20));
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMLEFT);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    EXPECT_EQ("99,200 21x29", window_->bounds().ToString());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, MagneticallyResize_LEFT) {
  window2_->SetBounds(gfx::Rect(89, 209, 10, 20));
  window_->SetBounds(gfx::Rect(100, 200, 20, 30));
  window2_->Show();

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTLEFT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
  EXPECT_EQ("99,200 21x30", window_->bounds().ToString());
}

// Test that the user moved window flag is getting properly set.
TEST_F(WorkspaceWindowResizerTest, CheckUserWindowManagedFlags) {
  window_->SetBounds(gfx::Rect(0, 50, 400, 200));
  window_->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorCanMaximize);

  std::vector<aura::Window*> no_attached_windows;
  // Check that an abort doesn't change anything.
  {
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get());
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the bottom.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 100), 0);
    EXPECT_EQ("0,150 400x200", window_->bounds().ToString());
    resizer->RevertDrag();

    EXPECT_FALSE(WindowState::Get(window_.get())->bounds_changed_by_user());
  }

  // Check that a completed move / size does change the user coordinates.
  {
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get());
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the bottom.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 100), 0);
    EXPECT_EQ("0,150 400x200", window_->bounds().ToString());
    resizer->CompleteDrag();
    EXPECT_TRUE(WindowState::Get(window_.get())->bounds_changed_by_user());
  }
}

// Test that a window with a specified max size doesn't exceed it when dragged.
TEST_F(WorkspaceWindowResizerTest, TestMaxSizeEnforced) {
  window_->SetBounds(gfx::Rect(0, 0, 400, 300));
  delegate_.set_maximum_size(gfx::Size(401, 301));

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMRIGHT);
  resizer->Drag(CalculateDragPoint(*resizer, 2, 2), 0);
  EXPECT_EQ(401, window_->bounds().width());
  EXPECT_EQ(301, window_->bounds().height());
}

// Test that a window with a specified max width doesn't restrict its height.
TEST_F(WorkspaceWindowResizerTest, TestPartialMaxSizeEnforced) {
  window_->SetBounds(gfx::Rect(0, 0, 400, 300));
  delegate_.set_maximum_size(gfx::Size(401, 0));

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOMRIGHT);
  resizer->Drag(CalculateDragPoint(*resizer, 2, 2), 0);
  EXPECT_EQ(401, window_->bounds().width());
  EXPECT_EQ(302, window_->bounds().height());
}

// Test that a window with a specified max size can't be snapped.
TEST_F(WorkspaceWindowResizerTest, PhantomSnapNonMaximizable) {
  // Make the window snappable.
  AllowSnap(window_.get());

  {
    // With max size not set we get a phantom window controller for dragging off
    // the right hand side.
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get());
    EXPECT_FALSE(snap_phantom_window_controller());
    resizer->Drag(CalculateDragPoint(*resizer, 801, 0), 0);
    EXPECT_TRUE(snap_phantom_window_controller());
    resizer->RevertDrag();
  }
  {
    // When it can't be maximzied, we get no phantom window for snapping.
    window_->SetBounds(gfx::Rect(0, 0, 400, 200));
    window_->SetProperty(
        aura::client::kResizeBehaviorKey,
        window_->GetProperty(aura::client::kResizeBehaviorKey) ^
            aura::client::kResizeBehaviorCanMaximize);
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get());
    resizer->Drag(CalculateDragPoint(*resizer, 801, 0), 0);
    EXPECT_FALSE(snap_phantom_window_controller());
    resizer->RevertDrag();
  }
}

TEST_F(WorkspaceWindowResizerTest, DontRewardRightmostWindowForOverflows) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  // Four 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect(100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  window4_->SetBounds(gfx::Rect(400, 100, 100, 100));
  delegate2_.set_maximum_size(gfx::Size(101, 0));

  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  windows.push_back(window4_.get());
  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), gfx::Point(), HTRIGHT,
                                    wm::WINDOW_MOVE_SOURCE_MOUSE, windows);
  ASSERT_TRUE(resizer.get());
  // Move it 51 to the left, which should contract w1 and expand w2-4.
  // w2 will hit its max size straight away, and in doing so will leave extra
  // pixels that a naive implementation may award to the rightmost window. A
  // fair implementation will give 25 pixels to each of the other windows.
  resizer->Drag(CalculateDragPoint(*resizer, -51, 0), 0);
  EXPECT_EQ("100,100 49x100", window_->bounds().ToString());
  EXPECT_EQ("149,100 101x100", window2_->bounds().ToString());
  EXPECT_EQ("250,100 125x100", window3_->bounds().ToString());
  EXPECT_EQ("375,100 125x100", window4_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, DontExceedMaxWidth) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  // Four 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect(100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  window4_->SetBounds(gfx::Rect(400, 100, 100, 100));
  delegate2_.set_maximum_size(gfx::Size(101, 0));
  delegate3_.set_maximum_size(gfx::Size(101, 0));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(
          window_.get(), HTRIGHT,
          {window2_.get(), window3_.get(), window4_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 52 to the left, which should contract w1 and expand w2-4.
  resizer->Drag(CalculateDragPoint(*resizer, -52, 0), 0);
  EXPECT_EQ("100,100 48x100", window_->bounds().ToString());
  EXPECT_EQ("148,100 101x100", window2_->bounds().ToString());
  EXPECT_EQ("249,100 101x100", window3_->bounds().ToString());
  EXPECT_EQ("350,100 150x100", window4_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, DontExceedMaxHeight) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  // Four 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect(100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(100, 200, 100, 100));
  window3_->SetBounds(gfx::Rect(100, 300, 100, 100));
  window4_->SetBounds(gfx::Rect(100, 400, 100, 100));
  delegate2_.set_maximum_size(gfx::Size(0, 101));
  delegate3_.set_maximum_size(gfx::Size(0, 101));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(
          window_.get(), HTBOTTOM,
          {window2_.get(), window3_.get(), window4_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 52 up, which should contract w1 and expand w2-4.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -52), 0);
  EXPECT_EQ("100,100 100x48", window_->bounds().ToString());
  EXPECT_EQ("100,148 100x101", window2_->bounds().ToString());
  EXPECT_EQ("100,249 100x101", window3_->bounds().ToString());
  EXPECT_EQ("100,350 100x150", window4_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, DontExceedMinHeight) {
  UpdateDisplay("600x500");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  // Four 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect(100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(100, 200, 100, 100));
  window3_->SetBounds(gfx::Rect(100, 300, 100, 100));
  window4_->SetBounds(gfx::Rect(100, 400, 100, 100));
  delegate2_.set_minimum_size(gfx::Size(0, 99));
  delegate3_.set_minimum_size(gfx::Size(0, 99));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(
          window_.get(), HTBOTTOM,
          {window2_.get(), window3_.get(), window4_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 52 down, which should expand w1 and contract w2-4.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 52), 0);
  EXPECT_EQ("100,100 100x152", window_->bounds().ToString());
  EXPECT_EQ("100,252 100x99", window2_->bounds().ToString());
  EXPECT_EQ("100,351 100x99", window3_->bounds().ToString());
  EXPECT_EQ("100,450 100x50", window4_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, DontExpandRightmostPastMaxWidth) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  // Three 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect(100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  delegate3_.set_maximum_size(gfx::Size(101, 0));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(
          window_.get(), HTRIGHT,
          {window2_.get(), window3_.get(), window4_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 51 to the left, which should contract w1 and expand w2-3.
  resizer->Drag(CalculateDragPoint(*resizer, -51, 0), 0);
  EXPECT_EQ("100,100 49x100", window_->bounds().ToString());
  EXPECT_EQ("149,100 150x100", window2_->bounds().ToString());
  EXPECT_EQ("299,100 101x100", window3_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MoveAttachedWhenGrownToMaxSize) {
  UpdateDisplay("600x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  // Three 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect(100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  delegate2_.set_maximum_size(gfx::Size(101, 0));
  delegate3_.set_maximum_size(gfx::Size(101, 0));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(
          window_.get(), HTRIGHT,
          {window2_.get(), window3_.get(), window4_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 52 to the left, which should contract w1 and expand and move w2-3.
  resizer->Drag(CalculateDragPoint(*resizer, -52, 0), 0);
  EXPECT_EQ("100,100 48x100", window_->bounds().ToString());
  EXPECT_EQ("148,100 101x100", window2_->bounds().ToString());
  EXPECT_EQ("249,100 101x100", window3_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MainWindowHonoursMaxWidth) {
  UpdateDisplay("400x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  // Three 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect(100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  delegate_.set_maximum_size(gfx::Size(102, 0));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(
          window_.get(), HTRIGHT,
          {window2_.get(), window3_.get(), window4_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 50 to the right, which should expand w1 and contract w2-3, as they
  // won't fit in the root window in their original sizes.
  resizer->Drag(CalculateDragPoint(*resizer, 50, 0), 0);
  EXPECT_EQ("100,100 102x100", window_->bounds().ToString());
  EXPECT_EQ("202,100 99x100", window2_->bounds().ToString());
  EXPECT_EQ("301,100 99x100", window3_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, MainWindowHonoursMinWidth) {
  UpdateDisplay("400x800");
  aura::Window* root = Shell::GetPrimaryRootWindow();
  WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  // Three 100x100 windows flush against eachother, starting at 100,100.
  window_->SetBounds(gfx::Rect(100, 100, 100, 100));
  window2_->SetBounds(gfx::Rect(200, 100, 100, 100));
  window3_->SetBounds(gfx::Rect(300, 100, 100, 100));
  delegate_.set_minimum_size(gfx::Size(98, 0));

  std::unique_ptr<WorkspaceWindowResizer> resizer =
      CreateWorkspaceResizerForTest(window_.get(), HTRIGHT,
                                    {window2_.get(), window3_.get()});
  ASSERT_TRUE(resizer.get());
  // Move it 50 to the left, which should contract w1 and expand w2-3.
  resizer->Drag(CalculateDragPoint(*resizer, -50, 0), 0);
  EXPECT_EQ("100,100 98x100", window_->bounds().ToString());
  EXPECT_EQ("198,100 101x100", window2_->bounds().ToString());
  EXPECT_EQ("299,100 101x100", window3_->bounds().ToString());
}

// The following variants test that windows are resized correctly to the edges
// of the screen using touch, when touch point is off of the window border.
TEST_F(WorkspaceWindowResizerTest, TouchResizeToEdge_RIGHT) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);

  InitTouchResizeWindow(gfx::Rect(100, 100, 600, kRootHeight - 200), HTRIGHT);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 200),
            touch_resize_window_->bounds());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     touch_resize_window_.get());

  // Drag out of the right border a bit and check if the border is aligned with
  // the touch point.
  generator.GestureScrollSequence(gfx::Point(715, kRootHeight / 2),
                                  gfx::Point(725, kRootHeight / 2),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 100, 625, kRootHeight - 200),
            touch_resize_window_->bounds());
  // Drag more, but stop before being snapped to the edge.
  generator.GestureScrollSequence(gfx::Point(725, kRootHeight / 2),
                                  gfx::Point(760, kRootHeight / 2),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 100, 660, kRootHeight - 200),
            touch_resize_window_->bounds());
  // Drag even more to snap to the edge.
  generator.GestureScrollSequence(gfx::Point(760, kRootHeight / 2),
                                  gfx::Point(775, kRootHeight / 2),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 100, 700, kRootHeight - 200),
            touch_resize_window_->bounds());
}

TEST_F(WorkspaceWindowResizerTest, TouchResizeToEdge_LEFT) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);

  InitTouchResizeWindow(gfx::Rect(100, 100, 600, kRootHeight - 200), HTLEFT);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 200),
            touch_resize_window_->bounds());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     touch_resize_window_.get());

  // Drag out of the left border a bit and check if the border is aligned with
  // the touch point.
  generator.GestureScrollSequence(gfx::Point(85, kRootHeight / 2),
                                  gfx::Point(75, kRootHeight / 2),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(75, 100, 625, kRootHeight - 200),
            touch_resize_window_->bounds());
  // Drag more, but stop before being snapped to the edge.
  generator.GestureScrollSequence(gfx::Point(75, kRootHeight / 2),
                                  gfx::Point(40, kRootHeight / 2),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(40, 100, 660, kRootHeight - 200),
            touch_resize_window_->bounds());
  // Drag even more to snap to the edge.
  generator.GestureScrollSequence(gfx::Point(40, kRootHeight / 2),
                                  gfx::Point(25, kRootHeight / 2),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(0, 100, 700, kRootHeight - 200),
            touch_resize_window_->bounds());
}

TEST_F(WorkspaceWindowResizerTest, TouchResizeToEdge_TOP) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);

  InitTouchResizeWindow(gfx::Rect(100, 100, 600, kRootHeight - 200), HTTOP);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 200),
            touch_resize_window_->bounds());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     touch_resize_window_.get());

  // Drag out of the top border a bit and check if the border is aligned with
  // the touch point.
  generator.GestureScrollSequence(gfx::Point(400, 85), gfx::Point(400, 75),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 75, 600, kRootHeight - 175),
            touch_resize_window_->bounds());
  // Drag more, but stop before being snapped to the edge.
  generator.GestureScrollSequence(gfx::Point(400, 75), gfx::Point(400, 40),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 40, 600, kRootHeight - 140),
            touch_resize_window_->bounds());
  // Drag even more to snap to the edge.
  generator.GestureScrollSequence(gfx::Point(400, 40), gfx::Point(400, 25),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 0, 600, kRootHeight - 100),
            touch_resize_window_->bounds());
}

TEST_F(WorkspaceWindowResizerTest, TouchResizeToEdge_BOTTOM) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);

  InitTouchResizeWindow(gfx::Rect(100, 100, 600, kRootHeight - 200), HTBOTTOM);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 200),
            touch_resize_window_->bounds());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     touch_resize_window_.get());

  // Drag out of the bottom border a bit and check if the border is aligned with
  // the touch point.
  generator.GestureScrollSequence(gfx::Point(400, kRootHeight - 85),
                                  gfx::Point(400, kRootHeight - 75),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 175),
            touch_resize_window_->bounds());
  // Drag more, but stop before being snapped to the edge.
  generator.GestureScrollSequence(gfx::Point(400, kRootHeight - 75),
                                  gfx::Point(400, kRootHeight - 40),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 140),
            touch_resize_window_->bounds());
  // Drag even more to snap to the edge.
  generator.GestureScrollSequence(gfx::Point(400, kRootHeight - 40),
                                  gfx::Point(400, kRootHeight - 25),
                                  base::Milliseconds(10), 5);
  EXPECT_EQ(gfx::Rect(100, 100, 600, kRootHeight - 100),
            touch_resize_window_->bounds());
}

TEST_F(WorkspaceWindowResizerTest, ResizeHistogram) {
  base::HistogramTester histograms;
  window_->SetBounds(gfx::Rect(20, 30, 400, 60));
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(), HTRIGHT);
  ASSERT_TRUE(resizer.get());
  resizer->Drag(gfx::PointF(50, 50), 0);

  // A resize should generate a histogram.
  EXPECT_NE(gfx::Size(400, 60), window_->bounds().size());
  EXPECT_TRUE(
      ui::WaitForNextFrameToBePresented(window_->GetHost()->compositor()));
  histograms.ExpectTotalCount("Ash.InteractiveWindowResize.TimeToPresent", 1);

  // Completing the drag should not generate another histogram.
  resizer->CompleteDrag();

  // Flush pending draws until there is no frame presented for 100ms (6 frames
  // worth time) and check that histogram is not updated.
  while (ui::WaitForNextFrameToBePresented(window_->GetHost()->compositor(),
                                           base::Milliseconds(100)))
    ;
  histograms.ExpectTotalCount("Ash.InteractiveWindowResize.TimeToPresent", 1);
}

// Tests that windows that are snapped and maximized have to be dragged a
// certain amount before the window bounds change.
TEST_F(WorkspaceWindowResizerTest, DraggingThresholdForSnappedAndMaximized) {
  UpdateDisplay("800x648");
  const gfx::Rect restore_bounds(30, 30, 300, 300);
  window_->SetBounds(restore_bounds);
  AllowSnap(window_.get());

  // Test that on a normal window, there is no minimal drag amount.
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  resizer->Drag(gfx::PointF(1.f, 1.f), 0);
  EXPECT_EQ(gfx::Rect(31, 31, 300, 300), window_->bounds());

  // End the drag in a snap region, and verify the window has snapped.
  resizer->Drag(gfx::PointF(2.f, 2.f), 0);
  resizer->CompleteDrag();
  auto* window_state = WindowState::Get(window_.get());
  ASSERT_TRUE(window_state->IsSnapped());
  const gfx::Rect snapped_bounds(400, 600);
  EXPECT_EQ(snapped_bounds, window_->bounds());

  resizer.reset();
  resizer = CreateResizerForTest(window_.get());

  // Tests that small drags do not change the window bounds.
  resizer->Drag(gfx::PointF(1.f, 1.f), 0);
  EXPECT_EQ(snapped_bounds, window_->bounds());

  // A large enough drag will change the window bounds to its restore bounds.
  resizer->Drag(gfx::PointF(10.f, 10.f), 0);
  EXPECT_EQ(restore_bounds.size(), window_->bounds().size());

  // Tests that on drag end, the window will get restored. Make sure the drag
  // does not end in a snap region.
  resizer->Drag(gfx::PointF(200.f, 200.f), 0);
  resizer->CompleteDrag();
  EXPECT_EQ(restore_bounds.size(), window_->bounds().size());
  EXPECT_TRUE(window_state->IsNormalStateType());

  // Tests the same things as the snapped window case for the maximized window
  // case.
  window_state->Maximize();
  const gfx::Rect maximized_bounds(800, 600);
  EXPECT_EQ(maximized_bounds, window_->bounds());
  resizer.reset();
  resizer = CreateResizerForTest(window_.get());
  resizer->Drag(gfx::PointF(1.f, 1.f), 0);
  EXPECT_EQ(maximized_bounds, window_->bounds());
  resizer->Drag(gfx::PointF(10.f, 10.f), 0);
  EXPECT_EQ(restore_bounds.size(), window_->bounds().size());
  resizer->Drag(gfx::PointF(200.f, 200.f), 0);
  resizer->CompleteDrag();
  EXPECT_EQ(restore_bounds.size(), window_->bounds().size());
  EXPECT_TRUE(window_state->IsNormalStateType());
}

// Tests dragging a window and snapping it to maximized state.
TEST_F(WorkspaceWindowResizerTest, DragToSnapMaximize) {
  UpdateDisplay("800x648");
  const gfx::Rect restore_bounds(30, 30, 300, 300);
  window_->SetBounds(restore_bounds);
  AllowSnap(window_.get());

  // Drag to a top region of the display and release. The window should be
  // maximized.
  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  resizer->Drag(gfx::PointF(400.f, 400.f), 0);
  resizer->Drag(gfx::PointF(400.f, 2.f), 0);
  DwellCountdownTimerFireNow();
  resizer->CompleteDrag();
  auto* window_state = WindowState::Get(window_.get());
  ASSERT_TRUE(window_state->IsMaximized());

  // Tests that dragging a maximized window and snapping it back to maximized
  // state works as expected.
  resizer.reset();
  resizer = CreateResizerForTest(window_.get());

  // First drag to "unmaximize". The window is still maximized at this point,
  // but the bounds have shrunk to make it look restored.
  resizer->Drag(gfx::PointF(200.f, 200.f), 0);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(gfx::Size(300, 300), window_->bounds().size());

  // End the drag in the snap to maximize region. The window should be maximized
  // and sized to fit the whole work area.
  resizer->Drag(gfx::PointF(200.f, 2.f), 0);
  DwellCountdownTimerFireNow();
  EXPECT_TRUE(!snap_phantom_window_controller()->GetMaximizeCueForTesting());
  resizer->CompleteDrag();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(800, 600), window_->bounds());
}

TEST_F(WorkspaceWindowResizerTest, DragToMaximizeStartingInSnapRegion) {
  UpdateDisplay("800x648");

  // Drag starting in the snap to maximize region. If we do not leave it, on
  // drag release the window will not get maximized.
  window_->SetBounds(gfx::Rect(200, 200));
  AllowSnap(window_.get());

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(400.f, 1.f));
  resizer->Drag(gfx::PointF(400.f, 5.f), 0);
  resizer->CompleteDrag();
  ASSERT_FALSE(WindowState::Get(window_.get())->IsMaximized());

  // This time exit the snap region. On returning and completing the drag, the
  // window should be maximized.
  WindowState::Get(window_.get())->Maximize();
  resizer.reset();
  resizer = CreateResizerForTest(window_.get(), gfx::Point(400.f, 1.f));
  resizer->Drag(gfx::PointF(400.f, 400.f), 0);
  resizer->Drag(gfx::PointF(400.f, 5.f), 0);
  DwellCountdownTimerFireNow();
  resizer->CompleteDrag();
  EXPECT_TRUE(WindowState::Get(window_.get())->IsMaximized());
}

TEST_F(WorkspaceWindowResizerTest, DragToMaximizeNumOfMisTriggersMetric) {
  base::HistogramTester histogram_tester;
  auto* window = window_.get();
  auto* window2 = window2_.get();
  AllowSnap(window);
  AllowSnap(window2);

  // Drag to maximize `window_` again.
  DragToMaximize(window);

  // Immediately restore it.
  DragToRestore(window);

  // Drag to maximize again immediately after it's restored.
  DragToMaximize(window);

  // Immediately restore it again.
  DragToRestore(window);

  // Wait for the drag to maximize behavior to be checked.
  DragToMaximizeBehaviorCheckCountdownTimerFireNow(window);

  window_.reset();

  // Verify that during the lifetime of `window_`, there're 2 drag to maximize
  // mis-triggers on it.
  histogram_tester.ExpectBucketCount(
      "Ash.Window.DragMaximized.NumberOfMisTriggers", 2, 1);

  // Drag to maximize `window2_`.
  DragToMaximize(window2);
  // Immediately restore it.
  DragToRestore(window2);

  DragToMaximizeBehaviorCheckCountdownTimerFireNow(window2);

  window2_.reset();

  // Verify that during the lifetime of `window2_`, there is one drag to
  // maximize mis-trigger on it.
  histogram_tester.ExpectBucketCount(
      "Ash.Window.DragMaximized.NumberOfMisTriggers", 1, 1);

  window3_.reset();
  // Verify that the number of mis-triggers is not recorded for `window3`, since
  // it's never been dragged to maximized.
  histogram_tester.ExpectBucketCount(
      "Ash.Window.DragMaximized.NumberOfMisTriggers", 0, 0);
}

TEST_F(WorkspaceWindowResizerTest, DragToMaximizeValidMetric) {
  base::HistogramTester histogram_tester;
  auto* window = window_.get();
  AllowSnap(window);

  // Drag to maximize `window_`.
  DragToMaximize(window);

  // Now immediately drag to restore `window_`.
  DragToRestore(window);

  // Wait for the drag to maximize behavior to be checked.
  DragToMaximizeBehaviorCheckCountdownTimerFireNow(window);

  // Verify that since `window_` is restored immediately after is dragged to
  // maximized, drag to maximize behavior should be considered as invalid.
  histogram_tester.ExpectBucketCount("Ash.Window.DragMaximized.Valid", true, 0);
  histogram_tester.ExpectBucketCount("Ash.Window.DragMaximized.Valid", false,
                                     1);

  // Drag to maximize `window_` again.
  DragToMaximize(window);

  // Don't restore `window_` and wait for the drag to maximize behavior to be
  // checked.
  DragToMaximizeBehaviorCheckCountdownTimerFireNow(window);

  DragToRestore(window);

  // Verify this time the drag to maximize behavior should be considered as
  // valid since the window is restored after 5 seconds.
  histogram_tester.ExpectBucketCount("Ash.Window.DragMaximized.Valid", true, 1);
  histogram_tester.ExpectBucketCount("Ash.Window.DragMaximized.Valid", false,
                                     1);

  // Drag to maximize `window_` again.
  DragToMaximize(window);

  // Immediately restore it.
  DragToRestore(window);

  // Drag to maximize again immediately after it's restored.
  DragToMaximize(window);

  // Verify that the first drag to maximize should be considered as invalid.
  histogram_tester.ExpectBucketCount("Ash.Window.DragMaximized.Valid", false,
                                     2);

  // Immediately restore it again.
  DragToRestore(window);

  // Wait for the drag to maximize behavior to be checked.
  DragToMaximizeBehaviorCheckCountdownTimerFireNow(window);

  // Verify that the second drag to maximize should also be considered as
  // invalid.'
  histogram_tester.ExpectBucketCount("Ash.Window.DragMaximized.Valid", false,
                                     3);
}

// Makes sure that we are not creating any resizer in kiosk mode.
TEST_F(WorkspaceWindowResizerTest, DoesNotWorkInAppMode) {
  GetSessionControllerClient()->SetIsRunningInAppMode(true);
  window_->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorNone);
  EXPECT_FALSE(CreateResizerForTest(window_.get()));
  window_->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorCanResize);
  EXPECT_TRUE(CreateResizerForTest(window_.get()));
}

TEST_F(WorkspaceWindowResizerTest, DoNotCreateResizerIfNotActiveSession) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);
  EXPECT_FALSE(CreateResizerForTest(window_.get()));

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_FALSE(CreateResizerForTest(window_.get()));

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(CreateResizerForTest(window_.get()));

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(CreateResizerForTest(window_.get()));
}

// Tests that windows dragged across multiple displays have their restore bounds
// updated.
TEST_F(WorkspaceWindowResizerTest, MultiDisplayRestoreBounds) {
  UpdateDisplay("800x700,800x700");

  // Create a window and maximize it on the primary display.
  window_->SetBounds(gfx::Rect(200, 200));
  AllowSnap(window_.get());

  auto* window_state = WindowState::Get(window_.get());
  window_state->Maximize();
  ASSERT_TRUE(window_state->HasRestoreBounds());
  ASSERT_EQ(gfx::Rect(200, 200), window_state->GetRestoreBoundsInScreen());

  // Drag the window to the secondary display and end it in a snap to maximize
  // region. Drag() doesn't update the display, so manually do it in the test.
  // Also we need to first drag the window out of the maximized snap zone,
  // otherwise it won't snap on drag completed.
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(400.f, 1.f), HTCAPTION);
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(
          gfx::Point(1200, 200)));
  resizer->Drag(gfx::PointF(1200.f, 200.f), 0);
  resizer->Drag(gfx::PointF(1200.f, 5.f), 0);
  DwellCountdownTimerFireNow();
  resizer->CompleteDrag();
  ASSERT_TRUE(window_state->IsMaximized());

  // Tests that the window and its restore bounds on on the secondary display.
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(display::Screen::GetScreen()->GetAllDisplays()[1].id(),
            window_state->GetDisplay().id());
  EXPECT_EQ(gfx::Rect(800, 0, 200, 200),
            window_state->GetRestoreBoundsInScreen());
}

// Tests that after dragging and flinging a maximized or snapped window,
// restoring it will have the same size it used to before it was maximized or
// snapped.
TEST_F(WorkspaceWindowResizerTest, FlingRestoreSize) {
  // Init the window with |window_size| before maximizing it. This is the
  // expected size after flinging the maximized window and restoring it.
  gfx::Size window_size(300, 300);
  InitTouchResizeWindow(gfx::Rect(gfx::Point(100, 100), window_size),
                        HTCAPTION);
  auto* window_state = WindowState::Get(touch_resize_window_.get());
  window_state->Maximize();
  ASSERT_TRUE(window_state->IsMaximized());

  // Create a fling sequence on |touch_resize_window_|. Verify that this results
  // in the window being minimized.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     touch_resize_window_.get());
  generator.GestureScrollSequence(gfx::Point(400, 10), gfx::Point(400, 210),
                                  base::Milliseconds(10), 10);
  ASSERT_TRUE(window_state->IsMinimized());

  // After unminimzing, the window bounds are the size they were before
  // maximizing.
  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(window_size, touch_resize_window_->bounds().size());

  // Snap a window and do the same test.
  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);
  ASSERT_TRUE(window_state->IsSnapped());
  const gfx::Rect snapped_bounds = window_state->window()->bounds();

  generator.GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 210),
                                  base::Milliseconds(10), 10);
  ASSERT_TRUE(window_state->IsMinimized());

  // After unminimzing, the window bounds are the size they were before
  // minimizing.
  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(snapped_bounds, touch_resize_window_->bounds());
}

using MultiDisplayWorkspaceWindowResizerTest = AshTestBase;

// Makes sure that window drag magnetism still works when a window is dragged
// between different displays.
TEST_F(MultiDisplayWorkspaceWindowResizerTest, Magnetism) {
  UpdateDisplay("800x600,600x500");
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  // Create two windows one on the first display, and the other on the second.
  auto win1 = CreateToplevelTestWindow(gfx::Rect(10, 10, 100, 100), /*id=*/1);
  auto win2 = CreateToplevelTestWindow(gfx::Rect(1250, 100, 50, 50), /*id=*/2);
  EXPECT_EQ(win1->GetRootWindow(), roots[0]);
  EXPECT_EQ(win2->GetRootWindow(), roots[1]);

  std::unique_ptr<WindowResizer> resizer = CreateWindowResizer(
      win1.get(), gfx::PointF(), HTCAPTION, wm::WINDOW_MOVE_SOURCE_MOUSE);
  ASSERT_TRUE(resizer.get());

  // Drag `win1` such that its right edge is 5 pixels from the left edge of
  // `win2`. Expect that `win1` will snap to `win2` on its left edge.
  resizer->Drag(CalculateDragPoint(*resizer, 1135, 0), /*event_flags=*/0);
  EXPECT_EQ(gfx::Rect(1150, 10, 100, 100), win1->GetBoundsInScreen());
}

// Makes sure that window drag locations are correct when a window is dragged
// between different displays.
TEST_F(MultiDisplayWorkspaceWindowResizerTest, DragWindowBetweenDisplays) {
  UpdateDisplay("800x600,1200x800@1.25");
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  // Create a window on the extended display.
  const gfx::Rect initial_bounds_in_screen(850, 100, 200, 150);
  auto win = CreateToplevelTestWindow(initial_bounds_in_screen);
  EXPECT_EQ(win->GetRootWindow(), roots[1]);

  auto delegate = std::make_unique<FakeWindowStateDelegate>();
  auto* delegate_ptr = delegate.get();
  auto* window_state = WindowState::Get(win.get());
  window_state->SetDelegate(std::move(delegate));

  const gfx::PointF initial_drag_point_in_screen(
      initial_bounds_in_screen.CenterPoint());
  gfx::PointF initial_drag_point_in_parent(initial_drag_point_in_screen);
  wm::ConvertPointFromScreen(win->GetRootWindow(),
                             &initial_drag_point_in_parent);

  // Create resizer with the initial drag location at the center of the window.
  std::unique_ptr<WindowResizer> resizer =
      CreateWindowResizer(win.get(), initial_drag_point_in_parent, HTCAPTION,
                          wm::WINDOW_MOVE_SOURCE_MOUSE);
  ASSERT_TRUE(resizer.get());

  const gfx::Vector2d drag_offset(-600, 0);
  const gfx::PointF drag_point_in_parent =
      CalculateDragPoint(*resizer, drag_offset.x(), drag_offset.y());

  resizer->Drag(drag_point_in_parent, /*event_flags=*/0);
  gfx::Rect expected_bounds_in_screen(initial_bounds_in_screen);
  expected_bounds_in_screen.Offset(drag_offset);
  EXPECT_EQ(expected_bounds_in_screen, win->GetBoundsInScreen());

  resizer->CompleteDrag();
  gfx::PointF expected_drag_point_in_screen(initial_drag_point_in_screen);
  expected_drag_point_in_screen.Offset(drag_offset.x(), drag_offset.y());
  EXPECT_EQ(expected_drag_point_in_screen, delegate_ptr->drag_end_location());
}

// Make sure metrics is recorded during tab dragging.
TEST_F(WorkspaceWindowResizerTest, TabDraggingHistogram) {
  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  struct {
    bool is_dragging_tab;
    gfx::PointF drag_to_point;
    int expected_latency_count;
    int expected_max_latency_count;
  } kTestCases[] = {// A tab dragging should generate a histogram.
                    {/*is_dragging_tab*/ true, gfx::PointF(200, 200), 1, 1},
                    // A window dragging should not generate a histogram.
                    {/*is_dragging_tab*/ false, gfx::PointF(200, 200), 0, 0},
                    // A tab dragging should not generate a histogram when
                    // the drag touches a different display.
                    {/*is_dragging_tab*/ true, gfx::PointF(850, 200), 0, 0}};

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "is_dragging_tab=" << test.is_dragging_tab);

    base::HistogramTester histogram_tester;
    window_->SetBounds(gfx::Rect(100, 100, 100, 100));
    window_->SetProperty(ash::kIsDraggingTabsKey, test.is_dragging_tab);

    std::unique_ptr<WindowResizer> resizer = CreateWindowResizer(
        window_.get(), gfx::PointF(), HTCAPTION, wm::WINDOW_MOVE_SOURCE_MOUSE);
    ASSERT_TRUE(resizer.get());
    resizer->Drag(test.drag_to_point, 0);

    EXPECT_TRUE(
        ui::WaitForNextFrameToBePresented(window_->GetHost()->compositor()));

    resizer->CompleteDrag();
    resizer.reset(nullptr);

    histogram_tester.ExpectTotalCount(
        "Ash.TabDrag.PresentationTime.ClamshellMode",
        test.expected_latency_count);
    histogram_tester.ExpectTotalCount(
        "Ash.TabDrag.PresentationTime.MaxLatency.ClamshellMode",
        test.expected_max_latency_count);
  }
}

// Test dwell time before snap to maximize.
TEST_F(WorkspaceWindowResizerTest, SnapMaximizeDwellTime) {
  UpdateDisplay("800x648");
  window_->SetBounds(gfx::Rect(10, 10, 100, 100));
  AllowSnap(window_.get());

  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(window_.get());
  // Ensure the timer is not running.
  EXPECT_FALSE(IsDwellCountdownTimerRunning());
  // Test when dwell timer finished countdown window is maximized.
  resizer->Drag(gfx::PointF(400.f, 400.f), 0);
  resizer->Drag(gfx::PointF(100.f, 3.f), 0);
  // Timer is triggered.
  EXPECT_TRUE(IsDwellCountdownTimerRunning());
  DwellCountdownTimerFireNow();
  resizer->CompleteDrag();
  auto* window_state = WindowState::Get(window_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_state->IsMaximized());

  // If dwell timer doesn't finish countdown,
  // Window will not be maximized.
  resizer.reset();
  resizer = CreateResizerForTest(window_.get());
  resizer->Drag(gfx::PointF(400.f, 400.f), 0);
  resizer->Drag(gfx::PointF(100.f, 3.f), 0);
  // Timer is triggered.
  EXPECT_TRUE(IsDwellCountdownTimerRunning());
  resizer->CompleteDrag();
  window_state = WindowState::Get(window_.get());
  EXPECT_FALSE(window_state->IsMaximized());

  // Once dwell timer starts, drag away the window
  // will not maximize the window if move more than
  // kSnapDragDwellTimeResetThreshold.
  resizer.reset();
  resizer = CreateResizerForTest(window_.get());
  resizer->Drag(gfx::PointF(400.f, 400.f), 0);
  resizer->Drag(gfx::PointF(100.f, 3.f), 0);
  DwellCountdownTimerFireNow();
  resizer->Drag(gfx::PointF(200.f, 3.f), 0);
  // Timer is triggered.
  EXPECT_TRUE(IsDwellCountdownTimerRunning());
  resizer->CompleteDrag();
  window_state = WindowState::Get(window_.get());
  EXPECT_FALSE(window_state->IsMaximized());

  // Once dwell timer starts, drag away the window
  // can still maximize the window if move less than
  // kSnapDragDwellTimeResetThreshold.
  resizer.reset();
  resizer = CreateResizerForTest(window_.get());
  resizer->Drag(gfx::PointF(400.f, 400.f), 0);
  resizer->Drag(gfx::PointF(100.f, 3.f), 0);
  DwellCountdownTimerFireNow();
  resizer->Drag(gfx::PointF(101.f, 3.f), 0);
  // Timer is triggered.
  EXPECT_TRUE(IsDwellCountdownTimerRunning());
  resizer->CompleteDrag();
  window_state = WindowState::Get(window_.get());
  EXPECT_TRUE(window_state->IsMaximized());
}

// Test horizontal move won't trigger snap.
TEST_F(WorkspaceWindowResizerTest, HorizontalMoveNotTriggerSnap) {
  UpdateDisplay("800x648");
  window_->SetBounds(gfx::Rect(10, 10, 100, 100));
  AllowSnap(window_.get());

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window_.get(), gfx::Point(400.f, 67.f));
  // Check if a horizontal move more than threshold will trigger snap.
  resizer->Drag(gfx::PointF(400.f, 67.f), 0);
  resizer->Drag(gfx::PointF(400.f, 2.f), 0);
  DwellCountdownTimerFireNow();
  resizer->CompleteDrag();
  auto* window_state = WindowState::Get(window_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_state->IsMaximized());

  // Check if a horizontal move less than threshold won't trigger snap.
  resizer.reset();
  resizer = CreateResizerForTest(window_.get());
  resizer->Drag(gfx::PointF(1.f, 1.f), 0);
  resizer->Drag(gfx::PointF(100.f, 1.f), 0);
  resizer->CompleteDrag();
  window_state = WindowState::Get(window_.get());
  EXPECT_FALSE(window_state->IsMaximized());
}

class PortraitWorkspaceWindowResizerTest : public WorkspaceWindowResizerTest {
 public:
  PortraitWorkspaceWindowResizerTest() = default;
  PortraitWorkspaceWindowResizerTest(
      const PortraitWorkspaceWindowResizerTest&) = delete;
  PortraitWorkspaceWindowResizerTest& operator=(
      const PortraitWorkspaceWindowResizerTest&) = delete;
  ~PortraitWorkspaceWindowResizerTest() override = default;

  // WorkspaceWindowResizerTest:
  void SetUp() override {
    WorkspaceWindowResizerTest::SetUp();
    UpdateDisplay("600x800");

    // Make the window snappable.
    AllowSnap(window_.get());
  }
};

// Tests that dragging window to top triggers top snap.
TEST_F(PortraitWorkspaceWindowResizerTest, SnapTop) {
  const gfx::Rect restore_bounds(50, 50, 100, 100);
  window_->SetBounds(restore_bounds);
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get());
  const float work_area_center_x = work_area.CenterPoint().x();

  constexpr int kScreenEdgeInsetForSnappingTop = 8;
  std::unique_ptr<WindowResizer> resizer(
      CreateResizerForTest(window_.get(), gfx::Point(0, 100), HTCAPTION));
  // Drag to a top-snap region should snap top.
  resizer->Drag(
      gfx::PointF(work_area_center_x, kScreenEdgeInsetForSnappingTop + 1), 0);
  EXPECT_FALSE(snap_phantom_window_controller());
  resizer->Drag(gfx::PointF(work_area_center_x, kScreenEdgeInsetForSnappingTop),
                0);
  auto* phantom_controller = snap_phantom_window_controller();
  ASSERT_TRUE(phantom_controller);

  const gfx::Rect expected_snapped_bounds(work_area.width(),
                                          work_area.height() / 2);
  EXPECT_EQ(expected_snapped_bounds,
            phantom_controller->GetTargetWindowBounds());
  EXPECT_TRUE(!!snap_phantom_window_controller()->GetMaximizeCueForTesting());
  resizer->CompleteDrag();
  EXPECT_TRUE(WindowState::Get(window_.get())->IsSnapped());
  EXPECT_EQ(expected_snapped_bounds, window_->bounds());
}

// Tests that dragging window to bottom area trigger bottom snap.
TEST_F(PortraitWorkspaceWindowResizerTest, SnapBottom) {
  const gfx::Rect restore_bounds(50, 50, 100, 100);
  window_->SetBounds(restore_bounds);
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get());
  const float work_area_center_x = work_area.CenterPoint().x();

  std::unique_ptr<WindowResizer> resizer(
      CreateResizerForTest(window_.get(), gfx::Point(), HTCAPTION));
  constexpr int kScreenEdgeInsetForSnappingSides = 32;
  EXPECT_FALSE(snap_phantom_window_controller());
  // Drag to a bottom-snap region should snap bottom. Bottom area should
  // be with
  resizer->Drag(
      gfx::PointF(work_area_center_x,
                  work_area.bottom() - kScreenEdgeInsetForSnappingSides - 2),
      0);
  EXPECT_FALSE(snap_phantom_window_controller());
  resizer->Drag(
      gfx::PointF(work_area_center_x,
                  work_area.bottom() - kScreenEdgeInsetForSnappingSides - 1),
      0);
  ASSERT_TRUE(snap_phantom_window_controller());

  const gfx::Rect expected_snapped_bounds(
      0, work_area.bottom() / 2, work_area.width(), work_area.height() / 2);

  resizer->CompleteDrag();
  EXPECT_TRUE(WindowState::Get(window_.get())->IsSnapped());
  EXPECT_EQ(expected_snapped_bounds, window_->bounds());
}

// Tests that in portrait display, holding a window at top position longer can
// transform top-snap phantom to maximize phantom window. Moreover, top snap
// phantom displays the maximize cue widget and hides the cue as soon as it
// transforms to maximize phantom.
TEST_F(PortraitWorkspaceWindowResizerTest, SnapTopTransitionToMaximize) {
  const gfx::Rect restore_bounds(50, 50, 100, 100);
  window_->SetBounds(restore_bounds);
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get());
  const float work_area_center_x = work_area.CenterPoint().x();

  constexpr int kScreenEdgeInsetForSnappingTop = 8;
  std::unique_ptr<WindowResizer> resizer(
      CreateResizerForTest(window_.get(), gfx::Point(0, 100), HTCAPTION));
  // Drag to a top-snap region.
  resizer->Drag(
      gfx::PointF(work_area_center_x, kScreenEdgeInsetForSnappingTop + 1), 0);
  EXPECT_FALSE(snap_phantom_window_controller());
  resizer->Drag(gfx::PointF(work_area_center_x, kScreenEdgeInsetForSnappingTop),
                0);
  auto* phantom_controller = snap_phantom_window_controller();
  ASSERT_TRUE(phantom_controller);

  // During dragging to snap top, the top phantom window should show up along
  // with the maximize cue widget.
  const gfx::Rect expected_top_snapped_bounds(work_area.width(),
                                              work_area.height() / 2);
  EXPECT_EQ(expected_top_snapped_bounds,
            phantom_controller->GetTargetWindowBounds());
  auto* maximize_cue_widget = phantom_controller->GetMaximizeCueForTesting();
  EXPECT_TRUE(!!maximize_cue_widget);
  EXPECT_TRUE(maximize_cue_widget->IsVisible());
  EXPECT_EQ(1.f, maximize_cue_widget->GetLayer()->opacity());
  EXPECT_TRUE(IsDwellCountdownTimerRunning());

  // Once the count down ends, the maximize cue widget is hidden from the view
  // and the top-snap phantom turns into maximize phantom window.
  DwellCountdownTimerFireNow();
  EXPECT_EQ(0.f, maximize_cue_widget->GetLayer()->opacity());
  EXPECT_EQ(work_area, phantom_controller->GetTargetWindowBounds());
  resizer->CompleteDrag();
  EXPECT_TRUE(WindowState::Get(window_.get())->IsMaximized());
  EXPECT_EQ(work_area, window_->bounds());
}

// Verifies the behavior of resizing a vertically snapped window.
TEST_F(PortraitWorkspaceWindowResizerTest, ResizeSnapped) {
  WindowState* window_state = WindowState::Get(window_.get());
  AllowSnap(window_.get());

  const gfx::Rect kInitialBounds(100, 100, 100, 100);
  window_->SetBounds(kInitialBounds);
  window_->Show();
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_.get());

  const WindowSnapWMEvent snap_top(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_top);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  gfx::Rect expected_snap_bounds =
      gfx::Rect(work_area.width(), work_area.height() / 2);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_EQ(expected_snap_bounds, window_->bounds());
  EXPECT_EQ(kInitialBounds, window_state->GetRestoreBoundsInParent());

  {
    // 1) Resizing a vertically snapped window to make it higher should not
    // unsnap the window.
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTBOTTOM);
    resizer->Drag(CalculateDragPoint(*resizer, 0, 30), 0);
    resizer->CompleteDrag();
    EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
    expected_snap_bounds.Inset(gfx::Insets::TLBR(0, 0, -30, 0));
    EXPECT_EQ(expected_snap_bounds, window_->bounds());
    EXPECT_EQ(kInitialBounds, window_state->GetRestoreBoundsInParent());
  }

  {
    // 2) Resizing a vertically snapped window horizontally and then undoing
    // the change should not unsnap.
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTLEFT);
    resizer->Drag(CalculateDragPoint(*resizer, 30, 0), 0);
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0), 0);
    resizer->CompleteDrag();
    EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
    EXPECT_EQ(expected_snap_bounds, window_->bounds());
    EXPECT_EQ(kInitialBounds, window_state->GetRestoreBoundsInParent());
  }

  {
    // 3) Resizing a vertically snapped window horizontally should unsnap.
    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(), HTLEFT);
    resizer->Drag(CalculateDragPoint(*resizer, 30, 0), 0);
    resizer->CompleteDrag();
    EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
    expected_snap_bounds.Inset(gfx::Insets::TLBR(0, 30, 0, 0));
    EXPECT_EQ(expected_snap_bounds, window_->bounds());
    EXPECT_FALSE(window_state->HasRestoreBounds());
  }
}

using MultiOrientationDisplayWorkspaceWindowResizerTest =
    WorkspaceWindowResizerTest;

// Test WorkspaceWindowResizer functionalities for two displays with different
// orientation: landscape and portrait. Assertions around dragging near the four
// edges of the display.
TEST_F(MultiOrientationDisplayWorkspaceWindowResizerTest, Edge) {
  UpdateDisplay("800x600,500x600");

  // Make the window snappable.
  AllowSnap(window_.get());

  window_->SetBounds(gfx::Rect(20, 30, 400, 60));
  WindowState* window_state = WindowState::Get(window_.get());
  // Test dragging to another display and snapping there.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  const gfx::Rect display2_work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(root_windows[1])
          .work_area();
  {
    EXPECT_EQ(gfx::Rect(20, 30, 400, 60), window_->GetBoundsInScreen());

    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get());
    ASSERT_TRUE(resizer.get());
    // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse
    // input without having to call |CursorManager::SetDisplay|. Move to the
    // second display. Drag to bottom right area of the second display to
    // trigger the bottom snap if vertical snap is enabled or the right snap
    // otherwise.
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
    resizer->Drag(CalculateDragPoint(*resizer, display2_work_area.right(),
                                     display2_work_area.bottom()),
                  0);
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());

    const gfx::Rect secondary_snap_bounds =
        gfx::Rect(0, display2_work_area.height() / 2,
                  display2_work_area.width(), display2_work_area.height() / 2);
    EXPECT_EQ(secondary_snap_bounds, window_->bounds());
    EXPECT_EQ(gfx::Rect(820, 30, 400, 60),
              window_state->GetRestoreBoundsInScreen());
  }

  // Restore the window to clear snapped state.
  window_state->Restore();

  {
    // Test dragging from a secondary display and snapping on the same display.
    EXPECT_EQ(gfx::Rect(820, 30, 400, 60), window_->GetBoundsInScreen());

    std::unique_ptr<WindowResizer> resizer =
        CreateResizerForTest(window_.get(), gfx::Point(0, 100));
    ASSERT_TRUE(resizer.get());
    // TODO(crbug.com/40638870): Unit tests should be able to simulate mouse
    // input without having to call |CursorManager::SetDisplay|. Drag to top
    // left area of the second display to trigger the top snap if vertical snap
    // is enabled or the bottom snap otherwise.
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
    resizer->Drag(CalculateDragPoint(*resizer, 0, -95), 0);
    resizer->Drag(CalculateDragPoint(*resizer, 0, -100), 0);
    resizer->CompleteDrag();
    const gfx::Rect primary_snap_bounds =
        gfx::Rect(display2_work_area.width(), display2_work_area.height() / 2);
    EXPECT_EQ(primary_snap_bounds, window_->bounds());
    EXPECT_EQ(gfx::Rect(820, 30, 400, 60),
              window_state->GetRestoreBoundsInScreen());
  }
}

}  // namespace ash
