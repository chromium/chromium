// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/capture_mode/action_button_view.h"
#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/capture_mode/capture_button_view.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/search_results_panel.h"
#include "ash/capture_mode/sunfish_capture_bar_view.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/scanner/scanner_metrics.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/auto_reset.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/scanner_provider.h"
#include "disclaimer_view.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ash {

using ::base::test::InvokeFuture;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::DoDefault;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::WithArg;

constexpr char kTestSearchUrl[] =
    "https://www.google.com/search?q=cat&gsc=1&masfc=c";
constexpr char kSunfishConsentDisclaimerAccepted[] =
    "ash.capture_mode.sunfish_consent_disclaimer_accepted";
constexpr std::string_view kSunfishEnabledPrefName =
    "ash.capture_mode.sunfish_enabled";

void WaitForImageCapturedForSearch(PerformCaptureType expected_capture_type) {
  base::test::TestFuture<void> image_captured_future;
  CaptureModeTestApi().SetOnImageCapturedForSearchCallback(
      base::BindLambdaForTesting([&](PerformCaptureType capture_type) {
        if (capture_type == expected_capture_type) {
          image_captured_future.SetValue();
        }
      }));
  EXPECT_TRUE(image_captured_future.Wait());
}

FakeScannerProfileScopedDelegate* GetFakeScannerProfileScopedDelegate(
    ScannerController& scanner_controller) {
  return static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller.delegate_for_testing()->GetProfileScopedDelegate());
}

// Returns a matcher for an action button that returns true if the action button
// has the specified `type`.
Matcher<ActionButtonView*> ActionButtonTypeIs(ActionButtonType type) {
  return Property(&ActionButtonView::rank,
                  Field(&ActionButtonRank::type, type));
}

// Returns a matcher for an action button that returns true if the action button
// is collapsed.
Matcher<ActionButtonView*> ActionButtonIsCollapsed() {
  return Property(&ActionButtonView::label_for_testing,
                  Property(&views::Label::GetVisible, false));
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

    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        kSunfishConsentDisclaimerAccepted, true);
  }

 private:
  // Calling the factory constructor is enough to set it up.
  TestAshWebViewFactory test_web_view_factory_;

  base::test::ScopedFeatureList scoped_feature_list_{features::kSunfishFeature};
  base::AutoReset<bool> ignore_sunfish_secret_key =
      switches::SetIgnoreSunfishSecretKeyForTest();
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

// Tests that the accelerator entry point is a no-op when the enabled pref is
// false.
TEST_F(SunfishTest, AccelEntryPointIsNoopIfEnabledPrefIsFalse) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishEnabledPrefName, false);

  PressAndReleaseKey(ui::VKEY_8,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->IsActive());
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

// Tests the session UI after a region is selected, adjusted, or repositioned,
// and the results panel is shown.
TEST_F(SunfishTest, OnRegionSelectedOrAdjusted) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi test_api(session);

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  auto* search_results_panel_widget = controller->search_results_panel_widget();
  EXPECT_TRUE(search_results_panel_widget);
  EXPECT_EQ(search_results_panel_widget->GetLayer()->GetTargetOpacity(), 1.f);

  // Test that the region selection UI remains visible.
  auto* session_layer = controller->capture_mode_session()->layer();
  EXPECT_TRUE(session_layer->IsVisible());
  EXPECT_EQ(session_layer->GetTargetOpacity(), 1.f);
  EXPECT_TRUE(test_api.AreAllUisVisible());

  // Adjust the region from the northwest corner.
  gfx::Rect old_region = controller->user_capture_region();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(100, 100));
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_EQ(ui::mojom::CursorType::kNorthWestResize,
            cursor_manager->GetCursor().type());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(50, 50));
  EXPECT_EQ(search_results_panel_widget->GetLayer()->GetTargetOpacity(), 0.f);
  event_generator->ReleaseLeftButton();
  EXPECT_NE(controller->user_capture_region(), old_region);

  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  EXPECT_TRUE(search_results_panel_widget);
  EXPECT_EQ(search_results_panel_widget->GetLayer()->GetTargetOpacity(), 1.f);

  // Reposition the region.
  old_region = controller->user_capture_region();
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_EQ(ui::mojom::CursorType::kMove, cursor_manager->GetCursor().type());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(200, 200));
  EXPECT_EQ(search_results_panel_widget->GetLayer()->GetTargetOpacity(), 0.f);
  event_generator->ReleaseLeftButton();
  EXPECT_NE(controller->user_capture_region(), old_region);

  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  EXPECT_TRUE(controller->search_results_panel_widget());
  EXPECT_EQ(search_results_panel_widget->GetLayer()->GetTargetOpacity(), 1.f);
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
  gfx::Rect bounds(100, 100, capture_mode::kSearchResultsPanelWidth,
                   capture_mode::kSearchResultsPanelHeight);
  auto widget =
      SearchResultsPanel::CreateWidget(Shell::GetPrimaryRootWindow(), bounds);
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

