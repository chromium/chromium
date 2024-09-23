// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/audio_effects_controller.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/return_to_app_panel.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
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
#include "components/prefs/pref_service.h"
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
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/views/test/button_test_api.h"

namespace ash::video_conference {

namespace {

// Helper to generate a fake image.
std::string CreateJpgBytes() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);
  return std::string(data.begin(), data.end());
}

bool IsNudgeShown(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->IsNudgeShown(id);
}

const std::u16string& GetNudgeText(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->GetNudgeBodyTextForTest(id);
}

views::View* GetNudgeAnchorView(const std::string& id) {
  return Shell::Get()->anchored_nudge_manager()->GetNudgeAnchorViewForTest(id);
}

}  // namespace

constexpr char kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.microphone_use_while_sw_disabled";
constexpr char kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.camera_use_while_sw_disabled";
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
  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
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
        {ash::features::kVcStopAllScreenShare,
         ash::features::kOnDeviceSpeechRecognition,
         ash::features::kFeatureManagementVideoConference,
         ash::features::kVcBackgroundReplace,
         ash::features::kShowLiveCaptionInVideoConferenceTray},
        {});
  }

  ~VideoConferenceIntegrationTest() override = default;

  void SetUpOnMainThread() override {
    is_incognito_mode_ = std::get<0>(GetParam());
    is_guest_mode_ = std::get<1>(GetParam());

    WebRtcTestBase::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Create an incognito browser when parameter is true.
    if (is_incognito_mode_) {
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

    // Enable test mode to mock the SetCameraEffects calls.
    camera_effects_controller()->bypass_set_camera_effects_for_testing(true);

    camera_background_img_dir_ = browser()->profile()->GetPath().AppendASCII(
        "camera_background_img_dir");
    camera_background_run_dir_ = browser()->profile()->GetPath().AppendASCII(
        "camera_background_run_dir");
    camera_effects_controller()->set_camera_background_img_dir_for_testing(
        camera_background_img_dir_);
    camera_effects_controller()->set_camera_background_run_dir_for_testing(
        camera_background_run_dir_);

    // Required for the VcBackgroundApp.
    ash::SystemWebAppManager::Get(browser()->profile())
        ->InstallSystemAppsForTesting();
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

  // Generates a background image with given id as name, and apply that as the
  // camera background.
  base::FilePath CreateAndApplyBackgroundImage(uint32_t id) {
    base::RunLoop run_loop;
    camera_effects_controller()->SetBackgroundImageFromContent(
        SeaPenImage(CreateJpgBytes(), id), "",
        base::BindLambdaForTesting([&](bool call_succeeded) {
          EXPECT_TRUE(call_succeeded);
          run_loop.Quit();
        }));
    run_loop.Run();

    return CameraEffectsController::SeaPenIdToRelativePath(id);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Flags use to automatically select the right desktop source and get
    // around security restrictions.
    // TODO(crbug.com/40274188): Use a less error-prone flag.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitchASCII(::switches::kAutoSelectDesktopCaptureSource,
                                    "Display");
#else
    command_line->AppendSwitchASCII(::switches::kAutoSelectDesktopCaptureSource,
                                    "Entire screen");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // If in guest mode.
    if (is_guest_mode_) {
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
    for (views::View* button :
         return_to_app_panel->container_view_->children()) {
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
      WAIT_FOR_CONDITION(camera_bt()->is_capturing());
    }

    if (use_microphone) {
      StartMicrophone(web_contents);
      WAIT_FOR_CONDITION(mic_bt()->is_capturing());
    }

    if (use_screen_sharing) {
      StartScreenSharing(web_contents);
      WAIT_FOR_CONDITION(share_bt()->is_capturing());
    }

    return web_contents;
  }

  VideoConferenceTrayButton* camera_bt() { return GetVcTray()->camera_icon(); }
  VideoConferenceTrayButton* mic_bt() { return GetVcTray()->audio_icon(); }
  VideoConferenceTrayButton* share_bt() {
    return GetVcTray()->screen_share_icon();
  }

  CameraEffectsController* camera_effects_controller() {
    return Shell::Get()->camera_effects_controller();
  }

 protected:
  raw_ptr<Browser, DanglingUntriaged> browser_ = nullptr;

  base::FilePath camera_background_img_dir_;
  base::FilePath camera_background_run_dir_;

  bool is_guest_mode_ = false;
  bool is_incognito_mode_ = false;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,  // Empty to simplify gtest output
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
  EXPECT_TRUE(camera_bt()->GetVisible());
  EXPECT_TRUE(camera_bt()->is_capturing());
  EXPECT_TRUE(camera_bt()->show_privacy_indicator());

  // audio_icon should be visible without green_dot.
  EXPECT_TRUE(mic_bt()->GetVisible());
  EXPECT_FALSE(mic_bt()->is_capturing());
  EXPECT_FALSE(mic_bt()->show_privacy_indicator());

  // screen_share_icon should be invisible.
  EXPECT_FALSE(share_bt()->GetVisible());
  EXPECT_FALSE(share_bt()->is_capturing());
  EXPECT_FALSE(share_bt()->show_privacy_indicator());

  // Stop camera and wait for is_capturing to populate.
  StopCamera(web_contents);
  WAIT_FOR_CONDITION(!camera_bt()->is_capturing());

  // camera_icon should be visible without green_dot.
  EXPECT_TRUE(camera_bt()->GetVisible());
  EXPECT_FALSE(camera_bt()->is_capturing());
  EXPECT_FALSE(camera_bt()->show_privacy_indicator());

  // Close tab and wait for the tray to dispear.
  web_contents->Close();
  WAIT_FOR_CONDITION(!GetVcTray()->GetVisible());
  // camera_icon should be invisible.
  EXPECT_FALSE(camera_bt()->GetVisible());
  EXPECT_FALSE(camera_bt()->is_capturing());
  EXPECT_FALSE(camera_bt()->show_privacy_indicator());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       CaptureAudioShowsVcTray) {
  // Trigger the VcTray with microphone accessing.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/false,
                     /*use_microphone=*/true,
                     /*use_screen_sharing=*/false);

  // camera_icon should be visible without green_dot.
  EXPECT_TRUE(camera_bt()->GetVisible());
  EXPECT_FALSE(camera_bt()->is_capturing());
  EXPECT_FALSE(camera_bt()->show_privacy_indicator());

  // audio_icon should be visible with green_dot.
  EXPECT_TRUE(mic_bt()->GetVisible());
  EXPECT_TRUE(mic_bt()->is_capturing());
  EXPECT_TRUE(mic_bt()->show_privacy_indicator());

  // screen_share_icon should be invisible.
  EXPECT_FALSE(share_bt()->GetVisible());
  EXPECT_FALSE(share_bt()->is_capturing());
  EXPECT_FALSE(share_bt()->show_privacy_indicator());

  // Stop microphone and wait for is_capturing to populate.
  StopMicrophone(web_contents);
  WAIT_FOR_CONDITION(!mic_bt()->is_capturing());

  // audio_icon should be visible without green_dot.
  EXPECT_TRUE(mic_bt()->GetVisible());
  EXPECT_FALSE(mic_bt()->is_capturing());
  EXPECT_FALSE(mic_bt()->show_privacy_indicator());

  // Close tab and wait for the tray to dispear.
  web_contents->Close();
  WAIT_FOR_CONDITION(!GetVcTray()->GetVisible());
  // audio_icon should be invisible.
  EXPECT_FALSE(mic_bt()->GetVisible());
  EXPECT_FALSE(mic_bt()->is_capturing());
  EXPECT_FALSE(mic_bt()->show_privacy_indicator());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       ScreenSharingShowsVcTray) {
  // Trigger the VcTray with screen sharing.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/false,
                     /*use_microphone=*/false,
                     /*use_screen_sharing=*/true);

  // camera_icon should be invisible.
  EXPECT_TRUE(camera_bt()->GetVisible());
  EXPECT_FALSE(camera_bt()->is_capturing());
  EXPECT_FALSE(camera_bt()->show_privacy_indicator());

  // audio_icon should be invisible with green_dot.
  EXPECT_TRUE(mic_bt()->GetVisible());
  EXPECT_FALSE(mic_bt()->is_capturing());
  EXPECT_FALSE(mic_bt()->show_privacy_indicator());

  // screen_share_icon should be visible.
  EXPECT_TRUE(share_bt()->GetVisible());
  EXPECT_TRUE(share_bt()->is_capturing());
  EXPECT_TRUE(share_bt()->show_privacy_indicator());

  // Stop microphone and wait for is_capturing to populate.
  StopScreenSharing(web_contents);
  WAIT_FOR_CONDITION(!share_bt()->is_capturing());

  EXPECT_FALSE(share_bt()->GetVisible());
  EXPECT_FALSE(share_bt()->is_capturing());
  EXPECT_FALSE(share_bt()->show_privacy_indicator());

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
  EXPECT_TRUE(camera_bt()->GetVisible());
  EXPECT_FALSE(mic_bt()->GetVisible());
  EXPECT_FALSE(share_bt()->GetVisible());
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
  EXPECT_FALSE(camera_bt()->GetVisible());
  EXPECT_TRUE(mic_bt()->GetVisible());
  EXPECT_FALSE(share_bt()->GetVisible());
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
  EXPECT_FALSE(camera_bt()->GetVisible());
  EXPECT_FALSE(mic_bt()->GetVisible());
  EXPECT_TRUE(share_bt()->GetVisible());
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
  ClickButton(mic_bt());
  WAIT_FOR_CONDITION(mic_bt()->toggled());
  EXPECT_FALSE(mic_bt()->show_privacy_indicator());

  // Clicking on the mic icon again should unmute it.
  ClickButton(mic_bt());
  WAIT_FOR_CONDITION(!mic_bt()->toggled());
  EXPECT_TRUE(mic_bt()->show_privacy_indicator());

  // Clicking on the camera icon should mute it.
  ClickButton(camera_bt());
  WAIT_FOR_CONDITION(camera_bt()->toggled());
  EXPECT_FALSE(camera_bt()->show_privacy_indicator());

  // Clicking on the camera icon again should unmute it.
  ClickButton(camera_bt());
  WAIT_FOR_CONDITION(!camera_bt()->toggled());
  EXPECT_FALSE(camera_bt()->show_privacy_indicator());
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
  WAIT_FOR_CONDITION(camera_bt()->show_privacy_indicator());

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
  WAIT_FOR_CONDITION(share_bt()->show_privacy_indicator());

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

  // Prevent "Speak-on-mute opt-in" nudge from showing so it doesn't cancel
  // other shown nudges.
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kShouldShowSpeakOnMuteOptInNudge, false);

  // Trigger the VcTray with microphone.
  content::WebContents* web_contents =
      TriggeringTray(/*use_camera=*/false,
                     /*use_microphone=*/true,
                     /*use_screen_sharing=*/false);

  auto* microphone_nudge_id =
      kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId;

  // Stop microphone and wait for is_capturing to populate.
  StopMicrophone(web_contents);
  WAIT_FOR_CONDITION(!mic_bt()->is_capturing());

  // Clicking on the mic icon should mute it.
  ClickButton(mic_bt());
  WAIT_FOR_CONDITION(mic_bt()->toggled());

  // Start accessing microphone should trigger UseWhileDisabled.
  StartMicrophone(web_contents);
  WAIT_FOR_CONDITION(IsNudgeShown(microphone_nudge_id));

  // Check the nudge message and anchor view is as expected.
  EXPECT_EQ(
      GetNudgeText(microphone_nudge_id),
      l10n_util::GetStringFUTF16(
          IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_DISABLED, kTitle1,
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_MICROPHONE_NAME)));
  EXPECT_EQ(GetNudgeAnchorView(microphone_nudge_id), GetVcTray()->audio_icon());

  // Remove current nudge for the next step.
  Shell::Get()->anchored_nudge_manager()->Cancel(microphone_nudge_id);
  WAIT_FOR_CONDITION(!IsNudgeShown(microphone_nudge_id));

  auto* camera_nudge_id = kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId;

  // Clicking on the camera icon should mute it.
  ClickButton(camera_bt());
  WAIT_FOR_CONDITION(camera_bt()->toggled());

  // Start accessing camera should trigger UseWhileDisabled.
  StartCamera(web_contents);
  WAIT_FOR_CONDITION(IsNudgeShown(camera_nudge_id));

  // Check the nudge message and anchor view is as expected.
  EXPECT_EQ(
      GetNudgeText(camera_nudge_id),
      l10n_util::GetStringFUTF16(
          IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_DISABLED, kTitle1,
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_CAMERA_NAME)));
  EXPECT_EQ(GetNudgeAnchorView(camera_nudge_id), GetVcTray()->camera_icon());
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
  WAIT_FOR_CONDITION(camera_bt()->show_privacy_indicator());
  WAIT_FOR_CONDITION(share_bt()->show_privacy_indicator());

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
  VideoConferenceTrayController::Get()->GetEffectsManager().UnregisterDelegate(
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

  for (views::View* row : toggle_effects_view->children()) {
    for (views::View* tile : row->children()) {
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

// TODO(crbug.com/40071631): re-enable once the bug is fixed.
IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       DISABLED_StopAllScreenShare) {
  // Open a tab.
  content::WebContents* web_contents_1 =
      NavigateTo("/video_conference_demo.html");

  // Start the screen sharing.
  StartScreenSharing(web_contents_1);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Get the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());

  // Check that web_contents_1 is sharing screen.
  auto buttons = GetReturnToAppButtons();
  EXPECT_EQ(buttons.size(), 1u);
  EXPECT_FALSE(buttons[0]->is_capturing_camera());
  EXPECT_FALSE(buttons[0]->is_capturing_microphone());
  EXPECT_TRUE(buttons[0]->is_capturing_screen());

  // Hide the ReturnToApp Panel.
  ClickButton(GetVcTray()->toggle_bubble_button());

  // Click on the screen share button.
  EXPECT_TRUE(share_bt()->is_capturing());
  ClickButton(share_bt());
  WAIT_FOR_CONDITION(!share_bt()->is_capturing());

  // Check that web_contents_1 has stopped sharing screen.
  ClickButton(GetVcTray()->toggle_bubble_button());
  WAIT_FOR_CONDITION(GetVcTray()->GetBubbleView()->GetVisible());
  EXPECT_FALSE(GetReturnToAppButtons()[0]->is_capturing_screen());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       ImageButtonShouldActivateWebui) {
  if (is_guest_mode_) {
    GTEST_SKIP() << "Skip for guest mode.";
  }

  // Trigger the VcTray with camera accessing.
  TriggeringTray(/*use_camera=*/true,
                 /*use_microphone=*/false,
                 /*use_screen_sharing=*/false);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Wait until the VcBubble is visible.
  ClickButton(GetVcTray()->toggle_bubble_button());
  auto* buble_view = GetVcTray()->GetBubbleView();
  WAIT_FOR_CONDITION(buble_view->GetVisible());

  views::View* background_image_button =
      buble_view->GetViewByID(BubbleViewID::kBackgroundBlurImageButton);

  // Expect the image button to be visible.
  EXPECT_TRUE(background_image_button->GetVisible());

  // Expect the SetCameraBackgroundView to be invisible.
  EXPECT_FALSE(buble_view->GetViewByID(BubbleViewID::kSetCameraBackgroundView)
                   ->GetVisible());

  content::WebContentsAddedObserver web_contents_added_observer;

  // Clicking on the Image button should open the VcBackgroundApp.
  ClickButton(views::AsViewClass<views::Button>(background_image_button));
  content::WebContents* new_contents =
      web_contents_added_observer.GetWebContents();

  EXPECT_EQ(GURL(vc_background_ui::kChromeUIVcBackgroundURL),
            new_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       CreateWithAiButtonShouldActivateWebui) {
  if (is_guest_mode_) {
    GTEST_SKIP() << "Skip for guest mode.";
  }

  CreateAndApplyBackgroundImage(123u);

  // Trigger the VcTray with camera accessing.
  TriggeringTray(/*use_camera=*/true,
                 /*use_microphone=*/false,
                 /*use_screen_sharing=*/false);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Wait until the VcBubble is visible.
  ClickButton(GetVcTray()->toggle_bubble_button());
  auto* buble_view = GetVcTray()->GetBubbleView();
  WAIT_FOR_CONDITION(buble_view->GetVisible());

  // SetCameraBackgroundView should be visible.
  EXPECT_TRUE(buble_view->GetViewByID(BubbleViewID::kSetCameraBackgroundView)
                  ->GetVisible());

  // Create with AI button should be visible.
  views::View* create_with_ai_button =
      buble_view->GetViewByID(BubbleViewID::kCreateWithAiButton);
  EXPECT_TRUE(create_with_ai_button->GetVisible());

  content::WebContentsAddedObserver web_contents_added_observer;

  // Clicking on the Create with AI button should open VcBackgroundApp.
  ClickButton(views::AsViewClass<views::Button>(create_with_ai_button));
  content::WebContents* new_contents =
      web_contents_added_observer.GetWebContents();

  EXPECT_EQ(GURL(vc_background_ui::kChromeUIVcBackgroundURL),
            new_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       ClickingOnBackgroundBlurHidesSetCameraBackgroundView) {
  if (is_guest_mode_) {
    GTEST_SKIP() << "Skip for guest mode.";
  }

  CreateAndApplyBackgroundImage(123u);

  // Trigger the VcTray with camera accessing.
  TriggeringTray(/*use_camera=*/true,
                 /*use_microphone=*/false,
                 /*use_screen_sharing=*/false);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Wait until the VcBubble is visible.
  ClickButton(GetVcTray()->toggle_bubble_button());
  auto* buble_view = GetVcTray()->GetBubbleView();
  WAIT_FOR_CONDITION(buble_view->GetVisible());

  // SetCameraBackgroundView should be visible.
  auto* set_camera_background_view =
      buble_view->GetViewByID(BubbleViewID::kSetCameraBackgroundView);
  EXPECT_TRUE(set_camera_background_view->GetVisible());

  auto* background_image_button = views::AsViewClass<views::Button>(
      buble_view->GetViewByID(BubbleViewID::kBackgroundBlurImageButton));

  // Constructing the image button is async, we need wait until that completes.
  WAIT_FOR_CONDITION(buble_view->GetViewByID(BubbleViewID::kBackgroundImage0) !=
                     nullptr);
  auto* first_background_image = views::AsViewClass<views::Button>(
      buble_view->GetViewByID(BubbleViewID::kBackgroundImage0));

  for (int button_id : {BubbleViewID::kBackgroundBlurOffButton,
                        BubbleViewID::kBackgroundBlurLightButton,
                        BubbleViewID::kBackgroundBlurFullButton}) {
    // Clicking on background blur button should turn off background replace.
    auto* button =
        views::AsViewClass<views::Button>(buble_view->GetViewByID(button_id));
    ClickButton(button);

    WAIT_FOR_CONDITION(!set_camera_background_view->GetVisible());
    EXPECT_FALSE(
        camera_effects_controller()->GetCameraEffects()->replace_enabled);

    // Clicking image button should set SetCameraBackgroundView visible.
    ClickButton(background_image_button);
    WAIT_FOR_CONDITION(set_camera_background_view->GetVisible());
    EXPECT_FALSE(
        camera_effects_controller()->GetCameraEffects()->replace_enabled);

    // Clicking on image button should apply that as background.
    // Setting background replace is async, so we can't directly check the
    // replace_enabled; but wait for that becomes true.
    ClickButton(first_background_image);
    WAIT_FOR_CONDITION(
        camera_effects_controller()->GetCameraEffects()->replace_enabled);
  }
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       ClickingOnBackgroundImageAppliesBackgroundReplace) {
  if (is_guest_mode_) {
    GTEST_SKIP() << "Skip for guest mode.";
  }

  const base::FilePath image1 = CreateAndApplyBackgroundImage(123u);
  const base::FilePath image2 = CreateAndApplyBackgroundImage(456u);

  // Trigger the VcTray with camera accessing.
  TriggeringTray(/*use_camera=*/true,
                 /*use_microphone=*/false,
                 /*use_screen_sharing=*/false);
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());

  // Wait until the VcBubble is visible.
  ClickButton(GetVcTray()->toggle_bubble_button());
  auto* buble_view = GetVcTray()->GetBubbleView();
  WAIT_FOR_CONDITION(buble_view->GetVisible());

  // Currently image2 should be set as background.
  EXPECT_EQ(
      camera_effects_controller()->GetCameraEffects()->background_filepath,
      image2);

  // Constructing the image button is async, we need wait until that completes.
  WAIT_FOR_CONDITION(buble_view->GetViewByID(BubbleViewID::kBackgroundImage0) !=
                     nullptr);
  WAIT_FOR_CONDITION(buble_view->GetViewByID(BubbleViewID::kBackgroundImage1) !=
                     nullptr);

  auto* first_background_image = views::AsViewClass<views::Button>(
      buble_view->GetViewByID(BubbleViewID::kBackgroundImage0));
  auto* second_background_image = views::AsViewClass<views::Button>(
      buble_view->GetViewByID(BubbleViewID::kBackgroundImage1));

  // Clicking on the second image will set image1 as background.
  ClickButton(second_background_image);
  // Setting background replace is async, so we can't directly check the
  // background_filepath; but wait for that becomes true.
  WAIT_FOR_CONDITION(
      camera_effects_controller()->GetCameraEffects()->background_filepath ==
      image1);

  // Clicking on the first image will set image2 as background.
  // Setting background replace is async, so we can't directly check the
  // background_filepath; but wait for that becomes true.
  ClickButton(first_background_image);
  WAIT_FOR_CONDITION(
      camera_effects_controller()->GetCameraEffects()->background_filepath ==
      image2);
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       TrayTriggeredByCaptureCamera) {
  ASSERT_EQ(1u, ash::WaitForCameraAvailabilityWithTimeout(base::Seconds(5)));
  ash::CaptureModeTestApi test_api;
  test_api.SelectCameraAtIndex(0);
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());

  // VcTray should be triggered.
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(VideoConferenceIntegrationTest,
                       TrayTriggeredByCaptureMicrophone) {
  ASSERT_EQ(1u, ash::WaitForCameraAvailabilityWithTimeout(base::Seconds(5)));
  ash::CaptureModeTestApi test_api;
  test_api.SetAudioRecordingMode(AudioRecordingMode::kMicrophone);
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());
  test_api.PerformCapture();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());

  // VcTray should be triggered.
  WAIT_FOR_CONDITION(GetVcTray()->GetVisible());
}

}  // namespace ash::video_conference
