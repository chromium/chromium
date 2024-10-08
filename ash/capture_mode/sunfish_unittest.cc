// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/capture_mode/capture_button_view.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/search_results_panel.h"
#include "ash/capture_mode/sunfish_capture_bar_view.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/auto_reset.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ash {

using ::testing::SizeIs;

void WaitForImageCapturedForSearch() {
  base::RunLoop run_loop;
  ash::CaptureModeTestApi().SetOnImageCapturedForSearchCallback(
      run_loop.QuitClosure());
  run_loop.Run();
}

FakeScannerProfileScopedDelegate* GetFakeScannerProfileScopedDelegate(
    ScannerController& scanner_controller) {
  return static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller.delegate_for_testing()->GetProfileScopedDelegate());
}

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

// Tests that the Enter key does not attempt to perform capture or image search.
TEST_F(SunfishTest, PressEnterKey) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  // While we are in waiting to select a capture region phase, pressing the
  // Enter key will do nothing.
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  auto* capture_button =
      session_test_api.GetCaptureLabelView()->capture_button_container();
  auto* capture_label = session_test_api.GetCaptureLabelInternalView();
  ASSERT_FALSE(capture_button->GetVisible());
  ASSERT_TRUE(capture_label->GetVisible());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_TRUE(controller->IsActive());

  // Immediately upon region selection, `PerformImageSearch()` and
  // `OnCaptureImageAttempted()` will be called once.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  ASSERT_FALSE(capture_button->GetVisible());
  ASSERT_FALSE(capture_label->GetVisible());
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(1, test_delegate->num_capture_image_attempts());

  // Test that pressing the Enter key does not attempt image capture again.
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(1, test_delegate->num_capture_image_attempts());
}

// Tests the session UI after a region is dragged and the results panel is
// shown.
TEST_F(SunfishTest, OnRegionSelected) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi test_api(session);

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch();
  EXPECT_TRUE(session->search_results_panel_widget());

  // Test that the region selection UI remains visible.
  auto* session_layer = controller->capture_mode_session()->layer();
  EXPECT_TRUE(session_layer->IsVisible());
  EXPECT_EQ(session_layer->GetTargetOpacity(), 1.f);
  EXPECT_TRUE(test_api.AreAllUisVisible());
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

  // Release the drag. The label and button should still both be hidden.
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(capture_button->GetVisible());
  EXPECT_FALSE(capture_label->GetVisible());
}

// Tests that sunfish checks DLP restrictions upon selecting a region.
TEST_F(SunfishTest, CheckDlpRestrictions) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  auto* session = controller->capture_mode_session();
  ASSERT_EQ(BehaviorType::kSunfish,
            session->active_behavior()->behavior_type());
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  test_delegate->set_is_allowed_by_dlp(false);

  // Tests after selecting a region, the session is ended.
  auto* event_generator = GetEventGenerator();
  SelectCaptureModeRegion(event_generator, gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/false);
  EXPECT_FALSE(controller->IsActive());
  test_delegate->set_is_allowed_by_dlp(true);
}

// Tests that a full screenshot can be taken while in sunfish session.
TEST_F(SunfishTest, PerformFullScreenshot) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());
  EXPECT_EQ(
      controller->capture_mode_session()->active_behavior()->behavior_type(),
      BehaviorType::kSunfish);

  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  base::FilePath file_saved_path = WaitForCaptureFileToBeSaved();
  ASSERT_TRUE(controller->IsActive());
  EXPECT_EQ(
      controller->capture_mode_session()->active_behavior()->behavior_type(),
      BehaviorType::kSunfish);
  const base::FilePath default_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  EXPECT_EQ(file_saved_path.DirName(), default_folder);
}