TEST_F(SunfishTest, DisclaimerAcceptRecordsHistogramOnce) {
  base::HistogramTester histogram_tester;
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishConsentDisclaimerAccepted, false);

  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  views::Widget* disclaimer = controller->disclaimer_widget();
  ASSERT_TRUE(disclaimer);

  views::View* accept_button =
      disclaimer->GetContentsView()->GetViewByID(kDisclaimerViewAcceptButtonId);
  LeftClickOn(accept_button);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kConsentDisclaimerAccepted, 1);
}

TEST_F(SunfishTest, DisclaimerDeclineRecordsHistogramOnce) {
  base::HistogramTester histogram_tester;
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishConsentDisclaimerAccepted, false);

  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  views::Widget* disclaimer = controller->disclaimer_widget();
  ASSERT_TRUE(disclaimer);

  views::View* decline_button = disclaimer->GetContentsView()->GetViewByID(
      kDisclaimerViewDeclineButtonId);
  LeftClickOn(decline_button);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kConsentDisclaimerRejected, 1);
}

TEST_F(SunfishTest,
       DisclaimerAcceptHidesDisclaimerSetPrefsAndContinuesSession) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishConsentDisclaimerAccepted, false);

  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  views::Widget* disclaimer = controller->disclaimer_widget();
  ASSERT_TRUE(disclaimer);

  views::View* accept_button =
      disclaimer->GetContentsView()->GetViewByID(kDisclaimerViewAcceptButtonId);
  LeftClickOn(accept_button);

  EXPECT_EQ(controller->disclaimer_widget(), nullptr);
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          kSunfishConsentDisclaimerAccepted));
  EXPECT_TRUE(controller->IsActive());
}

TEST_F(SunfishTest, DisclaimerDeclineHidesDisclaimerSetPrefsAndEndsSession) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishConsentDisclaimerAccepted, false);

  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  views::Widget* disclaimer = controller->disclaimer_widget();
  ASSERT_TRUE(disclaimer);

  views::View* decline_button = disclaimer->GetContentsView()->GetViewByID(
      kDisclaimerViewDeclineButtonId);
  LeftClickOn(decline_button);

  EXPECT_EQ(controller->disclaimer_widget(), nullptr);
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          kSunfishConsentDisclaimerAccepted));
  EXPECT_FALSE(controller->IsActive());
}

