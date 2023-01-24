// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_button_view.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/recording_type_menu_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
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

  // The focus is moved to the settings button.
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(test_api.GetCaptureModeBarView()->settings_button(),
            test_api.GetCurrentFocusedView()->GetView());
}

}  // namespace ash
