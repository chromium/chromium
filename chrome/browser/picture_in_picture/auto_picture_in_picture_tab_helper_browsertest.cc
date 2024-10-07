// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"

using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::AtLeast;
using testing::Return;

namespace {

const base::FilePath::CharType kAutoDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-document.html");

const base::FilePath::CharType kAutoDocumentVideoVisibilityPipPage[] =
    FILE_PATH_LITERAL(
        "media/picture-in-picture/autopip-document-video-visibility.html");

const base::FilePath::CharType kAutoVideoPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-video.html");

const base::FilePath::CharType kAutoVideoVisibilityPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/autopip-video-visibility.html");

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

// Helper class to wait for "recently audible" callbacks.
class WasRecentlyAudibleWaiter {
 public:
  using RecentlyAudibleCallback = base::RepeatingCallback<void(bool)>;

  WasRecentlyAudibleWaiter() = default;
  WasRecentlyAudibleWaiter(const WasRecentlyAudibleWaiter&) = delete;
  WasRecentlyAudibleWaiter(WasRecentlyAudibleWaiter&&) = delete;
  WasRecentlyAudibleWaiter& operator=(const WasRecentlyAudibleWaiter&) = delete;

  RecentlyAudibleCallback GetRecentlyAudibleCallback() {
    was_recently_audible_ = std::nullopt;

    // base::Unretained() is safe since no further tasks can run after
    // RunLoop::Run() returns.
    return base::BindRepeating(
        &WasRecentlyAudibleWaiter::OnRecentlyAudibleCallback,
        base::Unretained(this));
  }

