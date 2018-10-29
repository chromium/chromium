// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/ws/test_window_tree_client.h"
#include "services/ws/window_tree_test_helper.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

// A testing DragDropDelegate that accepts any drops.
class TestDragDropDelegate : public aura::client::DragDropDelegate {
 public:
  TestDragDropDelegate() = default;
  ~TestDragDropDelegate() override = default;

  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {}
  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }
  void OnDragExited() override {}
  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDragDropDelegate);
};

}  // namespace

// This test creates a top-level window via the WindowService in SetUp() and
// provides some ease of use functions to access WindowService related state.
class WindowServiceDelegateImplTest : public AshTestBase {
 public:
  WindowServiceDelegateImplTest() = default;
  ~WindowServiceDelegateImplTest() override = default;

  ws::Id GetTopLevelWindowId() {
    return GetWindowTreeTestHelper()->TransportIdForWindow(top_level_.get());
  }

  wm::WmToplevelWindowEventHandler* event_handler() {
    return Shell::Get()
        ->toplevel_window_event_handler()
        ->wm_toplevel_window_event_handler();
  }

  std::vector<ws::Change>* GetWindowTreeClientChanges() {
    return GetTestWindowTreeClient()->tracker()->changes();
  }

  void SetCanAcceptDrops() {
    aura::client::SetDragDropDelegate(top_level_.get(),
                                      &test_drag_drop_delegate_);
  }

  bool IsDragDropInProgress() {
    return aura::client::GetDragDropClient(top_level_->GetRootWindow())
        ->IsDragDropInProgress();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    top_level_ = CreateTestWindow(gfx::Rect(100, 100, 100, 100));
    ASSERT_TRUE(top_level_);
    GetEventGenerator()->PressLeftButton();
  }
  void TearDown() override {
    // Ash owns the WindowTree, which also handles deleting |top_level_|. This
    // needs to delete |top_level_| before the WindowTree is deleted, otherwise
    // the WindowTree will delete |top_level_|, leading to a double delete.
    top_level_.reset();
    AshTestBase::TearDown();
  }

 protected:
  TestDragDropDelegate test_drag_drop_delegate_;
  std::unique_ptr<aura::Window> top_level_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowServiceDelegateImplTest);
};

TEST_F(WindowServiceDelegateImplTest, RunWindowMoveLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      gfx::Point());
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(gfx::Point(5, 6));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
  GetWindowTreeClientChanges()->clear();
  GetEventGenerator()->ReleaseLeftButton();

  // Releasing the mouse completes the move loop.
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=21 success=true"));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
}

TEST_F(WindowServiceDelegateImplTest, RunWindowMoveWithMultipleDisplays) {
  UpdateDisplay("500x500,500x500");
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      top_level_->GetBoundsInScreen().origin());
  GetEventGenerator()->MoveMouseTo(gfx::Point(501, 1));
  GetWindowTreeClientChanges()->clear();
  GetEventGenerator()->ReleaseLeftButton();

  EXPECT_EQ(Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id()),
            top_level_->GetRootWindow());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "DisplayChanged window_id=0,1 display_id=2200000001"));
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      std::string("BoundsChanged window=0,1 old_bounds=500,0 104x100 "
                  "new_bounds=500,0 104x100 local_surface_id=*")));
}

TEST_F(WindowServiceDelegateImplTest, SetWindowBoundsToDifferentDisplay) {
  UpdateDisplay("500x500,500x500");
  EXPECT_EQ(gfx::Point(100, 100), top_level_->GetBoundsInScreen().origin());

  GetWindowTreeClientChanges()->clear();
  GetWindowTreeTestHelper()->window_tree()->SetWindowBounds(
      21, GetTopLevelWindowId(), gfx::Rect(600, 100, 100, 100),
      base::Optional<viz::LocalSurfaceId>());
  EXPECT_EQ(gfx::Point(600, 100), top_level_->GetBoundsInScreen().origin());
  EXPECT_EQ(Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id()),
            top_level_->GetRootWindow());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "DisplayChanged window_id=0,1 display_id=2200000001"));
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      std::string("BoundsChanged window=0,1 old_bounds=100,100 100x100 "
                  "new_bounds=600,100 104x100 local_surface_id=*")));
}

