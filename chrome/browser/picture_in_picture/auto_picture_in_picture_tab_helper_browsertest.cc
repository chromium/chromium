// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/media_start_stop_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "third_party/blink/public/common/features.h"

using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::AtLeast;
using testing::Return;

namespace {

const base::FilePath::CharType kAutoDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-document.html");

const base::FilePath::CharType kAutoVideoPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-video.html");

const base::FilePath::CharType kBlankPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/blank.html");

const base::FilePath::CharType kCameraPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-camera.html");

const base::FilePath::CharType kNotRegisteredPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-no-register.html");

const base::FilePath::CharType kAutopipDelayPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-delay.html");

const base::FilePath::CharType kAutopipToggleRegistrationPage[] =
    FILE_PATH_LITERAL(
        "media/picture-in-picture/autopip-toggle-registration.html");

class MockInputObserver : public content::RenderWidgetHost::InputEventObserver {
 public:
  MOCK_METHOD(void, OnInputEvent, (const blink::WebInputEvent&), (override));
};

class MockAutoBlocker : public permissions::PermissionDecisionAutoBlockerBase {
 public:
  MOCK_METHOD(bool,
              IsEmbargoed,
              (const GURL& request_origin, ContentSettingsType permission),
              (override));
  MOCK_METHOD(bool,
              RecordDismissAndEmbargo,
              (const GURL& url,
               ContentSettingsType permission,
               bool dismissed_prompt_was_quiet),
              (override));
  MOCK_METHOD(bool,
              RecordIgnoreAndEmbargo,
              (const GURL& url,
               ContentSettingsType permission,
               bool ignored_prompt_was_quiet),
              (override));
};

class AutoPictureInPictureTabHelperBrowserTest : public WebRtcTestBase {
 public:
  AutoPictureInPictureTabHelperBrowserTest() = default;

  AutoPictureInPictureTabHelperBrowserTest(
      const AutoPictureInPictureTabHelperBrowserTest&) = delete;
  AutoPictureInPictureTabHelperBrowserTest& operator=(
      const AutoPictureInPictureTabHelperBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    ResetAudioFocusObserver();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(GetEnabledFeatures(), {});
    InProcessBrowserTest::SetUp();
  }

  void LoadAutoVideoPipPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutoVideoPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutoDocumentPipPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutoDocumentPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadCameraMicrophonePage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kCameraPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadNotRegisteredPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kNotRegisteredPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutopipDelayPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutopipDelayPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutopipToggleRegistrationPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutopipToggleRegistrationPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void OpenNewTab(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kBlankPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, test_page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  void OpenPopUp(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kBlankPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, test_page_url, WindowOpenDisposition::NEW_POPUP,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  void PlayVideo(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(u"playVideo()",
                                                   base::NullCallback());
  }

  void PauseVideo(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"pauseVideo()", base::NullCallback());
  }

  void OpenPipManually(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"openPip({automatic: true})", base::NullCallback());
  }

  void RegisterForAutopip(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"register()", base::NullCallback());
  }

