// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_container_view.h"

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_web_ui_controller.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_views_delegate.h"
#include "base/run_loop.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "ui/views/window/frame_caption_button.h"

namespace ash {

namespace {

class TestViewsDelegate : public ash::AshTestViewsDelegate {
 public:
  TestViewsDelegate() = default;
  TestViewsDelegate(const TestViewsDelegate& other) = delete;
  TestViewsDelegate& operator=(const TestViewsDelegate& other) = delete;
  ~TestViewsDelegate() override = default;

  // views::ViewsDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateDefaultNonClientFrameView(
      views::Widget* widget) override {
    return ash::Shell::Get()->CreateDefaultNonClientFrameView(widget);
  }
};

class AssistantWebContainerViewTest : public AssistantAshTestBase {
 public:
  AssistantWebContainerViewTest() = default;

  AssistantWebContainerViewTest(const AssistantWebContainerViewTest&) = delete;
  AssistantWebContainerViewTest& operator=(
      const AssistantWebContainerViewTest&) = delete;

  ~AssistantWebContainerViewTest() override = default;

 protected:
  AssistantWebContainerView* view() {
    return Shell::Get()
        ->assistant_controller()
        ->web_ui_controller()
        ->GetViewForTest();
  }

  void OpenAssistantSettings() {
    Shell::Get()->assistant_controller()->OpenAssistantSettings();
  }

  views::FrameCaptionButton* GetBackButton(views::Widget* widget) {
    views::NonClientView* non_client_view = widget->non_client_view();
    NonClientFrameViewAsh* frame_view_ash =
        static_cast<NonClientFrameViewAsh*>(non_client_view->frame_view());
    return frame_view_ash->GetHeaderView()->GetFrameHeader()->GetBackButton();
  }

 private:
  std::unique_ptr<TestViewsDelegate> views_delegate_ =
      std::make_unique<TestViewsDelegate>();
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
  PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(view());
}

TEST_F(AssistantWebContainerViewTest, ShouldHaveBackButton) {
  // Show Assistant Settings UI.
  OpenAssistantSettings();
  ASSERT_TRUE(view());

  views::FrameCaptionButton* back_button = nullptr;
  back_button = GetBackButton(view()->GetWidget());
  ASSERT_FALSE(back_button);

  view()->SetCanGoBackForTesting(/*can_go_back=*/true);
  back_button = GetBackButton(view()->GetWidget());
  ASSERT_TRUE(back_button);
  ASSERT_TRUE(back_button->GetVisible());

  view()->SetCanGoBackForTesting(/*can_go_back=*/false);
  back_button = GetBackButton(view()->GetWidget());
  ASSERT_FALSE(back_button);
}

TEST_F(AssistantWebContainerViewTest, BackButtonShouldPaintAsActive) {
  // Show Assistant Settings UI.
  OpenAssistantSettings();
  ASSERT_TRUE(view());

  view()->SetCanGoBackForTesting(/*can_go_back=*/true);
  views::FrameCaptionButton* back_button = GetBackButton(view()->GetWidget());
  ASSERT_TRUE(back_button);
  ASSERT_TRUE(back_button->GetVisible());
  ASSERT_TRUE(back_button->GetPaintAsActive());

  // Activate another window will paint the back button as inactive.
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  ASSERT_TRUE(back_button->GetVisible());
  ASSERT_FALSE(back_button->GetPaintAsActive());

  // Activate Assistant web container will paint the back button as active.
  view()->GetWidget()->Activate();
  ASSERT_TRUE(back_button->GetVisible());
  ASSERT_TRUE(back_button->GetPaintAsActive());
}

TEST_F(AssistantWebContainerViewTest, ShouldRemoveBackButton) {
  // Show Assistant Settings UI.
  OpenAssistantSettings();
  ASSERT_TRUE(view());

  views::FrameCaptionButton* back_button = nullptr;
  back_button = GetBackButton(view()->GetWidget());
  ASSERT_FALSE(back_button);

  view()->OpenUrl(GURL("test1"));
  view()->DidStopLoading();
  back_button = GetBackButton(view()->GetWidget());
  ASSERT_FALSE(back_button);

  view()->SetCanGoBackForTesting(/*can_go_back=*/true);
  back_button = GetBackButton(view()->GetWidget());
  ASSERT_TRUE(back_button);
  ASSERT_TRUE(back_button->GetVisible());

  // Open another URL will remove the back button.
  view()->OpenUrl(GURL("test2"));
  view()->DidStopLoading();
  back_button = GetBackButton(view()->GetWidget());
  ASSERT_FALSE(back_button);
}

}  // namespace ash