// Tests that the search results panel receives mouse events.
TEST_F(SunfishTest, OnLocatedEvent) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  // Simulate opening the panel during an active session.
  controller->ShowSearchResultsPanel(gfx::ImageSkia(), GURL(kTestSearchUrl));
  views::Widget* widget = controller->search_results_panel_widget();
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
  controller->ShowSearchResultsPanel(gfx::ImageSkia(), GURL(kTestSearchUrl));
  views::Widget* widget = controller->search_results_panel_widget();
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
  event_generator->MoveMouseBy(10, 10);
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
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  EXPECT_TRUE(controller->search_results_panel_widget());

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

  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // Attempt to add a new action button using the API.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Do not show", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, 0));

  // The region has not been selected yet, so attempting to add a button should
  // do nothing.
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // Select a region on the far left of the screen so we have space for the
  // button between it and the search results panel.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  EXPECT_TRUE(controller->search_results_panel_widget());

  // Create another action button that, when clicked, will change the value of a
  // bool that can be verified later.
  bool pressed = false;
  capture_mode_util::AddActionButton(
      base::BindLambdaForTesting([&]() { pressed = true; }), u"Test",
      &kCaptureModeImageIcon, ActionButtonRank(ActionButtonType::kOther, 0));

  // There should only be one valid button in the session.
  const std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  EXPECT_EQ(action_buttons.size(), 1u);

  // Clicking the button should successfully run the callback, and change the
  // value of the bool.
  LeftClickOn(action_buttons[0]);
  ASSERT_TRUE(pressed);
}

// Tests that action buttons of different types and priorities are displayed in
// the correct order.
TEST_F(SunfishTest, ActionButtonRank) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // Select a region on the far left of the screen so we have space for the
  // button between it and the search results panel.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  EXPECT_TRUE(controller->search_results_panel_widget());

  // Add a default action button.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Default 1", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, 1));
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 1u);

  // Add a sunfish action button, which should appear in the rightmost position.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Sunfish", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kSunfish, 0));
  auto action_buttons = session_test_api.GetActionButtons();
  EXPECT_EQ(action_buttons.size(), 2u);
  EXPECT_EQ(action_buttons[1]->label_for_testing()->GetText(), u"Sunfish");

  // Add another default action button, which should appear in the leftmost
  // position.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Default 2", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, 0));
  action_buttons = session_test_api.GetActionButtons();
  EXPECT_EQ(action_buttons.size(), 3u);
  EXPECT_EQ(action_buttons[0]->label_for_testing()->GetText(), u"Default 2");
  EXPECT_EQ(action_buttons[1]->label_for_testing()->GetText(), u"Default 1");
  EXPECT_EQ(action_buttons[2]->label_for_testing()->GetText(), u"Sunfish");

  // Add a scanner action button, which should appear to the immediate left of
  // the Sunfish action button.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Scanner", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kScanner, 0));
  action_buttons = session_test_api.GetActionButtons();
  EXPECT_EQ(action_buttons.size(), 4u);
  EXPECT_EQ(action_buttons[0]->label_for_testing()->GetText(), u"Default 2");
  EXPECT_EQ(action_buttons[1]->label_for_testing()->GetText(), u"Default 1");
  EXPECT_EQ(action_buttons[2]->label_for_testing()->GetText(), u"Scanner");
  EXPECT_EQ(action_buttons[3]->label_for_testing()->GetText(), u"Sunfish");
}

// Tests the behavior of the region overlay in default capture mode.
TEST_F(SunfishTest, CaptureRegionOverlay) {
  // Start capture mode in a non-region source and type. Test the overlay
  // controller is created but doesn't support region overlay.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi test_api(session);
  EXPECT_TRUE(test_api.GetCaptureRegionOverlayController());
  EXPECT_FALSE(session->active_behavior()->CanPaintRegionOverlay());

  // Set the source to `kRegion`. Test the overlay controller is created and the
  // behavior supports region overlay.
  controller->SetSource(CaptureModeSource::kRegion);
  EXPECT_TRUE(test_api.GetCaptureRegionOverlayController());
  EXPECT_TRUE(session->active_behavior()->CanPaintRegionOverlay());

  // Set the source to `kWindow`. Though the overlay controller has been
  // created, the behavior does not support region overlay.
  controller->SetSource(CaptureModeSource::kWindow);
  EXPECT_TRUE(test_api.GetCaptureRegionOverlayController());
  EXPECT_FALSE(session->active_behavior()->CanPaintRegionOverlay());
}

