// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotator_controller.h"

#include "ash/annotator/annotation_tray.h"
#include "ash/annotator/annotator_metrics.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_container.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/annotator/test/mock_annotator_client.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

constexpr char kAnnotatorMarkerColorHistogramName[] =
    "Ash.Projector.MarkerColor.ClamshellMode";
constexpr char kProjectorToolbarHistogramName[] =
    "Ash.Projector.Toolbar.ClamshellMode";

}  // namespace

class AnnotatorControllerTest : public AshTestBase {
 public:
  AnnotatorControllerTest() = default;

  AnnotatorControllerTest(const AnnotatorControllerTest&) = delete;
  AnnotatorControllerTest& operator=(const AnnotatorControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto* annotator_controller = Shell::Get()->annotator_controller();
    annotator_controller->SetToolClient(&client_);
  }

  AnnotatorController* annotator_controller() {
    return Shell::Get()->annotator_controller();
  }

 protected:
  MockAnnotatorClient client_;
};

TEST_F(AnnotatorControllerTest, SetAnnotatorTool) {
  base::HistogramTester histogram_tester;
  AnnotatorTool tool;
  tool.color = kAnnotatorDefaultPenColor;
  EXPECT_CALL(client_, SetTool(tool));

  annotator_controller()->SetAnnotatorTool(tool);
  histogram_tester.ExpectUniqueSample(kAnnotatorMarkerColorHistogramName,
                                      AnnotatorMarkerColor::kMagenta,
                                      /*count=*/1);
}

