// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_container_view.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/assistant_web_ui_controller.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/public/cpp/assistant/assistant_settings.h"
#include "ash/shell.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

class AssistantWebContainerViewTest : public AssistantAshTestBase {
 public:
  AssistantWebContainerViewTest() = default;
  ~AssistantWebContainerViewTest() override = default;

 protected:
  AssistantWebContainerView* view() {
    return Shell::Get()
        ->assistant_controller()
        ->web_ui_controller()
        ->GetViewForTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantWebContainerViewTest);
};

}  // namespace

TEST_F(AssistantWebContainerViewTest, ShowAndCloseWindow) {
  // Show Assistant Settings UI.
  OpenAssistantSettings();
  AssistantWebContainerView* container_view = view();
  ASSERT_TRUE(container_view);

  // Close Assistant Settings UI.
  container_view->GetWidget()->CloseNow();
  container_view = view();
  ASSERT_FALSE(container_view);
}

TEST_F(AssistantWebContainerViewTest, CenterWindow) {
  // Test large and small screens.
  std::vector<std::string> resolutions{"1200x1000", "800x600"};

  for (const auto& resolution : resolutions) {
    UpdateDisplay(resolution);

    // Show Assistant Settings UI and grab a reference to our view under test.
    OpenAssistantSettings();
    AssistantWebContainerView* container_view = view();

    // We expect the view to appear in the work area where new windows will
    // open.
    gfx::Rect expected_work_area =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

    // We expect the view to be centered in screen.
    gfx::Rect expected_bounds = gfx::Rect(expected_work_area);
    expected_bounds.ClampToCenteredSize(
        container_view->GetWidget()->GetNativeWindow()->bounds().size());
    ASSERT_EQ(expected_bounds,
              container_view->GetWidget()->GetWindowBoundsInScreen());
    container_view->GetWidget()->CloseNow();
  }
}

TEST_F(AssistantWebContainerViewTest, CloseWindowByKeyEvent) {
  // Show Assistant Settings UI.
  OpenAssistantSettings();
  ASSERT_TRUE(view());

  // Close Assistant Settings UI by key event.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(view());
}

TEST_F(AssistantWebContainerViewTest, NoCaptionBarInAssistantWebView) {
  // Show Assistant Settings UI.
  OpenAssistantSettings();
  AssistantWebContainerView* container_view = view();
  ASSERT_TRUE(container_view);

  // AssistantWebContainerView's widget should have its own caption buttons in
  // the ash frame view. Therefore the AssistantWebView should not have the
  // caption bar.
  ASSERT_FALSE(container_view->GetCaptionBarForTesting());
}

}  // namespace ash
