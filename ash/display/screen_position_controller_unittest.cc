// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_position_controller.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/window_factory.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

ScreenPositionController* GetScreenPositionController() {
  return ShellTestApi().screen_position_controller();
}

class ScreenPositionControllerTest : public AshTestBase {
 public:
  ScreenPositionControllerTest() = default;
  ~ScreenPositionControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    window_ = window_factory::NewWindow(&window_delegate_);
    window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window_.get());
    window_->set_id(1);
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  // Converts a point (x, y) in host window's coordinate to screen and
  // returns its string representation.
  std::string ConvertHostPointToScreen(int x, int y) const {
    gfx::Point point(x, y);
    GetScreenPositionController()->ConvertHostPointToScreen(
        window_->GetRootWindow(), &point);
    return point.ToString();
  }

  void SetSecondaryDisplayLayout(display::DisplayPlacement::Position position) {
    std::unique_ptr<display::DisplayLayout> layout(
        display_manager()->GetCurrentDisplayLayout().Copy());
    layout->placement_list[0].position = position;
    display_manager()->SetLayoutForCurrentDisplays(std::move(layout));
  }

 protected:
  std::unique_ptr<aura::Window> window_;
  aura::test::TestWindowDelegate window_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenPositionControllerTest);
};

}  // namespace