// Tests that the action container widget's opacity is updated properly before a
// region is selected, while a region is selected, and while the region is being
// adjusted.
TEST_F(SunfishTest, ActionContainerWidgetOpacity) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // The action container widget should be transparent before a region has been
  // selected.
  auto* container_widget = session_test_api.GetActionContainerWidget();
  ASSERT_TRUE(container_widget);
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 0.f);

  // Attempt to add a new action button using the API. The container widget
  // opacity should not change.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Do not show", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, 0));
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 0.f);

  // Select a new capture region. The container widget should be opaque when we
  // have finished selecting a region.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(50, 50, 100, 100),
                          /*release_mouse=*/true, /*verify_region=*/true);
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 1.f);

  // Begin adjusting the capture region by dragging from the bottom right
  // corner. The container widget should be transluscent while the region is
  // being adjusted.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(300, 300));
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 0.f);

  // Finish adjusting the region. The widget should now be visible.
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 1.f);
}

// Tests that the search button is re-shown on region selected or adjusted in
// default mode.
TEST_F(SunfishTest, ShowSearchButtonOnRegionAdjusted) {
  // Start default mode and show the search button.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  VerifyActiveBehavior(BehaviorType::kDefault);

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi session_test_api(session);
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);
  auto* container_widget = session_test_api.GetActionContainerWidget();
  ASSERT_TRUE(container_widget);

  // Adjust the region from the northwest corner.
  gfx::Rect old_region = controller->user_capture_region();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(100, 100));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(50, 50));

  // During the drag, the buttons are hidden.
  ASSERT_TRUE(session->is_drag_in_progress());
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 0.f);
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // Release the drag. Test the buttons are re-shown.
  event_generator->ReleaseLeftButton();
  EXPECT_NE(controller->user_capture_region(), old_region);
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 1.f);
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);
  EXPECT_TRUE(session_test_api.GetActionButtons()[0]->GetVisible());

  // Reposition the region. During the drag, the buttons are hidden.
  old_region = controller->user_capture_region();
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(200, 200));
  ASSERT_TRUE(session->is_drag_in_progress());
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 0.f);
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // Release the drag. Test the buttons are re-shown.
  event_generator->ReleaseLeftButton();
  EXPECT_NE(controller->user_capture_region(), old_region);
  EXPECT_EQ(container_widget->GetLayer()->GetTargetOpacity(), 1.f);
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);
  EXPECT_TRUE(session_test_api.GetActionButtons()[0]->GetVisible());
}

// Tests that the search action button is shown in default capture mode.
TEST_F(SunfishTest, SearchActionButton) {
  // Start default capture mode *not* region selection.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi session_test_api(session);
  // Since we cannot select a region, no action buttons are shown.
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // Set the source type to region, then select a region.
  controller->SetSource(CaptureModeSource::kRegion);
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);

  // Click on the "Search" button. Test we end capture mode.
  LeftClickOn(session_test_api.GetActionButtons()[0]);
  WaitForImageCapturedForSearch(PerformCaptureType::kSearch);
  EXPECT_TRUE(controller->search_results_panel_widget());
  EXPECT_FALSE(controller->IsActive());
}

// Tests that the search action button is not shown in the default capture mode
// if the user has disabled Sunfish.
TEST_F(SunfishTest, SearchActionButtonNotShownIfEnabledPrefIsFalse) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishEnabledPrefName, false);
  // Start default capture mode *not* region selection.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi session_test_api(session);
  // Since we cannot select a region, no action buttons are shown.
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());

  // Set the source type to region, then select a region.
  controller->SetSource(CaptureModeSource::kRegion);
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  // There should not be any buttons.
  EXPECT_THAT(session_test_api.GetActionButtons(), IsEmpty());
}

TEST_F(SunfishTest, SearchBoxInDefaultMode) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(0, test_delegate->num_multimodal_search_requests());

  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  VerifyActiveBehavior(BehaviorType::kDefault);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());

  // Open the search results panel.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500));
  CaptureModeSessionTestApi session_test_api(session);
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);
  LeftClickOn(session_test_api.GetActionButtons()[0]);
  WaitForImageCapturedForSearch(PerformCaptureType::kSearch);
  auto* search_results_panel = controller->GetSearchResultsPanel();
  ASSERT_TRUE(search_results_panel);

  // Click on the search box.
  views::Textfield* textfield = search_results_panel->GetSearchBoxTextfield();
  LeftClickOn(textfield);
  EXPECT_TRUE(textfield->HasFocus());

  // Type and press Enter. Test it makes a multimodal search.
  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_EQ(u"a", textfield->GetText());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(1, test_delegate->num_multimodal_search_requests());
}

