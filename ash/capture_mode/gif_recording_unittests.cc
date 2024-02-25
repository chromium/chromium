// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_button_view.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/recording_type_menu_view.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/test/gtest_tags.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace ash {

class GifRecordingTest : public AshTestBase {
 public:
  GifRecordingTest() : scoped_feature_list_(features::kGifRecording) {}
  GifRecordingTest(const GifRecordingTest&) = delete;
  GifRecordingTest& operator=(const GifRecordingTest&) = delete;
  ~GifRecordingTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    CaptureModeController::Get()->SetUserCaptureRegion(gfx::Rect(200, 200),
                                                       /*by_user=*/true);
  }

  CaptureModeController* StartRegionVideoCapture() {
    return StartCaptureSession(CaptureModeSource::kRegion,
                               CaptureModeType::kVideo);
  }

  CaptureLabelView* GetCaptureLabelView() {
    return CaptureModeSessionTestApi().GetCaptureLabelView();
  }

  RecordingTypeMenuView* GetRecordingTypeMenuView() {
    return CaptureModeSessionTestApi().GetRecordingTypeMenuView();
  }

  views::LabelButton* GetCaptureButton() {
    auto* label_view = GetCaptureLabelView();
    return label_view->capture_button_container()->capture_button();
  }

  views::ImageButton* GetRecordingTypeDropDownButton() {
    auto* label_view = GetCaptureLabelView();
    EXPECT_TRUE(label_view->IsRecordingTypeDropDownButtonVisible());
    return label_view->capture_button_container()->drop_down_button();
  }

  views::Widget* GetRecordingTypeMenuWidget() {
    return CaptureModeSessionTestApi().GetRecordingTypeMenuWidget();
  }

  views::Widget* GetSettingsMenuWidget() {
    return CaptureModeSessionTestApi().GetCaptureModeSettingsWidget();
  }

  void ClickOnDropDownButton() {
    LeftClickOn(GetRecordingTypeDropDownButton());
  }

  void ClickOnSettingsButton() {
    CaptureModeBarView* bar_view =
        CaptureModeSessionTestApi().GetCaptureModeBarView();
    LeftClickOn(bar_view->settings_button());
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GifRecordingTest, DropDownButtonVisibility) {
  // With region video recording, the drop down button should be visible.
  auto* controller = StartRegionVideoCapture();
  auto* label_view = GetCaptureLabelView();
  EXPECT_TRUE(label_view->IsRecordingTypeDropDownButtonVisible());

  // It should hide, once we switch to image recording, but the label view
  // should remain interactable.
  controller->SetType(CaptureModeType::kImage);
  EXPECT_FALSE(label_view->IsRecordingTypeDropDownButtonVisible());
  EXPECT_TRUE(label_view->IsViewInteractable());

  // Switching to a fullscreen source, the label view becomes no longer
  // interactable, and the drop down button remains hidden.
  controller->SetSource(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(label_view->IsRecordingTypeDropDownButtonVisible());
  EXPECT_FALSE(label_view->IsViewInteractable());

  // Even when we switch back to video recording.
  controller->SetType(CaptureModeType::kVideo);
  EXPECT_FALSE(label_view->IsRecordingTypeDropDownButtonVisible());
  EXPECT_FALSE(label_view->IsViewInteractable());

  // Only region recording in video mode, that the label view is interactable,
  // and the button is visible.
  controller->SetSource(CaptureModeSource::kRegion);
  EXPECT_TRUE(label_view->IsRecordingTypeDropDownButtonVisible());
  EXPECT_TRUE(label_view->IsViewInteractable());
}

TEST_F(GifRecordingTest, RecordingTypeMenuCreation) {
  // The drop down button acts as a toggle.
  StartRegionVideoCapture();
  ClickOnDropDownButton();
  EXPECT_TRUE(GetRecordingTypeMenuWidget());
  ClickOnDropDownButton();
  EXPECT_FALSE(GetRecordingTypeMenuWidget());

  // The settings menu and the recording type menu are mutually exclusive,
  // opening one closes the other.
  ClickOnSettingsButton();
  EXPECT_TRUE(GetSettingsMenuWidget());
  ClickOnDropDownButton();
  EXPECT_TRUE(GetRecordingTypeMenuWidget());
  EXPECT_FALSE(GetSettingsMenuWidget());
  ClickOnSettingsButton();
  EXPECT_TRUE(GetSettingsMenuWidget());
  EXPECT_FALSE(GetRecordingTypeMenuWidget());
}

TEST_F(GifRecordingTest, EscKeyClosesMenu) {
  // Hitting the ESC key closes the recording type menu, but the session remains
  // active.
  auto* controller = StartRegionVideoCapture();
  ClickOnDropDownButton();
  EXPECT_TRUE(GetRecordingTypeMenuWidget());
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(GetRecordingTypeMenuWidget());
  EXPECT_TRUE(controller->IsActive());
}

TEST_F(GifRecordingTest, EnterKeyHidesMenuAndStartsCountDown) {
  StartRegionVideoCapture();
  ClickOnDropDownButton();
  auto* recording_type_menu = GetRecordingTypeMenuWidget();
  EXPECT_TRUE(recording_type_menu);

  // Pressing the ENTER key starts the recording count down, at which point, the
  // menu remains open but fades out to an opacity of 0.
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_TRUE(CaptureModeTestApi().IsInCountDownAnimation());
  ASSERT_EQ(recording_type_menu, GetRecordingTypeMenuWidget());
  EXPECT_FLOAT_EQ(recording_type_menu->GetLayer()->GetTargetOpacity(), 0);
}

TEST_F(GifRecordingTest, ClickingOutsideClosesMenu) {
  auto* controller = StartRegionVideoCapture();
  ClickOnDropDownButton();
  EXPECT_TRUE(GetRecordingTypeMenuWidget());

  // Clicking outside the menu widget should close it, but the region should not
  // change.
  const auto region = controller->user_capture_region();
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(region.bottom_right() + gfx::Vector2d(10, 10));
  generator->ClickLeftButton();
  EXPECT_FALSE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(region, controller->user_capture_region());
}

TEST_F(GifRecordingTest, ChangingTypeFromMenu) {
  auto* controller = StartRegionVideoCapture();
  EXPECT_EQ(RecordingType::kWebM, controller->recording_type());
  ClickOnDropDownButton();
  EXPECT_TRUE(GetRecordingTypeMenuWidget());

  // The WebM option should be selected and marked with a check. Once the GIF
  // option is clicked, the menu should close, and the recording type in the
  // controller is updated.
  auto* recording_type_menu_view = GetRecordingTypeMenuView();
  EXPECT_TRUE(
      recording_type_menu_view->IsOptionChecked(ToInt(RecordingType::kWebM)));
  LeftClickOn(recording_type_menu_view->GetGifOptionForTesting());
  EXPECT_FALSE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(RecordingType::kGif, controller->recording_type());
}

TEST_F(GifRecordingTest, MenuIsClosedWhenClickingCheckedOption) {
  auto* controller = StartRegionVideoCapture();
  EXPECT_EQ(RecordingType::kWebM, controller->recording_type());
  ClickOnDropDownButton();
  EXPECT_TRUE(GetRecordingTypeMenuWidget());

  // Clicking on the same checked option closes the menu even though there is no
  // change.
  auto* recording_type_menu_view = GetRecordingTypeMenuView();
  LeftClickOn(recording_type_menu_view->GetWebMOptionForTesting());
  EXPECT_FALSE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(RecordingType::kWebM, controller->recording_type());
}

TEST_F(GifRecordingTest, CaptureButtonStateUpdatedFromMenuSelection) {
  // Select GIF from the menu, the capture button label should be updated.
  StartRegionVideoCapture();
  ClickOnDropDownButton();
  LeftClickOn(GetRecordingTypeMenuView()->GetGifOptionForTesting());
  auto* capture_button = GetCaptureButton();
  EXPECT_EQ(capture_button->GetText(), u"Record GIF");

  // Select WebM from the menu, and expect the button label to be updated too.
  ClickOnDropDownButton();
  EXPECT_TRUE(GetRecordingTypeMenuWidget());
  LeftClickOn(GetRecordingTypeMenuView()->GetWebMOptionForTesting());
  EXPECT_EQ(capture_button->GetText(), u"Record video");
}

// When the recording type is set programmatically, the capture button should
// still get updated.
TEST_F(GifRecordingTest, CaptureButtonStateUpdatedFromController) {
  auto* controller = StartRegionVideoCapture();
  controller->SetRecordingType(RecordingType::kGif);
  auto* capture_button = GetCaptureButton();
  EXPECT_EQ(capture_button->GetText(), u"Record GIF");

  controller->SetRecordingType(RecordingType::kWebM);
  EXPECT_EQ(capture_button->GetText(), u"Record video");
}

// Recording type selection affects future capture sessions.
TEST_F(GifRecordingTest, FutureCaptureSessionsAffected) {
  auto* controller = StartRegionVideoCapture();
  ClickOnDropDownButton();
  LeftClickOn(GetRecordingTypeMenuView()->GetGifOptionForTesting());

  // Press the ESC key to exit the current session.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(controller->IsActive());

  // Start a new session, and expect that the capture button should be labeled
  // correctly.
  StartRegionVideoCapture();
  EXPECT_EQ(GetCaptureButton()->GetText(), u"Record GIF");

  // When the menu is open, the correct option is marked as checked.
  ClickOnDropDownButton();
  EXPECT_TRUE(
      GetRecordingTypeMenuView()->IsOptionChecked(ToInt(RecordingType::kGif)));
}

TEST_F(GifRecordingTest, TabNavigation) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-759f3130-3839-408a-8342-a373654e8927");

  auto* controller = StartRegionVideoCapture();

  // Tab 15 times until we reach the capture button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/15);
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(GetCaptureButton(), test_api.GetCurrentFocusedView()->GetView());

  // Tab one more time to get to the drop down button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(GetRecordingTypeDropDownButton(),
            test_api.GetCurrentFocusedView()->GetView());

  // Pressing the spacebar should open the menu, and we should be in the
  // `kPendingRecordingType` focus group.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(FocusGroup::kPendingRecordingType, test_api.GetCurrentFocusGroup());

  // The next tab will move the focus inside the menu.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kRecordingTypeMenu, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  // And the WebM option will be focused.
  auto* recording_type_menu_view = GetRecordingTypeMenuView();
  EXPECT_EQ(recording_type_menu_view->GetWebMOptionForTesting(),
            test_api.GetCurrentFocusedView()->GetView());

  // Tabbing again will focus on the GIF option.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(recording_type_menu_view->GetGifOptionForTesting(),
            test_api.GetCurrentFocusedView()->GetView());

  // The next tab will focus on the settings button, but the recording type menu
  // stays open.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(test_api.GetCaptureModeBarView()->settings_button(),
            test_api.GetCurrentFocusedView()->GetView());

  // Reverse tabbing should get us back to the GIF option.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kRecordingTypeMenu, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(recording_type_menu_view->GetGifOptionForTesting(),
            test_api.GetCurrentFocusedView()->GetView());

  // Pressing the spacebar should select GIF, and close the menu.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(RecordingType::kGif, controller->recording_type());

  // The focus is moved back to the drop down button.
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(GetRecordingTypeDropDownButton(),
            test_api.GetCurrentFocusedView()->GetView());
}