  void WaitUntilDone() {
    if (was_recently_audible_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  bool WasRecentlyAudible() {
    DCHECK(was_recently_audible_);
    return was_recently_audible_.value();
  }

 private:
  void OnRecentlyAudibleCallback(bool was_recently_audible) {
    was_recently_audible_ = was_recently_audible;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::optional<bool> was_recently_audible_;
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

  void LoadAutoVideoVisibilityPipPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutoVideoVisibilityPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutoDocumentVideoVisibilityPipPage(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutoDocumentVideoVisibilityPipPage));
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
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"playVideo()", base::NullCallback(),
            content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void PauseVideo(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"pauseVideo()", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void OpenPipManually(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"openPip()", base::NullCallback(),
            content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void RegisterForAutopip(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"register()", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void UnregisterForAutopip(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"unregister()", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void AddOverlayToVideo(content::WebContents* web_contents,
                         bool should_occlude) {
    const std::u16string script = base::UTF8ToUTF16(base::StrCat(
        {"addOverlayToVideo(", should_occlude ? "true" : "false", ")"}));

    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            script, base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void ForceLifecycleUpdate(content::WebContents* web_contents) {
    // Force a lifecycle update to trigger the |MediaVideoVisibilityTracker|
    // computation.
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(web_contents, "", "").error.empty());
  }

  void MuteVideo(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"muteVideo()", base::NullCallback(),
            content::ISOLATED_WORLD_ID_GLOBAL);
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
    // Flush so that the tab helper has also found out about this.
    content::MediaSession::FlushObserversForTesting(web_contents);
  }

  void WaitForMediaSessionPlaying(content::WebContents* web_contents) {
    media_session::test::MockMediaSessionMojoObserver observer(
        *content::MediaSession::Get(web_contents));
    observer.WaitForPlaybackState(
        media_session::mojom::MediaPlaybackState::kPlaying);
    // Flush so that the tab helper has also found out about this.
    content::MediaSession::FlushObserversForTesting(web_contents);
  }

  void WaitForAudioFocusGained() {
    audio_focus_observer_->WaitForGainedEvent();
  }

  void WaitForWasRecentlyAudible(content::WebContents* web_contents,
                                 bool expected_recently_audible = true) {
    auto* audible_helper = RecentlyAudibleHelper::FromWebContents(web_contents);
    if (audible_helper->WasRecentlyAudible() == expected_recently_audible) {
      return;
    }

    WasRecentlyAudibleWaiter audible_waiter;
    base::CallbackListSubscription subscription =
        audible_helper->RegisterCallbackForTesting(
            audible_waiter.GetRecentlyAudibleCallback());
    audible_waiter.WaitUntilDone();
    DCHECK_EQ(expected_recently_audible, audible_waiter.WasRecentlyAudible());
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

  void WaitForAutoPip(content::WebContents* opener_web_contents) {
    if (PictureInPictureWindowManager::GetInstance()->GetWebContents() ==
        opener_web_contents) {
      return;
    }
    content::MediaStartStopObserver enter_pip_observer(
        opener_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    enter_pip_observer.Wait();
    // Wait: this doesn't guarantee that it opened pip yet.  i think it means we
    // sent the action.  however, it should guarantee that the tab helper is in
    // the right state.  actually, the wait checks for a WebContentsObserver to
    // change into the "have pip" state, which might guarantee it.
  }

  // Switch to a tab that contains `web_contents`.
  void SwitchToExistingTab(content::WebContents* web_contents) {
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(web_contents));
  }

  void SwitchToNewTabAndWaitForAutoPip() {
    auto* opener_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto* tab_helper =
        AutoPictureInPictureTabHelper::FromWebContents(opener_web_contents);

    // There should not currently be a picture-in-picture window.
    EXPECT_FALSE(opener_web_contents->HasPictureInPictureVideo());
    EXPECT_FALSE(opener_web_contents->HasPictureInPictureDocument());
    // The tab helper should not report that we are, or would be, in auto-pip.
    EXPECT_FALSE(tab_helper->AreAutoPictureInPicturePreconditionsMet());
    EXPECT_FALSE(tab_helper->IsInAutoPictureInPicture());

    // Open and switch to a new tab.
    content::MediaStartStopObserver enter_pip_observer(
        opener_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();

    // The tab helper should indicate that we're now in pip.
    EXPECT_TRUE(tab_helper->IsInAutoPictureInPicture());
    // Once we're in PiP, the preconditions should not be met anymore.
    EXPECT_FALSE(tab_helper->AreAutoPictureInPicturePreconditionsMet());
  }

  void SwitchBackToOpenerAndWaitForPipToClose() {
    auto* opener_web_contents =
        PictureInPictureWindowManager::GetInstance()->GetWebContents();

    // Switch back to the original tab.
    content::MediaStartStopObserver exit_pip_observer(
        opener_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    SwitchToExistingTab(opener_web_contents);
    exit_pip_observer.Wait();

    // There should no longer be a picture-in-picture window.
    EXPECT_FALSE(opener_web_contents->HasPictureInPictureVideo());
    EXPECT_FALSE(opener_web_contents->HasPictureInPictureDocument());
  }

  void SwitchToNewTabAndBackAndExpectAutopip(bool should_video_pip,
                                             bool should_document_pip) {
    auto* original_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    SwitchToNewTabAndWaitForAutoPip();

    // A picture-in-picture window of the correct type should automatically
    // open.
    EXPECT_EQ(should_video_pip,
              original_web_contents->HasPictureInPictureVideo());
    EXPECT_EQ(should_document_pip,
              original_web_contents->HasPictureInPictureDocument());

    if (should_document_pip) {
      // Document picture-in-picture windows should not receive focus when
      // opened due to the AutoPictureInPictureTabHelper.
      auto* window_manager = PictureInPictureWindowManager::GetInstance();
      ASSERT_TRUE(window_manager->GetChildWebContents());
      EXPECT_FALSE(window_manager->GetChildWebContents()
                       ->GetRenderWidgetHostView()
                       ->HasFocus());
    }

    // Switch back to the original tab.  This will verify that pip has closed.
    SwitchBackToOpenerAndWaitForPipToClose();
  }

  void SwitchToNewTabAndDontExpectAutopip() {
    auto* opener_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto* tab_helper =
        AutoPictureInPictureTabHelper::FromWebContents(opener_web_contents);

    // There should not currently be a picture-in-picture window.
    EXPECT_FALSE(opener_web_contents->HasPictureInPictureVideo());
    EXPECT_FALSE(opener_web_contents->HasPictureInPictureDocument());
    // The tab helper should not report that we are, or would be, in auto-pip.
    EXPECT_FALSE(tab_helper->AreAutoPictureInPicturePreconditionsMet());
    EXPECT_FALSE(tab_helper->IsInAutoPictureInPicture());

    // Open and switch to a new tab.
    OpenNewTab(browser());

    // The tab helper should indicate that we are not in pip.
    EXPECT_FALSE(tab_helper->IsInAutoPictureInPicture());
    // Preconditions should remain unmet.
    EXPECT_FALSE(tab_helper->AreAutoPictureInPicturePreconditionsMet());

    // Verify that we did not enter pip.
    EXPECT_FALSE(opener_web_contents->HasPictureInPictureVideo());
    EXPECT_FALSE(opener_web_contents->HasPictureInPictureDocument());
  }

  void SetContentSetting(content::WebContents* web_contents,
                         ContentSetting content_setting) {
    GURL url = web_contents->GetLastCommittedURL();
    HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()))
        ->SetContentSettingDefaultScope(
            url, url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
            content_setting);
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

    input::NativeWebKeyboardEvent keyboard_event(
        blink::WebInputEvent::Type::kChar, /*modifiers=*/0,
        base::TimeTicks::Now());
    rwh->ForwardKeyboardEvent(keyboard_event);

    blink::WebGestureEvent gesture_event(
        blink::WebGestureEvent::Type::kGestureTap, /*modifiers=*/0,
        base::TimeTicks::Now(), blink::mojom::GestureDevice::kTouchpad);
    rwh->ForwardGestureEvent(gesture_event);

    rwh->RemoveInputEventObserver(&input_observer);
  }

  void PerformMouseClickOnButton(views::Button* button) {
    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  content::VideoPictureInPictureWindowController* window_controller(
      content::WebContents* web_contents) {
    return content::VideoPictureInPictureWindowController::
        GetOrCreateVideoPictureInPictureController(web_contents);
  }

  AutoPipSettingOverlayView* GetOverlayViewFromVideoPipWindow() {
    // Get the video pip window controller and video overlay window.
    auto* opener_web_contents =
        PictureInPictureWindowManager::GetInstance()->GetWebContents();
    auto* const video_pip_window_controller =
        window_controller(opener_web_contents);
    auto* const video_overlay_window = static_cast<VideoOverlayWindowViews*>(
        video_pip_window_controller->GetWindowForTesting());
    return video_overlay_window->get_overlay_view_for_testing();
  }

  // Return the settings overlay view, or null if there isn't one.  There must
  // be a pip window, though.
  AutoPipSettingOverlayView* GetOverlayViewFromDocumentPipWindow() {
    auto* pip_contents =
        PictureInPictureWindowManager::GetInstance()->GetChildWebContents();

    // Fetch the overlay view from the browser window.
    auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
        pip_contents->GetTopLevelNativeWindow());
    auto* pip_frame_view = static_cast<PictureInPictureBrowserFrameView*>(
        browser_view->frame()->GetFrameView());
    auto* overlay_view =
        pip_frame_view->get_auto_pip_setting_overlay_view_for_testing();

    return overlay_view;
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
    features.push_back(blink::features::kAutoPictureInPictureVideoHeuristics);
    return features;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       OpensAndClosesVideoAutopip) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

// TODO(crbug.com/335630150): Flaky.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DISABLED_OpensAndClosesDocumentAutopip) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotVideoAutopip_VideoNotSufficientlyVisible) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoVisibilityPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);

  AddOverlayToVideo(web_contents, /*should_occlude*/ true);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SwitchToNewTabAndDontExpectAutopip();
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotDocumentAutopip_VideoNotSufficientlyVisible) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentVideoVisibilityPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);

  AddOverlayToVideo(web_contents, /*should_occlude*/ true);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SwitchToNewTabAndDontExpectAutopip();
}

IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureWithVideoPlaybackBrowserTest,
    DoesNotDocumentAutopip_VideoSufficientlyVisibleInIframe) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentVideoVisibilityPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Move the video element into an iframe.
  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), R"(
      const iframe = document.createElement("iframe");
      iframe.style.width = "100%";
      iframe.style.height = "100%";
      iframe.style.position = "absolute";

      document.body.appendChild(iframe);
      iframe.contentDocument.body.appendChild(video);
  )"));

  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);