// Tests that the search box sends multimodal search requests.
TEST_F(SunfishTest, SearchBoxTextfield) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(0, test_delegate->num_multimodal_search_requests());

  controller->StartSunfishSession();
  VerifyActiveBehavior(BehaviorType::kSunfish);

  // Open the search results panel.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);

  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  auto* widget = controller->search_results_panel_widget();
  ASSERT_TRUE(widget);

  // Click on the search box.
  auto* search_results_panel =
      views::AsViewClass<SearchResultsPanel>(widget->GetContentsView());
  ASSERT_TRUE(search_results_panel);
  views::Textfield* textfield = search_results_panel->GetSearchBoxTextfield();
  LeftClickOn(textfield);
  EXPECT_TRUE(textfield->HasFocus());

  // Type and press Enter. Test it makes a multimodal search.
  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_EQ(u"a", textfield->GetText());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(1, test_delegate->num_multimodal_search_requests());
}

// Tests that the search results panel is preserved between sessions.
TEST_F(SunfishTest, SwitchSessionsWhilePanelOpen) {
  // Open the search results panel.
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  auto* generator = GetEventGenerator();
  SelectCaptureModeRegion(generator, gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  auto* search_results_panel = controller->GetSearchResultsPanel();
  ASSERT_TRUE(search_results_panel);
  search_results_panel->SetSearchBoxText(u"cat");
  EXPECT_EQ(u"cat", search_results_panel->GetSearchBoxTextfield()->GetText());

  // Switch to default mode. Test the panel remains the same.
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  ASSERT_TRUE(search_results_panel);
  EXPECT_EQ(u"cat", search_results_panel->GetSearchBoxTextfield()->GetText());
}

// Tests no dangling ptr and crash when closing the panel.
TEST_F(SunfishTest, NoCrashOnUpdateCaptureUisOpacity) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  auto* generator = GetEventGenerator();
  SelectCaptureModeRegion(generator, gfx::Rect(50, 50, 400, 400),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  auto* panel_widget = controller->search_results_panel_widget();
  ASSERT_TRUE(panel_widget);

  // Click the close button and wait for it to close.
  LeftClickOn(controller->GetSearchResultsPanel()->close_button());
  views::test::WidgetDestroyedWaiter(panel_widget).Wait();
  EXPECT_FALSE(controller->search_results_panel_widget());

  // Start sunfish again and re-select a region. Test no crash.
  controller->StartSunfishSession();
  SelectCaptureModeRegion(generator, gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/false);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  EXPECT_TRUE(controller->search_results_panel_widget());
}

class ScannerTest : public AshTestBase {
 public:
  ScannerTest() = default;
  ScannerTest(const ScannerTest&) = delete;
  ScannerTest& operator=(const ScannerTest&) = delete;
  ~ScannerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        kSunfishConsentDisclaimerAccepted, true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kScannerUpdate};
  base::AutoReset<bool> ignore_scanner_update_secret_key_ =
      switches::SetIgnoreScannerUpdateSecretKeyForTest();
};

// Tests that a Scanner session is created when a Sunfish session begins.
TEST_F(ScannerTest, CreatesScannerSession) {
  CaptureModeController::Get()->StartSunfishSession();

  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->HasActiveSessionForTesting());
}

// Tests that action buttons are created when a Scanner response includes
// suggested actions.
TEST_F(ScannerTest, CreatesScannerActionButtons) {
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(fetch_actions_future)));
  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);

  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  fetch_actions_future.Take().Run(std::move(output), manta::MantaStatus());

  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  EXPECT_THAT(session_test_api.GetActionButtons(), SizeIs(2));
}