TEST_F(GifRecordingTest, PressingEnterOnAFocusedItemBehavesLikeSpace) {
  auto* controller = StartRegionVideoCapture();

  // Tab 16 times until we reach the drop down button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/16);
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(GetRecordingTypeDropDownButton(),
            test_api.GetCurrentFocusedView()->GetView());

  // Pressing the enter should open the menu, and we should be in the
  // `kPendingRecordingType` focus group.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_TRUE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(FocusGroup::kPendingRecordingType, test_api.GetCurrentFocusGroup());

  // Then tab twice to reach the GIF recording option.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/2);
  EXPECT_EQ(FocusGroup::kRecordingTypeMenu, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  auto* recording_type_menu_view = GetRecordingTypeMenuView();
  EXPECT_EQ(recording_type_menu_view->GetGifOptionForTesting(),
            test_api.GetCurrentFocusedView()->GetView());

  // Pressing the enter key should select GIF, and close the menu.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(RecordingType::kGif, controller->recording_type());

  // The focus is moved back to the drop down button.
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(GetRecordingTypeDropDownButton(),
            test_api.GetCurrentFocusedView()->GetView());
}
TEST_F(GifRecordingTest, CloseRecordingMenuWhileFocusIsSomewhereElse) {
  auto* controller = StartRegionVideoCapture();

  // Tab 16 times until we reach the drop down button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/16);
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(GetRecordingTypeDropDownButton(),
            test_api.GetCurrentFocusedView()->GetView());

  // Pressing the spacebar should open the menu, and we should be in the
  // `kPendingRecordingType` focus group.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(FocusGroup::kPendingRecordingType, test_api.GetCurrentFocusGroup());

  // Now tab 4 times to put the focus on the close button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/4);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(test_api.GetCaptureModeBarView()->close_button(),
            test_api.GetCurrentFocusedView()->GetView());

  // Press the escape key, the menu should close, but the focus should not
  // change, since focus was not in or about to be in the menu.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_FALSE(GetRecordingTypeMenuWidget());
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(test_api.GetCaptureModeBarView()->close_button(),
            test_api.GetCurrentFocusedView()->GetView());
}