// Tests that the capture region is reset if sunfish is restarted.
TEST_F(SunfishTest, ResetCaptureRegion) {
  // Start sunfish, then select a region.
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_EQ(
      BehaviorType::kSunfish,
      controller->capture_mode_session()->active_behavior()->behavior_type());

  const gfx::Rect capture_region(100, 100, 600, 500);
  SelectCaptureModeRegion(GetEventGenerator(), capture_region,
                          /*release_mouse=*/false);
  EXPECT_EQ(capture_region, controller->user_capture_region());

  // Exit sunfish, then restart sunfish.
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  ASSERT_FALSE(controller->IsActive());

  controller->StartSunfishSession();
  EXPECT_TRUE(controller->user_capture_region().IsEmpty());
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  auto* capture_label = test_api.GetCaptureLabelInternalView();
  EXPECT_TRUE(capture_label->GetVisible());
  EXPECT_EQ(u"Drag to select an area to search", capture_label->GetText());
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

class MockSearchResultsPanel : public SearchResultsPanel {
 public:
  MockSearchResultsPanel() = default;
  MockSearchResultsPanel(MockSearchResultsPanel&) = delete;
  MockSearchResultsPanel& operator=(MockSearchResultsPanel&) = delete;
  ~MockSearchResultsPanel() override = default;

  void OnMouseEvent(ui::MouseEvent* event) override {
    mouse_events_received_ = true;
    SearchResultsPanel::OnMouseEvent(event);

    // Set random cursor types for testing.
    auto* cursor_manager = Shell::Get()->cursor_manager();
    switch (event->type()) {
      case ui::EventType::kMousePressed:
        cursor_manager->SetCursorForced(ui::mojom::CursorType::kPointer);
        break;
      case ui::EventType::kMouseDragged:
        cursor_manager->SetCursorForced(ui::mojom::CursorType::kCross);
        break;
      case ui::EventType::kMouseReleased:
        cursor_manager->SetCursorForced(ui::mojom::CursorType::kHand);
        break;
      default:
        break;
    }
  }

  bool mouse_events_received() const { return mouse_events_received_; }

 private:
  bool mouse_events_received_ = false;
};

// Tests that the search results panel receives mouse events.
TEST_F(SunfishTest, OnLocatedEvent) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());

  // Simulate opening the panel during an active session.
  session->ShowSearchResultsPanel(gfx::ImageSkia());
  views::Widget* widget = session->search_results_panel_widget();
  ASSERT_TRUE(widget);
  auto* search_results_panel =
      widget->SetContentsView(std::make_unique<MockSearchResultsPanel>());
  ASSERT_TRUE(controller->IsActive());
  EXPECT_FALSE(search_results_panel->mouse_events_received());

  // Simulate a click on the panel.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      search_results_panel->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // Test the panel receives mouse events.
  EXPECT_TRUE(search_results_panel->mouse_events_received());
}

// Tests that the search results panel updates the cursor type.
TEST_F(SunfishTest, UpdateCursor) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_EQ(ui::mojom::CursorType::kCell, cursor_manager->GetCursor().type());

  // Simulate opening the panel during an active session.
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  session->ShowSearchResultsPanel(gfx::ImageSkia());
  views::Widget* widget = session->search_results_panel_widget();
  ASSERT_TRUE(widget);
  auto* search_results_panel =
      widget->SetContentsView(std::make_unique<MockSearchResultsPanel>());
  ASSERT_TRUE(controller->IsActive());
  EXPECT_EQ(ui::mojom::CursorType::kCell, cursor_manager->GetCursor().type());

  // Simulate a click on the panel.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      search_results_panel->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            cursor_manager->GetCursor().type());

  // Simulate a drag in the panel.
  event_generator->MoveMouseTo(
      search_results_panel->GetBoundsInScreen().bottom_right());
  EXPECT_EQ(ui::mojom::CursorType::kCross, cursor_manager->GetCursor().type());

  // Simulate mouse release in the panel.
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(ui::mojom::CursorType::kHand, cursor_manager->GetCursor().type());
}

