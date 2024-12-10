// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/search_box_view.h"
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
#include "ash/capture_mode/capture_region_overlay_controller.h"
#include "ash/capture_mode/search_results_panel.h"
#include "ash/capture_mode/sunfish_capture_bar_view.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/scanner/scanner_metrics.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/tab_slider_button.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/auto_reset.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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
#include "ui/gfx/animation/throb_animation.h"
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
using ::testing::AnyOf;
using ::testing::DoDefault;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::WithArg;

constexpr char kSunfishConsentDisclaimerAccepted[] =
    "ash.capture_mode.sunfish_consent_disclaimer_accepted";
constexpr char kCaptureModeTextCopiedToastId[] = "capture_mode_text_copied";

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
// has the specified `id`.
Matcher<ActionButtonView*> ActionButtonIdIs(int id) {
  return Property(&ActionButtonView::GetID, id);
}

// Returns a matcher for an action button that returns true if the action button
// is collapsed.
Matcher<ActionButtonView*> ActionButtonIsCollapsed() {
  return Property(&ActionButtonView::label_for_testing,
                  Property(&views::Label::GetVisible, false));
}

class SunfishTestBase : public AshTestBase {
 public:
  SunfishTestBase()
      : AshTestBase(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}
  SunfishTestBase(const SunfishTestBase&) = delete;
  SunfishTestBase& operator=(const SunfishTestBase&) = delete;
  ~SunfishTestBase() override = default;

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
};

class SunfishDisabledTest : public SunfishTestBase {
 public:
  SunfishDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kSunfishFeature);
  }
  SunfishDisabledTest(const SunfishDisabledTest&) = delete;
  SunfishDisabledTest& operator=(const SunfishDisabledTest&) = delete;
  ~SunfishDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the accelerator entry point is a no-op when the feature is not
// enabled.
TEST_F(SunfishDisabledTest, AccelEntryPointIsNoop) {
  PressAndReleaseKey(ui::VKEY_8,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->IsActive());
}

// Tests that the search action button is not shown in the default capture mode
// if the feature is disabled.
TEST_F(SunfishDisabledTest, SearchActionButtonNotShown) {
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

// Tests that no text detection request is ever made in default capture mode if
// the feature is disabled.
TEST_F(SunfishDisabledTest, NoTextDetectionInDefaultMode) {
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);

  // No text detection request should have been made, so there should not be a
  // pending DLP check.
  EXPECT_FALSE(CaptureModeTestApi().IsPendingDlpCheck());
}

// Tests that the feedback button is not shown in default capture mode if the
// feature is disabled.
TEST_F(SunfishDisabledTest, FeedbackButtonNotShownInDefaultMode) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);

  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_TRUE(session);
  CaptureModeSessionTestApi session_test_api(session);
  views::Widget* feedback_button_widget =
      session_test_api.GetFeedbackButtonWidget();
  // There are various ways a widget can be hidden. Any of them should pass this
  // test.
  EXPECT_THAT(
      feedback_button_widget,
      AnyOf(IsNull(), Property("IsVisible", &views::Widget::IsVisible, false),
            Property("GetLayer", &views::Widget::GetLayer,
                     AnyOf(Property("GetTargetOpacity",
                                    &ui::Layer::GetTargetOpacity, 0.f),
                           Property("GetTargetVisibility",
                                    &ui::Layer::GetTargetVisibility, false)))));
}

class SunfishTest : public SunfishTestBase {
 public:
  SunfishTest() = default;
  SunfishTest(const SunfishTest&) = delete;
  SunfishTest& operator=(const SunfishTest&) = delete;
  ~SunfishTest() override = default;

 private:
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
      prefs::kSunfishEnabled, false);

  PressAndReleaseKey(ui::VKEY_8,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(SunfishTest, RecordsStartSunfishSessionMetric) {
  base::HistogramTester histogram_tester;

  CaptureModeController::Get()->StartSunfishSession();

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kSunfishScreenEnteredViaShortcut, 1);
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

TEST_F(SunfishTest, NoCrashOnEscapeBeforePanelShown) {
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(810, 10, 400, 400)));
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(root_windows[0], w1->GetRootWindow());
  ASSERT_EQ(root_windows[1], w2->GetRootWindow());

  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();

  // Simulate pressing the Escape key, then the panel loading after the session
  // is ended.
  auto* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  controller->OnSearchUrlFetched(gfx::Rect(10, 10, 400, 400), gfx::ImageSkia(),
                                 GURL("kTestUrl"));
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
  EXPECT_TRUE(search_results_panel_widget->IsVisible());

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
  ASSERT_TRUE(controller->capture_mode_session()->is_drag_in_progress());
  EXPECT_FALSE(search_results_panel_widget->IsVisible());
  event_generator->ReleaseLeftButton();
  EXPECT_NE(controller->user_capture_region(), old_region);

  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  EXPECT_TRUE(search_results_panel_widget);
  EXPECT_TRUE(search_results_panel_widget->IsVisible());

  // Reposition the region.
  old_region = controller->user_capture_region();
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_EQ(ui::mojom::CursorType::kMove, cursor_manager->GetCursor().type());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(200, 200));
  ASSERT_TRUE(controller->capture_mode_session()->is_drag_in_progress());
  EXPECT_FALSE(search_results_panel_widget->IsVisible());
  event_generator->ReleaseLeftButton();
  EXPECT_NE(controller->user_capture_region(), old_region);

  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  EXPECT_TRUE(controller->search_results_panel_widget());
  EXPECT_TRUE(search_results_panel_widget->IsVisible());
  event_generator->ReleaseLeftButton();
}