TEST_F(GifRecordingTest, GifIsNotSupportedForFullscreenOrWindow) {
  struct {
    const char* const scope_name;
    CaptureModeSource source;
  } kTestCases[] = {
      {"Testing fullscreen", CaptureModeSource::kFullscreen},
      {"Testing window", CaptureModeSource::kWindow},
  };

  auto window = CreateTestWindow(gfx::Rect(200, 200));

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_name);
    auto* controller = StartRegionVideoCapture();
    controller->SetRecordingType(RecordingType::kGif);

    // Audio recording is not supported for GIF, but switching to fullscreen or
    // window recording should switch to webm recording, which do support audio
    // recording, so we should expect that.
    controller->SetAudioRecordingMode(AudioRecordingMode::kMicrophone);

    // Switch to another source than region.
    controller->SetSource(test_case.source);
    // The recording type remains the same, and is still set as GIF. However,
    // the recording will be forced to webm, since GIF is only supported for
    // `kRegion`.
    EXPECT_EQ(controller->recording_type(), RecordingType::kGif);

    // This is needed for window recording.
    GetEventGenerator()->MoveMouseToCenterOf(window.get());

    StartVideoRecordingImmediately();

    EXPECT_TRUE(controller->is_recording_in_progress());
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());
    CaptureModeTestApi().FlushRecordingServiceForTesting();
    EXPECT_TRUE(test_delegate->IsDoingAudioRecording());
    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);

    // The resulting file should have a ".webm" extension.
    const auto file = WaitForCaptureFileToBeSaved();
    EXPECT_TRUE(file.MatchesExtension(".webm"));
  }
}