TEST_F(ScreenPositionControllerTest, ConvertHostPointToScreen) {
  // Make sure that the point is in host coordinates. (crbug.com/521919)
  UpdateDisplay("100+100-200x200,100+300-200x200");
  // The point 150,210 should be in host coords, and detected as outside.
  EXPECT_EQ("350,10", ConvertHostPointToScreen(150, 210));

  UpdateDisplay("100+100-200x200,100+500-200x200");

  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  EXPECT_EQ(
      "100,100",
      root_windows[0]->GetHost()->GetBoundsInPixels().origin().ToString());
  EXPECT_EQ("200x200",
            root_windows[0]->GetHost()->GetBoundsInPixels().size().ToString());
  EXPECT_EQ(
      "100,500",
      root_windows[1]->GetHost()->GetBoundsInPixels().origin().ToString());
  EXPECT_EQ("200x200",
            root_windows[1]->GetHost()->GetBoundsInPixels().size().ToString());

  const gfx::Point window_pos(100, 100);
  window_->SetBoundsInScreen(
      gfx::Rect(window_pos, gfx::Size(100, 100)),
      display::Screen::GetScreen()->GetDisplayNearestPoint(window_pos));
  SetSecondaryDisplayLayout(display::DisplayPlacement::RIGHT);
  // The point is on the primary root window.
  EXPECT_EQ("50,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,250", ConvertHostPointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("250,0", ConvertHostPointToScreen(50, 400));

  SetSecondaryDisplayLayout(display::DisplayPlacement::BOTTOM);
  // The point is on the primary root window.
  EXPECT_EQ("50,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,250", ConvertHostPointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("50,200", ConvertHostPointToScreen(50, 400));

  SetSecondaryDisplayLayout(display::DisplayPlacement::LEFT);
  // The point is on the primary root window.
  EXPECT_EQ("50,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,250", ConvertHostPointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("-150,0", ConvertHostPointToScreen(50, 400));

  SetSecondaryDisplayLayout(display::DisplayPlacement::TOP);
  // The point is on the primary root window.
  EXPECT_EQ("50,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,250", ConvertHostPointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("50,-200", ConvertHostPointToScreen(50, 400));

  SetSecondaryDisplayLayout(display::DisplayPlacement::RIGHT);
  const gfx::Point window_pos2(300, 100);
  window_->SetBoundsInScreen(
      gfx::Rect(window_pos2, gfx::Size(100, 100)),
      display::Screen::GetScreen()->GetDisplayNearestPoint(window_pos2));
  // The point is on the secondary display.
  EXPECT_EQ("250,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("450,250", ConvertHostPointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("50,0", ConvertHostPointToScreen(50, -400));

  SetSecondaryDisplayLayout(display::DisplayPlacement::BOTTOM);
  // The point is on the secondary display.
  EXPECT_EQ("50,250", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,450", ConvertHostPointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("50,0", ConvertHostPointToScreen(50, -400));

  SetSecondaryDisplayLayout(display::DisplayPlacement::LEFT);
  // The point is on the secondary display.
  EXPECT_EQ("-150,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("50,250", ConvertHostPointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("50,0", ConvertHostPointToScreen(50, -400));

  SetSecondaryDisplayLayout(display::DisplayPlacement::TOP);
  // The point is on the secondary display.
  EXPECT_EQ("50,-150", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,50", ConvertHostPointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("50,0", ConvertHostPointToScreen(50, -400));
}

TEST_F(ScreenPositionControllerTest, ConvertHostPointToScreenHiDPI) {
  UpdateDisplay("50+50-200x200*2,50+300-300x300");

  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  EXPECT_EQ("50,50 200x200",
            root_windows[0]->GetHost()->GetBoundsInPixels().ToString());
  EXPECT_EQ("50,300 300x300",
            root_windows[1]->GetHost()->GetBoundsInPixels().ToString());

  // Put |window_| to the primary 2x display.
  window_->SetBoundsInScreen(gfx::Rect(20, 20, 50, 50),
                             display::Screen::GetScreen()->GetPrimaryDisplay());
  // (30, 30) means the host coordinate, so the point is still on the primary
  // root window.  Since it's 2x, the specified native point was halved.
  EXPECT_EQ("15,15", ConvertHostPointToScreen(30, 30));
  // Similar to above but the point is out of the all root windows.
  EXPECT_EQ("200,200", ConvertHostPointToScreen(400, 400));
  // Similar to above but the point is on the secondary display.
  EXPECT_EQ("100,15", ConvertHostPointToScreen(200, 30));

  // On secondary display. The position on the 2nd host window is (150,200)
  // so the screen position is (100,0) + (150,200).
  EXPECT_EQ("250,200", ConvertHostPointToScreen(150, 450));

  // At the edge but still in the primary display.  Remaining of the primary
  // display is (50, 50) but adding ~100 since it's 2x-display.
  EXPECT_EQ("79,79", ConvertHostPointToScreen(158, 158));
  // At the edge of the secondary display.
  EXPECT_EQ("80,80", ConvertHostPointToScreen(160, 160));
}

TEST_F(ScreenPositionControllerTest, ConvertHostPointToScreenRotate) {
  // 1st display is rotated 90 clockise, and 2nd display is rotated
  // 270 clockwise.
  UpdateDisplay("100+100-200x200/r,100+500-200x200/l");
  // Put |window_| to the 1st.
  window_->SetBoundsInScreen(gfx::Rect(20, 20, 50, 50),
                             display::Screen::GetScreen()->GetPrimaryDisplay());

  // The point is on the 1st host.
  EXPECT_EQ("70,150", ConvertHostPointToScreen(50, 70));
  // The point is out of the host windows.
  EXPECT_EQ("250,-50", ConvertHostPointToScreen(250, 250));
  // The point is on the 2nd host. Point on 2nd host (30,150) -
  // rotate 270 clockwise -> (149, 30) - layout [+(200,0)] -> (349,30).
  EXPECT_EQ("350,30", ConvertHostPointToScreen(30, 450));

  // Move |window_| to the 2nd.
  window_->SetBoundsInScreen(gfx::Rect(300, 20, 50, 50),
                             display_manager()->GetSecondaryDisplay());
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  EXPECT_EQ(root_windows[1], window_->GetRootWindow());

  // The point is on the 2nd host. (50,70) on 2n host -
  // roatate 270 clockwise -> (129,50) -layout [+(200,0)] -> (329,50)
  EXPECT_EQ("330,50", ConvertHostPointToScreen(50, 70));
  // The point is out of the host windows.
  EXPECT_EQ("450,50", ConvertHostPointToScreen(50, -50));
  // The point is on the 2nd host. Point on 2nd host (50,50) -
  // rotate 90 clockwise -> (50, 149)
  EXPECT_EQ("50,150", ConvertHostPointToScreen(50, -350));
}

TEST_F(ScreenPositionControllerTest, ConvertHostPointToScreenZoomScale) {
  // 1st display is 2x density with 1.5 UI scale.
  UpdateDisplay("100+100-200x200*2@0.8,100+500-200x200");
  // Put |window_| to the 1st.
  window_->SetBoundsInScreen(gfx::Rect(20, 20, 50, 50),
                             display::Screen::GetScreen()->GetPrimaryDisplay());

  // The point is on the 1st host.
  EXPECT_EQ("37,37", ConvertHostPointToScreen(60, 60));
  // The point is out of the host windows.
  EXPECT_EQ("37,187", ConvertHostPointToScreen(60, 300));
  // The point is on the 2nd host. Point on 2nd host (60,150) -
  // - screen [+(150,0)]
  EXPECT_EQ("185,50", ConvertHostPointToScreen(60, 450));

  // Move |window_| to the 2nd.
  window_->SetBoundsInScreen(gfx::Rect(300, 20, 50, 50),
                             display_manager()->GetSecondaryDisplay());
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  EXPECT_EQ(root_windows[1], window_->GetRootWindow());

  // The point is on the 2nd host. (50,70) - ro
  EXPECT_EQ("185,70", ConvertHostPointToScreen(60, 70));
  // The point is out of the host windows.
  EXPECT_EQ("185,-50", ConvertHostPointToScreen(60, -50));
  // The point is on the 2nd host. Point on 1nd host (60, 60)
  // 1/2 * 1 / 0.8 = (45,45)
  EXPECT_EQ("37,37", ConvertHostPointToScreen(60, -340));
}

namespace {

// EventHandler which tracks whether it got any MouseEvents whose location could
// not be converted to screen coordinates.
class ConvertToScreenEventHandler : public ui::EventHandler {
 public:
  ConvertToScreenEventHandler() : could_convert_to_screen_(true) {
    aura::Env::GetInstance()->AddPreTargetHandler(this);
  }
  ~ConvertToScreenEventHandler() override {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

  bool could_convert_to_screen() const { return could_convert_to_screen_; }

 private:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
      return;

    aura::Window* root =
        static_cast<aura::Window*>(event->target())->GetRootWindow();
    if (!aura::client::GetScreenPositionClient(root))
      could_convert_to_screen_ = false;
  }

  bool could_convert_to_screen_;

  DISALLOW_COPY_AND_ASSIGN(ConvertToScreenEventHandler);
};

}  // namespace

// Test that events are only dispatched when a ScreenPositionClient is available
// to convert the event to screen coordinates. The ScreenPositionClient is
// detached from the root window prior to the root window being destroyed. Test
// that no events are dispatched at this time.
TEST_F(ScreenPositionControllerTest,
       ConvertToScreenWhileRemovingSecondaryDisplay) {
  UpdateDisplay("600x600,600x600");
  base::RunLoop().RunUntilIdle();

  // Create a window on the secondary display.
  window_->SetBoundsInScreen(gfx::Rect(600, 0, 400, 400),
                             display_manager()->GetSecondaryDisplay());

  // Move the mouse cursor over |window_|. Synthetic mouse moves are dispatched
  // asynchronously when a window which contains the mouse cursor is destroyed.
  // We want to check that none of these synthetic events are dispatched after
  // ScreenPositionClient has been detached from the root window.
  GetEventGenerator()->MoveMouseTo(800, 200);
  EXPECT_TRUE(window_->GetBoundsInScreen().Contains(
      aura::Env::GetInstance()->last_mouse_location()));

  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  aura::WindowTracker tracker;
  tracker.Add(root_windows[1]);
  std::unique_ptr<ConvertToScreenEventHandler> event_handler(
      new ConvertToScreenEventHandler);

  // Remove the secondary monitor.
  UpdateDisplay("600x600");

  // The secondary root window is not immediately destroyed.
  EXPECT_TRUE(tracker.Contains(root_windows[1]));

  base::RunLoop().RunUntilIdle();

  // Check that we waited long enough and that the secondary root window was
  // destroyed.
  EXPECT_FALSE(tracker.Contains(root_windows[1]));

  // Check that we could convert all of the mouse events we got to screen
  // coordinates.
  EXPECT_TRUE(event_handler->could_convert_to_screen());
}

}  // namespace ash