TEST_F(WindowServiceDelegateImplTest, DeleteWindowWithInProgressRunLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      29, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      gfx::Point());
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  top_level_.reset();
  EXPECT_FALSE(event_handler()->is_drag_in_progress());
  // Deleting the window implicitly cancels the drag.
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=29 success=false"));
}

TEST_F(WindowServiceDelegateImplTest, RunWindowMoveLoopInSecondaryDisplay) {
  UpdateDisplay("500x400,500x400");
  top_level_->SetBoundsInScreen(gfx::Rect(600, 100, 100, 100),
                                GetSecondaryDisplay());

  EXPECT_EQ(Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id()),
            top_level_->GetRootWindow());
  EXPECT_EQ(gfx::Point(600, 100), top_level_->GetBoundsInScreen().origin());

  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      gfx::Point(605, 106));

  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(gfx::Point(615, 120));
  EXPECT_EQ(gfx::Point(610, 114), top_level_->GetBoundsInScreen().origin());
}

TEST_F(WindowServiceDelegateImplTest, CancelWindowMoveLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      gfx::Point());
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(gfx::Point(5, 6));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
  GetWindowTreeClientChanges()->clear();
  GetWindowTreeTestHelper()->window_tree()->CancelWindowMove(
      GetTopLevelWindowId());
  EXPECT_FALSE(event_handler()->is_drag_in_progress());
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=21 success=false"));
  EXPECT_EQ(gfx::Point(100, 100), top_level_->bounds().origin());
}

TEST_F(WindowServiceDelegateImplTest, RunDragLoop) {
  SetCanAcceptDrops();
  GetWindowTreeTestHelper()->window_tree()->PerformDragDrop(
      21, GetTopLevelWindowId(), gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  base::RunLoop run_loop;

  // Post mouse move and release to allow the nested drag loop to pick it up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        ASSERT_TRUE(IsDragDropInProgress());

        // Move mouse to center of |top_level_|.
        GetEventGenerator()->MoveMouseTo(gfx::Point(150, 150));
        GetWindowTreeClientChanges()->clear();
        GetEventGenerator()->ReleaseLeftButton();
      }));

  // Let the drop loop task run.
  run_loop.RunUntilIdle();

  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "OnPerformDragDropCompleted id=21 success=true action=1"));
}

TEST_F(WindowServiceDelegateImplTest, DeleteWindowWithInProgressDragLoop) {
  SetCanAcceptDrops();
  GetWindowTreeTestHelper()->window_tree()->PerformDragDrop(
      21, GetTopLevelWindowId(), gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  base::RunLoop run_loop;

  // Post the task to delete the window to allow nested drag loop to pick it up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        ASSERT_TRUE(IsDragDropInProgress());

        // Deletes the window.
        top_level_.reset();

        // Move mouse and release the button and it should not crash.
        GetEventGenerator()->MoveMouseTo(gfx::Point(150, 150));
        GetWindowTreeClientChanges()->clear();
        GetEventGenerator()->ReleaseLeftButton();
      }));

  // Let the drop loop task run.
  run_loop.RunUntilIdle();

  // It fails because the target window |top_level_| is deleted.
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      "OnPerformDragDropCompleted id=21 success=false action=0"));
}

TEST_F(WindowServiceDelegateImplTest, CancelDragDropBeforeDragLoopRun) {
  SetCanAcceptDrops();
  GetWindowTreeTestHelper()->window_tree()->PerformDragDrop(
      21, GetTopLevelWindowId(), gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  // Cancel the drag before the drag loop runs.
  GetWindowTreeTestHelper()->window_tree()->CancelDragDrop(
      GetTopLevelWindowId());

  // Let the drop loop task run.
  base::RunLoop().RunUntilIdle();

  // It fails because the drag is canceled.
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      "OnPerformDragDropCompleted id=21 success=false action=0"));
}