TEST_F(AnnotatorControllerTest, RegisterAndUnregisterView) {
  auto* annotation_tray = Shell::GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->annotation_tray();
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(annotation_tray->visible_preferred());
  annotator_controller()->UnregisterView(Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(annotation_tray->visible_preferred());
}

TEST_F(AnnotatorControllerTest, RegisterAndUnregisterViewMultipleDisplays) {
  UpdateDisplay("800x700,801+0-800x700");
  aura::Window::Windows roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  auto* primary_display_tray = Shell::GetPrimaryRootWindowController()
                                   ->GetStatusAreaWidget()
                                   ->annotation_tray();
  auto* external_display_tray = RootWindowController::ForWindow(roots[1])
                                    ->GetStatusAreaWidget()
                                    ->annotation_tray();

  // Show tray on primary root window.
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(primary_display_tray->visible_preferred());
  EXPECT_FALSE(external_display_tray->visible_preferred());

  // Change the root window.
  annotator_controller()->UnregisterView(Shell::GetPrimaryRootWindow());
  annotator_controller()->RegisterView(roots[1]);
  EXPECT_FALSE(primary_display_tray->visible_preferred());
  EXPECT_TRUE(external_display_tray->visible_preferred());

  annotator_controller()->UnregisterView(roots[1]);
  EXPECT_FALSE(primary_display_tray->visible_preferred());
  EXPECT_FALSE(external_display_tray->visible_preferred());
}

TEST_F(AnnotatorControllerTest, UnregisterViewWhenAnnotatorIsEnabled) {
  base::HistogramTester histogram_tester;

  auto* annotation_tray = Shell::GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->annotation_tray();
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(annotation_tray->visible_preferred());

  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->EnableAnnotatorTool();
  EXPECT_TRUE(annotator_controller()->is_annotator_enabled());

  annotator_controller()->UnregisterView(Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(annotation_tray->visible_preferred());
  histogram_tester.ExpectUniqueSample(kProjectorToolbarHistogramName,
                                      ProjectorToolbar::kMarkerTool,
                                      /*count=*/1);
}

TEST_F(AnnotatorControllerTest, EnableAnnotator) {
  // Does nothing as there is no view for annotating.
  annotator_controller()->EnableAnnotatorTool();
  EXPECT_FALSE(annotator_controller()->is_annotator_enabled());

  // Enables the annotator after creating the view for annotating.
  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->EnableAnnotatorTool();
  EXPECT_TRUE(annotator_controller()->is_annotator_enabled());
}

TEST_F(AnnotatorControllerTest, DisableAnnotator) {
  base::HistogramTester histogram_tester;

  auto* annotation_tray = Shell::GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->annotation_tray();
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(annotation_tray->visible_preferred());

  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->EnableAnnotatorTool();
  EXPECT_TRUE(annotator_controller()->is_annotator_enabled());

  annotator_controller()->DisableAnnotator();
  EXPECT_FALSE(annotation_tray->visible_preferred());
  EXPECT_FALSE(annotator_controller()->is_annotator_enabled());
}

// Verifies that toggling on the marker enables the annotator tool.
TEST_F(AnnotatorControllerTest, EnablingDisablingMarker) {
  base::HistogramTester histogram_tester;

  // Enable marker.
  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->EnableAnnotatorTool();
  EXPECT_TRUE(annotator_controller()->is_annotator_enabled());

  EXPECT_CALL(client_, Clear());
  annotator_controller()->ResetTools();
  EXPECT_FALSE(annotator_controller()->is_annotator_enabled());

  histogram_tester.ExpectUniqueSample(kProjectorToolbarHistogramName,
                                      ProjectorToolbar::kMarkerTool,
                                      /*count=*/1);
}

TEST_F(AnnotatorControllerTest, ToggleMarkerWithKeyboardShortcut) {
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->OnCanvasInitialized(true);

  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(annotator_controller()->is_annotator_enabled());

  event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(annotator_controller()->is_annotator_enabled());
}

TEST_F(AnnotatorControllerTest, SetAnnotatorToolAndStorePref) {
  auto* annotation_tray = Shell::GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->annotation_tray();
  // Assert that the initial pref is unset.
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_EQ(
      pref_service->GetUint64(prefs::kProjectorAnnotatorLastUsedMarkerColor),
      0u);

  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->OnCanvasInitialized(true);
  // Assert that the icon image for the annotator is set.
  ASSERT_EQ(annotation_tray->tray_container()->children().size(), 1u);
  const views::ImageView* image_view = static_cast<views::ImageView*>(
      annotation_tray->tray_container()->children()[0]);
  EXPECT_FALSE(image_view->GetImageModel().IsEmpty());

  LeftClickOn(annotation_tray);
  EXPECT_TRUE(annotator_controller()->is_annotator_enabled());

  annotator_controller()->DisableAnnotator();
  EXPECT_FALSE(annotator_controller()->is_annotator_enabled());
  // Check that the last used color is stored in the pref.
  EXPECT_EQ(
      pref_service->GetUint64(prefs::kProjectorAnnotatorLastUsedMarkerColor),
      kAnnotatorDefaultPenColor);
}

// Tests that long pressing the AnnotationTray shows a bubble.
TEST_F(AnnotatorControllerTest, LongPressShowsBubble) {
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->OnCanvasInitialized(true);

  auto* annotation_tray = Shell::GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->annotation_tray();

  // Long press the tray item, it should show a bubble.
  gfx::Point location = annotation_tray->GetBoundsInScreen().CenterPoint();

  // Temporarily reconfigure gestures so that the long press takes 2
  // milliseconds.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  const int old_long_press_time_in_ms = gesture_config->long_press_time_in_ms();
  const base::TimeDelta old_short_press_time =
      gesture_config->short_press_time();
  const int old_show_press_delay_in_ms =
      gesture_config->show_press_delay_in_ms();
  gesture_config->set_long_press_time_in_ms(1);
  gesture_config->set_short_press_time(base::Milliseconds(1));
  gesture_config->set_show_press_delay_in_ms(1);

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(location);
  event_generator->PressTouch();

  // Hold the press down for 2 ms, to trigger a long press.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
  run_loop.Run();

  gesture_config->set_long_press_time_in_ms(old_long_press_time_in_ms);
  gesture_config->set_short_press_time(old_short_press_time);
  gesture_config->set_show_press_delay_in_ms(old_show_press_delay_in_ms);

  event_generator->ReleaseTouch();

  EXPECT_TRUE(annotation_tray->GetBubbleWidget());
}

// Tests that tapping the AnnotationTray enables annotation.
TEST_F(AnnotatorControllerTest, TapEnabledAnnotation) {
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->OnCanvasInitialized(true);

  GestureTapOn(Shell::GetPrimaryRootWindowController()
                   ->GetStatusAreaWidget()
                   ->annotation_tray());

  EXPECT_TRUE(annotator_controller()->is_annotator_enabled());
}

TEST_F(AnnotatorControllerTest, OnCanvasInitialized) {
  auto* annotation_tray = Shell::GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->annotation_tray();
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());

  EXPECT_FALSE(annotation_tray->GetEnabled());

  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->OnCanvasInitialized(/*success=*/true);
  EXPECT_TRUE(annotator_controller()->GetAnnotatorAvailability());
  EXPECT_TRUE(annotation_tray->GetEnabled());

  annotator_controller()->OnCanvasInitialized(/*success=*/false);
  EXPECT_FALSE(annotator_controller()->GetAnnotatorAvailability());
  EXPECT_FALSE(annotation_tray->GetEnabled());
}

TEST_F(AnnotatorControllerTest, ToggleAnnotationTray) {
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  annotator_controller()->CreateAnnotationOverlayForWindow(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  annotator_controller()->OnCanvasInitialized(true);
  EXPECT_FALSE(annotator_controller()->is_annotator_enabled());

  annotator_controller()->ToggleAnnotationTray();
  EXPECT_TRUE(annotator_controller()->is_annotator_enabled());
}

TEST_F(AnnotatorControllerTest, CreateAnnotationsOverlayView) {
  auto overlay = annotator_controller()->CreateAnnotationsOverlayView();
  EXPECT_TRUE(overlay.get());
}

TEST_F(AnnotatorControllerTest, UpdateRootViewMultipleDisplays) {
  UpdateDisplay("800x700,801+0-800x700");
  aura::Window::Windows roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  auto* primary_display_tray = Shell::GetPrimaryRootWindowController()
                                   ->GetStatusAreaWidget()
                                   ->annotation_tray();
  auto* external_display_tray = RootWindowController::ForWindow(roots[1])
                                    ->GetStatusAreaWidget()
                                    ->annotation_tray();

  // Show tray on primary root window.
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(primary_display_tray->visible_preferred());
  EXPECT_FALSE(external_display_tray->visible_preferred());

  // Change the root window of the recorded window.
  annotator_controller()->UpdateRootView(roots[1]);
  EXPECT_FALSE(primary_display_tray->visible_preferred());
  EXPECT_TRUE(external_display_tray->visible_preferred());

  annotator_controller()->DisableAnnotator();
  EXPECT_FALSE(primary_display_tray->visible_preferred());
  EXPECT_FALSE(external_display_tray->visible_preferred());
}

TEST_F(AnnotatorControllerTest, RegisterViewTwice) {
  UpdateDisplay("800x700,801+0-800x700");
  aura::Window::Windows roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  auto* primary_display_tray = Shell::GetPrimaryRootWindowController()
                                   ->GetStatusAreaWidget()
                                   ->annotation_tray();
  auto* external_display_tray = RootWindowController::ForWindow(roots[1])
                                    ->GetStatusAreaWidget()
                                    ->annotation_tray();

  // Register view twice on 2 different roots.
  annotator_controller()->RegisterView(Shell::GetPrimaryRootWindow());
  annotator_controller()->RegisterView(roots[1]);
  // Expect the tray to be visible only on the second root window, as the first
  // root window was unregistered. Annotator allows only one root window at a
  // time.
  EXPECT_FALSE(primary_display_tray->visible_preferred());
  EXPECT_TRUE(external_display_tray->visible_preferred());

  annotator_controller()->DisableAnnotator();
  EXPECT_FALSE(primary_display_tray->visible_preferred());
  EXPECT_FALSE(external_display_tray->visible_preferred());
}

}  // namespace ash