TEST_F(GifRecordingTest, RecordingTypeIsRespected) {
  auto* controller = StartRegionVideoCapture();
  controller->SetRecordingType(RecordingType::kGif);

  // Even though audio recording is enabled, when performing a GIF recording,
  // the recording service should not be asked to connect to the audio streaming
  // factory and should not be doing any audio recording.
  controller->SetAudioRecordingMode(AudioRecordingMode::kMicrophone);
  StartVideoRecordingImmediately();

  // Test that the configuration histogram was reported correctly, and that the
  // audio histogram was never reported.
  histogram_tester_.ExpectUniqueSample(
      "Ash.CaptureModeController.CaptureConfiguration.ClamshellMode",
      CaptureModeConfiguration::kRegionGifRecording,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "Ash.CaptureModeController.CaptureAudioOnMetric.ClamshellMode",
      /*expected_count=*/0);

  EXPECT_TRUE(controller->is_recording_in_progress());
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  CaptureModeTestApi().FlushRecordingServiceForTesting();
  EXPECT_FALSE(test_delegate->IsDoingAudioRecording());

  // Record for one second so that we can test the recording length histogram.
  WaitForSeconds(1);
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);

  // The resulting file should have a ".gif" extension.
  const auto file = WaitForCaptureFileToBeSaved();
  EXPECT_TRUE(file.MatchesExtension(".gif"));

  histogram_tester_.ExpectUniqueSample(
      "Ash.CaptureModeController.GIFRecordingLength.ClamshellMode",
      /*sample=*/1,  // 1 second.
      /*expected_bucket_count=*/1);

  // Since getting the file size is an async operation, we have to run a loop
  // until the task that records the file size is done.
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectTotalCount(
      "Ash.CaptureModeController.GIFRecordingFileSize.ClamshellMode",
      /*expected_count=*/1);
}

TEST_F(GifRecordingTest, RegionToScreenRatioHistogram) {
  UpdateDisplay("900x600");

  // Contains 3 test cases where the user region areas are different percentages
  // of the full screen area.
  struct {
    const char* const scope_title;
    gfx::Rect user_region_bounds;
    int expected_percent_ratio;
  } kTestCases[] = {
      {"With region 450x300", gfx::Rect(450, 300), 25},   // 25%.
      {"With region 900x300", gfx::Rect(900, 300), 50},   // 50%.
      {"With region 900x600", gfx::Rect(900, 600), 100},  // 100%.
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_title);
    auto* controller = CaptureModeController::Get();
    controller->SetUserCaptureRegion(test_case.user_region_bounds,
                                     /*by_user=*/true);
    controller->SetRecordingType(RecordingType::kGif);

    StartRegionVideoCapture();
    StartVideoRecordingImmediately();

    histogram_tester_.ExpectBucketCount(
        "Ash.CaptureModeController.GIFRecordingRegionToScreenRatio."
        "ClamshellMode",
        /*sample=*/test_case.expected_percent_ratio,
        /*expected_count=*/1);

    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    WaitForCaptureFileToBeSaved();
  }
}