TEST_F(WindowServiceDelegateImplTest, CancelDragDropAfterDragLoopRun) {
  SetCanAcceptDrops();
  GetWindowTreeTestHelper()->window_tree()->PerformDragDrop(
      21, GetTopLevelWindowId(), gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  base::RunLoop run_loop;

  // Post the task to cancel to allow nested drag loop to pick it up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        ASSERT_TRUE(IsDragDropInProgress());

        GetWindowTreeTestHelper()->window_tree()->CancelDragDrop(
            GetTopLevelWindowId());
      }));

  // Let the drop loop task run.
  run_loop.RunUntilIdle();

  // It fails because the drag is canceled.
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      "OnPerformDragDropCompleted id=21 success=false action=0"));
}

TEST_F(WindowServiceDelegateImplTest, ObserveTopmostWindow) {
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(150, 100, 100, 100));
  std::unique_ptr<aura::Window> window3(
      CreateTestWindowInShell(SK_ColorRED, kShellWindowId_DefaultContainer,
                              gfx::Rect(100, 150, 100, 100)));

  // Left button is pressed on SetUp() -- release it first.
  GetEventGenerator()->ReleaseLeftButton();
  GetEventGenerator()->MoveMouseTo(gfx::Point(105, 105));
  GetEventGenerator()->PressLeftButton();
  GetWindowTreeClientChanges()->clear();

  GetWindowTreeTestHelper()->window_tree()->ObserveTopmostWindow(
      ws::mojom::MoveLoopSource::MOUSE, GetTopLevelWindowId());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=null"));
  GetWindowTreeClientChanges()->clear();

  GetEventGenerator()->MoveMouseTo(gfx::Point(155, 105));
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=0,2"));
  GetWindowTreeClientChanges()->clear();

  GetEventGenerator()->MoveMouseTo(gfx::Point(155, 115));
  EXPECT_FALSE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=0,2"));
  GetWindowTreeClientChanges()->clear();

  GetEventGenerator()->MoveMouseTo(gfx::Point(155, 155));
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=null"));
  GetWindowTreeClientChanges()->clear();

  window3.reset();
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=0,2"));
  GetWindowTreeClientChanges()->clear();

  window2->Hide();
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=null"));
  GetWindowTreeClientChanges()->clear();

  GetWindowTreeTestHelper()->window_tree()->StopObservingTopmostWindow();
}

TEST_F(WindowServiceDelegateImplTest, MoveAcrossDisplays) {
  UpdateDisplay("600x400,600+0-400x300");

  GetWindowTreeClientChanges()->clear();

  display::Screen* screen = display::Screen::GetScreen();
  display::Display display1 = screen->GetPrimaryDisplay();
  display::Display display2 = GetSecondaryDisplay();
  EXPECT_EQ(display1.id(),
            screen->GetDisplayNearestWindow(top_level_.get()).id());

  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      gfx::Point());
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(gfx::Point(610, 6));
  GetWindowTreeClientChanges()->clear();
  GetEventGenerator()->ReleaseLeftButton();

  EXPECT_EQ(display2.id(),
            screen->GetDisplayNearestWindow(top_level_.get()).id());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     std::string("DisplayChanged window_id=0,1 display_id=") +
                         base::NumberToString(display2.id())));
}

TEST_F(WindowServiceDelegateImplTest, RemoveDisplay) {
  UpdateDisplay("500x400,500x400");
  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display::Display display2 = GetSecondaryDisplay();

  GetWindowTreeClientChanges()->clear();
  top_level_->SetBoundsInScreen(gfx::Rect(600, 100, 100, 100),
                                GetSecondaryDisplay());
  EXPECT_EQ(Shell::GetRootWindowForDisplayId(display2.id()),
            top_level_->GetRootWindow());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     std::string("DisplayChanged window_id=0,1 display_id=") +
                         base::NumberToString(display2.id())));

  GetWindowTreeClientChanges()->clear();
  UpdateDisplay("500x400");
  EXPECT_EQ(Shell::GetRootWindowForDisplayId(display1.id()),
            top_level_->GetRootWindow());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     std::string("DisplayChanged window_id=0,1 display_id=") +
                         base::NumberToString(display1.id())));
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      std::string("BoundsChanged window=0,1 old_bounds=* "
                  "new_bounds=100,100 104x100 local_surface_id=*")));
}

}  // namespace ash