// Tests that action buttons are created when the Scanner response returns as
// fast as possible.
TEST_F(ScannerTest, FetchActionsImmediately) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  // Synchronously send the fake actions response after the image is captured.
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(std::move(output), manta::MantaStatus()));

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);

  // We should have successfully created two action buttons.
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  EXPECT_THAT(session_test_api.GetActionButtons(), SizeIs(2));
}

// Tests that action buttons for a stale Scanner request are not added to a new
// and different region the same session.
TEST_F(ScannerTest, DoesNotCreateActionButtonsForNewerDifferentRegion) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      stale_fetch_actions_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(stale_fetch_actions_future)))
      .WillOnce(DoDefault());

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(50, 50, 500, 400),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  stale_fetch_actions_future.Take().Run(std::move(output),
                                        manta::MantaStatus());

  // The callback for the stale actions should not affect the current actions.
  EXPECT_THAT(session_test_api.GetActionButtons(), IsEmpty());
}

// Tests that action buttons for a stale Scanner request are not added to a new
// but identical region in the same session.
TEST_F(ScannerTest, DoesNotCreateActionButtonsForNewerSameRegion) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      stale_fetch_actions_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(stale_fetch_actions_future)))
      .WillOnce(DoDefault());

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  // Reselect the same region. Trying to drag the exact same bounds will instead
  // modify the existing region, so we need to clear the region first.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(),
                          /*release_mouse=*/true, /*verify_region=*/true);
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  stale_fetch_actions_future.Take().Run(std::move(output),
                                        manta::MantaStatus());

  // The callback for the stale actions should not affect the current actions.
  EXPECT_THAT(session_test_api.GetActionButtons(), IsEmpty());
}

// Tests that action buttons for a stale Scanner request are not added to a new
// empty region in the same session.
TEST_F(ScannerTest, DoesNotCreateActionButtonsForNewerEmptyRegion) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      stale_fetch_actions_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(stale_fetch_actions_future)));

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(),
                          /*release_mouse=*/true, /*verify_region=*/true);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  stale_fetch_actions_future.Take().Run(std::move(output),
                                        manta::MantaStatus());

  // The callback for the stale actions should not affect the current actions.
  EXPECT_THAT(session_test_api.GetActionButtons(), IsEmpty());
}

// Tests that action buttons for a stale Scanner request are not added to region
// that is currently being selected, that is identical to the region that was
// selected for the stale Scanner request.
TEST_F(ScannerTest, DoesNotCreateActionButtonsForNewerSameRegionBeingSelected) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      stale_fetch_actions_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(stale_fetch_actions_future)));

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  // Reselect the same region without releasing the mouse. Trying to drag the
  // exact same bounds will instead modify the existing region, so we need to
  // clear the region first.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(),
                          /*release_mouse=*/true, /*verify_region=*/true);
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/false, /*verify_region=*/true);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  stale_fetch_actions_future.Take().Run(std::move(output),
                                        manta::MantaStatus());

  // The callback for the stale actions should not affect the current actions.
  EXPECT_THAT(session_test_api.GetActionButtons(), IsEmpty());
}

// Tests that action buttons for a stale Scanner request are not added to region
// that is currently being selected, that is identical to the region that was
// selected for the stale Scanner request.
TEST_F(ScannerTest,
       DoesNotCreateActionButtonsForNewerEmptyRegionBeingSelected) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      stale_fetch_actions_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(stale_fetch_actions_future)));

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(),
                          /*release_mouse=*/false, /*verify_region=*/true);
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  stale_fetch_actions_future.Take().Run(std::move(output),
                                        manta::MantaStatus());

  // The callback for the stale actions should not affect the current actions.
  EXPECT_THAT(session_test_api.GetActionButtons(), IsEmpty());
}