// Tests that the panel is not hidden on mouse down.
TEST_F(SunfishTest, DoNotHidePanelOnMousePressed) {
  // Start sunfish and open the panel.
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  auto* generator = GetEventGenerator();
  SelectCaptureModeRegion(generator, gfx::Rect(50, 50, 400, 400),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_TRUE(controller->search_results_panel_widget());

  // Press the sunfish bar close button.
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  auto* close_button = test_api.GetCaptureModeBarView()->close_button();
  ASSERT_TRUE(close_button);

  // Press down on a location. Test the panel is still visible.
  generator->MoveMouseTo(10, 10);
  generator->PressLeftButton();
  ASSERT_TRUE(controller->capture_mode_session()->is_drag_in_progress());
  ASSERT_TRUE(controller->search_results_panel_widget());
  EXPECT_TRUE(controller->search_results_panel_widget()->IsVisible());
  EXPECT_EQ(
      controller->search_results_panel_widget()->GetLayer()->GetTargetOpacity(),
      1.f);
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
  widget->SetBounds(gfx::Rect(100, 100, capture_mode::kSearchResultsPanelWidth,
                              capture_mode::kSearchResultsPanelHeight));
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

  MOCK_METHOD(void, SetSearchBoxImage, (const gfx::ImageSkia&));
  MOCK_METHOD(void, Navigate, (const GURL&));

  bool mouse_events_received() const { return mouse_events_received_; }

 private:
  bool mouse_events_received_ = false;
};

// Tests that the search results panel receives mouse events.
TEST_F(SunfishTest, OnLocatedEvent) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  ASSERT_TRUE(controller->IsActive());

  // Simulate opening the panel during an active session.
  auto* event_generator = GetEventGenerator();
  SelectCaptureModeRegion(event_generator, gfx::Rect(50, 50, 400, 400),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  views::Widget* widget = controller->search_results_panel_widget();
  ASSERT_TRUE(widget && widget->IsVisible());
  auto* search_results_panel =
      widget->SetContentsView(std::make_unique<MockSearchResultsPanel>());
  ASSERT_TRUE(controller->IsActive());
  EXPECT_FALSE(search_results_panel->mouse_events_received());

  // Simulate a click on the panel.
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

  // Select a capture region that doesn't overlap with the panel when it is
  // shown.
  auto* event_generator = GetEventGenerator();
  SelectCaptureModeRegion(event_generator, gfx::Rect(10, 10, 50, 50),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  views::Widget* widget = controller->search_results_panel_widget();
  ASSERT_TRUE(widget && widget->IsVisible());
  widget->SetContentsView(std::make_unique<MockSearchResultsPanel>());

  const gfx::Rect panel_bounds(widget->GetWindowBoundsInScreen());
  ASSERT_FALSE(
      panel_bounds.Contains(event_generator->current_screen_location()));
  ASSERT_TRUE(controller->IsActive());
  event_generator->MoveMouseTo(panel_bounds.x() - 10, panel_bounds.y() - 10);
  EXPECT_EQ(ui::mojom::CursorType::kCell, cursor_manager->GetCursor().type());

  // Simulate a click on the panel.
  event_generator->MoveMouseTo(panel_bounds.CenterPoint());
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

TEST_F(SunfishTest, IsSearchResultsPanelVisible) {
  // Show the panel.
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  auto* generator = GetEventGenerator();
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_EQ(ui::mojom::CursorType::kCell, cursor_manager->GetCursor().type());

  // Select a capture region that will not overlap with the panel.
  SelectCaptureModeRegion(generator, gfx::Rect(10, 10, 50, 50),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  views::Widget* widget = controller->search_results_panel_widget();
  ASSERT_TRUE(widget);
  widget->SetContentsView(std::make_unique<MockSearchResultsPanel>());
  ASSERT_TRUE(controller->IsActive());
  const gfx::Rect panel_bounds(widget->GetWindowBoundsInScreen());

  // Start selecting a new region obviously outside the panel.
  generator->MoveMouseTo(panel_bounds.x() - 50, panel_bounds.y() - 50);
  ASSERT_FALSE(controller->user_capture_region().Contains(
      generator->current_screen_location()));
  generator->PressLeftButton();
  generator->MoveMouseTo(panel_bounds.CenterPoint(), /*count=*/10);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_TRUE(session->is_drag_in_progress());
  ASSERT_TRUE(session->is_selecting_region());
  EXPECT_FALSE(controller->search_results_panel_widget()->IsVisible());

  // Test the panel does not receive events.
  EXPECT_EQ(ui::mojom::CursorType::kSouthEastResize,
            cursor_manager->GetCursor().type());
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
      ActionButtonRank(ActionButtonType::kOther, 0),
      ActionButtonViewID::kScannerButton);

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
      &kCaptureModeImageIcon, ActionButtonRank(ActionButtonType::kOther, 0),
      ActionButtonViewID::kScannerButton);

  // There should only be one valid button in the session.
  const std::vector<ActionButtonView*> action_buttons =
      session_test_api.GetActionButtons();
  ASSERT_EQ(action_buttons.size(), 1u);

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
      ActionButtonRank(ActionButtonType::kOther, 1),
      ActionButtonViewID::kScannerButton);
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 1u);

  // Add a sunfish action button, which should appear in the rightmost position.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Sunfish", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kSunfish, 0),
      ActionButtonViewID::kSearchButton);
  auto action_buttons = session_test_api.GetActionButtons();
  EXPECT_EQ(action_buttons.size(), 2u);
  EXPECT_EQ(action_buttons[1]->label_for_testing()->GetText(), u"Sunfish");

  // Add another default action button, which should appear in the leftmost
  // position.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Default 2", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, 0),
      ActionButtonViewID::kScannerButton);
  action_buttons = session_test_api.GetActionButtons();
  EXPECT_EQ(action_buttons.size(), 3u);
  EXPECT_EQ(action_buttons[0]->label_for_testing()->GetText(), u"Default 2");
  EXPECT_EQ(action_buttons[1]->label_for_testing()->GetText(), u"Default 1");
  EXPECT_EQ(action_buttons[2]->label_for_testing()->GetText(), u"Sunfish");

  // Add a scanner action button, which should appear to the immediate left of
  // the Sunfish action button.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Scanner", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kScanner, 0),
      ActionButtonViewID::kScannerButton);
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
  ASSERT_FALSE(session_test_api.GetActionContainerWidget());

  // Attempt to add a new action button using the API. The container widget
  // opacity should not change.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Do not show", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, 0),
      ActionButtonViewID::kScannerButton);
  ASSERT_FALSE(session_test_api.GetActionContainerWidget());

  // Select a new capture region that does not overlap with the search results
  // panel.
  auto* event_generator = GetEventGenerator();
  SelectCaptureModeRegion(event_generator, gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  auto* container_widget = session_test_api.GetActionContainerWidget();
  EXPECT_TRUE(container_widget->IsVisible());

  // Begin adjusting the capture region by dragging from the bottom right
  // corner. The container widget should be transluscent while the region is
  // being adjusted.
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(300, 300));
  ASSERT_TRUE(controller->capture_mode_session()->is_drag_in_progress());
  EXPECT_FALSE(container_widget->IsVisible());

  // Finish adjusting the region. The widget should now be visible.
  event_generator->ReleaseLeftButton();
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_FALSE(controller->capture_mode_session()->is_drag_in_progress());
  EXPECT_TRUE(container_widget->IsVisible());
}

TEST_F(SunfishTest, DismissButtonsOnSourceChange) {
  // Start default mode. Test the buttons are hidden.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  VerifyActiveBehavior(BehaviorType::kDefault);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi session_test_api(session);
  ASSERT_FALSE(session_test_api.GetActionContainerWidget());

  // Select a region large enough that the video capture button will not
  // intersect with where the search action button previously was and wait for
  // the buttons to show up.
  auto* generator = GetEventGenerator();
  SelectCaptureModeRegion(generator, gfx::Rect(10, 10, 200, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  auto* container_widget = session_test_api.GetActionContainerWidget();
  ASSERT_TRUE(container_widget->IsVisible());
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);
  auto* search_button =
      session_test_api.GetButtonWithViewID(ActionButtonViewID::kSearchButton);
  gfx::Rect search_button_bounds(search_button->GetBoundsInScreen());

  // Set the type to `kVideo`. Test the buttons are hidden.
  controller->SetType(CaptureModeType::kVideo);
  EXPECT_FALSE(container_widget->IsVisible());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);
  auto* label_view = session_test_api.GetCaptureLabelView();
  ASSERT_TRUE(label_view->IsRecordingTypeDropDownButtonVisible());
  const gfx::Rect capture_button_bounds(label_view->capture_button_container()
                                            ->capture_button()
                                            ->GetBoundsInScreen());
  ASSERT_FALSE(capture_button_bounds.Intersects(search_button_bounds));
  auto* cursor_manager = Shell::Get()->cursor_manager();

  // Hover over where the search button was previously shown.
  generator->MoveMouseTo(search_button_bounds.CenterPoint());
  EXPECT_EQ(ui::mojom::CursorType::kCell, cursor_manager->GetCursor().type());

  // Switch back to `kImage` then select a region.
  controller->SetType(CaptureModeType::kImage);
  CaptureModeTestApi().SetUserSelectedRegion(gfx::Rect());
  SelectCaptureModeRegion(generator, gfx::Rect(10, 10, 200, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  EXPECT_TRUE(container_widget->IsVisible());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 1u);
  search_button =
      session_test_api.GetButtonWithViewID(ActionButtonViewID::kSearchButton);
  ASSERT_TRUE(search_button);
  EXPECT_TRUE(search_button->GetVisible());
  search_button_bounds = search_button->GetBoundsInScreen();

  // Set the source to `kFullscreen`. Test the buttons are hidden.
  controller->SetSource(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(container_widget->IsVisible());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);
  EXPECT_EQ(ui::mojom::CursorType::kCustom, cursor_manager->GetCursor().type());

  // Hover over where the search button was previously shown.
  generator->MoveMouseTo(search_button_bounds.CenterPoint());
  EXPECT_EQ(ui::mojom::CursorType::kCustom, cursor_manager->GetCursor().type());

  // Switch back to `kRegion` then select a region.
  controller->SetSource(CaptureModeSource::kRegion);
  CaptureModeTestApi().SetUserSelectedRegion(gfx::Rect());
  SelectCaptureModeRegion(generator, gfx::Rect(10, 10, 50, 50),
                          /*release_mouse=*/true, /*verify_region=*/true);
  EXPECT_TRUE(container_widget->IsVisible());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 1u);
  EXPECT_TRUE(
      session_test_api.GetButtonWithViewID(ActionButtonViewID::kSearchButton));

  // Set the source to `kWindow`. Test the buttons are hidden.
  controller->SetSource(CaptureModeSource::kWindow);
  EXPECT_FALSE(container_widget->IsVisible());
  EXPECT_EQ(session_test_api.GetActionButtons().size(), 0u);

  // TODO(b/377569542): Re-show the action buttons if the mode changes back to
  // image region.
}

