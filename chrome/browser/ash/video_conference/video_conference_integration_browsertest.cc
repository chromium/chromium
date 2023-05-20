// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/audio_effects_controller.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/return_to_app_panel.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"

namespace ash::video_conference {

namespace {

bool IsNudgeShown(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->IsNudgeShown(id);
}

const std::u16string& GetNudgeText(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->GetNudgeText(id);
}

views::View* GetNudgeAnchorView(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->GetNudgeAnchorView(id);
}

}  // namespace

constexpr char kVideoConferenceTrayUseWhileDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.use_while_disabled";
const char16_t kTitle1[] = u"Title1";
const char16_t kTitle2[] = u"Title2";

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

// Simulates left click on the `button`.
void ClickButton(views::Button* button) {
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(button).NotifyClick(event);
}

// Parameter stands for whether in incognito mode; and whether in guest mode.
class VideoConferenceIntegrationTest
    : public testing::WithParamInterface<std::tuple<bool, bool>>,
      public WebRtcTestBase {
 public:
  VideoConferenceIntegrationTest() {
    // kOnDeviceSpeechRecognition is to support live caption.
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kVideoConference,
         ash::features::kOnDeviceSpeechRecognition},
        {});
  }

  ~VideoConferenceIntegrationTest() override = default;

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Create an incognito browser when parameter is true.
    if (std::get<0>(GetParam())) {
      browser_ = Browser::Create(Browser::CreateParams(
          browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
          true));
      // This creates a blank page which is more consistent with normal mode.
      ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
          browser_, GURL("chrome://blank"), 1,
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
              ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    } else {
      browser_ = browser();
    }

    camera_bt_ = GetVcTray()->camera_icon();
    mic_bt_ = GetVcTray()->audio_icon();
    share_bt_ = GetVcTray()->screen_share_icon();
  }

  // Navigate to the url in a new tab.
  content::WebContents* NavigateTo(const std::string& url_str) {
    const GURL url(embedded_test_server()->GetURL(url_str));
    content::RenderFrameHost* main_rfh = ui_test_utils::
        NavigateToURLWithDispositionBlockUntilNavigationsComplete(
            browser_, url, 1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
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
    HostContentSettingsMapFactory::GetForProfile(browser_->profile())
        ->SetContentSettingDefaultScope(web_contents->GetURL(), GURL(), type,
                                        result);
  }

  void StartCamera(content::WebContents* web_contents) {
    // Foreground is required for multiple tabs cases.
    web_contents->GetDelegate()->ActivateContents(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents, "startVideo();"));
  }

  void StopCamera(content::WebContents* web_contents) {
    // Foreground is required for multiple tabs cases.
    web_contents->GetDelegate()->ActivateContents(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents, "stopVideo();"));
  }

  void StartMicrophone(content::WebContents* web_contents) {
    // Foreground is required for multiple tabs cases.
    web_contents->GetDelegate()->ActivateContents(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents, "startAudio();"));
  }
  void StopMicrophone(content::WebContents* web_contents) {
    // Foreground is required for multiple tabs cases.
    web_contents->GetDelegate()->ActivateContents(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents, "stopAudio();"));
  }

  void StartScreenSharing(content::WebContents* web_contents) {
    // Foreground is required for multiple tabs cases.
    web_contents->GetDelegate()->ActivateContents(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents, "startScreenSharing();"));
  }
  void StopScreenSharing(content::WebContents* web_contents) {
    // Foreground is required for multiple tabs cases.
    web_contents->GetDelegate()->ActivateContents(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents, "stopScreenSharing();"));
  }

  // Changes the title of the `web_contents` to be `title`.
  void SetTitle(content::WebContents* web_contents,
                const std::u16string& title) {
    web_contents->GetController().GetLastCommittedEntry()->SetTitle(title);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        ::ash::switches::kCameraEffectsSupportedByHardware);
    // Used for bypassing tab capturing selection.
    command_line->AppendSwitch(::switches::kThisTabCaptureAutoAccept);

    // If in guest mode.
    if (std::get<1>(GetParam())) {
      command_line->AppendSwitch(ash::switches::kGuestSession);
      command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                      user_manager::kGuestUserName);
      command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                      TestingProfile::kTestUserProfileDir);
      command_line->AppendSwitch(::switches::kIncognito);
    }
  }

  // Returns all `ReturnToAppButton`s into a vector for easier check.
  std::vector<ReturnToAppButton*> GetReturnToAppButtons() {
    ReturnToAppPanel* return_to_app_panel = static_cast<ReturnToAppPanel*>(
        GetVcTray()->GetBubbleView()->GetViewByID(BubbleViewID::kReturnToApp));

    std::vector<ReturnToAppButton*> output;
    for (auto* button : return_to_app_panel->container_view_->children()) {
      output.push_back(static_cast<ReturnToAppButton*>(button));
    }
    return output;
  }

  // Returns the ReturnToAppButton with the given `title`.
  ReturnToAppButton* FindReturnToAppButtonByTitle(const std::u16string& title) {
    for (auto* bt : GetReturnToAppButtons()) {
      if (bt->label()->GetText() == title) {
        return bt;
      }
    }

    return nullptr;
  }

  // Helper function that triggers the VcTray with
  //  (1) tab video_conference_demo.html
  //  (2) both camera and microphone permissions.
  //  (3) capturing state based the parameters.
  // This function is to simplify test cases.
  content::WebContents* TriggeringTray(bool use_camera,
                                       bool use_microphone,
                                       bool use_screen_sharing) {
    // Open a tab.
    content::WebContents* web_contents =
        NavigateTo("/video_conference_demo.html");
    // Set permissions as allow.
    SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_CAMERA,
                  CONTENT_SETTING_ALLOW);
    SetPermission(web_contents, ContentSettingsType::MEDIASTREAM_MIC,
                  CONTENT_SETTING_ALLOW);

    // Change title.
    SetTitle(web_contents, kTitle1);

    if (use_camera) {
      StartCamera(web_contents);
      WAIT_FOR_CONDITION(camera_bt_->is_capturing());
    }

    if (use_microphone) {
      StartMicrophone(web_contents);
      WAIT_FOR_CONDITION(mic_bt_->is_capturing());
    }

    if (use_screen_sharing) {
      StartScreenSharing(web_contents);
      WAIT_FOR_CONDITION(share_bt_->is_capturing());
    }

    return web_contents;
  }

 protected:
  raw_ptr<VideoConferenceTrayButton, ExperimentalAsh> camera_bt_ = nullptr;
  raw_ptr<VideoConferenceTrayButton, ExperimentalAsh> mic_bt_ = nullptr;
  raw_ptr<VideoConferenceTrayButton, ExperimentalAsh> share_bt_ = nullptr;

  raw_ptr<Browser, ExperimentalAsh> browser_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoConferenceIntegrationTest,
    ::testing::Values(std::make_tuple<bool, bool>(false, false),
                      std::make_tuple<bool, bool>(true, false),
                      std::make_tuple<bool, bool>(false, true)));

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       CaptureVideoShowsVcTray) {
  // Trigger the VcTray with camera accessing.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/true,
                     /*use_microphone=*/false,
                     /*use_screen_sharing=*/false);

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
  WAIT_FOR_CONDITION(!camera_bt_->is_capturing());

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

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       CaptureAudioShowsVcTray) {
  // Trigger the VcTray with microphone accessing.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/false,
                     /*use_microphone=*/true,
                     /*use_screen_sharing=*/false);

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
  WAIT_FOR_CONDITION(!mic_bt_->is_capturing());

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

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       ScreenSharingShowsVcTray) {
  // Trigger the VcTray with screen sharing.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/false,
                     /*use_microphone=*/false,
                     /*use_screen_sharing=*/true);

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
  WAIT_FOR_CONDITION(!share_bt_->is_capturing());

  EXPECT_FALSE(share_bt_->GetVisible());
  EXPECT_FALSE(share_bt_->is_capturing());
  EXPECT_FALSE(share_bt_->show_privacy_indicator());

  // VcTray should be invisible.
  EXPECT_TRUE(GetVcTray()->GetVisible());

  web_contents->Close();
  WAIT_FOR_CONDITION(!GetVcTray()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       MicWithoutPermissionShouldNotShow) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-92ebd14e-9017-4734-ae47-e9dc6afc6e87");

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

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       CameraWithoutPermissionShouldNotShow) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-2d9bddf4-8d96-4304-a605-1140f9a0b45e");

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

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
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

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       ClickOnTheMicOrCameraIconsShouldMute) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-cc18f9a3-e46f-4192-b505-876975c5ef4b");

  // Trigger the VcTray with microphone.
  TriggeringTray(/*use_camera=*/false,
                 /*use_microphone=*/true,
                 /*use_screen_sharing=*/false);

  // Clicking on the mic icon should mute it.
  ClickButton(mic_bt_);
  WAIT_FOR_CONDITION(mic_bt_->toggled());
  EXPECT_FALSE(mic_bt_->show_privacy_indicator());

  // Clicking on the mic icon again should unmute it.
  ClickButton(mic_bt_);
  WAIT_FOR_CONDITION(!mic_bt_->toggled());
  EXPECT_TRUE(mic_bt_->show_privacy_indicator());

  // Clicking on the camera icon should mute it.
  ClickButton(camera_bt_);
  WAIT_FOR_CONDITION(camera_bt_->toggled());
  EXPECT_FALSE(camera_bt_->show_privacy_indicator());

  // Clicking on the camera icon again should unmute it.
  ClickButton(camera_bt_);
  WAIT_FOR_CONDITION(!camera_bt_->toggled());
  EXPECT_FALSE(camera_bt_->show_privacy_indicator());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       OneTabReturnToAppInformation) {
  // Trigger the VcTray with microphone.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/false,
                     /*use_microphone=*/true,
                     /*use_screen_sharing=*/false);

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  // Chceck the button for title and capturing information.
  // Only is_capturing_microphone should be true.
  auto buttons = GetReturnToAppButtons();
  EXPECT_EQ(buttons.size(), 1u);
  EXPECT_FALSE(buttons[0]->is_capturing_camera());
  EXPECT_TRUE(buttons[0]->is_capturing_microphone());
  EXPECT_FALSE(buttons[0]->is_capturing_screen());
  EXPECT_EQ(buttons[0]->label()->GetText(), kTitle1);

  // We want to close the panel and open it every time.
  GetVcTray()->CloseBubble();

  // Start accessing camera.
  StartCamera(web_contents);
  WAIT_FOR_CONDITION(camera_bt_->show_privacy_indicator());

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  // Chceck the button for title and capturing information.
  // is_capturing_camera and is_capturing_microphone should be true.
  buttons = GetReturnToAppButtons();
  EXPECT_EQ(buttons.size(), 1u);
  EXPECT_TRUE(buttons[0]->is_capturing_camera());
  EXPECT_TRUE(buttons[0]->is_capturing_microphone());
  EXPECT_FALSE(buttons[0]->is_capturing_screen());
  EXPECT_EQ(buttons[0]->label()->GetText(), kTitle1);

  // We want to close the panel and open it every time.
  GetVcTray()->CloseBubble();

  // Start screen sharing.
  StartScreenSharing(web_contents);
  WAIT_FOR_CONDITION(share_bt_->show_privacy_indicator());

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  // Chceck the button for title and capturing information.
  // All capturing should be true.
  buttons = GetReturnToAppButtons();
  EXPECT_EQ(buttons.size(), 1u);
  EXPECT_TRUE(buttons[0]->is_capturing_camera());
  EXPECT_TRUE(buttons[0]->is_capturing_microphone());
  EXPECT_TRUE(buttons[0]->is_capturing_screen());
  EXPECT_EQ(buttons[0]->label()->GetText(), kTitle1);
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest, OneTabReturnToApp) {
  // Trigger the VcTray with microphone.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/false,
                     /*use_microphone=*/true,
                     /*use_screen_sharing=*/false);

  // Switch to the default tab at 0; this should make the `web_contents` hidden.
  browser_->tab_strip_model()->ActivateTabAt(0);
  WAIT_FOR_CONDITION(web_contents->GetVisibility() ==
                     content::Visibility::HIDDEN);

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  // Click on the ReturnToApp button should make the `web_contents` visible
  // again.
  ClickButton(GetReturnToAppButtons()[0]);
  WAIT_FOR_CONDITION(web_contents->GetVisibility() ==
                     content::Visibility::VISIBLE);

  // We want to close the panel and open it every time.
  GetVcTray()->CloseBubble();

  // Minimize the browser window; this should make the `web_contents` hidden.
  browser_->window()->Minimize();
  WAIT_FOR_CONDITION(web_contents->GetVisibility() ==
                     content::Visibility::HIDDEN);

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  // Click on the ReturnToApp button should make the `web_contents` visible
  // again.
  ClickButton(GetReturnToAppButtons()[0]);
  WAIT_FOR_CONDITION(web_contents->GetVisibility() ==
                     content::Visibility::VISIBLE);
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest, UseWhileDisabled) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-f583c1ff-db6f-460e-b1f2-ddec173359a6");
  base::AddFeatureIdTagToTestResult(
      "screenplay-3042cdd9-978d-432c-8488-77684b09a9e4");

  // Trigger the VcTray with microphone.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/false,
                     /*use_microphone=*/true,
                     /*use_screen_sharing=*/false);

  auto* nudge_id = kVideoConferenceTrayUseWhileDisabledNudgeId;

  // Stop microphone and wait for is_capturing to populate.
  StopMicrophone(web_contents);
  WAIT_FOR_CONDITION(!mic_bt_->is_capturing());

  // Clicking on the mic icon should mute it.
  ClickButton(mic_bt_);
  WAIT_FOR_CONDITION(mic_bt_->toggled());

  // Start accessing microphone should trigger UseWhileDisabled.
  StartMicrophone(web_contents);
  WAIT_FOR_CONDITION(IsNudgeShown(nudge_id));

  // Check the nudge message and anchor view is as expected.
  EXPECT_EQ(
      GetNudgeText(nudge_id),
      l10n_util::GetStringFUTF16(
          IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_SOFTWARE_DISABLED, kTitle1,
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_MICROPHONE_NAME)));
  EXPECT_EQ(GetNudgeAnchorView(nudge_id), GetVcTray()->audio_icon());

  // Remove current nudge for the next step.
  Shell::Get()->anchored_nudge_manager()->Cancel(nudge_id);
  WAIT_FOR_CONDITION(!IsNudgeShown(nudge_id));

  // Clicking on the camera icon should mute it.
  ClickButton(camera_bt_);
  WAIT_FOR_CONDITION(camera_bt_->toggled());

  // Start accessing camera should trigger UseWhileDisabled.
  StartCamera(web_contents);
  WAIT_FOR_CONDITION(IsNudgeShown(nudge_id));

  // Check the nudge message and anchor view is as expected.
  EXPECT_EQ(
      GetNudgeText(nudge_id),
      l10n_util::GetStringFUTF16(
          IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_SOFTWARE_DISABLED, kTitle1,
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_CAMERA_NAME)));
  EXPECT_EQ(GetNudgeAnchorView(nudge_id), GetVcTray()->camera_icon());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       TwoTabsButtonInformation) {
  // Open a tab.
  content::WebContents* web_contents_1 =
      NavigateTo("/video_conference_demo.html");
  // Set permissions as allow.
  SetPermission(web_contents_1, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_ALLOW);
  SetPermission(web_contents_1, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_ALLOW);
  // Set title.
  SetTitle(web_contents_1, kTitle1);

  // Open second tab.
  content::WebContents* web_contents_2 =
      NavigateTo("/video_conference_demo.html");
  // Set title.
  SetTitle(web_contents_2, kTitle2);

  StartMicrophone(web_contents_1);
  StartMicrophone(web_contents_2);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  // There should be three buttons, the first one is the summary button.
  auto buttons = GetReturnToAppButtons();
  EXPECT_EQ(buttons.size(), 3u);

  // Button[0] is the summary button.
  EXPECT_FALSE(buttons[0]->is_capturing_camera());
  EXPECT_TRUE(buttons[0]->is_capturing_microphone());
  EXPECT_FALSE(buttons[0]->is_capturing_screen());
  EXPECT_EQ(buttons[0]->label()->GetText(), u"Used by 2 apps");

  // Check information of the button for web_contents_1.
  const auto* bt_1 = FindReturnToAppButtonByTitle(kTitle1);
  EXPECT_FALSE(bt_1->is_capturing_camera());
  EXPECT_TRUE(bt_1->is_capturing_microphone());
  EXPECT_FALSE(bt_1->is_capturing_screen());

  // Check information of the button for web_contents_2.
  const auto* bt_2 = FindReturnToAppButtonByTitle(kTitle2);
  EXPECT_FALSE(bt_2->is_capturing_camera());
  EXPECT_TRUE(bt_2->is_capturing_microphone());
  EXPECT_FALSE(bt_2->is_capturing_screen());

  // We want to close the panel and open it every time.
  GetVcTray()->CloseBubble();
  StartCamera(web_contents_1);
  StartScreenSharing(web_contents_2);

  // Wait for signals to populate.
  WAIT_FOR_CONDITION(camera_bt_->show_privacy_indicator());
  WAIT_FOR_CONDITION(share_bt_->show_privacy_indicator());

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  // There should be three buttons, the first one is the summary button.
  buttons = GetReturnToAppButtons();
  EXPECT_EQ(buttons.size(), 3u);

  // Button[0] is the summary button.
  EXPECT_TRUE(buttons[0]->is_capturing_camera());
  EXPECT_TRUE(buttons[0]->is_capturing_microphone());
  EXPECT_TRUE(buttons[0]->is_capturing_screen());
  EXPECT_EQ(buttons[0]->label()->GetText(), u"Used by 2 apps");

  // Check information of the button for web_contents_1.
  bt_1 = FindReturnToAppButtonByTitle(kTitle1);
  EXPECT_TRUE(bt_1->is_capturing_camera());
  EXPECT_TRUE(bt_1->is_capturing_microphone());
  EXPECT_FALSE(bt_1->is_capturing_screen());

  // Check information of the button for web_contents_2.
  bt_2 = FindReturnToAppButtonByTitle(kTitle2);
  EXPECT_FALSE(bt_2->is_capturing_camera());
  EXPECT_TRUE(bt_2->is_capturing_microphone());
  EXPECT_TRUE(bt_2->is_capturing_screen());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest, TwoTabsReturnToApp) {
  // Open a tab.
  content::WebContents* web_contents_1 =
      NavigateTo("/video_conference_demo.html");
  // Set permissions as allow.
  SetPermission(web_contents_1, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_ALLOW);
  SetPermission(web_contents_1, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_ALLOW);
  // Set title.
  SetTitle(web_contents_1, kTitle1);

  // Open second tab.
  content::WebContents* web_contents_2 =
      NavigateTo("/video_conference_demo.html");
  // Set title.
  SetTitle(web_contents_2, kTitle2);

  StartMicrophone(web_contents_1);
  StartMicrophone(web_contents_2);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  auto buttons = GetReturnToAppButtons();
  EXPECT_EQ(buttons.size(), 3u);

  // Verify that web_contents_2 is foregrounded and web_contents_1 is
  // backgrounded.
  EXPECT_EQ(web_contents_2->GetVisibility(), content::Visibility::VISIBLE);
  EXPECT_NE(web_contents_1->GetVisibility(), content::Visibility::VISIBLE);

  // Click on web_contents_1 and expect the visibility to change.
  ClickButton(FindReturnToAppButtonByTitle(kTitle1));
  WAIT_FOR_CONDITION(web_contents_1->GetVisibility() ==
                     content::Visibility::VISIBLE);
  EXPECT_NE(web_contents_2->GetVisibility(), content::Visibility::VISIBLE);
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       ExpectedButtonsAreShown) {
  // In order to set noise cancellation support, we need to call
  //   CrasAudioHandler::Get()->SetNoiseCancellationSupportedForTesting(true)
  // But by the time it reaches here, the audio_effects_controller is already
  // constructed based on the fact that the Noise Cancellation is not supported.
  // The way to fix that is:
  //  (1) Unregister audio_effects_controller
  //  (2) SetNoiseCancellationSupportedForTesting(true)
  //  (3) Register audio_effects_controller again with
  //      OnActiveUserPrefServiceChanged.
  auto* audio_effects_controller = Shell::Get()->audio_effects_controller();
  VideoConferenceTrayController::Get()->effects_manager().UnregisterDelegate(
      audio_effects_controller);
  CrasAudioHandler::Get()->SetNoiseCancellationSupportedForTesting(true);
  audio_effects_controller->OnActiveUserPrefServiceChanged(
      g_browser_process->local_state());

  // Trigger the VcTray with microphone.
  TriggeringTray(/*use_camera=*/false,
                 /*use_microphone=*/true,
                 /*use_screen_sharing=*/false);

  // Wait for VcPanel to appear.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  bool found_live_caption_button = false;
  bool found_noise_cancellation_buttion = false;

  auto* toggle_effects_view = GetVcTray()->GetBubbleView()->GetViewByID(
      BubbleViewID::kToggleEffectsView);

  for (auto* row : toggle_effects_view->children()) {
    for (auto* tile : row->children()) {
      // Each tile is a button to click on; we want its label.
      views::Label* label = static_cast<views::Label*>(
          tile->GetViewByID(BubbleViewID::kToggleEffectLabel));

      if (label->GetText() ==
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION)) {
        found_live_caption_button = true;
      }

      if (label->GetText() ==
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION)) {
        found_noise_cancellation_buttion = true;
      }
    }
  }

  EXPECT_TRUE(found_live_caption_button);
  EXPECT_TRUE(found_noise_cancellation_buttion);
}

}  // namespace ash::video_conference
