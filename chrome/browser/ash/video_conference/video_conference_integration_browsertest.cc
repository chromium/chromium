// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::video_conference {

// Periodically checks the condition and move on with the rest of the code when
// the condition becomes true. Please tell me this is something you have been
// wanting for very long time.
#define WAIT_FOR_CONDITION(condition)                                 \
  {                                                                   \
    base::RunLoop run_loop;                                           \
    CheckForConditionAndWaitMoreIfNeeded(                             \
        base::BindRepeating(                                          \
            base::BindLambdaForTesting([&]() { return condition; })), \
        run_loop.QuitClosure());                                      \
    run_loop.Run();                                                   \
  }

// Periodically calls `condition` until it becomes true then calls
// `quit_closure`.
void CheckForConditionAndWaitMoreIfNeeded(
    base::RepeatingCallback<bool()> condition,
    base::OnceClosure quit_closure) {
  if (condition.Run()) {
    std::move(quit_closure).Run();
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CheckForConditionAndWaitMoreIfNeeded,
                     std::move(condition), std::move(quit_closure)),
      TestTimeouts::tiny_timeout());
}

// Helper for getting the VcTray.
ash::VideoConferenceTray* GetVcTray() {
  return ash::StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->video_conference_tray();
}

class VideoConferenceIntegrationTest : public WebRtcTestBase {
 public:
  VideoConferenceIntegrationTest() = default;
  ~VideoConferenceIntegrationTest() override = default;

  // Navigate to the url in a new tab.
  content::WebContents* NavigateTo(const std::string& url_str) {
    const GURL url(embedded_test_server()->GetURL(url_str));
    content::RenderFrameHost* main_rfh = ui_test_utils::
        NavigateToURLWithDispositionBlockUntilNavigationsComplete(
            browser(), url, 1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
            ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(main_rfh);
    return web_contents;
  }

  // Allows or disallow permissions for `web_contents`.
  void SetPermission(content::WebContents* web_contents,
                     ContentSettingsType type,
                     ContentSetting result) {
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetContentSettingDefaultScope(web_contents->GetURL(), GURL(), type,
                                        result);
  }

  void StartCamera(content::WebContents* web_contents) {
    EXPECT_TRUE(content::ExecJs(web_contents, "startVideo();"));
  }

  void StopCamera(content::WebContents* web_contents) {
    EXPECT_TRUE(content::ExecJs(web_contents, "stopVideo();"));
  }

  void StartMicrophone(content::WebContents* web_contents) {
    EXPECT_TRUE(content::ExecJs(web_contents, "startAudio();"));
  }
  void StopMicrophone(content::WebContents* web_contents) {
    EXPECT_TRUE(content::ExecJs(web_contents, "stopAudio();"));
  }

  void StartScreenSharing(content::WebContents* web_contents) {
    EXPECT_TRUE(content::ExecJs(web_contents, "startScreenSharing();"));
  }
  void StopScreenSharing(content::WebContents* web_contents) {
    EXPECT_TRUE(content::ExecJs(web_contents, "stopScreenSharing();"));
  }

  void SetUpOnMainThread() override {
    // Used for bypassing tab capturing selection.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kThisTabCaptureAutoAccept);

    WebRtcTestBase::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    camera_bt_ = GetVcTray()->camera_icon();
    mic_bt_ = GetVcTray()->audio_icon();
    share_bt_ = GetVcTray()->screen_share_icon();
  }

 protected:
  VideoConferenceTrayButton* camera_bt_ = nullptr;
  VideoConferenceTrayButton* mic_bt_ = nullptr;
  VideoConferenceTrayButton* share_bt_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kVideoConference};
};

IN_PROC_BROWSER_TEST_F(VideoConferenceIntegrationTest,
                       CaptureVideoShowsVcTray) {
  // Open a tab.
  content::WebContents* web_contents =
      NavigateTo("/video_conference_demo.html");

  // Set permissions as allow.
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_ALLOW);
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_ALLOW);

  // Start camera and wait for the tray to show.
  StartCamera(web_contents);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // camera_icon should be visible with green_dot.
  EXPECT_TRUE(camera_bt_->GetVisible());
  EXPECT_TRUE(camera_bt_->is_capturing());
  EXPECT_TRUE(camera_bt_->show_privacy_indicator());

  // audio_icon should be visible without green_dot.
  EXPECT_TRUE(mic_bt_->GetVisible());
  EXPECT_FALSE(mic_bt_->is_capturing());
  EXPECT_FALSE(mic_bt_->show_privacy_indicator());

  // screen_share_icon should be invisible.
  EXPECT_FALSE(share_bt_->GetVisible());
  EXPECT_FALSE(share_bt_->is_capturing());
  EXPECT_FALSE(share_bt_->show_privacy_indicator());

  // Stop camera and wait for is_capturing to populate.
  StopCamera(web_contents);
  WAIT_FOR_CONDITION(!GetVcTray()->camera_icon()->is_capturing());

  // camera_icon should be visible without green_dot.
  EXPECT_TRUE(camera_bt_->GetVisible());
  EXPECT_FALSE(camera_bt_->is_capturing());
  EXPECT_FALSE(camera_bt_->show_privacy_indicator());

  // Close tab and wait for the tray to dispear.
  web_contents->Close();
  WAIT_FOR_CONDITION(!GetVcTray()->GetVisible());
  // camera_icon should be invisible.
  EXPECT_FALSE(camera_bt_->GetVisible());
  EXPECT_FALSE(camera_bt_->is_capturing());
  EXPECT_FALSE(camera_bt_->show_privacy_indicator());
}