  void UnregisterForAutopip(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"unregister()", base::NullCallback());
  }

  void WaitForMediaSessionActionRegistered(content::WebContents* web_contents) {
    media_session::test::MockMediaSessionMojoObserver observer(
        *content::MediaSession::Get(web_contents));
    observer.WaitForExpectedActions(
        {MediaSessionAction::kEnterPictureInPicture,
         MediaSessionAction::kEnterAutoPictureInPicture,
         MediaSessionAction::kExitPictureInPicture});
  }

  void WaitForMediaSessionActionUnregistered(
      content::WebContents* web_contents) {
    media_session::test::MockMediaSessionMojoObserver observer(
        *content::MediaSession::Get(web_contents));
    observer.WaitForEmptyActions();
  }

  void WaitForMediaSessionPaused(content::WebContents* web_contents) {
    media_session::test::MockMediaSessionMojoObserver observer(
        *content::MediaSession::Get(web_contents));
    observer.WaitForPlaybackState(
        media_session::mojom::MediaPlaybackState::kPaused);
  }

  void WaitForAudioFocusGained() {
    audio_focus_observer_->WaitForGainedEvent();
  }

  void ResetAudioFocusObserver() {
    mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
    content::GetMediaSessionService().BindAudioFocusManager(
        audio_focus_remote.BindNewPipeAndPassReceiver());
    audio_focus_observer_ =
        std::make_unique<media_session::test::TestAudioFocusObserver>();
    audio_focus_remote->AddObserver(
        audio_focus_observer_->BindNewPipeAndPassRemote());
  }

  void SwitchToNewTabAndBackAndExpectAutopip(bool should_video_pip,
                                             bool should_document_pip) {
    auto* original_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto* tab_helper =
        AutoPictureInPictureTabHelper::FromWebContents(original_web_contents);

    // There should not currently be a picture-in-picture window.
    EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
    EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
    // The tab helper should not report that we are, or would be, in auto-pip.
    EXPECT_FALSE(tab_helper->AreAutoPictureInPicturePreconditionsMet());
    EXPECT_FALSE(tab_helper->IsInAutoPictureInPicture());

    // Open and switch to a new tab.
    content::MediaStartStopObserver enter_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();

    // A picture-in-picture window of the correct type should automatically
    // open.
    EXPECT_EQ(should_video_pip,
              original_web_contents->HasPictureInPictureVideo());
    EXPECT_EQ(should_document_pip,
              original_web_contents->HasPictureInPictureDocument());

    // The tab helper should indicate that we're now in pip.
    EXPECT_TRUE(tab_helper->IsInAutoPictureInPicture());
    // Once we're in PiP, the preconditions should not be met anymore.
    EXPECT_FALSE(tab_helper->AreAutoPictureInPicturePreconditionsMet());

    if (should_document_pip) {
      // Document picture-in-picture windows should not receive focus when
      // opened due to the AutoPictureInPictureTabHelper.
      auto* window_manager = PictureInPictureWindowManager::GetInstance();
      ASSERT_TRUE(window_manager->GetChildWebContents());
      EXPECT_FALSE(window_manager->GetChildWebContents()
                       ->GetRenderWidgetHostView()
                       ->HasFocus());
    }

    // Switch back to the original tab.
    content::MediaStartStopObserver exit_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(
            original_web_contents));
    exit_pip_observer.Wait();

    // There should no longer be a picture-in-picture window.
    EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
    EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
  }

  void SetContentSettingEnabled(content::WebContents* web_contents,
                                bool enabled) {
    GURL url = web_contents->GetLastCommittedURL();
    ContentSetting setting =
        enabled ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
    HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()))
        ->SetContentSettingDefaultScope(
            url, url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE, setting);
  }

  void OverrideURL(const GURL& url) {
    // Lie about the URL.
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    web_contents->GetController().GetVisibleEntry()->SetVirtualURL(url);
  }

  // Send some events to `web_contents`, and see if they arrive or not.
  // `expect_events` should be true if we expect them, and false if we should
  // not.  The goal is to infer if the WebContents might be blocking events.
  void CheckIfEventsAreForwarded(content::WebContents* web_contents,
                                 bool expect_events) {
    auto* rwh = web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
    MockInputObserver input_observer;
    rwh->AddInputEventObserver(&input_observer);
    EXPECT_CALL(input_observer, OnInputEvent(_)).Times(expect_events ? 4 : 0);

    blink::WebMouseEvent mouse_event(
        blink::WebMouseEvent::Type::kMouseDown, /*position=*/{},
        /*global_position=*/{}, blink::WebPointerProperties::Button::kLeft,
        /*click_count_param=*/1,
        /*modifiers_param=*/0, base::TimeTicks::Now());
    rwh->ForwardMouseEvent(mouse_event);

    blink::WebMouseWheelEvent mouse_wheel_event(
        blink::WebMouseWheelEvent::Type::kMouseWheel, /*modifiers=*/0,
        base::TimeTicks::Now());
    mouse_wheel_event.phase = blink::WebMouseWheelEvent::Phase::kPhaseBegan;
    rwh->ForwardWheelEvent(mouse_wheel_event);

    content::NativeWebKeyboardEvent keyboard_event(
        blink::WebInputEvent::Type::kChar, /*modifiers=*/0,
        base::TimeTicks::Now());
    rwh->ForwardKeyboardEvent(keyboard_event);

    blink::WebGestureEvent gesture_event(
        blink::WebGestureEvent::Type::kGestureTap, /*modifiers=*/0,
        base::TimeTicks::Now(), blink::mojom::GestureDevice::kTouchpad);
    rwh->ForwardGestureEvent(gesture_event);

    rwh->RemoveInputEventObserver(&input_observer);
  }

 protected:
  virtual std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    return {blink::features::kDocumentPictureInPictureAPI,
            blink::features::kMediaSessionEnterPictureInPicture};
  }

 private:
  std::unique_ptr<media_session::test::TestAudioFocusObserver>
      audio_focus_observer_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class AutoPictureInPictureWithVideoPlaybackBrowserTest
    : public AutoPictureInPictureTabHelperBrowserTest {
 public:
  AutoPictureInPictureWithVideoPlaybackBrowserTest() = default;

  AutoPictureInPictureWithVideoPlaybackBrowserTest(
      const AutoPictureInPictureWithVideoPlaybackBrowserTest&) = delete;
  AutoPictureInPictureWithVideoPlaybackBrowserTest& operator=(
      const AutoPictureInPictureWithVideoPlaybackBrowserTest&) = delete;

  std::vector<base::test::FeatureRef> GetEnabledFeatures() override {
    auto features =
        AutoPictureInPictureTabHelperBrowserTest::GetEnabledFeatures();
    features.push_back(media::kAutoPictureInPictureForVideoPlayback);
    return features;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       OpensAndClosesVideoAutopip) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  PlayVideo(browser()->tab_strip_model()->GetActiveWebContents());
  WaitForAudioFocusGained();

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       OpensAndClosesDocumentAutopip) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  PlayVideo(browser()->tab_strip_model()->GetActiveWebContents());
  WaitForAudioFocusGained();

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       CanAutopipWithCameraMicrophone) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  GetUserMediaAndAccept(browser()->tab_strip_model()->GetActiveWebContents());

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       CannotAutopipViaHttp) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  // Since it's hard to test that autopip is not triggered, settle to make sure
  // that the tab helper switches from "okay" to "not okay".
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);
  EXPECT_TRUE(tab_helper->IsEligibleForAutoPictureInPicture());
  OverrideURL(GURL("http://should.not.work.com"));
  EXPECT_FALSE(tab_helper->IsEligibleForAutoPictureInPicture());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       CanAutopipViaHttps) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  GetUserMediaAndAccept(browser()->tab_strip_model()->GetActiveWebContents());
  OverrideURL(GURL("https://should.work.great.com"));

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       OverlaySettingViewIsShownForDocumentPip) {
  auto* window_manager = PictureInPictureWindowManager::GetInstance();

  LoadCameraMicrophonePage(browser());
  GetUserMediaAndAccept(browser()->tab_strip_model()->GetActiveWebContents());
  OpenNewTab(browser());

  // Use the setting helper as a proxy for "did return an overlay view", since
  // the window manager won't keep it.
  auto* setting_helper = window_manager->get_setting_helper_for_testing();
  ASSERT_TRUE(setting_helper);

  // Verify that input has been blocked.
  auto* pip_contents = window_manager->GetChildWebContents();
  CheckIfEventsAreForwarded(pip_contents, /*expect_events=*/false);

  // Verify that acknowledging the helper restores it.
  setting_helper->take_result_cb_for_testing().Run(
      AutoPipSettingView::UiResult::kAllowOnce);
  CheckIfEventsAreForwarded(pip_contents, /*expect_events=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotAutopipWithoutPlayback) {
  // Load a page that registers for autopip but doesn't start playback.
  LoadAutoVideoPipPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // There should not be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotAutopipWhenPaused) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();

  // Pause the video.
  PauseVideo(original_web_contents);
  WaitForMediaSessionPaused(original_web_contents);

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // There should not be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       OverlaySettingViewIsShownForVideoPip) {
  auto* window_manager = PictureInPictureWindowManager::GetInstance();

  LoadAutoVideoPipPage(browser());
  PlayVideo(browser()->tab_strip_model()->GetActiveWebContents());
  WaitForAudioFocusGained();
  OpenNewTab(browser());

  // Use the setting helper as a proxy for "did return an overlay view", since
  // the window manager won't keep it.
  auto* setting_helper = window_manager->get_setting_helper_for_testing();
  ASSERT_TRUE(setting_helper);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotCloseManuallyOpenedPip) {
  // Load a page that registers for autopip.
  LoadCameraMicrophonePage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(original_web_contents);

  // Open a picture-in-picture window manually.
  content::MediaStartStopObserver enter_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  OpenPipManually(original_web_contents);
  enter_pip_observer.Wait();

  // A pip window should have opened.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // The pip window should still be open.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());

  // Switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));

  // The pip window should still be open.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       ShowsMostRecentlyHiddenTab) {
  // Load a page that registers for autopip.
  LoadCameraMicrophonePage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(original_web_contents);

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  {
    content::MediaStartStopObserver enter_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();
  }

  // A picture-in-picture window should automatically open.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());

  // In the new tab, load another autopip-eligible page.
  LoadCameraMicrophonePage(browser());
  auto* second_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(second_web_contents);

  // The original tab should still be in picture-in-picture.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());
  EXPECT_FALSE(second_web_contents->HasPictureInPictureDocument());

  // Switch back to the original tab.
  {
    content::MediaStartStopObserver exit_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    content::MediaStartStopObserver enter_pip_observer(
        second_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(
            original_web_contents));
    exit_pip_observer.Wait();
    enter_pip_observer.Wait();
  }

  // The second tab should now be in picture-in-picture.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
  EXPECT_TRUE(second_web_contents->HasPictureInPictureDocument());

  // Open a third tab.
  {
    content::MediaStartStopObserver exit_pip_observer(
        second_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    content::MediaStartStopObserver enter_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    exit_pip_observer.Wait();
    enter_pip_observer.Wait();
  }

  // The original tab should now be in picture-in-picture.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());
  EXPECT_FALSE(second_web_contents->HasPictureInPictureDocument());

  // Switch back to the original tab.
  {
    content::MediaStartStopObserver exit_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(
            original_web_contents));
    exit_pip_observer.Wait();
  }

  // Nothing should be in picture-in-picture.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
  EXPECT_FALSE(second_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotAutopipWhenSwitchingToADifferentWindow) {
  // Load a page that registers for autopip.
  LoadCameraMicrophonePage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(original_web_contents);

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new popup window.
  OpenPopUp(browser());

  // There should not be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       RespectsAutoPictureInPictureContentSetting) {
  // Load a page that registers for autopip.
  LoadCameraMicrophonePage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(original_web_contents);

  // Disable the AUTO_PICTURE_IN_PICTURE content setting.
  SetContentSettingEnabled(original_web_contents, false);

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(browser());
  auto* second_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // There should not be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));

  // There should still be no picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Re-enable the content setting.
  SetContentSettingEnabled(original_web_contents, true);

  // Switch back to the second tab.
  content::MediaStartStopObserver enter_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(second_web_contents));
  enter_pip_observer.Wait();

  // A picture-in-picture window should automatically open.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());

  // Switch back to the original tab.
  content::MediaStartStopObserver exit_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kExitPictureInPicture);
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));
  exit_pip_observer.Wait();

  // There should no longer be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotAutopipIfNotRegistered) {
  // Load a page that does not register for autopip and start video playback.
  LoadNotRegisteredPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // The page should not autopip since it is not registered.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       ImmediatelyClosesAutopipIfTabIsAlreadyFocused) {
  // Load a page that is registered for autopip (delayed).
  LoadAutopipDelayPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(original_web_contents);

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  content::MediaStartStopObserver enter_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  content::MediaStartStopObserver exit_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kExitPictureInPicture);

  // Open and switch to a new tab.
  OpenNewTab(browser());

  // Immediately switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));

  // When the page enters autopip after its delay it should immediately be
  // exited.
  enter_pip_observer.Wait();
  exit_pip_observer.Wait();

  // The page should no longer be in picture-in-picture.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       HasEverBeenRegistered) {
  // Load a page that can register and unregister for autopip.
  LoadAutopipToggleRegistrationPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  // Since the page has not yet registered, it should initially be false.
  EXPECT_FALSE(tab_helper->HasAutoPictureInPictureBeenRegistered());

  // Register for autopip. It should then return true.
  RegisterForAutopip(web_contents);
  WaitForMediaSessionActionRegistered(web_contents);
  EXPECT_TRUE(tab_helper->HasAutoPictureInPictureBeenRegistered());

  // After unregistering, it should still return true.
  UnregisterForAutopip(web_contents);
  WaitForMediaSessionActionUnregistered(web_contents);
  EXPECT_TRUE(tab_helper->HasAutoPictureInPictureBeenRegistered());

  // If we navigate the tab, it should return false again.
  LoadNotRegisteredPage(browser());
  EXPECT_FALSE(tab_helper->HasAutoPictureInPictureBeenRegistered());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotOpenIfEmbargoed) {
  // Load a page that registers for autopip.
  LoadCameraMicrophonePage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(original_web_contents);

  // Embargo!
  MockAutoBlocker auto_blocker;
  EXPECT_CALL(auto_blocker,
              IsEmbargoed(_, ContentSettingsType::AUTO_PICTURE_IN_PICTURE))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(original_web_contents);
  tab_helper->set_auto_blocker_for_testing(&auto_blocker);

  // Open and switch to a new tab.
  content::MediaStartStopObserver enter_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  OpenNewTab(browser());

  // A picture-in-picture window should not automatically open.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  tab_helper->set_auto_blocker_for_testing(nullptr);
}