  // There should not be a picture-in-picture window, since the video element is
  // inside an iframe.
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SwitchToNewTabAndDontExpectAutopip();
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       OpensAndClosesVideoAutopip_VideoSufficientlyVisible) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoVisibilityPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);

  AddOverlayToVideo(web_contents, /*should_occlude*/ false);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       OpensAndClosesVideoAutopip_NonVisibleElementIgnored) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoVisibilityPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);

  // Add occluding overlay to video, followed by making the occluding overlay
  // not visible.
  AddOverlayToVideo(web_contents, /*should_occlude*/ true);
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      u"makeOccludingOverlayInvisible()", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);

  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotVideoAutopip_NotRecentlyAudible) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  ForceLifecycleUpdate(web_contents);

  // Wait for video to be recently audible, to ensure we start from a known
  // state. Then mute audio and wait for video to not be recently audible.
  WaitForWasRecentlyAudible(web_contents, /*expected_recently_audible=*/true);
  MuteVideo(web_contents);
  WaitForWasRecentlyAudible(web_contents, /*expected_recently_audible=*/false);

  SwitchToNewTabAndDontExpectAutopip();
}

// TODO(crbug.com/40923043): Flaky on "Linux ASan LSan Tests (1)"
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_OpensAndClosesDocumentAutopip_VideoSufficientlyVisible \
  DISABLED_OpensAndClosesDocumentAutopip_VideoSufficientlyVisible