TEST_F(SunfishTest, HideFeedbackButtonOnSourceTypeChange) {
  // Start default image region capture mode.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  views::Widget* feedback_button_widget =
      session_test_api.GetFeedbackButtonWidget();
  ASSERT_TRUE(feedback_button_widget);
  EXPECT_EQ(1.f, feedback_button_widget->GetLayer()->GetTargetOpacity());

  // Simulate clicking the video toggle button to change the type.
  LeftClickOn(GetVideoToggleButton());
  ASSERT_EQ(CaptureModeType::kVideo, controller->type());

  // Test the feedback button is hidden and hovering over it does not update the
  // cursor.
  ASSERT_TRUE(feedback_button_widget);
  EXPECT_FALSE(feedback_button_widget->IsVisible());
  EXPECT_EQ(0.f, feedback_button_widget->GetLayer()->GetTargetOpacity());

  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(
      feedback_button_widget->GetWindowBoundsInScreen().CenterPoint());
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_EQ(ui::mojom::CursorType::kCell, cursor_manager->GetCursor().type());

  // Switch back to image region capture.
  LeftClickOn(GetImageToggleButton());
  ASSERT_EQ(CaptureModeType::kImage, controller->type());

  // Test the feedback button is re-shown and hovering over it updates the
  // cursor.
  ASSERT_TRUE(feedback_button_widget);
  EXPECT_TRUE(feedback_button_widget->IsVisible());
  EXPECT_EQ(1.f, feedback_button_widget->GetLayer()->GetTargetOpacity());

  generator->MoveMouseTo(
      feedback_button_widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_EQ(ui::mojom::CursorType::kHand, cursor_manager->GetCursor().type());
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
  EXPECT_TRUE(
      session_test_api.GetButtonWithViewID(ActionButtonViewID::kSearchButton));

  // Click on the "Search" button. Test we end capture mode.
  LeftClickOn(session_test_api.GetActionButtons()[0]);
  WaitForImageCapturedForSearch(PerformCaptureType::kSearch);
  EXPECT_TRUE(controller->search_results_panel_widget());
  EXPECT_FALSE(controller->IsActive());

  // Restart capture mode session. The panel will be reset.
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  EXPECT_FALSE(controller->search_results_panel_widget());
}

// Tests that the search action button is not shown in the default capture mode
// if the user has disabled Sunfish.
TEST_F(SunfishTest, SearchActionButtonNotShownIfEnabledPrefIsFalse) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSunfishEnabled, false);
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

// Tests that receiving a newly fetched URL updates the panel. Regression test
// for http://b/378019622.
TEST_F(SunfishTest, SendMultimodalSearch) {
  // Start default mode.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  VerifyActiveBehavior(BehaviorType::kDefault);

  // Open the search results panel to end the session.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500));
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);
  LeftClickOn(session_test_api.GetActionButtons()[0]);
  WaitForImageCapturedForSearch(PerformCaptureType::kSearch);
  ASSERT_FALSE(controller->IsActive());
  ASSERT_TRUE(controller->GetSearchResultsPanel());

  auto* search_results_panel =
      controller->search_results_panel_widget()->SetContentsView(
          std::make_unique<MockSearchResultsPanel>());

  // Mock getting a new response from the server. Test the panel is updated.
  EXPECT_CALL(*search_results_panel, SetSearchBoxImage(testing::_));
  EXPECT_CALL(*search_results_panel, Navigate(testing::_));

  controller->ShowSearchResultsPanel(gfx::ImageSkia(), GURL("kTestUrl2"));
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
  ASSERT_FALSE(controller->IsActive());
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
  base::HistogramTester histogram_tester;
  constexpr char kMultimodalSearchRequestHistogram[] =
      "Ash.CaptureModeController.MultimodalSearchRequest.ClamshellMode";

  histogram_tester.ExpectTotalCount(kMultimodalSearchRequestHistogram, 0);

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

  histogram_tester.ExpectTotalCount(kMultimodalSearchRequestHistogram, 0);

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

  histogram_tester.ExpectTotalCount(kMultimodalSearchRequestHistogram, 1);
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

  // Switch to default mode. Test the panel is closed.
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  ASSERT_FALSE(controller->GetSearchResultsPanel());
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

TEST_F(SunfishTest, ClosePanelOnLockScreen) {
  // Open the panel.
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  auto* generator = GetEventGenerator();
  SelectCaptureModeRegion(generator, gfx::Rect(50, 50, 400, 400),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_TRUE(controller->search_results_panel_widget());

  // Lock the screen. Test the panel is hidden.
  GetSessionControllerClient()->LockScreen();
  ASSERT_FALSE(controller->search_results_panel_widget());
}

// Tests no crash when tabbing.
TEST_F(SunfishTest, NoCrashOnTabKeyEvent) {
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();

  PressAndReleaseKey(ui::VKEY_TAB);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(), GetCloseButton());
}

// Tests that the cursor is re-shown after performing Capture and Search
// in quick succession.
TEST_F(SunfishTest, IsCursorVisible) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  VerifyActiveBehavior(BehaviorType::kDefault);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Immediately capture the image, before the Search button and panel
  // loads.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500));
  CaptureModeSessionTestApi session_test_api(session);
  controller->CaptureScreenshotsOfAllDisplays();
  controller->PerformImageSearch(PerformCaptureType::kSearch);
  WaitForCaptureFileToBeSaved();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

// Tests that the feedback button is shown in the Sunfish session.
TEST_F(SunfishTest, FeedbackButtonShown) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller);
  controller->StartSunfishSession();

  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_TRUE(session);
  CaptureModeSessionTestApi session_test_api(session);
  views::Widget* feedback_button_widget =
      session_test_api.GetFeedbackButtonWidget();
  ASSERT_TRUE(feedback_button_widget);
  EXPECT_TRUE(feedback_button_widget->IsVisible());
  ui::Layer* feedback_button_layer = feedback_button_widget->GetLayer();
  ASSERT_TRUE(feedback_button_layer);
  EXPECT_TRUE(feedback_button_layer->GetTargetVisibility());
  EXPECT_GT(feedback_button_layer->GetTargetOpacity(), 0.f);
}