TEST_F(ScannerTest, ActionButtonsEndSessionOnActionSuccess) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  manta::proto::ScannerOutput output;
  output.add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event 1");
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_action_details_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionDetailsForImage)
      .WillOnce(WithArg<2>(InvokeFuture(fetch_action_details_future)));

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  ASSERT_THAT(
      action_buttons,
      ElementsAre(Property(&ActionButtonView::label_for_testing,
                           Property(&views::Label::GetText, u"New event"))));
  LeftClickOn(action_buttons[0]);
  fetch_action_details_future.Take().Run(
      std::make_unique<manta::proto::ScannerOutput>(output),
      manta::MantaStatus());

  EXPECT_FALSE(capture_mode_controller->IsActive());
}

TEST_F(ScannerTest, ActionButtonsEndSessionOnActionSuccessAfterFailure) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  manta::proto::ScannerOutput output;
  output.add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event 1");
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_action_details_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionDetailsForImage)
      .Times(2)
      .WillRepeatedly(WithArg<2>(InvokeFuture(fetch_action_details_future)));

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  ASSERT_THAT(
      action_buttons,
      ElementsAre(Property(&ActionButtonView::label_for_testing,
                           Property(&views::Label::GetText, u"New event"))));
  LeftClickOn(action_buttons[0]);
  fetch_action_details_future.Take().Run(nullptr, manta::MantaStatus());
  LeftClickOn(action_buttons[0]);
  fetch_action_details_future.Take().Run(
      std::make_unique<manta::proto::ScannerOutput>(output),
      manta::MantaStatus());

  EXPECT_FALSE(capture_mode_controller->IsActive());
}

TEST_F(ScannerTest, ActionButtonsDoNotEndSessionOnActionFailure) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  output->add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event 1");
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(std::move(output), manta::MantaStatus()));
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_action_details_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionDetailsForImage)
      .WillOnce(WithArg<2>(InvokeFuture(fetch_action_details_future)));

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  ASSERT_THAT(
      action_buttons,
      ElementsAre(Property(&ActionButtonView::label_for_testing,
                           Property(&views::Label::GetText, u"New event"))));
  LeftClickOn(action_buttons[0]);
  fetch_action_details_future.Take().Run(nullptr, manta::MantaStatus());

  EXPECT_TRUE(capture_mode_controller->IsActive());
}

TEST_F(ScannerTest, ActionButtonsAreDisabledWhenScannerActionIsRunning) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(std::move(output), manta::MantaStatus()));
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionDetailsForImage)
      .Times(1);

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  ASSERT_THAT(action_buttons,
              AllOf(SizeIs(2), Each(Property("GetEnabled",
                                             &views::View::GetEnabled, true))));
  LeftClickOn(action_buttons[0]);

  EXPECT_THAT(
      session_test_api.GetActionButtons(),
      AllOf(SizeIs(2),
            Each(Property("GetEnabled", &views::View::GetEnabled, false))));
}

TEST_F(ScannerTest,
       ActionButtonsAreEnabledWhenScannerActionFinishesWithFailure) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(std::move(output), manta::MantaStatus()));
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_action_details_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionDetailsForImage)
      .WillOnce(WithArg<2>(InvokeFuture(fetch_action_details_future)));

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  ASSERT_THAT(action_buttons,
              AllOf(SizeIs(2), Each(Property("GetEnabled",
                                             &views::View::GetEnabled, true))));
  LeftClickOn(action_buttons[0]);
  ASSERT_THAT(
      session_test_api.GetActionButtons(),
      AllOf(SizeIs(2),
            Each(Property("GetEnabled", &views::View::GetEnabled, false))));
  fetch_action_details_future.Take().Run(nullptr, manta::MantaStatus());

  EXPECT_THAT(session_test_api.GetActionButtons(),
              AllOf(SizeIs(2), Each(Property("GetEnabled",
                                             &views::View::GetEnabled, true))));
}

// Tests that no text detection request is ever made in default capture mode if
// the Sunfish enabled pref is false.
TEST_F(ScannerTest, NoTextDetectionInDefaultModeIfEnabledPrefIsFalse) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishEnabledPrefName, false);
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);

  // No text detection request should have been made, so there should not be a
  // pending DLP check.
  EXPECT_FALSE(CaptureModeTestApi().IsPendingDlpCheck());
}

