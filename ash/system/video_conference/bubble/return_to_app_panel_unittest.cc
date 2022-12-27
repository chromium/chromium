// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/return_to_app_panel.h"
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace {

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
    scoped_feature_list_.InitAndEnableFeature(features::kVcControlsUi);

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
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_.reset();
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
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
  auto return_to_app_panel = std::make_unique<ReturnToAppPanel>();
  EXPECT_EQ(1u, return_to_app_panel->children().size());

  auto* app_button =
      static_cast<ReturnToAppButton*>(return_to_app_panel->children().front());
  VerifyReturnToAppButtonInfo(app_button, is_capturing_camera,
                              is_capturing_microphone, is_capturing_screen,
                              kExpectedGoogleMeetDisplayedUrl);
}

TEST_F(ReturnToAppPanelTest, MultipleApps) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(crosapi::mojom::VideoConferenceMediaAppInfo::New(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/base::Time::Now(),
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/GURL(kGoogleMeetTestUrl)));
  controller()->AddMediaApp(crosapi::mojom::VideoConferenceMediaAppInfo::New(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/base::Time::Now(),
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/absl::nullopt));

  // There should be three children, one representing the summary row and two
  // for two running media apps.
  auto return_to_app_panel = std::make_unique<ReturnToAppPanel>();
  EXPECT_EQ(3u, return_to_app_panel->children().size());

  // The first row should be the summary row, representing the state of
  // capturing from all apps and showing that 2 apps are running.
  auto* summary_row =
      static_cast<ReturnToAppButton*>(return_to_app_panel->children().front());
  VerifyReturnToAppButtonInfo(
      summary_row, /*is_capturing_camera=*/true,
      /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true,
      l10n_util::GetStringFUTF16Int(
          IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SUMMARY_TEXT, 2));

  // Verify the next 2 rows, representing the 2 running apps.
  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_panel->children()[1]);
  VerifyReturnToAppButtonInfo(first_app_row, /*is_capturing_camera=*/true,
                              /*is_capturing_microphone=*/false,
                              /*is_capturing_screen=*/false,
                              kExpectedGoogleMeetDisplayedUrl);

  // If the url is not provided, the button should display the app title.
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_panel->children()[2]);
  VerifyReturnToAppButtonInfo(second_app_row, /*is_capturing_camera=*/false,
                              /*is_capturing_microphone=*/true,
                              /*is_capturing_screen=*/true, u"Zoom");
}

}  // namespace ash::video_conference