// Tests that the feedback button is shown in default capture mode.
TEST_F(SunfishTest, FeedbackButtonShownInDefaultMode) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);

  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_TRUE(session);
  CaptureModeSessionTestApi session_test_api(session);
  views::Widget* feedback_button_widget =
      session_test_api.GetFeedbackButtonWidget();
  ASSERT_TRUE(feedback_button_widget);
  EXPECT_TRUE(feedback_button_widget->IsVisible());
  ui::Layer* feedback_button_layer = feedback_button_widget->GetLayer();
  ASSERT_TRUE(feedback_button_layer);
  EXPECT_TRUE(feedback_button_layer->GetTargetVisibility());
  EXPECT_GT(feedback_button_layer->GetTargetOpacity(), 0.f);
}

// Tests that the feedback button is hidden in default capture mode if the
// enabled pref is false.
TEST_F(SunfishTest, FeedbackButtonNotShownInDefaultModeIfEnabledPrefIsFalse) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSunfishEnabled, false);

  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);

  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_TRUE(session);
  CaptureModeSessionTestApi session_test_api(session);
  views::Widget* feedback_button_widget =
      session_test_api.GetFeedbackButtonWidget();
  // There are various ways a widget can be hidden. Any of them should pass this
  // test.
  EXPECT_THAT(
      feedback_button_widget,
      AnyOf(IsNull(), Property("IsVisible", &views::Widget::IsVisible, false),
            Property("GetLayer", &views::Widget::GetLayer,
                     AnyOf(Property("GetTargetOpacity",
                                    &ui::Layer::GetTargetOpacity, 0.f),
                           Property("GetTargetVisibility",
                                    &ui::Layer::GetTargetVisibility, false)))));
}

// Tests the search button shown and pressed metrics are being properly
// recorded.
TEST_F(SunfishTest, RecordSearchButtonShownAndPressed) {
  base::HistogramTester histogram_tester;
  constexpr char kSearchButtonPressedHistogram[] =
      "Ash.CaptureModeController.SearchButtonPressed.ClamshellMode";
  constexpr char kSearchButtonShownHistogram[] =
      "Ash.CaptureModeController.SearchButtonShown.ClamshellMode";

  histogram_tester.ExpectBucketCount(kSearchButtonShownHistogram, true, 0);
  histogram_tester.ExpectBucketCount(kSearchButtonPressedHistogram, true, 0);

  // Start default capture mode.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  VerifyActiveBehavior(BehaviorType::kDefault);

  // Select a region, which should show the search button.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi session_test_api(session);
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);
  auto* container_widget = session_test_api.GetActionContainerWidget();
  ASSERT_TRUE(container_widget);

  histogram_tester.ExpectBucketCount(kSearchButtonShownHistogram, true, 1);
  histogram_tester.ExpectBucketCount(kSearchButtonPressedHistogram, true, 0);

  // Click on the search button.
  LeftClickOn(
      session_test_api.GetButtonWithViewID(ActionButtonViewID::kSearchButton));
  WaitForImageCapturedForSearch(PerformCaptureType::kSearch);
  EXPECT_TRUE(controller->search_results_panel_widget());

  histogram_tester.ExpectBucketCount(kSearchButtonShownHistogram, true, 1);
  histogram_tester.ExpectBucketCount(kSearchButtonPressedHistogram, true, 1);
}

// Tests the `SearchResultsPanel` entry point metrics are being properly
// recorded.
TEST_F(SunfishTest, RecordSearchResultsPanelEntryType) {
  base::HistogramTester histogram_tester;
  constexpr char kSearchResultsPanelEntryPointHistogram[] =
      "Ash.CaptureModeController.SearchResultsPanelEntryPoint.ClamshellMode";

  histogram_tester.ExpectTotalCount(kSearchResultsPanelEntryPointHistogram, 0);

  // Start a Sunfish sunfish session, and select a region to create the search
  // results panel.
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_TRUE(controller->GetSearchResultsPanel());

  histogram_tester.ExpectBucketCount(
      kSearchResultsPanelEntryPointHistogram,
      SearchResultsPanelEntryType::kSunfishRegionSelection, 1);

  // Stop the Sunfish session so we can start a default capture session instead.
  // The search results panel should automatically close.
  controller->Stop();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  ASSERT_FALSE(controller->GetSearchResultsPanel());

  // The region will be remembered from the Sunfish session, but the action
  // buttons will not be present, so reselect the region to make the Search
  // button appear.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(50, 50, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CaptureModeSessionTestApi session_test_api(session);
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);

  // Click on the search button to perform an image search and open
  // the search results panel.
  LeftClickOn(
      session_test_api.GetButtonWithViewID(ActionButtonViewID::kSearchButton));
  WaitForImageCapturedForSearch(PerformCaptureType::kSearch);
  ASSERT_TRUE(controller->search_results_panel_widget());

  histogram_tester.ExpectBucketCount(
      kSearchResultsPanelEntryPointHistogram,
      SearchResultsPanelEntryType::kDefaultSearchButton, 1);
  histogram_tester.ExpectTotalCount(kSearchResultsPanelEntryPointHistogram, 2);
}

// Tests that the panel position is updated properly based on the location of
// the selected region.
TEST_F(SunfishTest, PanelBounds) {
  UpdateDisplay("2000x1000");

  // Start a Sunfish sunfish session, and select a region to create the search
  // results panel.
  auto* controller = CaptureModeController::Get();
  controller->StartSunfishSession();
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(1000, 400, 100, 400),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_TRUE(controller->GetSearchResultsPanel());

  // Get the in-screen bounds of the work area and the feedback button.
  const gfx::Rect work_area =
      controller->search_results_panel_widget()->GetWorkAreaBoundsInScreen();
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  const gfx::Rect feedback_bounds =
      session_test_api.GetFeedbackButton()->GetBoundsInScreen();

  // Define the known possible coordinates of the search results panel.
  const int left_x = work_area.x() + capture_mode::kPanelWorkAreaSpacing;
  const int right_x = work_area.right() -
                      capture_mode::kSearchResultsPanelWidth -
                      capture_mode::kPanelWorkAreaSpacing;
  const int default_y = work_area.bottom() -
                        capture_mode::kSearchResultsPanelHeight -
                        capture_mode::kPanelWorkAreaSpacing;
  // TODO(hewer): Remove this when the feedback button is removed.
  const int above_button_y = feedback_bounds.y() -
                             capture_mode::kSearchResultsPanelHeight -
                             capture_mode::kPanelButtonSpacing;

  // By default, the panel should appear on the left side.
  gfx::Rect target_bounds(left_x, default_y,
                          capture_mode::kSearchResultsPanelWidth,
                          capture_mode::kSearchResultsPanelHeight);
  EXPECT_EQ(controller->GetSearchResultsPanel()->GetBoundsInScreen(),
            target_bounds);

  // Readjust the region on the left so it would intersect with the panel.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(1000, 400));
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_EQ(ui::mojom::CursorType::kNorthWestResize,
            cursor_manager->GetCursor().type());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(200, 400);
  event_generator->ReleaseLeftButton();
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_TRUE(controller->GetSearchResultsPanel());

  // The panel should now appear on the right side instead, just above the
  // feedback button.
  target_bounds.set_x(right_x);
  target_bounds.set_y(above_button_y);
  EXPECT_EQ(controller->GetSearchResultsPanel()->GetBoundsInScreen(),
            target_bounds);

  // Readjust the region on the right so it would intersect with both panel
  // positions, with slightly more space on the left side.
  event_generator->MoveMouseTo(gfx::Point(1100, 400));
  EXPECT_EQ(ui::mojom::CursorType::kNorthEastResize,
            cursor_manager->GetCursor().type());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(1900, 400);
  event_generator->ReleaseLeftButton();
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_TRUE(controller->GetSearchResultsPanel());

  // The panel should appear back on the left side since there is more space.
  target_bounds.set_x(left_x);
  target_bounds.set_y(default_y);
  EXPECT_EQ(controller->GetSearchResultsPanel()->GetBoundsInScreen(),
            target_bounds);

  // Readjust the region once more, so there is no space on the left side and
  // a small amount of space on the right side.
  event_generator->MoveMouseTo(gfx::Point(200, 400));
  EXPECT_EQ(ui::mojom::CursorType::kNorthWestResize,
            cursor_manager->GetCursor().type());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(0, 400);
  event_generator->ReleaseLeftButton();
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);
  ASSERT_TRUE(controller->GetSearchResultsPanel());

  // The panel should appear on the right side since there is more space.
  target_bounds.set_x(right_x);
  target_bounds.set_y(above_button_y);
  EXPECT_EQ(controller->GetSearchResultsPanel()->GetBoundsInScreen(),
            target_bounds);
}

