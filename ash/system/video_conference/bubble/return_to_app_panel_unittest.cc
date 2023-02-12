// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/return_to_app_panel.h"
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace {

crosapi::mojom::VideoConferenceMediaAppInfoPtr CreateFakeMediaApp(
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen,
    const std::u16string& title,
    std::string url,
    const base::UnguessableToken& id = base::UnguessableToken::Create()) {
  return crosapi::mojom::VideoConferenceMediaAppInfo::New(
      id,
      /*last_activity_time=*/base::Time::Now(), is_capturing_camera,
      is_capturing_microphone, is_capturing_screen, title,
      /*url=*/GURL(url));
}

// Verifies the information of `ReturnToAppButton`.
void VerifyReturnToAppButtonInfo(
    ash::video_conference::ReturnToAppButton* button,
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen,
    const std::u16string& display_text) {
  EXPECT_EQ(is_capturing_camera, button->is_capturing_camera());
  EXPECT_EQ(is_capturing_microphone, button->is_capturing_microphone());
  EXPECT_EQ(is_capturing_screen, button->is_capturing_screen());
  EXPECT_EQ(display_text, button->label()->GetText());
}

// Used for verifying displayed url.
const std::string kGoogleMeetTestUrl = "https://meet.google.com/abc-xyz/ab-123";
const std::u16string kExpectedGoogleMeetDisplayedUrl =
    u"meet.google.com/abc-xyz/ab-123";

}  // namespace

namespace ash::video_conference {

class ReturnToAppPanelTest : public AshTestBase {
 public:
  ReturnToAppPanelTest() = default;
  ReturnToAppPanelTest(const ReturnToAppPanelTest&) = delete;
  ReturnToAppPanelTest& operator=(const ReturnToAppPanelTest&) = delete;
  ~ReturnToAppPanelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVideoConference);

    // Here we have to create the global instance of `CrasAudioHandler` before
    // `FakeVideoConferenceTrayController`, so we do it here and not do it in
    // `AshTestBase`.
    CrasAudioClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    set_create_global_cras_audio_handler(false);
    AshTestBase::SetUp();

    // Make the video conference tray visible for testing.
    video_conference_tray()->SetVisiblePreferred(true);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_.reset();
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  IconButton* toggle_bubble_button() {
    return video_conference_tray()->toggle_bubble_button_;
  }

  // Get the `ReturnToAppPanel` from the test `StatusAreaWidget`.
  ReturnToAppPanel* GetReturnToAppPanel() {
    return static_cast<ReturnToAppPanel*>(
        video_conference_tray()->GetBubbleView()->GetViewByID(
            BubbleViewID::kReturnToApp));
  }

  views::View* GetReturnToAppContainer(ReturnToAppPanel* panel) {
    return panel->container_view_;
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

TEST_F(ReturnToAppPanelTest, NoApp) {
  controller()->ClearMediaApps();

  // The view should not be visible when there's no app.
  auto return_to_app_panel = std::make_unique<ReturnToAppPanel>();
  EXPECT_FALSE(return_to_app_panel->GetVisible());
}

TEST_F(ReturnToAppPanelTest, OneApp) {
  bool is_capturing_camera = true;
  bool is_capturing_microphone = false;
  bool is_capturing_screen = false;
  controller()->ClearMediaApps();
  controller()->AddMediaApp(crosapi::mojom::VideoConferenceMediaAppInfo::New(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/base::Time::Now(), is_capturing_camera,
      is_capturing_microphone, is_capturing_screen, /*title=*/u"Google Meet",
      /*url=*/GURL(kGoogleMeetTestUrl)));

  // There should be one child representing the only one running media app.
  auto panel = std::make_unique<ReturnToAppPanel>();
  auto* return_to_app_container = GetReturnToAppContainer(panel.get());

  EXPECT_EQ(1u, return_to_app_container->children().size());

  auto* app_button = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  EXPECT_FALSE(app_button->expand_indicator()->GetVisible());
  VerifyReturnToAppButtonInfo(app_button, is_capturing_camera,
                              is_capturing_microphone, is_capturing_screen,
                              kExpectedGoogleMeetDisplayedUrl);
}

TEST_F(ReturnToAppPanelTest, MultipleApps) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/kGoogleMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  // There should be three children, one representing the summary row and two
  // for two running media apps.
  auto panel = std::make_unique<ReturnToAppPanel>();
  auto* return_to_app_container = GetReturnToAppContainer(panel.get());
  EXPECT_EQ(3u, return_to_app_container->children().size());

  // The first row should be the summary row, representing the state of
  // capturing from all apps and showing that 2 apps are running.
  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  VerifyReturnToAppButtonInfo(
      summary_row, /*is_capturing_camera=*/true,
      /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true,
      l10n_util::GetStringFUTF16Int(
          IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SUMMARY_TEXT, 2));

  // Verify the next 2 rows, representing the 2 running apps.
  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  VerifyReturnToAppButtonInfo(first_app_row, /*is_capturing_camera=*/true,
                              /*is_capturing_microphone=*/false,
                              /*is_capturing_screen=*/false,
                              kExpectedGoogleMeetDisplayedUrl);

  // If the url is not provided, the button should display the app title.
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);
  VerifyReturnToAppButtonInfo(second_app_row, /*is_capturing_camera=*/false,
                              /*is_capturing_microphone=*/true,
                              /*is_capturing_screen=*/true, u"Zoom");
}