// Tests that the copy text button is shown in default capture mode if text is
// detected in the selected region.
TEST_F(ScannerTest, CopyTextButtonShownForDetectedText) {
  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_CALL(*test_delegate, DetectTextInImage)
      .WillOnce(WithArg<1>(InvokeFuture(detect_text_future)));

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  detect_text_future.Take().Run("detected text");

  const CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  // Copy text button should have been created.
  // TODO(crbug.com/376174530): Add a test API to get buttons of a particular
  // type / rank and use it here.
  std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  ASSERT_THAT(action_buttons,
              ElementsAre(_, ActionButtonTypeIs(ActionButtonType::kCopyText)));
  // Clipboard should currently be empty.
  std::u16string clipboard_data;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard_data);
  EXPECT_EQ(clipboard_data, u"");
  // Clicking on the button should copy text to clipboard.
  LeftClickOn(action_buttons[1]);
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard_data);
  EXPECT_EQ(clipboard_data, u"detected text");
}

// Tests that the copy text button is not shown in default capture mode if no
// text is detected in the selected region.
TEST_F(ScannerTest, NoCopyTextButtonIfNoDetectedText) {
  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_CALL(*test_delegate, DetectTextInImage)
      .WillOnce(WithArg<1>(InvokeFuture(detect_text_future)));

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  detect_text_future.Take().Run("");

  const CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_THAT(session_test_api.GetActionButtons(),
              Not(Contains(ActionButtonTypeIs(ActionButtonType::kCopyText))));
}

// Tests that the copy text button is not shown if the selected region changes
// before text detection completes.
TEST_F(ScannerTest, NoCopyTextButtonIfSelectedRegionChanges) {
  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_CALL(*test_delegate, DetectTextInImage)
      .WillOnce(WithArg<1>(InvokeFuture(detect_text_future)));

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 50),
                          /*release_mouse=*/true, /*verify_region=*/true);
  OnTextDetectionComplete callback = detect_text_future.Take();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 50, 50),
                          /*release_mouse=*/true, /*verify_region=*/true);
  std::move(callback).Run("detected text");

  const CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_THAT(session_test_api.GetActionButtons(),
              Not(Contains(ActionButtonTypeIs(ActionButtonType::kCopyText))));
}

// Tests that the smart actions button is shown in default capture mode if text
// is detected in the selected region.
TEST_F(ScannerTest, SmartActionsButtonShownForDetectedText) {
  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_CALL(*test_delegate, DetectTextInImage)
      .WillOnce(WithArg<1>(InvokeFuture(detect_text_future)));

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  detect_text_future.Take().Run("detected text");

  const CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  // Smart actions button should have been created.
  std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  ASSERT_THAT(action_buttons,
              ElementsAre(AllOf(ActionButtonTypeIs(ActionButtonType::kScanner),
                                ActionButtonIsCollapsed()),
                          AllOf(ActionButtonTypeIs(ActionButtonType::kCopyText),
                                Not(ActionButtonIsCollapsed()))));

  // Click the smart actions button.
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(fetch_actions_future)));
  LeftClickOn(action_buttons[0]);
  WaitForImageCapturedForSearch(PerformCaptureType::kScanner);

  // Smart actions button should have been removed, and the copy text button
  // should be collapsed.
  EXPECT_THAT(session_test_api.GetActionButtons(),
              ElementsAre(AllOf(ActionButtonTypeIs(ActionButtonType::kCopyText),
                                ActionButtonIsCollapsed())));

  // Simulate a single fetched Scanner action.
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event");
  fetch_actions_future.Take().Run(std::move(output), manta::MantaStatus());

  // Check for a Scanner action button and a collapsed copy text button.
  EXPECT_THAT(session_test_api.GetActionButtons(),
              ElementsAre(AllOf(ActionButtonTypeIs(ActionButtonType::kScanner),
                                Not(ActionButtonIsCollapsed())),
                          AllOf(ActionButtonTypeIs(ActionButtonType::kCopyText),
                                ActionButtonIsCollapsed())));
}

}  // namespace ash