// Tests that the sunfish launcher nudge appears and closes properly in
// clamshell mode.
TEST_F(SunfishTest, ClamshellLauncherNudge) {
  // Advance clock so we aren't at zero time.
  task_environment()->AdvanceClock(base::Hours(25));

  // Open the app list by clicking on the home button.
  LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  auto* bubble_view = GetAppListBubbleView();
  ASSERT_TRUE(bubble_view);
  auto* sunfish_button = bubble_view->search_box_view()->sunfish_button();
  ASSERT_TRUE(sunfish_button);

  // The Sunfish launcher nudge should be visible.
  AnchoredNudgeManagerImpl* nudge_manager =
      Shell::Get()->anchored_nudge_manager();
  const AnchoredNudge* nudge =
      nudge_manager->GetNudgeIfShown(capture_mode::kSunfishLauncherNudgeId);
  ASSERT_TRUE(nudge);
  EXPECT_TRUE(nudge->GetVisible());

  // Start the sunfish session using the launcher button. The nudge should
  // close.
  LeftClickOn(sunfish_button);
  VerifyActiveBehavior(BehaviorType::kSunfish);
  EXPECT_FALSE(
      nudge_manager->GetNudgeIfShown(capture_mode::kSunfishLauncherNudgeId));
}

// Tests that the sunfish launcher nudge appears and closes properly in
// tablet mode.
TEST_F(SunfishTest, TabletLauncherNudge) {
  // Advance clock so we aren't at zero time.
  task_environment()->AdvanceClock(base::Hours(25));

  SwitchToTabletMode();

  // The app list should be open by default when we enter tablet mode.
  auto* app_list_view = GetAppListView();
  ASSERT_TRUE(app_list_view);
  auto* sunfish_button = app_list_view->search_box_view()->sunfish_button();
  ASSERT_TRUE(sunfish_button);

  // The Sunfish launcher nudge should be visible.
  AnchoredNudgeManagerImpl* nudge_manager =
      Shell::Get()->anchored_nudge_manager();
  const AnchoredNudge* nudge =
      nudge_manager->GetNudgeIfShown(capture_mode::kSunfishLauncherNudgeId);
  ASSERT_TRUE(nudge);
  EXPECT_TRUE(nudge->GetVisible());

  // Start the sunfish session using the launcher button. The nudge should
  // close.
  GestureTapOn(sunfish_button);
  VerifyActiveBehavior(BehaviorType::kSunfish);
  EXPECT_FALSE(
      nudge_manager->GetNudgeIfShown(capture_mode::kSunfishLauncherNudgeId));
}

// Tests that the sunfish nudge only appears three times at most, with at least
// 24 hours between showings.
TEST_F(SunfishTest, LauncherNudgeLimits) {
  // Advance clock so we aren't at zero time.
  task_environment()->AdvanceClock(base::Hours(25));

  AnchoredNudgeManagerImpl* nudge_manager =
      Shell::Get()->anchored_nudge_manager();
  AppListBubblePresenter* bubble_presenter =
      Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  for (int i = 0; i < 3; i++) {
    // Click on the shelf home button to open up the app list launcher.
    LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
    ASSERT_TRUE(GetAppListBubbleView());
    ASSERT_TRUE(bubble_presenter->IsShowing());

    // Get the list of visible nudges from the nudge manager and make sure our
    // education nudge is in the list and visible.
    const AnchoredNudge* nudge =
        nudge_manager->GetNudgeIfShown(capture_mode::kSunfishLauncherNudgeId);
    ASSERT_TRUE(nudge);
    EXPECT_TRUE(nudge->GetVisible());

    // Showing the nudge should also update the preferences.
    EXPECT_EQ(capture_mode_util::GetActiveUserPrefService()->GetInteger(
                  prefs::kSunfishLauncherNudgeShownCount),
              i + 1);

    // Click on the shelf home button again to close the app list launcher (the
    // view will still exist but not be visible).
    LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
    ASSERT_FALSE(bubble_presenter->IsShowing());

    // Closing the app list (and therefore the sunfish button) should hide the
    // nudge.
    EXPECT_FALSE(
        nudge_manager->GetNudgeIfShown(capture_mode::kSunfishLauncherNudgeId));

    // Advance the clock so we can show the nudge again.
    task_environment()->AdvanceClock(base::Hours(25));
  }

  // Click on the shelf home button to open up the app list launcher.
  LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  ASSERT_TRUE(GetAppListBubbleView());
  ASSERT_TRUE(bubble_presenter->IsShowing());

  // The nudge should not show anymore, as we have hit the show limit.
  EXPECT_FALSE(
      nudge_manager->GetNudgeIfShown(capture_mode::kSunfishLauncherNudgeId));
  EXPECT_EQ(capture_mode_util::GetActiveUserPrefService()->GetInteger(
                prefs::kSunfishLauncherNudgeShownCount),
            3);
}

