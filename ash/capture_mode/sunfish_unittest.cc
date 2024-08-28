// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/capture_mode/capture_button_view.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/search_results_panel.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace ash {

class SunfishTest : public AshTestBase {
 public:
  SunfishTest() = default;
  SunfishTest(const SunfishTest&) = delete;
  SunfishTest& operator=(const SunfishTest&) = delete;
  ~SunfishTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshDebugShortcuts);
    AshTestBase::SetUp();
  }

 private:
  // Calling the factory constructor is enough to set it up.
  TestAshWebViewFactory test_web_view_factory_;

  base::test::ScopedFeatureList scoped_feature_list_{features::kSunfishFeature};
};

// Tests that the accelerator starts capture mode in a new behavior.
TEST_F(SunfishTest, AccelEntryPoint) {
  PressAndReleaseKey(ui::VKEY_8,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller->IsActive());
  CaptureModeBehavior* active_behavior =
      controller->capture_mode_session()->active_behavior();
  ASSERT_TRUE(active_behavior);
  EXPECT_EQ(active_behavior->behavior_type(), BehaviorType::kSunfish);
}

// Tests that the ESC key ends capture mode session.
TEST_F(SunfishTest, PressEscapeKey) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();

  // Tests it starts sunfish behavior.
  auto* session = controller->capture_mode_session();
  ASSERT_EQ(BehaviorType::kSunfish,
            session->active_behavior()->behavior_type());

  // Tests pressing ESC ends the session.
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  ASSERT_FALSE(controller->IsActive());
  EXPECT_FALSE(controller->capture_mode_session());
}

// Tests the sunfish capture label view.
TEST_F(SunfishTest, CaptureLabelView) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  auto* session = controller->capture_mode_session();
  ASSERT_EQ(BehaviorType::kSunfish,
            session->active_behavior()->behavior_type());

  CaptureModeSessionTestApi test_api(session);
  auto* capture_button =
      test_api.GetCaptureLabelView()->capture_button_container();
  auto* capture_label = test_api.GetCaptureLabelInternalView();

  // Before the drag, only the capture label is visible and is in waiting to
  // select a capture region phase.
  EXPECT_FALSE(capture_button->GetVisible());
  EXPECT_TRUE(capture_label->GetVisible());
  EXPECT_EQ(u"Drag to select an area to search", capture_label->GetText());

  // Tests it can drag and select a region.
  auto* event_generator = GetEventGenerator();
  SelectCaptureModeRegion(event_generator, gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/false);
  auto* dimensions_label = test_api.GetDimensionsLabelWidget();
  EXPECT_TRUE(dimensions_label && dimensions_label->IsVisible());

  // During the drag, the label and button are both hidden.
  EXPECT_FALSE(capture_button->GetVisible());
  EXPECT_FALSE(capture_label->GetVisible());

  // Release the drag. The label and button are both hidden.
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(capture_button->GetVisible());
  EXPECT_FALSE(capture_label->GetVisible());
}

// Tests the sunfish capture mode bar view.
TEST_F(SunfishTest, CaptureBarView) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  auto* session = controller->capture_mode_session();
  ASSERT_EQ(BehaviorType::kSunfish,
            session->active_behavior()->behavior_type());

  CaptureModeSessionTestApi test_api(session);
  auto* bar_view = test_api.GetCaptureModeBarView();
  ASSERT_TRUE(bar_view);

  // The bar view should only have a close button.
  auto* close_button = bar_view->close_button();
  ASSERT_TRUE(close_button);
  ASSERT_FALSE(bar_view->settings_button());

  // The sunfish bar does not have a settings button, so trying to set the menu
  // shown should instead do nothing.
  bar_view->SetSettingsMenuShown(true);
  EXPECT_FALSE(bar_view->settings_button());

  // Close the session using the button.
  LeftClickOn(bar_view->close_button());
  ASSERT_FALSE(controller->capture_mode_session());
}

// Tests that the search results panel is draggable.
TEST_F(SunfishTest, DragSearchResultsPanel) {
  auto widget = SearchResultsPanel::CreateWidget(Shell::GetPrimaryRootWindow());
  widget->Show();

  auto* search_results_panel =
      views::AsViewClass<SearchResultsPanel>(widget->GetContentsView());
  auto* event_generator = GetEventGenerator();

  // The results panel can be dragged by points outside the search results view
  // and searchbox textfield.
  const gfx::Point draggable_point(search_results_panel->search_results_view()
                                       ->GetBoundsInScreen()
                                       .origin() +
                                   gfx::Vector2d(0, -3));
  event_generator->MoveMouseTo(draggable_point);

  // Test that dragging the panel to arbitrary points repositions the panel.
  constexpr gfx::Vector2d kTestDragOffsets[] = {
      gfx::Vector2d(-25, -5), gfx::Vector2d(-10, 20), gfx::Vector2d(0, 30),
      gfx::Vector2d(35, -15)};
  for (const gfx::Vector2d& offset : kTestDragOffsets) {
    const gfx::Rect widget_bounds(widget->GetWindowBoundsInScreen());
    event_generator->DragMouseBy(offset.x(), offset.y());
    EXPECT_EQ(widget->GetWindowBoundsInScreen(), widget_bounds + offset);
  }
}

}  // namespace ash