TEST_F(ReturnToAppPanelTest, ExpandCollapse) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/kGoogleMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  auto panel = std::make_unique<ReturnToAppPanel>();
  auto* return_to_app_container = GetReturnToAppContainer(panel.get());
  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  EXPECT_TRUE(summary_row->expand_indicator()->GetVisible());

  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);

  // The panel should be collapsed by default.
  EXPECT_FALSE(summary_row->expanded());

  // Verify the views in collapsed state:
  EXPECT_TRUE(summary_row->icons_container()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SHOW_TOOLTIP),
            summary_row->expand_indicator()->GetTooltipText());
  EXPECT_FALSE(first_app_row->GetVisible());
  EXPECT_FALSE(second_app_row->GetVisible());

  // Clicking the summary row should expand the panel.
  summary_row->OnButtonClicked(/*id=*/base::UnguessableToken::Null());
  EXPECT_TRUE(summary_row->expanded());

  // Verify the views in expanded state:
  EXPECT_FALSE(summary_row->icons_container()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_HIDE_TOOLTIP),
            summary_row->expand_indicator()->GetTooltipText());
  EXPECT_TRUE(first_app_row->GetVisible());
  EXPECT_TRUE(second_app_row->GetVisible());

  // Click again. Should be in collapsed state.
  summary_row->OnButtonClicked(/*id=*/base::UnguessableToken::Null());
  EXPECT_FALSE(summary_row->expanded());
}

TEST_F(ReturnToAppPanelTest, MaxCapturingCount) {
  // Test the panel's `max_capturing_count_` to make sure the buttons are
  // aligned correctly.
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/kGoogleMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));
  auto return_to_app_panel = std::make_unique<ReturnToAppPanel>();
  EXPECT_EQ(1, return_to_app_panel->max_capturing_count());

  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/kGoogleMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));
  return_to_app_panel = std::make_unique<ReturnToAppPanel>();
  EXPECT_EQ(2, return_to_app_panel->max_capturing_count());

  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/kGoogleMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));
  return_to_app_panel = std::make_unique<ReturnToAppPanel>();
  EXPECT_EQ(3, return_to_app_panel->max_capturing_count());
}

TEST_F(ReturnToAppPanelTest, ReturnToApp) {
  auto app_id1 = base::UnguessableToken::Create();
  auto app_id2 = base::UnguessableToken::Create();

  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/kGoogleMeetTestUrl, /*id=*/app_id1));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/"", /*id=*/app_id2));

  LeftClickOn(toggle_bubble_button());
  auto* return_to_app_panel = GetReturnToAppPanel();
  auto* return_to_app_container = GetReturnToAppContainer(return_to_app_panel);

  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);

  // Clicking on the summary row should not launch any apps (it switched the
  // panel to expanded state).
  LeftClickOn(summary_row);
  ASSERT_TRUE(summary_row->expanded());
  EXPECT_FALSE(controller()->app_to_launch_state_[app_id1]);
  EXPECT_FALSE(controller()->app_to_launch_state_[app_id2]);

  // Clicking each row should open the corresponding app.
  LeftClickOn(first_app_row);
  EXPECT_TRUE(controller()->app_to_launch_state_[app_id1]);
  EXPECT_FALSE(controller()->app_to_launch_state_[app_id2]);

  LeftClickOn(second_app_row);
  EXPECT_TRUE(controller()->app_to_launch_state_[app_id2]);
}

}  // namespace ash::video_conference