// Test that the action buttons can be navigated using the keyboard, and that
// the indices are correct when new buttons are added.
TEST_F(SunfishTest, KeyboardNavigationActionButtons) {
  // Start default mode.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  VerifyActiveBehavior(BehaviorType::kDefault);

  // Select a capture region. Only the "Search with Lens" should be present by
  // default.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500));
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 1u);

  // Use tab to navigate to the action button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN, /*count=*/3);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kActionButtons,
            session_test_api.GetCurrentFocusGroup());
  ASSERT_EQ(0u, session_test_api.GetCurrentFocusIndex());
  EXPECT_EQ(session_test_api.GetActionButtons()[0],
            session_test_api.GetCurrentFocusedView()->GetView());

  // Add a fake "Copy Text" button to the front of the list.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Copy Text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, 0),
      ActionButtonViewID::kCopyTextButton);

  // The "Search with Lens" buttons should still be focused.
  EXPECT_TRUE(CaptureModeSessionFocusCycler::HighlightHelper::Get(
                  session_test_api.GetActionButtons()[1])
                  ->has_focus());

  // Cycle backwards once.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kActionButtons,
            session_test_api.GetCurrentFocusGroup());
  ASSERT_EQ(0u, session_test_api.GetCurrentFocusIndex());

  // We should now be focused on the "Copy Text" button, even though the focus
  // index is still 0.
  ActionButtonView* copy_text_button = session_test_api.GetActionButtons()[0];
  EXPECT_EQ(copy_text_button, session_test_api.GetButtonWithViewID(
                                  ActionButtonViewID::kCopyTextButton));
  EXPECT_TRUE(
      CaptureModeSessionFocusCycler::HighlightHelper::Get(copy_text_button)
          ->has_focus());

  // Cycle forwards once.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kActionButtons,
            session_test_api.GetCurrentFocusGroup());
  ASSERT_EQ(1u, session_test_api.GetCurrentFocusIndex());

  // Add another generic test button to the end of the list.
  capture_mode_util::AddActionButton(
      views::Button::PressedCallback(), u"Test", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kSunfish, 1),
      ActionButtonViewID::kSmartActionsButton);

  // Cycle forwards once.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kActionButtons,
            session_test_api.GetCurrentFocusGroup());
  ASSERT_EQ(2u, session_test_api.GetCurrentFocusIndex());

  // We should now be focused on the newest test button.
  ActionButtonView* smart_action_button =
      session_test_api.GetActionButtons()[2];
  EXPECT_EQ(smart_action_button, session_test_api.GetButtonWithViewID(
                                     ActionButtonViewID::kSmartActionsButton));
  EXPECT_TRUE(
      CaptureModeSessionFocusCycler::HighlightHelper::Get(smart_action_button)
          ->has_focus());
}

class ScannerTest : public AshTestBase {
 public:
  ScannerTest()
      : AshTestBase(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}
  ScannerTest(const ScannerTest&) = delete;
  ScannerTest& operator=(const ScannerTest&) = delete;
  ~ScannerTest() override = default;

  // Starts a default capture session, and mocks selecting a user region with
  // text to show the "Smart Actions" button.
  ActionButtonView* GetSmartActionsButton() {
    auto* controller = CaptureModeController::Get();
    StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
    base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());
    EXPECT_CALL(*test_delegate, DetectTextInImage)
        .WillOnce(WithArg<1>(InvokeFuture(detect_text_future)));

    // Reset the user-selected region to ensure the region passed to
    // `SelectCaptureModeRegion()` is verified without interference from any
    // previously selected region. This guarantees consistent and predictable
    // behavior during tests.
    CaptureModeTestApi().SetUserSelectedRegion(gfx::Rect());

    // Select a region to trigger text detection.
    SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                            /*release_mouse=*/true, /*verify_region=*/true);
    WaitForImageCapturedForSearch(PerformCaptureType::kTextDetection);
    detect_text_future.Take().Run("detected text");

    CaptureModeSessionTestApi session_test_api(
        controller->capture_mode_session());
    ActionButtonView* action_button = session_test_api.GetButtonWithViewID(
        ActionButtonViewID::kSmartActionsButton);
    EXPECT_TRUE(action_button);
    return action_button;
  }

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

TEST_F(ScannerTest,
       SunfishScreenInitialScreenCaptureSentToScannerServerMetricRecorded) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(fetch_actions_future)));
  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::
          kSunfishScreenInitialScreenCaptureSentToScannerServer,
      0);

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::
          kSunfishScreenInitialScreenCaptureSentToScannerServer,
      1);
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
// that was fine-tuned, but did not change.
TEST_F(ScannerTest, DoesNotCreateActionButtonsForFineTuneNoop) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .Times(2)
      .WillRepeatedly(WithArg<1>(InvokeFuture(fetch_actions_future)));
  constexpr auto kCaptureRegion = gfx::Rect(100, 100, 600, 500);

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  const CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  SelectCaptureModeRegion(GetEventGenerator(), kCaptureRegion,
                          /*release_mouse=*/true, /*verify_region=*/true);
  manta::ScannerProvider::ScannerProtoResponseCallback stale_fetch_actions =
      fetch_actions_future.Take();
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  const gfx::Point drag_affordance_location =
      capture_mode_util::GetLocationForFineTunePosition(
          kCaptureRegion, FineTunePosition::kBottomRightVertex);
  GetEventGenerator()->MoveMouseTo(drag_affordance_location);
  GetEventGenerator()->ClickLeftButton();
  ASSERT_TRUE(fetch_actions_future.Wait())
      << "Fetch actions was not called again after fine-tune";
  ASSERT_THAT(session_test_api.GetActionButtons(), IsEmpty());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event 1");
  objects.add_actions()->mutable_new_event()->set_title("Event 2");
  std::move(stale_fetch_actions).Run(std::move(output), manta::MantaStatus());

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
      ElementsAre(ActionButtonIdIs(ActionButtonViewID::kScannerButton)));
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
      ElementsAre(ActionButtonIdIs(ActionButtonViewID::kScannerButton)));
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
      ElementsAre(ActionButtonIdIs(ActionButtonViewID::kScannerButton)));
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
      prefs::kSunfishEnabled, false);
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
  const ActionButtonView* copy_text_button =
      session_test_api.GetButtonWithViewID(ActionButtonViewID::kCopyTextButton);
  ASSERT_TRUE(copy_text_button);
  // Clipboard should currently be empty.
  std::u16string clipboard_data;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard_data);
  EXPECT_EQ(clipboard_data, u"");
  // Clicking on the button should copy text to clipboard and show a toast.
  LeftClickOn(copy_text_button);
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &clipboard_data);
  EXPECT_EQ(clipboard_data, u"detected text");
  EXPECT_TRUE(ToastManager::Get()->IsToastShown(kCaptureModeTextCopiedToastId));
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
  EXPECT_FALSE(session_test_api.GetButtonWithViewID(
      ActionButtonViewID::kCopyTextButton));
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
  EXPECT_FALSE(session_test_api.GetButtonWithViewID(
      ActionButtonViewID::kCopyTextButton));
}

// Tests that the copy text button is not shown if the selected region is
// fine-tuned, but does not change, before text detection completes.
TEST_F(ScannerTest, NoCopyTextButtonIfSelectedRegionChangesByFineTuneNoop) {
  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_CALL(*test_delegate, DetectTextInImage)
      .Times(2)
      .WillRepeatedly(WithArg<1>(InvokeFuture(detect_text_future)));

  constexpr auto kCaptureRegion = gfx::Rect(0, 0, 50, 50);
  SelectCaptureModeRegion(GetEventGenerator(), kCaptureRegion,
                          /*release_mouse=*/true, /*verify_region=*/true);
  OnTextDetectionComplete callback = detect_text_future.Take();
  const gfx::Point drag_affordance_location =
      capture_mode_util::GetLocationForFineTunePosition(
          kCaptureRegion, FineTunePosition::kBottomRightVertex);
  GetEventGenerator()->MoveMouseTo(drag_affordance_location);
  GetEventGenerator()->ClickLeftButton();
  ASSERT_TRUE(detect_text_future.Wait())
      << "Detect text was not called again after fine-tune";
  std::move(callback).Run("detected text");

  const CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_FALSE(session_test_api.GetButtonWithViewID(
      ActionButtonViewID::kCopyTextButton));
}