IN_PROC_BROWSER_TEST_F(VideoConferenceIntegrationTest,
                       CaptureAudioShowsVcTray) {
  // Open a tab.
  content::WebContents* web_contents =
      NavigateTo("/video_conference_demo.html");

  // Set permissions as allow.
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_ALLOW);
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_ALLOW);

  // Start microphone and wait for the tray to show.
  StartMicrophone(web_contents);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // camera_icon should be visible without green_dot.
  EXPECT_TRUE(camera_bt_->GetVisible());
  EXPECT_FALSE(camera_bt_->is_capturing());
  EXPECT_FALSE(camera_bt_->show_privacy_indicator());

  // audio_icon should be visible with green_dot.
  EXPECT_TRUE(mic_bt_->GetVisible());
  EXPECT_TRUE(mic_bt_->is_capturing());
  EXPECT_TRUE(mic_bt_->show_privacy_indicator());

  // screen_share_icon should be invisible.
  EXPECT_FALSE(share_bt_->GetVisible());
  EXPECT_FALSE(share_bt_->is_capturing());
  EXPECT_FALSE(share_bt_->show_privacy_indicator());

  // Stop microphone and wait for is_capturing to populate.
  StopMicrophone(web_contents);
  WAIT_FOR_CONDITION(!GetVcTray()->audio_icon()->is_capturing());

  // audio_icon should be visible without green_dot.
  EXPECT_TRUE(mic_bt_->GetVisible());
  EXPECT_FALSE(mic_bt_->is_capturing());
  EXPECT_FALSE(mic_bt_->show_privacy_indicator());

  // Close tab and wait for the tray to dispear.
  web_contents->Close();
  WAIT_FOR_CONDITION(!GetVcTray()->GetVisible());
  // audio_icon should be invisible.
  EXPECT_FALSE(mic_bt_->GetVisible());
  EXPECT_FALSE(mic_bt_->is_capturing());
  EXPECT_FALSE(mic_bt_->show_privacy_indicator());
}

IN_PROC_BROWSER_TEST_F(VideoConferenceIntegrationTest,
                       ScreenSharingShowsVcTray) {
  // Open a tab.
  content::WebContents* web_contents =
      NavigateTo("/video_conference_demo.html");

  // Set permissions as allow.
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_ALLOW);
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_ALLOW);

  // Start screen sharing and wait for the tray to show.
  StartScreenSharing(web_contents);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // camera_icon should be invisible.
  EXPECT_TRUE(camera_bt_->GetVisible());
  EXPECT_FALSE(camera_bt_->is_capturing());
  EXPECT_FALSE(camera_bt_->show_privacy_indicator());

  // audio_icon should be invisible with green_dot.
  EXPECT_TRUE(mic_bt_->GetVisible());
  EXPECT_FALSE(mic_bt_->is_capturing());
  EXPECT_FALSE(mic_bt_->show_privacy_indicator());

  // screen_share_icon should be visible.
  EXPECT_TRUE(share_bt_->GetVisible());
  EXPECT_TRUE(share_bt_->is_capturing());
  EXPECT_TRUE(share_bt_->show_privacy_indicator());

  // Stop microphone and wait for is_capturing to populate.
  StopScreenSharing(web_contents);
  WAIT_FOR_CONDITION(!GetVcTray()->screen_share_icon()->is_capturing());

  EXPECT_FALSE(share_bt_->GetVisible());
  EXPECT_FALSE(share_bt_->is_capturing());
  EXPECT_FALSE(share_bt_->show_privacy_indicator());

  // VcTray should be invisible.
  EXPECT_TRUE(GetVcTray()->GetVisible());

  web_contents->Close();
  WAIT_FOR_CONDITION(!GetVcTray()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VideoConferenceIntegrationTest,
                       MicWithoutPermissionShouldNotShow) {
  // Open a tab.
  content::WebContents* web_contents =
      NavigateTo("/video_conference_demo.html");

  // Set permissions.
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_ALLOW);
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_BLOCK);

  // Start camera and wait for the tray to show.
  StartCamera(web_contents);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Audio icon should not show because the permission is blocked.
  EXPECT_TRUE(camera_bt_->GetVisible());
  EXPECT_FALSE(mic_bt_->GetVisible());
  EXPECT_FALSE(share_bt_->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VideoConferenceIntegrationTest,
                       CameraWithoutPermissionShouldNotShow) {
  // Open a tab.
  content::WebContents* web_contents =
      NavigateTo("/video_conference_demo.html");

  // Set permissions.
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_BLOCK);
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_ALLOW);

  // Start microphone and wait for the tray to show.
  StartMicrophone(web_contents);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Camera icon should not show because the permission is blocked.
  EXPECT_FALSE(camera_bt_->GetVisible());
  EXPECT_TRUE(mic_bt_->GetVisible());
  EXPECT_FALSE(share_bt_->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VideoConferenceIntegrationTest,
                       CameraMicWithoutPermissionShouldNotShow) {
  // Open a tab.
  content::WebContents* web_contents =
      NavigateTo("/video_conference_demo.html");

  // Set permissions.
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_BLOCK);
  SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_BLOCK);

  // Start screen sharing and wait for the tray to show.
  StartScreenSharing(web_contents);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Both microphone and camera should not show.
  EXPECT_FALSE(camera_bt_->GetVisible());
  EXPECT_FALSE(mic_bt_->GetVisible());
  EXPECT_TRUE(share_bt_->GetVisible());
}

}  // namespace ash::video_conference