// Regression test for b/293340894. When the region is selected in a such a way
// that will cause the default bounds of the recording type menu to go outside
// the display bounds, the bounds should be adjusted such that it remains within
// the target display.
TEST_F(GifRecordingTest, RecordingMenuOutsideOfBounds) {
  UpdateDisplay("800x700,801+0-800x700");
  auto* controller = CaptureModeController::Get();
  controller->SetUserCaptureRegion(gfx::Rect(1550, 650, 50, 50),
                                   /*by_user=*/true);
  controller->SetRecordingType(RecordingType::kGif);

  auto* event_generator = GetEventGenerator();
  // Move cursor to the second display so capture mode is created there when it
  // starts.
  event_generator->MoveMouseTo(gfx::Point(1000, 500));
  StartRegionVideoCapture();
  ClickOnDropDownButton();

  // The menu should be created without any crashes and should be contained
  // within the bounds of the external display.
  auto* recording_type_menu_widget = GetRecordingTypeMenuWidget();
  ASSERT_TRUE(recording_type_menu_widget);
  const gfx::Rect display_bounds{801, 0, 800, 700};
  EXPECT_TRUE(display_bounds.Contains(
      recording_type_menu_widget->GetWindowBoundsInScreen()));
}

// Regression test for b/319551191.
TEST_F(GifRecordingTest, RecordingMenuAtTheLeftOrRightEdge) {
  // Set a region touching the left edge of the screen.
  const gfx::Size region_size(177, 165);
  auto* controller = CaptureModeController::Get();
  controller->SetUserCaptureRegion(gfx::Rect(gfx::Point(0, 0), region_size),
                                   /*by_user=*/true);

  // There should be no crashes when we open the recording type menu.
  StartRegionVideoCapture();
  ClickOnDropDownButton();

  // Restart the session with a region touching the right edge of the screen.
  controller->Stop();
  const auto root_bounds = Shell::GetPrimaryRootWindow()->bounds();
  controller->SetUserCaptureRegion(
      gfx::Rect(gfx::Point(root_bounds.right() - region_size.width(), 0),
                region_size),
      /*by_user=*/true);

  // Similarly, there should be no crashes.
  StartRegionVideoCapture();
  ClickOnDropDownButton();
}

// -----------------------------------------------------------------------------
// ProjectorGifRecordingTest:

class ProjectorGifRecordingTest : public GifRecordingTest {
 public:
  ProjectorGifRecordingTest() = default;
  ~ProjectorGifRecordingTest() override = default;

  // ProjectorGifRecordingTest:
  void SetUp() override {
    GifRecordingTest::SetUp();
    projector_helper_.SetUp();
  }

  void StartProjectorModeSession() {
    projector_helper_.StartProjectorModeSession();
  }

 private:
  ProjectorCaptureModeIntegrationHelper projector_helper_;
};

TEST_F(ProjectorGifRecordingTest, ProjectorRecordingType) {
  // Start a normal session and enable GIF recording.
  auto* controller = StartRegionVideoCapture();
  controller->SetRecordingType(RecordingType::kGif);

  // Exit this session and start a new projector-initiated session. The active
  // recording type should be `kWebM`.
  controller->Stop();
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(controller->recording_type(), RecordingType::kWebM);

  // By default, the capture source is fullscreen in projector sessions. Switch
  // to `kRegion` and expect that the capture button will show the correct text.
  controller->SetSource(CaptureModeSource::kRegion);
  EXPECT_EQ(GetCaptureButton()->GetText(), u"Record video");

  // The drop-down button should not be created in this case.
  EXPECT_FALSE(
      GetCaptureLabelView()->capture_button_container()->drop_down_button());

  // Exit this session and start a new normal session with the most recent
  // values and expect that the pre-projector-session recording type was
  // restored.
  controller->Stop();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_EQ(controller->recording_type(), RecordingType::kGif);
  EXPECT_EQ(GetCaptureButton()->GetText(), u"Record GIF");
  EXPECT_TRUE(
      GetCaptureLabelView()->capture_button_container()->drop_down_button());
}

}  // namespace ash