// Tests that while a video recording is in progress, starting sunfish works
// correctly.
TEST_F(SunfishTest, StartRecordingThenStartSunfish) {
  // Start Capture Mode in a fullscreen video recording mode.
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());

  // Start a video recording.
  StartVideoRecordingImmediately();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Start sunfish session.
  controller->StartSunfishSession();
  EXPECT_TRUE(controller->IsActive());

  // Expect the behavior and UI to be updated.
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  EXPECT_EQ(session->active_behavior()->behavior_type(),
            BehaviorType::kSunfish);
  CaptureModeSessionTestApi test_api(session);
  EXPECT_TRUE(views::AsViewClass<SunfishCaptureBarView>(
      test_api.GetCaptureModeBarView()));

  // Before the drag, only the capture label is visible and is in waiting to
  // select a capture region phase.
  auto* capture_button =
      test_api.GetCaptureLabelView()->capture_button_container();
  auto* capture_label = test_api.GetCaptureLabelInternalView();
  EXPECT_FALSE(capture_button->GetVisible());
  EXPECT_TRUE(capture_label->GetVisible());
  EXPECT_EQ(u"Drag to select an area to search", capture_label->GetText());

  // Test we can select a region and show the search results panel.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch();
  EXPECT_TRUE(session->search_results_panel_widget());

  // Test we can stop video recording.
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
}

// Tests that when capture mode session is active, switching between behavior
// types updates the session type and UI.
TEST_F(SunfishTest, SwitchBehaviorTypes) {
  // Start default capture mode session.
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  VerifyActiveBehavior(BehaviorType::kDefault);

  // Switch to sunfish session.
  PressAndReleaseKey(ui::VKEY_8,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  VerifyActiveBehavior(BehaviorType::kSunfish);

  // Switch to default capture mode session.
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  VerifyActiveBehavior(BehaviorType::kDefault);

  // Switch to sunfish session.
  PressAndReleaseKey(ui::VKEY_8,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  VerifyActiveBehavior(BehaviorType::kSunfish);
}

// Tests that while a sunfish session has a region selected, calling the API
// will successfully create a new action button.
TEST_F(SunfishTest, AddActionButton) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());

  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // Attempt to add a new action button using the API.
  capture_mode_util::AddActionButton(views::Button::PressedCallback(),
                                     u"Do not show", &kCaptureModeImageIcon);

  // The region has not been selected yet, so attempting to add a button should
  // do nothing.
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // Select a region on the far left of the screen so we have space for the
  // button between it and the search results panel.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch();
  EXPECT_TRUE(session->search_results_panel_widget());

  // Create another action button that, when clicked, will change the value of a
  // bool that can be verified later.
  bool pressed = false;
  capture_mode_util::AddActionButton(
      base::BindLambdaForTesting([&]() { pressed = true; }), u"Test",
      &kCaptureModeImageIcon);

  // There should only be one valid button in the session.
  const std::vector<PillButton*> action_buttons =
      session_test_api.GetActionButtons();
  EXPECT_EQ(action_buttons.size(), 1u);

  // Clicking the button should successfully run the callback, and change the
  // value of the bool.
  LeftClickOn(action_buttons[0]);
  ASSERT_TRUE(pressed);
}

class SunfishWithScannerTest : public SunfishTest {
 public:
  SunfishWithScannerTest() = default;
  SunfishWithScannerTest(const SunfishWithScannerTest&) = delete;
  SunfishWithScannerTest& operator=(const SunfishWithScannerTest&) = delete;
  ~SunfishWithScannerTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kScannerUpdate};
  base::AutoReset<bool> ignore_scanner_update_secret_key_ =
      switches::SetIgnoreScannerUpdateSecretKeyForTest();
};

// Tests that a Scanner session is created when a Sunfish session begins.
TEST_F(SunfishWithScannerTest, CreatesScannerSession) {
  CaptureModeController::Get()->StartSunfishSession();

  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->HasActiveSessionForTesting());
}

// Tests that action buttons are created when a Scanner response includes
// suggested actions.
TEST_F(SunfishWithScannerTest, CreatesScannerActionButtons) {
  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch();

  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  GetFakeScannerProfileScopedDelegate(*scanner_controller)
      ->SendFakeActionsResponse(base::ok(std::vector<ScannerAction>{
          NewCalendarEventAction("Event 1"),
          NewCalendarEventAction("Event 2"),
      }));

  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  EXPECT_THAT(session_test_api.GetActionButtons(), SizeIs(2));
}

}  // namespace ash