#else
#define MAYBE_OpensAndClosesDocumentAutopip_VideoSufficientlyVisible \
  OpensAndClosesDocumentAutopip_VideoSufficientlyVisible
#endif
IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureWithVideoPlaybackBrowserTest,
    MAYBE_OpensAndClosesDocumentAutopip_VideoSufficientlyVisible) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentVideoVisibilityPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);

  AddOverlayToVideo(web_contents, /*should_occlude*/ false);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);
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

#if BUILDFLAG(IS_LINUX)
#define MAYBE_OverlaySettingViewIsShownForDocumentPip \
  DISABLED_OverlaySettingViewIsShownForDocumentPip
#else
#define MAYBE_OverlaySettingViewIsShownForDocumentPip \
  OverlaySettingViewIsShownForDocumentPip
#endif
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       MAYBE_OverlaySettingViewIsShownForDocumentPip) {
  auto* window_manager = PictureInPictureWindowManager::GetInstance();

  LoadCameraMicrophonePage(browser());
  auto* opener_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(opener_contents);
  {
    content::MediaStartStopObserver enter_pip_observer(
        opener_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();
    // NOTE: this does not guarantee that pip is opened, only that the
    // mediasession action has been sent.
  }

  // Fetch the overlay view from the browser window.
  auto* pip_contents = window_manager->GetChildWebContents();
  auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      pip_contents->GetTopLevelNativeWindow());
  ASSERT_TRUE(browser_view);
  auto* pip_frame_view = static_cast<PictureInPictureBrowserFrameView*>(
      browser_view->frame()->GetFrameView());
  ASSERT_TRUE(pip_frame_view);
  auto* overlay_view = GetOverlayViewFromDocumentPipWindow();
  // The overlay should be shown.
  ASSERT_TRUE(overlay_view);

  // Verify that input has been blocked.
  CheckIfEventsAreForwarded(pip_contents, /*expect_events=*/false);

  // Verify that acknowledging the helper restores it.
  overlay_view->get_view_for_testing()->simulate_button_press_for_testing(
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
  LOG(ERROR) << "DEBUG: loading video page";
  LoadAutoVideoPipPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  LOG(ERROR) << "DEBUG: starting playback";
  PlayVideo(original_web_contents);
  LOG(ERROR) << "DEBUG: waiting for audio focus";
  WaitForAudioFocusGained();
  LOG(ERROR) << "DEBUG: waiting for playing";
  WaitForMediaSessionPlaying(original_web_contents);
  WaitForWasRecentlyAudible(original_web_contents);

  // Pause the video.
  LOG(ERROR) << "DEBUG: pausing playback";
  PauseVideo(original_web_contents);
  LOG(ERROR) << "DEBUG: waiting for mediasession pause";
  WaitForMediaSessionPaused(original_web_contents);

  // There should not currently be a picture-in-picture window.
  LOG(ERROR) << "DEBUG: checking in on expectations";
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  LOG(ERROR) << "DEBUG: opening a new tab";
  OpenNewTab(browser());

  // There should not be a picture-in-picture window.
  LOG(ERROR) << "DEBUG: again checking in on expectations";
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

// TODO(crbug.com/335565116): Re-enable this test.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_OverlaySettingViewIsShownForVideoPip \
  DISABLED_OverlaySettingViewIsShownForVideoPip
#else
#define MAYBE_OverlaySettingViewIsShownForVideoPip \
  OverlaySettingViewIsShownForVideoPip
#endif
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       MAYBE_OverlaySettingViewIsShownForVideoPip) {
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);

  {
    // Open and switch to a new tab.
    content::MediaStartStopObserver enter_pip_observer(
        web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();
  }

  // Make sure that the overlay view is shown.
  EXPECT_TRUE(GetOverlayViewFromVideoPipWindow());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       OverlayViewRemovedWhenHidden) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);

  // Set content setting to CONTENT_SETTING_ASK.
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SetContentSetting(original_web_contents, CONTENT_SETTING_ASK);
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(original_web_contents);

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
  // The tab helper should not report that we are, or would be, in auto-pip.
  EXPECT_FALSE(tab_helper->AreAutoPictureInPicturePreconditionsMet());
  EXPECT_FALSE(tab_helper->IsInAutoPictureInPicture());

  {
    // Open and switch to a new tab.
    content::MediaStartStopObserver enter_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();
  }

  // A picture-in-picture window of the correct type should automatically
  // open.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureVideo());

  auto* const video_overlay_view = GetOverlayViewFromVideoPipWindow();

  // Get AutoPipSettingView "allow once" button and click it.
  auto* allow_once_button = views::Button::AsButton(
      video_overlay_view->get_view_for_testing()
          ->GetWidget()
          ->GetContentsView()
          ->GetViewByID(
              static_cast<int>(AutoPipSettingView::UiResult::kAllowOnce)));
  PerformMouseClickOnButton(allow_once_button);

  // Verify that the AutoPipSettingOverlay view has been removed.
  EXPECT_EQ(nullptr, GetOverlayViewFromVideoPipWindow());
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
  SwitchToExistingTab(original_web_contents);

  // The pip window should still be open.
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());
}