// Records the time taken for the on device OCR.
TEST_F(ScannerTest, OnSelectCaptureRegionRecordTextDetectionTimer) {
  base::HistogramTester histogram_tester;

  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_CALL(*test_delegate, DetectTextInImage)
      .WillOnce(WithArg<1>(InvokeFuture(detect_text_future)));

  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);

  ASSERT_TRUE(detect_text_future.Wait());
  task_environment()->FastForwardBy(base::Milliseconds(500));
  detect_text_future.Take().Run("detected text");

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.Timer.OnDeviceTextDetection", 500, 1);
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
  ASSERT_THAT(
      action_buttons,
      ElementsAre(
          AllOf(ActionButtonIdIs(ActionButtonViewID::kSmartActionsButton),
                ActionButtonIsCollapsed()),
          AllOf(ActionButtonIdIs(ActionButtonViewID::kCopyTextButton),
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
  EXPECT_THAT(
      session_test_api.GetActionButtons(),
      ElementsAre(AllOf(ActionButtonIdIs(ActionButtonViewID::kCopyTextButton),
                        ActionButtonIsCollapsed())));

  // Simulate a single fetched Scanner action.
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event");
  fetch_actions_future.Take().Run(std::move(output), manta::MantaStatus());

  // Check for a Scanner action button and a collapsed copy text button.
  EXPECT_THAT(
      session_test_api.GetActionButtons(),
      ElementsAre(AllOf(ActionButtonIdIs(ActionButtonViewID::kScannerButton),
                        Not(ActionButtonIsCollapsed())),
                  AllOf(ActionButtonIdIs(ActionButtonViewID::kCopyTextButton),
                        ActionButtonIsCollapsed())));
}

TEST_F(ScannerTest, SmartActionsButtonShownForDetectedTextRecordsHistogram) {
  base::HistogramTester histogram_tester;
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
  EXPECT_TRUE(session_test_api.GetButtonWithViewID(
      ActionButtonViewID::kSmartActionsButton));
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kScreenCaptureModeScannerButtonShown, 1);
}

TEST_F(
    ScannerTest,
    SmartActionsButtonShouldRecordMetricWhenImageSentToServerToFetchActions) {
  base::HistogramTester histogram_tester;
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
  const ActionButtonView* smart_actions_button =
      session_test_api.GetButtonWithViewID(
          ActionButtonViewID::kSmartActionsButton);
  ASSERT_TRUE(smart_actions_button);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::
          kScreenCaptureModeInitialScreenCaptureSentToScannerServer,
      0);

  // Click the smart actions button.
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(fetch_actions_future)));
  LeftClickOn(smart_actions_button);
  WaitForImageCapturedForSearch(PerformCaptureType::kScanner);

  // Simulate a single fetched Scanner action.
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event");
  fetch_actions_future.Take().Run(std::move(output), manta::MantaStatus());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::
          kScreenCaptureModeInitialScreenCaptureSentToScannerServer,
      1);
}

// Tests that the capture label is hidden while capturing an image to send to
// the Scanner backend.
TEST_F(ScannerTest, CaptureLabelHiddenWhilePerformingCaptureForScanner) {
  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_CALL(*test_delegate, DetectTextInImage)
      .WillOnce(WithArg<1>(InvokeFuture(detect_text_future)));

  // Select a region to trigger text detection. The capture label should be
  // hidden so that it does not interfere with text detection.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_FALSE(session_test_api.GetCaptureLabelWidget()->IsVisible());

  // The capture label should be reshown once capture completes.
  WaitForImageCapturedForSearch(PerformCaptureType::kTextDetection);
  EXPECT_TRUE(session_test_api.GetCaptureLabelWidget()->IsVisible());

  detect_text_future.Take().Run("detected text");
  // Smart actions button should have been created.
  const ActionButtonView* smart_actions_button =
      session_test_api.GetButtonWithViewID(
          ActionButtonViewID::kSmartActionsButton);
  ASSERT_TRUE(smart_actions_button);

  // Click the smart actions button. The capture label should be hidden so that
  // it does not interfere with detecting Scanner actions.
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  LeftClickOn(smart_actions_button);
  EXPECT_FALSE(session_test_api.GetCaptureLabelWidget()->IsVisible());

  // The capture label should be reshown once capture completes.
  WaitForImageCapturedForSearch(PerformCaptureType::kScanner);
  EXPECT_TRUE(session_test_api.GetCaptureLabelWidget()->IsVisible());
}

// Tests that a glow animation is shown when Scanner actions are being fetched
// during a Sunfish session.
TEST_F(ScannerTest, GlowAnimationWhenFetchingActionsDuringSunfishSession) {
  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->StartSunfishSession();
  CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  CaptureRegionOverlayController* region_overlay_controller =
      session_test_api.GetCaptureRegionOverlayController();
  ASSERT_TRUE(region_overlay_controller);
  EXPECT_FALSE(region_overlay_controller->HasGlowAnimation());

  // Select a region to start fetching Scanner actions.
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(fetch_actions_future)));
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 100, 600, 500),
                          /*release_mouse=*/true, /*verify_region=*/true);
  WaitForImageCapturedForSearch(PerformCaptureType::kSunfish);

  // Glow should be animating.
  EXPECT_TRUE(region_overlay_controller->HasGlowAnimation());
  const gfx::ThrobAnimation* glow_animation =
      region_overlay_controller->glow_animation_for_testing();
  ASSERT_TRUE(glow_animation);
  EXPECT_TRUE(glow_animation->is_animating());

  // Finish fetching Scanner actions.
  fetch_actions_future.Take().Run(
      std::make_unique<manta::proto::ScannerOutput>(), manta::MantaStatus());

  // Glow should be pausing.
  EXPECT_TRUE(region_overlay_controller->HasGlowAnimation());
  EXPECT_EQ(glow_animation->cycles_remaining(), 0);
}

// Tests that a glow animation is shown when Scanner actions are being fetched
// after the smart actions button is pressed.
TEST_F(ScannerTest, GlowAnimationAfterPressingSmartActionsButton) {
  ActionButtonView* smart_actions_button = GetSmartActionsButton();
  ASSERT_TRUE(smart_actions_button);
  CaptureModeSessionTestApi session_test_api(
      CaptureModeController::Get()->capture_mode_session());
  CaptureRegionOverlayController* region_overlay_controller =
      session_test_api.GetCaptureRegionOverlayController();
  ASSERT_TRUE(region_overlay_controller);
  EXPECT_FALSE(region_overlay_controller->HasGlowAnimation());

  // Click on the smart actions button to start fetching Scanner actions.
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(fetch_actions_future)));
  LeftClickOn(smart_actions_button);
  WaitForImageCapturedForSearch(PerformCaptureType::kScanner);

  // Glow should be animating.
  EXPECT_TRUE(region_overlay_controller->HasGlowAnimation());
  const gfx::ThrobAnimation* glow_animation =
      region_overlay_controller->glow_animation_for_testing();
  ASSERT_TRUE(glow_animation);
  EXPECT_TRUE(glow_animation->is_animating());

  // Finish fetching Scanner actions.
  fetch_actions_future.Take().Run(
      std::make_unique<manta::proto::ScannerOutput>(), manta::MantaStatus());

  // Glow should be pausing.
  EXPECT_TRUE(region_overlay_controller->HasGlowAnimation());
  EXPECT_EQ(glow_animation->cycles_remaining(), 0);

  // Reselect a capture region.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(100, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);

  // Glow should be removed.
  EXPECT_FALSE(region_overlay_controller->HasGlowAnimation());
}