// TODO(https://crbug.com/371850487): failing on Windows ASAN.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_ShowsMostRecentlyHiddenTab DISABLED_ShowsMostRecentlyHiddenTab
#else
#define MAYBE_ShowsMostRecentlyHiddenTab ShowsMostRecentlyHiddenTab
#endif
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       MAYBE_ShowsMostRecentlyHiddenTab) {
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

  // Switch back to the original tab.  Since the original tab is in autopip,
  // switching back to it should allow the second tab's autopip to replace it.
  {
    content::MediaStartStopObserver exit_pip_observer(
        original_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    content::MediaStartStopObserver enter_pip_observer(
        second_web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    SwitchToExistingTab(original_web_contents);
    exit_pip_observer.Wait();
    enter_pip_observer.Wait();
  }

  // The second tab should now be in picture-in-picture.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
  EXPECT_TRUE(second_web_contents->HasPictureInPictureDocument());

  // Open a third tab.
  // Nothing should change, because leaving the original page while the second
  // page's autopip window is open will not replace the second page's autopip
  // window unless the second page is becoming active.
  OpenNewTab(browser());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
  EXPECT_TRUE(second_web_contents->HasPictureInPictureDocument());

  // Switch back to the second tab.
  {
    content::MediaStartStopObserver exit_pip_observer(
        second_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    SwitchToExistingTab(second_web_contents);
    exit_pip_observer.Wait();
  }

  // Nothing should be in picture-in-picture.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
  EXPECT_FALSE(second_web_contents->HasPictureInPictureDocument());
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_DoesNotAutopipWhenSwitchingToADifferentWindow \
  DISABLED_DoesNotAutopipWhenSwitchingToADifferentWindow
#else
#define MAYBE_DoesNotAutopipWhenSwitchingToADifferentWindow \
  DoesNotAutopipWhenSwitchingToADifferentWindow
#endif
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       MAYBE_DoesNotAutopipWhenSwitchingToADifferentWindow) {
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
  SetContentSetting(original_web_contents, CONTENT_SETTING_BLOCK);

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
  SwitchToExistingTab(original_web_contents);

  // There should still be no picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Re-enable the content setting.
  SetContentSetting(original_web_contents, CONTENT_SETTING_ALLOW);

  // Switch back to the second tab.
  content::MediaStartStopObserver enter_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  SwitchToExistingTab(second_web_contents);
  enter_pip_observer.Wait();

  // A picture-in-picture window should automatically open.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());

  // Switch back to the original tab.
  content::MediaStartStopObserver exit_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kExitPictureInPicture);
  SwitchToExistingTab(original_web_contents);
  exit_pip_observer.Wait();

  // There should no longer be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       ContentSettingAskIsBlockForIncognito) {
  // Load a page that registers for autopip.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  LoadCameraMicrophonePage(incognito_browser);
  auto* original_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(original_web_contents);

  // There should not currently be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Open and switch to a new tab.
  OpenNewTab(incognito_browser);
  auto* second_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  // There should not be a picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Switch back to the original tab.
  incognito_browser->tab_strip_model()->ActivateTabAt(
      incognito_browser->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));

  // There should still be no picture-in-picture window.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_FALSE(original_web_contents->HasPictureInPictureDocument());

  // Explicitly enable the content setting.
  SetContentSetting(original_web_contents, CONTENT_SETTING_ALLOW);

  // Switch back to the second tab.
  content::MediaStartStopObserver enter_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  incognito_browser->tab_strip_model()->ActivateTabAt(
      incognito_browser->tab_strip_model()->GetIndexOfWebContents(
          second_web_contents));
  enter_pip_observer.Wait();

  // A picture-in-picture window should automatically open.
  EXPECT_FALSE(original_web_contents->HasPictureInPictureVideo());
  EXPECT_TRUE(original_web_contents->HasPictureInPictureDocument());

  // Switch back to the original tab.
  content::MediaStartStopObserver exit_pip_observer(
      original_web_contents,
      content::MediaStartStopObserver::Type::kExitPictureInPicture);
  incognito_browser->tab_strip_model()->ActivateTabAt(
      incognito_browser->tab_strip_model()->GetIndexOfWebContents(
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
  WaitForMediaSessionPlaying(original_web_contents);

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
  SwitchToExistingTab(original_web_contents);

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

// TODO(crbug.com/331493435): Test is flaky.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DISABLED_AllowOncePersistsUntilNavigation) {
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  ForceLifecycleUpdate(web_contents);

  // Set content setting to CONTENT_SETTING_ASK.
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SetContentSetting(original_web_contents, CONTENT_SETTING_ASK);

  SwitchToNewTabAndWaitForAutoPip();

  // Get AutoPipSettingView "allow once" button and click it.
  ASSERT_TRUE(original_web_contents->HasPictureInPictureVideo());
  auto* const video_overlay_view = GetOverlayViewFromVideoPipWindow();
  auto* allow_once_button = views::Button::AsButton(
      video_overlay_view->get_view_for_testing()
          ->GetWidget()
          ->GetContentsView()
          ->GetViewByID(
              static_cast<int>(AutoPipSettingView::UiResult::kAllowOnce)));
  PerformMouseClickOnButton(allow_once_button);

  // Verify that the AutoPipSettingOverlay view has been removed.
  EXPECT_EQ(nullptr, GetOverlayViewFromVideoPipWindow());

  // Switch back to the opener to close pip, then switch to a new tab to verify
  // that the permission prompt doesn't reappear.
  SwitchBackToOpenerAndWaitForPipToClose();
  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_EQ(nullptr, GetOverlayViewFromVideoPipWindow());

  // Switch back, navigate, and verify that the prompt reappears.
  SwitchBackToOpenerAndWaitForPipToClose();
  LoadAutoVideoPipPage(browser());
  ASSERT_EQ(web_contents, browser()->tab_strip_model()->GetActiveWebContents());
  ResetAudioFocusObserver();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  ForceLifecycleUpdate(web_contents);
  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_NE(nullptr, GetOverlayViewFromVideoPipWindow());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       ForwardsWindowCloseToSettingHelper) {
  LoadCameraMicrophonePage(browser());
  auto* opener_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(browser()->tab_strip_model()->GetActiveWebContents());

  // Set content setting to CONTENT_SETTING_ASK.
  SetContentSetting(opener_web_contents, CONTENT_SETTING_ASK);

  // Replace the blocker so that we can check if `RecordDismissAndEmbargo` is
  // called.  It'd be nice if we could check that the right call is made to the
  // setting helper instead, but this is simpler.  Anyway, also configure this
  // to have no embargo.
  MockAutoBlocker auto_blocker;
  EXPECT_CALL(auto_blocker,
              IsEmbargoed(_, ContentSettingsType::AUTO_PICTURE_IN_PICTURE))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(auto_blocker, RecordDismissAndEmbargo(_, _, _)).Times(1);

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(opener_web_contents);
  tab_helper->set_auto_blocker_for_testing(&auto_blocker);

  // Open document pip.
  SwitchToNewTabAndWaitForAutoPip();

  auto* window_manager = PictureInPictureWindowManager::GetInstance();

  // Close the window and verify that the setting helper found out.
  {
    content::MediaStartStopObserver exit_pip_observer(
        opener_web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    window_manager->ExitPictureInPictureViaWindowUi(
        PictureInPictureWindowManager::UiBehavior::kCloseWindowOnly);
    exit_pip_observer.Wait();
  }

  tab_helper->set_auto_blocker_for_testing(nullptr);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       MediaVideoVisibilityTrackerHistogramSamplesHaveCount) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoVisibilityPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  base::HistogramTester histograms;
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);

  AddOverlayToVideo(web_contents, /*should_occlude*/ true);
  ForceLifecycleUpdate(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SwitchToNewTabAndDontExpectAutopip();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  const char* const histogram_names[] = {
      "Media.MediaVideoVisibilityTracker.ComputeOcclusion.ComputeOccludingArea."
      "TotalDuration",
      "Media.MediaVideoVisibilityTracker.ComputeOcclusion.TotalDuration",
      "Media.MediaVideoVisibilityTracker.GetClientIdsSet.ItemsInSetCount."
      "TotalCount",
      "Media.MediaVideoVisibilityTracker.GetClientIdsSet.NotContentType."
      "Percentage",
      "Media.MediaVideoVisibilityTracker.GetClientIdsSet.NotContentTypeCount."
      "TotalCount",
      "Media.MediaVideoVisibilityTracker.GetClientIdsSet.SetConstruction."
      "TotalDuration",
      "Media.MediaVideoVisibilityTracker."
      "HitTestedNodesContributingToOcclusionCount.ExponentialHistogram."
      "TotalCount",
      "Media.MediaVideoVisibilityTracker."
      "HitTestedNodesContributingToOcclusionCount.LinearHistogram.TotalCount",
      "Media.MediaVideoVisibilityTracker.HitTestedNodesCount."
      "ExponentialHistogram.TotalCount",
      "Media.MediaVideoVisibilityTracker.HitTestedNodesCount.LinearHistogram."
      "TotalCount",
      "Media.MediaVideoVisibilityTracker.IgnoredNodesNotOpaque.Percentage",
      "Media.MediaVideoVisibilityTracker.IgnoredNodesNotOpaqueCount."
      "ExponentialHistogram.TotalCount",
      "Media.MediaVideoVisibilityTracker.IgnoredNodesNotOpaqueCount."
      "LinearHistogram.TotalCount",
      "Media.MediaVideoVisibilityTracker.IgnoredNodesUserAgentShadowRoot."
      "Percentage",
      "Media.MediaVideoVisibilityTracker.IgnoredNodesUserAgentShadowRootCount."
      "ExponentialHistogram.TotalCount",
      "Media.MediaVideoVisibilityTracker.IgnoredNodesUserAgentShadowRootCount."
      "LinearHistogram.TotalCount",
      "Media.MediaVideoVisibilityTracker.NodesContributingToOcclusion."
      "Percentage",
      "Media.MediaVideoVisibilityTracker.OccludingRectsCount."
      "ExponentialHistogram.TotalCount",
      "Media.MediaVideoVisibilityTracker.OccludingRectsCount.LinearHistogram."
      "TotalCount",
      "Media.MediaVideoVisibilityTracker.UpdateTime.TotalDuration",
  };

  for (const auto* histogram_name : histogram_names) {
    auto samples = histograms.GetHistogramSamplesSinceCreation(histogram_name);
    EXPECT_GE(samples->TotalCount(), 1);
  }
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       DoesNotCloseAutomaticallyOpenedPip) {
  // Load a page that registers for autopip.
  LoadCameraMicrophonePage(browser());
  auto* first_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(first_web_contents);

  // Load a second page that registers for autopip.  This should trigger autopip
  // from `first_web_contents`.
  OpenNewTab(browser());
  WaitForAutoPip(first_web_contents);
  LoadCameraMicrophonePage(browser());
  auto* second_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(second_web_contents);

  // Open a third page.  The pip window from `first_web_contents` should stay
  // open, since autopip should not overwrite another autopip instance.
  OpenNewTab(browser());
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance()->GetWebContents(),
            first_web_contents);

  // Switch back to the second tab.  Nothing should change.
  SwitchToExistingTab(second_web_contents);

  // Switch back to the original tab.  Since this is the pip owner, it should
  // close and the second tab should autopip.
  SwitchToExistingTab(first_web_contents);
  WaitForAutoPip(second_web_contents);
  EXPECT_TRUE(second_web_contents->HasPictureInPictureDocument());

  // Now switch to the second tab and verify that it works that way too.  This
  // is important, since the tab strip observers for the two tabs could be
  // triggered in either order.
  SwitchToExistingTab(second_web_contents);
  WaitForAutoPip(first_web_contents);
  EXPECT_TRUE(first_web_contents->HasPictureInPictureDocument());
}