// Tests that a glow animation is removed if the capture source changes.
TEST_F(ScannerTest, GlowAnimationRemovedOnCaptureSourceChange) {
  ActionButtonView* smart_actions_button = GetSmartActionsButton();
  ASSERT_TRUE(smart_actions_button);
  auto* capture_mode_controller = CaptureModeController::Get();
  CaptureModeSessionTestApi session_test_api(
      capture_mode_controller->capture_mode_session());
  CaptureRegionOverlayController* region_overlay_controller =
      session_test_api.GetCaptureRegionOverlayController();
  ASSERT_TRUE(region_overlay_controller);
  EXPECT_FALSE(region_overlay_controller->HasGlowAnimation());

  // Click on the smart actions button. Glow should be shown.
  LeftClickOn(smart_actions_button);
  WaitForImageCapturedForSearch(PerformCaptureType::kScanner);

  EXPECT_TRUE(region_overlay_controller->HasGlowAnimation());

  // Switch to video. Glow should be removed.
  capture_mode_controller->SetType(CaptureModeType::kVideo);

  EXPECT_FALSE(region_overlay_controller->HasGlowAnimation());
}

TEST_F(ScannerTest, DisclaimerAcceptContinuesScannerSession) {
  base::HistogramTester histogram_tester;
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishConsentDisclaimerAccepted, false);

  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  base::test::TestFuture<OnTextDetectionComplete> detect_text_future;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_CALL(*test_delegate, DetectTextInImage)
      .WillOnce(WithArg<1>(InvokeFuture(detect_text_future)));

  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_actions_future;
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(WithArg<1>(InvokeFuture(fetch_actions_future)));

  // Select a region to trigger text detection. The capture label should be
  // hidden so that it does not interfere with text detection.
  SelectCaptureModeRegion(GetEventGenerator(), gfx::Rect(0, 0, 50, 200),
                          /*release_mouse=*/true, /*verify_region=*/true);
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_FALSE(session_test_api.GetCaptureLabelWidget()->IsVisible());

  // The capture label should be reshown once capture completes.
  WaitForImageCapturedForSearch(PerformCaptureType::kTextDetection);
  EXPECT_TRUE(session_test_api.GetCaptureLabelWidget()->IsVisible());

  detect_text_future.Take().Run("detected text");
  // Smart actions button should have been created.
  const ActionButtonView* smart_actions_button =
      session_test_api.GetButtonWithViewID(
          ActionButtonViewID::kSmartActionsButton);
  ASSERT_TRUE(smart_actions_button);

  // Click the smart actions button.
  LeftClickOn(smart_actions_button);
  views::Widget* disclaimer = session_test_api.GetDisclaimerWidget();
  ASSERT_TRUE(disclaimer);

  views::View* accept_button =
      disclaimer->GetContentsView()->GetViewByID(kDisclaimerViewAcceptButtonId);
  LeftClickOn(accept_button);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kConsentDisclaimerAccepted, 1);
  EXPECT_EQ(session_test_api.GetDisclaimerWidget(), nullptr);
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          kSunfishConsentDisclaimerAccepted));
  EXPECT_TRUE(controller->IsActive());
  WaitForImageCapturedForSearch(PerformCaptureType::kScanner);

  // Smart actions button should have been removed, and the copy text button
  // should be collapsed.
  EXPECT_THAT(
      session_test_api.GetActionButtons(),
      ElementsAre(AllOf(ActionButtonIdIs(ActionButtonViewID::kCopyTextButton),
                        ActionButtonIsCollapsed())));

  // Simulate a single fetched Scanner action.
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& objects = *output->add_objects();
  objects.add_actions()->mutable_new_event()->set_title("Event");
  fetch_actions_future.Take().Run(std::move(output), manta::MantaStatus());

  // Check for a Scanner action button and a collapsed copy text button.
  EXPECT_THAT(
      session_test_api.GetActionButtons(),
      ElementsAre(AllOf(ActionButtonIdIs(ActionButtonViewID::kScannerButton),
                        Not(ActionButtonIsCollapsed())),
                  AllOf(ActionButtonIdIs(ActionButtonViewID::kCopyTextButton),
                        ActionButtonIsCollapsed())));
}

TEST_F(ScannerTest, DisclaimerDeclinedGoesBackToScreenshotMode) {
  base::HistogramTester histogram_tester;
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishConsentDisclaimerAccepted, false);

  ActionButtonView* smart_actions_button = GetSmartActionsButton();
  ASSERT_TRUE(smart_actions_button);

  // Click the smart actions button.
  LeftClickOn(smart_actions_button);
  auto* controller = CaptureModeController::Get();
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  views::Widget* disclaimer = session_test_api.GetDisclaimerWidget();
  ASSERT_TRUE(disclaimer);

  views::View* decline_button = disclaimer->GetContentsView()->GetViewByID(
      kDisclaimerViewDeclineButtonId);
  LeftClickOn(decline_button);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kConsentDisclaimerRejected, 1);
  EXPECT_EQ(session_test_api.GetDisclaimerWidget(), nullptr);
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          kSunfishConsentDisclaimerAccepted));
  EXPECT_TRUE(controller->IsActive());
  // Did not get filled with new actions buttons, stays the same as before.
  EXPECT_THAT(session_test_api.GetActionButtons(),
              ElementsAre(smart_actions_button, _));

  // Click the smart actions button again, should show the disclaimer again.
  LeftClickOn(smart_actions_button);
  EXPECT_TRUE(session_test_api.GetDisclaimerWidget());

  // Exit the session. The disclaimer will be dismissed.
  controller->Stop();
  ASSERT_FALSE(controller->IsActive());

  // Re-enter the session. The disclaimer will be re-shown.
  smart_actions_button = GetSmartActionsButton();
  ASSERT_TRUE(smart_actions_button);
  LeftClickOn(smart_actions_button);
  EXPECT_TRUE(CaptureModeSessionTestApi(controller->capture_mode_session())
                  .GetDisclaimerWidget());
}

// Tests that the consent disclaimer can be properly navigated using the
// keyboard.
TEST_F(ScannerTest, KeyboardNavigationDisclaimer) {
  base::HistogramTester histogram_tester;
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      kSunfishConsentDisclaimerAccepted, false);

  ActionButtonView* smart_actions_button = GetSmartActionsButton();
  ASSERT_TRUE(smart_actions_button);
  auto* controller = CaptureModeController::Get();
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  ASSERT_EQ(session_test_api.GetActionButtons().size(), 2u);

  // Use tab to navigate to the smart actions button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN, /*count=*/4);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kActionButtons,
            session_test_api.GetCurrentFocusGroup());
  ASSERT_EQ(0u, session_test_api.GetCurrentFocusIndex());
  ASSERT_EQ(session_test_api.GetActionButtons()[0], smart_actions_button);

  // Press enter to open the consent disclaimer.
  SendKey(ui::VKEY_RETURN, event_generator);
  auto* disclaimer = session_test_api.GetDisclaimerWidget();
  ASSERT_TRUE(disclaimer);

  // Press tab once. The focus should shift to the decline button in the consent
  // disclaimer.
  SendKey(ui::VKEY_TAB, event_generator);
  views::View* decline_button = disclaimer->GetContentsView()->GetViewByID(
      kDisclaimerViewDeclineButtonId);
  EXPECT_TRUE(decline_button->HasFocus());

  // Press tab again. The accept button should now be focused.
  SendKey(ui::VKEY_TAB, event_generator);
  views::View* accept_button =
      disclaimer->GetContentsView()->GetViewByID(kDisclaimerViewAcceptButtonId);
  EXPECT_TRUE(accept_button->HasFocus());

  // Press tab one more time. The focus should stay inside the disclaimer, and
  // loop back around to the decline button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_TRUE(decline_button->HasFocus());
}

}  // namespace ash
