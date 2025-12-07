// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"

#include <memory>

#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/media/mock_media_engagement_service.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "media/base/picture_in_picture_events_info.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::AtLeast;
using testing::Return;

namespace {

using UkmEntry = ukm::builders::
    Media_AutoPictureInPicture_EnterPictureInPicture_AutomaticReason_PromptResultV2;
using PromptResult = AutoPipSettingHelper::PromptResult;

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

constexpr char kIframeAutoDocumentMediaPlaybackPipPage[] =
    "/media/picture-in-picture/iframe-autopip-document-media-playback.html";

const char kVideoConferencingHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
    "VideoConferencing.PromptResultV2";

const char kMediaPlaybackHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
    "MediaPlayback.PromptResultV2";

const char kVideoConferencingTotalTimeHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
    "VideoConferencing.TotalTime";

const char kMediaPlaybackTotalTimeHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
    "MediaPlayback.TotalTime";

const char kVideoConferencingTotalTimeForSessionHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
    "VideoConferencing.TotalTimeForSession";

const char kMediaPlaybackTotalTimeForSessionHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
    "MediaPlayback.TotalTimeForSession";

const char kMediaPlaybackTotalPlaybackTimeHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
    "MediaPlayback.TotalPlaybackTime";

const char kMediaPlaybackPlaybackToTotalTimeRatioHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
    "MediaPlayback.PlaybackToTotalTimeRatio";

const char kBrowserInitiatedHistogram[] =
    "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
    "BrowserInitiated.PromptResultV2";

class MockInputObserver : public content::RenderWidgetHost::InputEventObserver {
 public:
  MOCK_METHOD(void,
              OnInputEvent,
              (const content::RenderWidgetHost& widget,
               const blink::WebInputEvent&),
              (override));
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

class MockContentBrowserClient : public ChromeContentBrowserClient {
 public:
  MOCK_METHOD(media::PictureInPictureEventsInfo::AutoPipInfo,
              GetAutoPipInfo,
              (const content::WebContents& web_contents),
              (const, override));
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

// Helper class to wait for DevTools to receive auto picture in picture events
// information.
class AutoPipInfoDevToolsWaiter : public content::DevToolsInspectorLogWatcher::
                                      DevToolsInspectorLogWatcherObserver {
 public:
  explicit AutoPipInfoDevToolsWaiter(
      content::DevToolsInspectorLogWatcher* log_watcher) {
    auto_pip_dev_tools_waiter_observation_.Observe(log_watcher);
  }
  AutoPipInfoDevToolsWaiter(const AutoPipInfoDevToolsWaiter&) = delete;
  AutoPipInfoDevToolsWaiter(AutoPipInfoDevToolsWaiter&&) = delete;
  AutoPipInfoDevToolsWaiter& operator=(const AutoPipInfoDevToolsWaiter&) =
      delete;

  void WaitUntilDone() {
    if (auto_pip_event_info_set_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void OnLastAutoPipEventInfoSet() override {
    auto_pip_event_info_set_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
    auto_pip_dev_tools_waiter_observation_.Reset();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool auto_pip_event_info_set_ = false;
  base::ScopedObservation<
      content::DevToolsInspectorLogWatcher,
      content::DevToolsInspectorLogWatcher::DevToolsInspectorLogWatcherObserver>
      auto_pip_dev_tools_waiter_observation_{this};
};

// Simulates clicking on the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ui::test::TestEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  ASSERT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Helper function to find a label with specific text in a view hierarchy.
bool FindLabelWithText(const views::View* view, const std::u16string& text) {
  if (auto* label = views::AsViewClass<const views::Label>(view)) {
    if (label->GetText() == text) {
      return true;
    }
  }

  for (const views::View* child : view->children()) {
    if (FindLabelWithText(child, text)) {
      return true;
    }
  }

  return false;
}

bool IsPermissionPresentInPageInfo(ContentSettingsType type) {
  auto* bubble = views::AsViewClass<PageInfoBubbleView>(
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
  if (!bubble) {
    return false;
  }

  const auto* permissions_view = bubble->GetViewByID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_PERMISSION_VIEW);
  if (!permissions_view) {
    return false;
  }

  const std::u16string permission_name =
      PageInfoUI::PermissionTypeToUIString(type);
  for (const views::View* view : permissions_view->children()) {
    if (FindLabelWithText(view, permission_name)) {
      return true;
    }
  }

  return false;
}

// Helper class to wait for widget bound updates.
class WidgetBoundsChangeWaiter : public views::WidgetObserver {
 public:
  enum class Comparison { kIsEqual, kIsDifferent };

  explicit WidgetBoundsChangeWaiter(views::Widget* widget,
                                    Comparison comparison)
      : comparison_(comparison) {
    if (widget) {
      observation_.Observe(widget);
    }
  }

  // Waits for the widget bounds to meet the comparison criteria with
  // `reference_bounds`.
  void Wait(const gfx::Rect& reference_bounds) {
    views::Widget* widget = observation_.GetSource();
    if (!widget) {
      return;
    }

    reference_bounds_ = reference_bounds;

    if (IsConditionMet(widget->GetWindowBoundsInScreen())) {
      return;
    }

    CHECK(bounds_future_.Wait()) << "Waiting for value timed out.";
  }

 private:
  bool IsConditionMet(const gfx::Rect& new_bounds) const {
    CHECK(reference_bounds_.has_value());
    return (comparison_ == Comparison::kIsEqual)
               ? (new_bounds == *reference_bounds_)
               : (new_bounds != *reference_bounds_);
  }

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (reference_bounds_.has_value() && IsConditionMet(new_bounds) &&
        !bounds_future_.IsReady()) {
      bounds_future_.SetValue();
    }
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    // If the widget is destroyed before the expected bounds are met, stop
    // waiting.
    if (!bounds_future_.IsReady()) {
      bounds_future_.SetValue();
    }
    observation_.Reset();
  }

  base::test::TestFuture<void> bounds_future_;
  const Comparison comparison_;
  std::optional<gfx::Rect> reference_bounds_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
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
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(GetEnabledFeatures(),
                                          GetDisabledFeatures());
    InProcessBrowserTest::SetUp();
  }

  void LoadAutoVideoPipPage(Browser* browser) {
    GURL test_page_url = chrome_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutoVideoPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutoDocumentPipPage(Browser* browser,
                               std::string_view hostname = {}) {
    GURL test_page_url;
    if (hostname.empty()) {
      test_page_url = chrome_test_utils::GetTestUrl(
          base::FilePath(base::FilePath::kCurrentDirectory),
          base::FilePath(kAutoDocumentPipPage));
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
      return;
    }

    ASSERT_TRUE(embedded_https_test_server().Start());
    test_page_url = embedded_https_test_server().GetURL(
        hostname, base::FilePath(FILE_PATH_LITERAL("/"))
                      .Append(kAutoDocumentPipPage)
                      .MaybeAsASCII());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadIframeAutoDocumentMediaPlaybackPipPage(Browser* browser) {
    ASSERT_TRUE(embedded_https_test_server().Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, embedded_https_test_server().GetURL(
                     "a.com", kIframeAutoDocumentMediaPlaybackPipPage)));
  }

  void LoadCameraMicrophonePage(Browser* browser,
                                std::string_view hostname = {}) {
    GURL test_page_url;
    if (hostname.empty()) {
      test_page_url = chrome_test_utils::GetTestUrl(
          base::FilePath(base::FilePath::kCurrentDirectory),
          base::FilePath(kCameraPage));
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
      return;
    }

    ASSERT_TRUE(embedded_https_test_server().Start());
    test_page_url = embedded_https_test_server().GetURL(
        hostname, base::FilePath(FILE_PATH_LITERAL("/"))
                      .Append(kCameraPage)
                      .MaybeAsASCII());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadNotRegisteredPage(Browser* browser) {
    GURL test_page_url = chrome_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kNotRegisteredPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutopipDelayPage(Browser* browser) {
    GURL test_page_url = chrome_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutopipDelayPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void LoadAutopipToggleRegistrationPage(Browser* browser) {
    GURL test_page_url = chrome_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kAutopipToggleRegistrationPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));
  }

  void OpenNewTab(Browser* browser) {
    GURL test_page_url = chrome_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kBlankPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, test_page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  void OpenPopUp(Browser* browser) {
    GURL test_page_url = chrome_test_utils::GetTestUrl(
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

  void PlayIframeVideo(content::RenderFrameHost* rfh) {
    ASSERT_TRUE(ExecJs(rfh, R"(
        playVideo();
      )"));
  }

  void PauseVideo(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"pauseVideo()", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
  }

  // Clicks the button to select the document Picture-in-Picture type.
  // "select-document-pip" is the ID of the button element in `kCameraPage`.
  // This type is selected by default, so calling this is mainly for test
  // clarity.
  void SetPiPTypeToDocument(content::WebContents* web_contents) {
    ASSERT_TRUE(
        ExecJs(web_contents,
               "document.getElementById('select-document-pip').click();"));
  }

  // Clicks the button to select the video Picture-in-Picture type.
  // "select-video-pip" is the ID of the button element in `kCameraPage`.
  void SetPiPTypeToVideo(content::WebContents* web_contents) {
    ASSERT_TRUE(ExecJs(web_contents,
                       "document.getElementById('select-video-pip').click();"));
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

  void MuteVideo(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"muteVideo()", base::NullCallback(),
            content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void TriggerNewPlayerCreation(content::WebContents* web_contents) {
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"triggerNewPlayerCreation()", base::NullCallback(),
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

  void WaitForMediaSessionCameraState(
      content::WebContents* web_contents,
      media_session::mojom::CameraState wanted_state) {
    media_session::test::MockMediaSessionMojoObserver observer(
        *content::MediaSession::Get(web_contents));
    observer.WaitForCameraState(wanted_state);
    // Flush so that the tab helper has also found out about this.
    content::MediaSession::FlushObserversForTesting(web_contents);
  }

  void WaitForMediaSessionMicrophoneState(
      content::WebContents* web_contents,
      media_session::mojom::MicrophoneState wanted_state) {
    media_session::test::MockMediaSessionMojoObserver observer(
        *content::MediaSession::Get(web_contents));
    observer.WaitForMicrophoneState(wanted_state);
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
        audible_helper->RegisterRecentlyAudibleChangedCallback(
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
    EXPECT_CALL(input_observer, OnInputEvent(_, _))
        .Times(expect_events ? 4 : 0);

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
        browser_view->browser_widget()->GetFrameView());
    auto* overlay_view =
        pip_frame_view->get_auto_pip_setting_overlay_view_for_testing();

    return overlay_view;
  }

  void CheckPromptResultUkmRecorded(GURL url,
                                    std::string_view expected_metric_name,
                                    PromptResult expected_prompt_result) {
    const auto& entries =
        ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName);
    size_t found_count = 0u;
    const ukm::mojom::UkmEntry* last_entry = nullptr;
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* source =
          ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (!source || source->url() != url) {
        continue;
      }
      if (!ukm_recorder()->EntryHasMetric(entry, expected_metric_name)) {
        continue;
      }
      found_count++;
      last_entry = entry;
    }
    ASSERT_NE(nullptr, last_entry);

    const int64_t* metric =
        ukm::TestUkmRecorder::GetEntryMetric(last_entry, expected_metric_name);
    ASSERT_TRUE(metric);
    EXPECT_EQ(static_cast<int>(expected_prompt_result), *metric);
    EXPECT_EQ(1u, found_count);
  }

  void CheckPromptResultUkmMetricNotRecorded(
      GURL url,
      std::string_view not_expected_metric_name) {
    const auto& entries =
        ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* source =
          ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (!source || source->url() != url) {
        continue;
      }
      ASSERT_FALSE(
          ukm_recorder()->EntryHasMetric(entry, not_expected_metric_name));
    }
  }

  media::PictureInPictureEventsInfo::AutoPipReason GetAutoPipReason(
      const content::WebContents& web_contents) {
    return content::GetContentClientForTesting()
        ->browser()
        ->GetAutoPipInfo(web_contents)
        .auto_pip_reason;
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

 protected:
  virtual std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    return {blink::features::kDocumentPictureInPictureAPI};
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {blink::features::kBrowserInitiatedAutomaticPictureInPicture};
  }

 private:
  std::unique_ptr<media_session::test::TestAudioFocusObserver>
      audio_focus_observer_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

class AutoPictureInPictureWithVideoPlaybackBrowserTest
    : public AutoPictureInPictureTabHelperBrowserTest {
 public:
  AutoPictureInPictureWithVideoPlaybackBrowserTest()
      : safe_browsing_factory_(
            std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>()),
        dependency_manager_subscription_(
            BrowserContextDependencyManager::GetInstance()
                ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                    &AutoPictureInPictureWithVideoPlaybackBrowserTest::
                        SetTestingFactory,
                    // base::Unretained() is safe because `this` outlives the
                    // dependency manager subscription.
                    base::Unretained(this)))) {}

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

  void AddDangerousUrl(const GURL& dangerous_url) {
    fake_safe_browsing_database_manager_->AddDangerousUrl(
        dangerous_url,
        safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  }

  void ClearDangerousUrl(const GURL& dangerous_url) {
    fake_safe_browsing_database_manager_->ClearDangerousUrl(dangerous_url);
  }

  MediaEngagementService* GetMediaEngagementService() const {
    return MediaEngagementServiceFactory::GetForProfile(browser()->profile());
  }

  void SetExpectedHasHighEngagement(bool has_high_engagenent) const {
    auto* mock_media_engagement_service =
        static_cast<MockMediaEngagementService*>(GetMediaEngagementService());
    EXPECT_CALL(*mock_media_engagement_service, HasHighEngagement(testing::_))
        .WillRepeatedly(testing::Return(has_high_engagenent));
  }

 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    fake_safe_browsing_database_manager_ =
        base::MakeRefCounted<safe_browsing::FakeSafeBrowsingDatabaseManager>(
            content::GetUIThreadTaskRunner({}));
    safe_browsing_factory_->SetTestDatabaseManager(
        fake_safe_browsing_database_manager_.get());
    safe_browsing::SafeBrowsingService::RegisterFactory(
        safe_browsing_factory_.get());
  }

  void SetTestingFactory(content::BrowserContext* context) {
    MediaEngagementServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockMediaEngagementService));
  }

 private:
  scoped_refptr<safe_browsing::FakeSafeBrowsingDatabaseManager>
      fake_safe_browsing_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
  base::CallbackListSubscription dependency_manager_subscription_;
};

class BrowserInitiatedAutoPictureInPictureBrowserTest
    : public AutoPictureInPictureWithVideoPlaybackBrowserTest {
 public:
  BrowserInitiatedAutoPictureInPictureBrowserTest() = default;
  BrowserInitiatedAutoPictureInPictureBrowserTest(
      const BrowserInitiatedAutoPictureInPictureBrowserTest&) = delete;
  BrowserInitiatedAutoPictureInPictureBrowserTest& operator=(
      const BrowserInitiatedAutoPictureInPictureBrowserTest&) = delete;

  std::vector<base::test::FeatureRef> GetEnabledFeatures() override {
    auto features =
        AutoPictureInPictureWithVideoPlaybackBrowserTest::GetEnabledFeatures();
    features.push_back(
        blink::features::kBrowserInitiatedAutomaticPictureInPicture);
    return features;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {};
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
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

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
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesDocumentAutopip_VideoInLocalIframe) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
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
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // There should be a picture-in-picture window, since the video element is
  // inside a local iframe.
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotDocumentAutopip_VideoInRemoteIFrame) {
  // Load a page that registers for autopip.
  LoadIframeAutoDocumentMediaPlaybackPipPage(browser());

  // Get the render frame host for main_frame (a.com) and sub_frame (b.com).
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  auto* sub_frame = ChildFrameAt(main_frame, 0);

  // Register message handler for b.com to handle `playVideo` message actions.
  ASSERT_TRUE(ExecJs(sub_frame, R"(
      window.addEventListener('message', (event) => {
          console.log(`Received message: ${event.data.action}`);
          if(event.data.action == 'playVideo') {
            playVideo();
          }
      });
  )"));

  // Send a `postMessage` to b.com to start video playback.
  PlayIframeVideo(main_frame);

  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  // There should not be a picture-in-picture window, since the video element is
  // inside a remote iframe.
  SwitchToNewTabAndDontExpectAutopip();

  // Verify that `has_safe_url_` is false, since the video element is within a
  // remote iframe.
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  EXPECT_FALSE(tab_helper->has_safe_url_);

  // Verify that `MeetsMediaEngagementConditions` returns false (even though the
  // mock high engagement is set to return true), since the video element is
  // within a remote iframe.
  EXPECT_FALSE(tab_helper->MeetsMediaEngagementConditions());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotVideoAutopip_NotRecentlyAudible) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);

  // Wait for video to be recently audible, to ensure we start from a known
  // state. Then mute audio and wait for video to not be recently audible.
  WaitForWasRecentlyAudible(web_contents, /*expected_recently_audible=*/true);
  MuteVideo(web_contents);
  WaitForWasRecentlyAudible(web_contents, /*expected_recently_audible=*/false);

  SwitchToNewTabAndDontExpectAutopip();
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotVideoAutopip_DangerousURL) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  AddDangerousUrl(web_contents->GetLastCommittedURL());
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  SwitchToNewTabAndDontExpectAutopip();
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotVideoAutopip_FromSafeToDangerousURL) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // Expect AutoPiP since URL is safe.
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);

  // Do not expect AutoPiP since URL is unsafe.
  AddDangerousUrl(web_contents->GetLastCommittedURL());
  SwitchToNewTabAndDontExpectAutopip();
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesVideoAutopip_FromDangerousToSafeURL) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  AddDangerousUrl(web_contents->GetLastCommittedURL());
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // Do not expect AutoPiP since URL is unsafe.
  SwitchToNewTabAndDontExpectAutopip();
  SwitchToExistingTab(web_contents);

  // Expect AutoPiP since URL is safe.
  ClearDangerousUrl(web_contents->GetLastCommittedURL());
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotVideoAutopip_LowEngagementScore) {
  // Load a page, with HTTPS scheme, that registers for autopip and start video
  // playback.
  ASSERT_TRUE(embedded_https_test_server().Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(
                     "a.com", "/media/picture-in-picture/autopip-video.html")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents->GetLastCommittedURL().SchemeIs(url::kHttpsScheme));
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(false);
  WaitForWasRecentlyAudible(web_contents);

  SwitchToNewTabAndDontExpectAutopip();
}

IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureWithVideoPlaybackBrowserTest,
    DoesVideoAutopip_LowEngagementScoreAndUrlWithFileScheme) {
  // Load a page, with file scheme, that registers for autopip and start video
  // playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents->GetLastCommittedURL().SchemeIsFile());
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(false);
  WaitForWasRecentlyAudible(web_contents);

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureWithVideoPlaybackBrowserTest,
    DoesVideoAutopip_ContentSettingAllowAndLowEngagementScore) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);

  // Set content setting to allow and has high media engagement to false.
  SetContentSetting(web_contents, CONTENT_SETTING_ALLOW);
  SetExpectedHasHighEngagement(false);

  // Verify that we did enter pip, even though media engagement is low. This is
  // because media engagement is ignored when content setting is set to allow.
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       CanAutoDocPipWithCameraMicrophone) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  GetUserMediaAndAccept(browser()->tab_strip_model()->GetActiveWebContents());

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                        /*should_document_pip=*/true);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       CanAutoVideoPipWithCameraMicrophone) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  // Click the button to select the "video" PiP type.
  SetPiPTypeToVideo(web_contents);

  // Wait for the "VIDEO_PIP_READY" signal from the page indicating that the
  // video element has loaded metadata.
  const std::u16string pip_ready_title = u"VIDEO_PIP_READY";
  content::TitleWatcher title_watcher(web_contents, pip_ready_title);
  EXPECT_EQ(title_watcher.WaitAndGetTitle(), pip_ready_title);

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
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
  EXPECT_TRUE(tab_helper->IsEligibleForAutoPictureInPicture(
      /*should_record_blocking_metrics=*/false));
  OverrideURL(GURL("http://should.not.work.com"));
  EXPECT_FALSE(tab_helper->IsEligibleForAutoPictureInPicture(
      /*should_record_blocking_metrics=*/false));
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
      browser_view->browser_widget()->GetFrameView());
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

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       PromptResultRecorded_VideoConferencingAllowOnce) {
  // Load a page that registers for autopip and start video playback.
  LoadCameraMicrophonePage(browser(), "a.com");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  base::HistogramTester histograms;
  {
    content::MediaStartStopObserver enter_pip_observer(
        web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();
  }
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  auto* const overlay_view = GetOverlayViewFromDocumentPipWindow();
  overlay_view->get_view_for_testing()->simulate_button_press_for_testing(
      AutoPipSettingView::UiResult::kAllowOnce);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kVideoConferencingHistogram);

  // Verify metrics.
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<int>(PromptResult::kAllowOnce)));
  CheckPromptResultUkmRecorded(web_contents->GetLastCommittedURL(),
                               UkmEntry::kVideoConferencingName,
                               PromptResult::kAllowOnce);
}

IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureTabHelperBrowserTest,
    PromptResultRecorded_VideoConferencingNotShownAllowedOnEveryVisit) {
  // Load a page that registers for autopip and start video playback.
  LoadCameraMicrophonePage(browser(), "a.com");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(browser()->tab_strip_model()->GetActiveWebContents());
  SetContentSetting(web_contents, CONTENT_SETTING_ALLOW);

  base::HistogramTester histograms;
  {
    content::MediaStartStopObserver enter_pip_observer(
        web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();
  }
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kVideoConferencingHistogram);

  // Verify metrics.
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<int>(
                   PromptResult::kNotShownAllowedOnEveryVisit)));
  CheckPromptResultUkmRecorded(web_contents->GetLastCommittedURL(),
                               UkmEntry::kVideoConferencingName,
                               PromptResult::kNotShownAllowedOnEveryVisit);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       PromptResultRecorded_VideoConferencingNotShownBlocked) {
  // Load a page that registers for autopip and start video playback.
  LoadCameraMicrophonePage(browser(), "a.com");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(browser()->tab_strip_model()->GetActiveWebContents());
  SetContentSetting(web_contents, CONTENT_SETTING_BLOCK);

  base::HistogramTester histograms;
  OpenNewTab(browser());
  EXPECT_FALSE(web_contents->HasPictureInPictureDocument());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kVideoConferencingHistogram);

  // Verify metrics.
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(
      1, samples->GetCount(static_cast<int>(PromptResult::kNotShownBlocked)));
  CheckPromptResultUkmRecorded(web_contents->GetLastCommittedURL(),
                               UkmEntry::kVideoConferencingName,
                               PromptResult::kNotShownBlocked);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       PromptResultNotRecorded) {
  // Load a page that registers for autopip and do not starts using
  // camera/microphone.
  LoadCameraMicrophonePage(browser(), "a.com");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);
  EXPECT_FALSE(tab_helper->MeetsVideoPlaybackConditions());
  EXPECT_FALSE(tab_helper->IsUsingCameraOrMicrophone());

  // Check for autopip eligibility, and verify that no prompt results are
  // recorded.
  base::HistogramTester histograms;
  EXPECT_FALSE(tab_helper->IsEligibleForAutoPictureInPicture(
      /*should_record_blocking_metrics=*/true));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kVideoConferencingHistogram);

  // Verify metrics.
  EXPECT_EQ(0, samples->TotalCount());
  CheckPromptResultUkmMetricNotRecorded(web_contents->GetLastCommittedURL(),
                                        UkmEntry::kVideoConferencingName);
}

IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureTabHelperBrowserTest,
    PromptResultRecorded_VideoConferencingNotShownIncognito) {
  // Load a page that registers for autopip and start video playback.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  LoadCameraMicrophonePage(incognito_browser, "a.com");
  auto* web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(
      incognito_browser->tab_strip_model()->GetActiveWebContents());
  SetContentSetting(web_contents, CONTENT_SETTING_ASK);

  base::HistogramTester histograms;
  OpenNewTab(incognito_browser);
  EXPECT_FALSE(web_contents->HasPictureInPictureDocument());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kVideoConferencingHistogram);

  // Verify metrics.
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(
      1, samples->GetCount(static_cast<int>(PromptResult::kNotShownIncognito)));
  CheckPromptResultUkmMetricNotRecorded(web_contents->GetLastCommittedURL(),
                                        UkmEntry::kVideoConferencingName);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       VideoConferencingTotalTimeRecorded) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  // Trigger metric recording.
  base::HistogramTester histograms;
  SwitchToNewTabAndWaitForAutoPip();
  test_clock.Advance(base::Milliseconds(5000));
  SwitchBackToOpenerAndWaitForPipToClose();

  // Verify expectations.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples = histograms.GetHistogramSamplesSinceCreation(
      kVideoConferencingTotalTimeHistogram);
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(5000));
}

IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureTabHelperBrowserTest,
    ManuallyOpenedPip_VideoConferencingTotalTimeNotRecorded) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  base::HistogramTester histograms;

  // Open a picture-in-picture window manually.
  content::MediaStartStopObserver enter_pip_observer(
      web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      u"openPip({disallowReturnToOpener: false})", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  enter_pip_observer.Wait();

  // Trigger metric recording.
  test_clock.Advance(base::Milliseconds(5000));
  web_contents->ClosePage();
  ui_test_utils::WaitForBrowserToClose(browser());

  // Verify expectations.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples = histograms.GetHistogramSamplesSinceCreation(
      kVideoConferencingTotalTimeHistogram);
  EXPECT_EQ(0, samples->TotalCount());
}

// TODO(crbug.com/394322967): Flaky. Re-enable when fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_VideoConferencing_TotalPipTimeForSessionRecorded \
  DISABLED_VideoConferencing_TotalPipTimeForSessionRecorded
#else
#define MAYBE_VideoConferencing_TotalPipTimeForSessionRecorded \
  VideoConferencing_TotalPipTimeForSessionRecorded
#endif
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       MAYBE_VideoConferencing_TotalPipTimeForSessionRecorded) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  // Simulate the accumulatation of video conferencing pip time.
  base::HistogramTester histograms;
  SwitchToNewTabAndWaitForAutoPip();
  test_clock.Advance(base::Milliseconds(5000));
  SwitchBackToOpenerAndWaitForPipToClose();

  SwitchToNewTabAndWaitForAutoPip();
  test_clock.Advance(base::Milliseconds(5000));
  SwitchBackToOpenerAndWaitForPipToClose();

  // Trigger metric recording.
  CloseBrowserSynchronously(browser());

  // Verify expectations.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples = histograms.GetHistogramSamplesSinceCreation(
      kVideoConferencingTotalTimeForSessionHistogram);
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(10000));
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       AutoPipReasonSetForDocumentPip_VideoConferencing) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  EXPECT_EQ(media::PictureInPictureEventsInfo::AutoPipReason::kUnknown,
            GetAutoPipReason(*web_contents));
  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  media::PictureInPictureEventsInfo::AutoPipReason expected_reason =
      media::PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing;
  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));

  SwitchBackToOpenerAndWaitForPipToClose();
  expected_reason = media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       AutoPipPermissionShownInPageInfoOnChange) {
  // Load a page that can register for autopip.
  ASSERT_TRUE(embedded_https_test_server().Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL(
          "a.com",
          "/media/picture-in-picture/autopip-toggle-registration.html")));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  OpenPageInfoBubble(browser());

  // Initially, the permission should not be shown.
  EXPECT_FALSE(IsPermissionPresentInPageInfo(
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE));

  // Simulate the page registering for auto-pip, and wait for the page info
  // widget to resize.
  views::Widget* page_info_widget =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting()->GetWidget();
  ASSERT_TRUE(page_info_widget);

  const gfx::Rect bounds_before_register =
      page_info_widget->GetWindowBoundsInScreen();
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"register()", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  WaitForMediaSessionActionRegistered(web_contents);
  WidgetBoundsChangeWaiter(page_info_widget,
                           WidgetBoundsChangeWaiter::Comparison::kIsDifferent)
      .Wait(bounds_before_register);

  // Now the permission should be shown.
  EXPECT_TRUE(IsPermissionPresentInPageInfo(
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE));

  // After unregistering, the permission should still be shown.
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"unregister()", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  WaitForMediaSessionActionUnregistered(web_contents);

  EXPECT_TRUE(IsPermissionPresentInPageInfo(
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE));
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       AutoPipReasonSetForDocumentPip_Unknown) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  media::PictureInPictureEventsInfo::AutoPipReason expected_reason =
      media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));

  // Open a picture-in-picture window manually.
  content::MediaStartStopObserver enter_pip_observer(
      web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  OpenPipManually(web_contents);
  enter_pip_observer.Wait();
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));

  web_contents->ClosePage();
  ui_test_utils::WaitForBrowserToClose(browser());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       CachedBoundsUsedWhenPermissionPromtNotVisible) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  // Allow the AUTO_PICTURE_IN_PICTURE content setting. This will prevent
  // showing the permission prompt.
  SetContentSetting(web_contents, CONTENT_SETTING_ALLOW);
  SwitchToNewTabAndWaitForAutoPip();

  // Wait for widget to be visible.
  auto* pip_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_contents);
  auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      pip_contents->GetTopLevelNativeWindow());
  ASSERT_TRUE(browser_view);
  auto* pip_widget = browser_view->GetWidget();
  ASSERT_TRUE(pip_widget);
  views::test::WidgetVisibleWaiter(pip_widget).Wait();

  // Get the current picture-in-picture window bounds.
  const gfx::Rect initial_bounds = pip_widget->GetWindowBoundsInScreen();

  // Move the picture-in-picture window to a new location.
  const gfx::Rect moved_bounds = initial_bounds - gfx::Vector2d(5, 10);
  pip_widget->SetBounds(moved_bounds);

  // Wait for the move to complete, and close the picture-in-picture window.
  WidgetBoundsChangeWaiter(pip_widget,
                           WidgetBoundsChangeWaiter::Comparison::kIsEqual)
      .Wait(moved_bounds);
  SwitchBackToOpenerAndWaitForPipToClose();

  // Re-open the picture-in-picture window, and verify that the new new and old
  // bounds are equal.
  SwitchToNewTabAndWaitForAutoPip();
  auto* new_pip_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, new_pip_contents);
  auto* new_pip_widget = BrowserView::GetBrowserViewForNativeWindow(
                             new_pip_contents->GetTopLevelNativeWindow())
                             ->GetWidget();
  EXPECT_EQ(moved_bounds, new_pip_widget->GetWindowBoundsInScreen());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       CachedBoundsIgnoredWhenPermissionPromtIsVisible) {
  // Load a page that registers for autopip and starts using camera/microphone.
  LoadCameraMicrophonePage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(web_contents);

  // Switch to a new tab and wait for auto picture-in-piture, the permission
  // prompt should be shown.
  SwitchToNewTabAndWaitForAutoPip();

  // Wait for widget to be visible.
  auto* pip_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_contents);
  auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      pip_contents->GetTopLevelNativeWindow());
  ASSERT_TRUE(browser_view);
  auto* pip_widget = browser_view->GetWidget();
  ASSERT_TRUE(pip_widget);
  views::test::WidgetVisibleWaiter(pip_widget).Wait();

  // Get the current picture-in-picture window bounds.
  const gfx::Rect initial_bounds = pip_widget->GetWindowBoundsInScreen();

  // Move the picture-in-picture window to a new location.
  const gfx::Rect moved_bounds = initial_bounds - gfx::Vector2d(5, 10);
  pip_widget->SetBounds(moved_bounds);

  // Wait for the move to complete, and close the picture-in-picture window.
  WidgetBoundsChangeWaiter(pip_widget,
                           WidgetBoundsChangeWaiter::Comparison::kIsEqual)
      .Wait(moved_bounds);
  SwitchBackToOpenerAndWaitForPipToClose();

  // Re-open the picture-in-picture window, and verify that the new new and old
  // bounds are not equal.
  SwitchToNewTabAndWaitForAutoPip();
  auto* new_pip_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, new_pip_contents);
  auto* new_pip_widget = BrowserView::GetBrowserViewForNativeWindow(
                             new_pip_contents->GetTopLevelNativeWindow())
                             ->GetWidget();
  EXPECT_NE(moved_bounds, new_pip_widget->GetWindowBoundsInScreen());
  EXPECT_EQ(initial_bounds, new_pip_widget->GetWindowBoundsInScreen());
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
  SetExpectedHasHighEngagement(true);

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
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

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
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

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

// TODO(https://crbug.com/371850487): failing on Windows.
#if BUILDFLAG(IS_WIN)
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

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DoesNotAutopipIfNotRegistered) {
  // Load a page that does not register for autopip and start video playback.
  LoadNotRegisteredPage(browser());
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(original_web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(original_web_contents);
  WaitForWasRecentlyAudible(original_web_contents);
  SetExpectedHasHighEngagement(true);

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
  SetExpectedHasHighEngagement(true);

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

// TODO(crbug.com/372777367): Test failing on Windows
// TODO(crbug.com/409069588): Re-enable this test on Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_DoesNotCloseAutomaticallyOpenedPip \
  DISABLED_DoesNotCloseAutomaticallyOpenedPip
#else
#define MAYBE_DoesNotCloseAutomaticallyOpenedPip \
  DoesNotCloseAutomaticallyOpenedPip
#endif
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabHelperBrowserTest,
                       MAYBE_DoesNotCloseAutomaticallyOpenedPip) {
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

// TODO(crbug.com/382340033): Re-enable failing test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DevToolsMediaLogsRecordedForOpener \
  DISABLED_DevToolsMediaLogsRecordedForOpener
#else
#define MAYBE_DevToolsMediaLogsRecordedForOpener \
  DevToolsMediaLogsRecordedForOpener
#endif
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       MAYBE_DevToolsMediaLogsRecordedForOpener) {
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  // Trigger the creation of a new player. This will help us test if
  // subsequently generated media logs are correctly routed.
  TriggerNewPlayerCreation(web_contents);

  {
    // Start watching the DevTools logs and clear the latest media notification.
    content::DevToolsInspectorLogWatcher log_watcher(
        web_contents, content::DevToolsInspectorLogWatcher::Domain::Media);
    log_watcher.ClearLastMediaNotification();

    // Generate media logs.
    PlayVideo(web_contents);
    WaitForMediaSessionPlaying(web_contents);

    // Verify that media logs were recorded, since the player should be using
    // the media element execution context of the opener document.
    log_watcher.FlushAndStopWatching();
    ASSERT_FALSE(log_watcher.last_media_notification().empty());
  }

  SwitchBackToOpenerAndWaitForPipToClose();
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       DevToolsMediaLogsNotRecordedForPipWindow) {
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  {
    // Start watching the DevTools logs.
    auto* pip_web_contents =
        PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
    ASSERT_NE(nullptr, pip_web_contents);
    content::DevToolsInspectorLogWatcher pip_log_watcher(
        pip_web_contents, content::DevToolsInspectorLogWatcher::Domain::Media);

    // Trigger the creation of a new player. This will help us test if
    // subsequently generated media logs are correctly routed.
    TriggerNewPlayerCreation(web_contents);

    // Generate media logs.
    PlayVideo(web_contents);
    WaitForMediaSessionPlaying(web_contents);

    // Verify that media logs were not recorded, since the player should be
    // using the media element execution context of the opener document.
    pip_log_watcher.FlushAndStopWatching();
    ASSERT_TRUE(pip_log_watcher.last_media_notification().empty());
  }

  SwitchBackToOpenerAndWaitForPipToClose();
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       PromptResultRecorded_VideoPlaybackAllowOnce) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser(), "a.com");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  base::HistogramTester histograms;
  {
    content::MediaStartStopObserver enter_pip_observer(
        web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    content::TestNavigationObserver document_pip_load_observer(
        GURL("about:blank"));
    document_pip_load_observer.StartWatchingNewWebContents();
    OpenNewTab(browser());
    // Wait for the newly opened document picture-in-picture window to open and
    // be loaded.
    document_pip_load_observer.Wait();
    enter_pip_observer.Wait();
  }
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  auto* const overlay_view = GetOverlayViewFromDocumentPipWindow();
  overlay_view->get_view_for_testing()->simulate_button_press_for_testing(
      AutoPipSettingView::UiResult::kAllowOnce);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kMediaPlaybackHistogram);

  // Verify metrics.
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<int>(PromptResult::kAllowOnce)));
  CheckPromptResultUkmRecorded(web_contents->GetLastCommittedURL(),
                               UkmEntry::kMediaPlaybackName,
                               PromptResult::kAllowOnce);
}

IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureWithVideoPlaybackBrowserTest,
    PromptResultRecorded_VideoAndMediaPlaybackgNotShownBlocked) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // Starts using camera/microphone.
  GetUserMediaAndAccept(web_contents);

  SetContentSetting(web_contents, CONTENT_SETTING_BLOCK);

  base::HistogramTester histograms;
  OpenNewTab(browser());
  EXPECT_FALSE(web_contents->HasPictureInPictureDocument());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kVideoConferencingHistogram);

  // Verify that the "not shown blocked" prompt result is recorded once for the
  // "video conferencing" metric.
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(6));  // Not shown blocked
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       PromptResultRecorded_VideoConferencingTakesPrecedence) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser(), "a.com");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // Starts using camera/microphone.
  GetUserMediaAndAccept(web_contents);

  base::HistogramTester histograms;
  {
    content::MediaStartStopObserver enter_pip_observer(
        web_contents,
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    OpenNewTab(browser());
    enter_pip_observer.Wait();
  }
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  auto* const overlay_view = GetOverlayViewFromDocumentPipWindow();
  overlay_view->get_view_for_testing()->simulate_button_press_for_testing(
      AutoPipSettingView::UiResult::kAllowOnce);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kVideoConferencingHistogram);

  // Verify that the "allow once" prompt result is recorded for the "video
  // conferencing" metric, even though the site meets both enter auto picture in
  // picture reasons: "video conferencing" and "media playback". This is because
  // the video conferencing check is always performed first.
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<int>(PromptResult::kAllowOnce)));

  // Verify UKMs.
  CheckPromptResultUkmRecorded(web_contents->GetLastCommittedURL(),
                               UkmEntry::kVideoConferencingName,
                               PromptResult::kAllowOnce);
  CheckPromptResultUkmMetricNotRecorded(web_contents->GetLastCommittedURL(),
                                        UkmEntry::kMediaPlaybackName);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       MediaPlaybackTotalTimeRecorded) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  // Trigger metric recording.
  base::HistogramTester histograms;
  SwitchToNewTabAndWaitForAutoPip();
  test_clock.Advance(base::Milliseconds(5000));
  SwitchBackToOpenerAndWaitForPipToClose();

  // Verify expectations.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples = histograms.GetHistogramSamplesSinceCreation(
      kMediaPlaybackTotalTimeHistogram);
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(5000));
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       MediaPlaybackTotalPlaybackTimeRecorded) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  // Trigger metric recording.
  base::HistogramTester histograms;
  SwitchToNewTabAndWaitForAutoPip();
  // Playing for 5000 ms
  test_clock.Advance(base::Milliseconds(5000));
  PauseVideo(web_contents);
  WaitForMediaSessionPaused(web_contents);
  // Paused for 2000 ms.
  test_clock.Advance(base::Milliseconds(2000));
  PlayVideo(web_contents);
  WaitForMediaSessionPlaying(web_contents);
  // Playing for 3000 ms
  test_clock.Advance(base::Milliseconds(3000));
  SwitchBackToOpenerAndWaitForPipToClose();

  // Verify expectations.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount(kMediaPlaybackTotalPlaybackTimeHistogram, 1);
  histograms.ExpectBucketCount(kMediaPlaybackTotalPlaybackTimeHistogram, 8000,
                               1);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       MediaPlayback_PlaybackToTotalTimeRatioRecorded) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  base::HistogramTester histograms;

  // Trigger Auto-PiP.
  SwitchToNewTabAndWaitForAutoPip();

  // Advance clock by 10 seconds while playing.
  test_clock.Advance(base::Milliseconds(10000));

  // Pause video.
  PauseVideo(web_contents);
  WaitForMediaSessionPaused(web_contents);

  // Advance clock by another 10 seconds while paused.
  test_clock.Advance(base::Milliseconds(10000));

  // Close Auto-PiP.
  SwitchBackToOpenerAndWaitForPipToClose();

  // Verify metrics.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Total Playback Time should be 10s.
  auto playback_samples = histograms.GetHistogramSamplesSinceCreation(
      kMediaPlaybackTotalPlaybackTimeHistogram);
  EXPECT_EQ(1, playback_samples->TotalCount());
  EXPECT_EQ(1, playback_samples->GetCount(10000));

  // Total Time should be 20s.
  auto total_time_samples = histograms.GetHistogramSamplesSinceCreation(
      kMediaPlaybackTotalTimeHistogram);
  EXPECT_EQ(1, total_time_samples->TotalCount());
  EXPECT_EQ(1, total_time_samples->GetCount(20000));

  // Ratio should be 50%.
  auto ratio_samples = histograms.GetHistogramSamplesSinceCreation(
      kMediaPlaybackPlaybackToTotalTimeRatioHistogram);
  EXPECT_EQ(1, ratio_samples->TotalCount());
  EXPECT_EQ(1, ratio_samples->GetCount(50));
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       ManuallyOpenedPip_MediaPlaybackTotalTimeNotRecorded) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  base::HistogramTester histograms;

  // Open a picture-in-picture window manually.
  content::MediaStartStopObserver enter_pip_observer(
      web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  OpenPipManually(web_contents);
  enter_pip_observer.Wait();

  // Trigger metric recording.
  test_clock.Advance(base::Milliseconds(5000));
  web_contents->ClosePage();
  ui_test_utils::WaitForBrowserToClose(browser());

  // Verify expectations.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples = histograms.GetHistogramSamplesSinceCreation(
      kMediaPlaybackTotalTimeHistogram);
  EXPECT_EQ(0, samples->TotalCount());
}

// TODO(crbug.com/394322967): Flaky. Re-enable when fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MediaPlayback_TotalPipTimeForSessionRecorded \
  DISABLED_MediaPlayback_TotalPipTimeForSessionRecorded
#else
#define MAYBE_MediaPlayback_TotalPipTimeForSessionRecorded \
  MediaPlayback_TotalPipTimeForSessionRecorded
#endif
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       MAYBE_MediaPlayback_TotalPipTimeForSessionRecorded) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  // Simulate the accumulatation of media playback pip time.
  base::HistogramTester histograms;
  SwitchToNewTabAndWaitForAutoPip();
  test_clock.Advance(base::Milliseconds(5000));
  SwitchBackToOpenerAndWaitForPipToClose();

  SwitchToNewTabAndWaitForAutoPip();
  test_clock.Advance(base::Milliseconds(5000));
  SwitchBackToOpenerAndWaitForPipToClose();

  // Trigger metric recording.
  CloseBrowserSynchronously(browser());

  // Verify expectations.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples = histograms.GetHistogramSamplesSinceCreation(
      kMediaPlaybackTotalTimeForSessionHistogram);
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(10000));
}

// TODO(crbug.com/394322967): Flaky. Re-enable when fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_VideoConferencingAndMediaPlayback_TotalPipTimeForSessionRecorded \
  DISABLED_VideoConferencingAndMediaPlayback_TotalPipTimeForSessionRecorded
#else
#define MAYBE_VideoConferencingAndMediaPlayback_TotalPipTimeForSessionRecorded \
  VideoConferencingAndMediaPlayback_TotalPipTimeForSessionRecorded
#endif
IN_PROC_BROWSER_TEST_F(
    AutoPictureInPictureWithVideoPlaybackBrowserTest,
    MAYBE_VideoConferencingAndMediaPlayback_TotalPipTimeForSessionRecorded) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  // Set clock for testing.
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  tab_helper->set_clock_for_testing(&test_clock);

  // Simulate the accumulatation of media playback pip time.
  base::HistogramTester histograms;
  SwitchToNewTabAndWaitForAutoPip();
  test_clock.Advance(base::Milliseconds(5000));
  SwitchBackToOpenerAndWaitForPipToClose();

  // Starts using camera/microphone.
  GetUserMediaAndAccept(web_contents);

  // Simulate the accumulatation of video conferencing pip time.
  SwitchToNewTabAndWaitForAutoPip();
  test_clock.Advance(base::Milliseconds(5000));
  SwitchBackToOpenerAndWaitForPipToClose();

  // Trigger metrics recording.
  CloseBrowserSynchronously(browser());

  // Verify video conferencing expectations.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto video_conferencing_samples = histograms.GetHistogramSamplesSinceCreation(
      kVideoConferencingTotalTimeForSessionHistogram);
  EXPECT_EQ(1, video_conferencing_samples->TotalCount());
  EXPECT_EQ(1, video_conferencing_samples->GetCount(5000));

  // Verify media playback expectations.
  auto media_playback_samples = histograms.GetHistogramSamplesSinceCreation(
      kMediaPlaybackTotalTimeForSessionHistogram);
  EXPECT_EQ(1, media_playback_samples->TotalCount());
  EXPECT_EQ(1, media_playback_samples->GetCount(5000));
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       AutoPipReasonSetForVideoPip_MediaPlayback) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  EXPECT_EQ(media::PictureInPictureEventsInfo::AutoPipReason::kUnknown,
            GetAutoPipReason(*web_contents));
  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_TRUE(web_contents->HasPictureInPictureVideo());

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  media::PictureInPictureEventsInfo::AutoPipReason expected_reason =
      media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback;
  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));

  SwitchBackToOpenerAndWaitForPipToClose();
  expected_reason = media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       AutoPipReasonSetForDocumentPip_MediaPlayback) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  EXPECT_EQ(media::PictureInPictureEventsInfo::AutoPipReason::kUnknown,
            GetAutoPipReason(*web_contents));
  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  media::PictureInPictureEventsInfo::AutoPipReason expected_reason =
      media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback;
  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));

  SwitchBackToOpenerAndWaitForPipToClose();
  expected_reason = media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       AutoPipReasonSetForDocumentPip_Unknown) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  media::PictureInPictureEventsInfo::AutoPipReason expected_reason =
      media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));
  // Open a picture-in-picture window manually.
  content::MediaStartStopObserver enter_pip_observer(
      web_contents,
      content::MediaStartStopObserver::Type::kEnterPictureInPicture);
  OpenPipManually(web_contents);
  enter_pip_observer.Wait();
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));

  web_contents->ClosePage();
  ui_test_utils::WaitForBrowserToClose(browser());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       AutoPipReasonIsResetForDocumentPip_Unknown) {
  // Load a page that registers for autopip and start video playback.
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  EXPECT_EQ(media::PictureInPictureEventsInfo::AutoPipReason::kUnknown,
            GetAutoPipReason(*web_contents));
  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_TRUE(web_contents->HasPictureInPictureDocument());

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  media::PictureInPictureEventsInfo::AutoPipReason expected_reason =
      media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback;
  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));

  auto* pip_web_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);

  // Closing the picture in picture window, without returning to the opener,
  // should reset the `auto_pip_trigger_reason_` to
  // `media::PictureInPictureEventsInfo::AutoPipReason::kUnknown`.
  {
    content::MediaStartStopObserver exit_pip_observer(
        web_contents,
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    pip_web_contents->ClosePage();
    exit_pip_observer.Wait();
  }

  expected_reason = media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
  EXPECT_EQ(expected_reason, tab_helper->GetAutoPipTriggerReason());
  EXPECT_EQ(expected_reason, GetAutoPipReason(*web_contents));

  CloseBrowserSynchronously(browser());
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       AutoPipInfoRecordedInDevTools) {
  LoadAutoDocumentPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);
  WaitForWasRecentlyAudible(web_contents);

  {
    // Start watching the DevTools logs and clear the latest media notification.
    content::DevToolsInspectorLogWatcher log_watcher(
        web_contents, content::DevToolsInspectorLogWatcher::Domain::Media);
    log_watcher.ClearLastAutoPictureInPictureEventInfo();

    // Generate media logs.
    AutoPipInfoDevToolsWaiter pip_devtools_info_waiter(&log_watcher);
    SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/false,
                                          /*should_document_pip=*/true);
    pip_devtools_info_waiter.WaitUntilDone();

    // Verify that the auto picture in picture information was recorded in the
    // DevTools media logs.
    log_watcher.FlushAndStopWatching();
    ASSERT_FALSE(log_watcher.last_auto_picture_in_picture_event_info().empty());
    const std::string expected_auto_pip_info =
        "{\"auto_picture_in_picture_info\":{\"blocked_due_to_content_setting\":"
        "false,\"has_audio_focus\":true,\"has_safe_url\":true,\"is_playing\":"
        "true,\"meets_media_engagement_conditions\":true,\"reason\":"
        "\"MediaPlayback\",\"was_recently_audible\":true},\"event\":"
        "\"kAutoPictureInPictureInfoChanged\"}";
    EXPECT_EQ(expected_auto_pip_info,
              log_watcher.last_auto_picture_in_picture_event_info());
  }
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWithVideoPlaybackBrowserTest,
                       ReportAutoPictureInPictureInfoChangedCalledOnce) {
  // Setup mock browser client.
  MockContentBrowserClient mock_client;
  content::ContentBrowserClient* old_client =
      content::SetBrowserClientForTesting(&mock_client);

  // Load a page that registers for autopip and start video playback.
  LoadAutoVideoPipPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  // We use `GetAutoPipInfo` as a proxy for calls to
  // `ReportAutoPictureInPictureInfoChanged`. We want to make sure that we only
  // report changes to auto-pip info once on tab switch. This is specially
  // important due to the asynchronous nature of the URL safety checks.
  EXPECT_CALL(mock_client, GetAutoPipInfo(_)).Times(1);

  // Switch tabs to trigger auto-pip.
  SwitchToNewTabAndWaitForAutoPip();

  // Clean up.
  SwitchBackToOpenerAndWaitForPipToClose();
  content::SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(BrowserInitiatedAutoPictureInPictureBrowserTest,
                       OpensAndClosesVideoBrowserAutopip) {
  // Load a page that does not register for autopip and start video playback.
  LoadNotRegisteredPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(BrowserInitiatedAutoPictureInPictureBrowserTest,
                       DoesNotBrowserAutopip_NotRecentlyAudible) {
  // Load a page that does not register for autopip and start video playback.
  LoadNotRegisteredPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  SetExpectedHasHighEngagement(true);

  // Wait for video to be recently audible, to ensure we start from a known
  // state. Then mute audio and wait for video to not be recently audible.
  WaitForWasRecentlyAudible(web_contents, /*expected_recently_audible=*/true);
  MuteVideo(web_contents);
  WaitForWasRecentlyAudible(web_contents, /*expected_recently_audible=*/false);

  SwitchToNewTabAndDontExpectAutopip();
}

IN_PROC_BROWSER_TEST_F(BrowserInitiatedAutoPictureInPictureBrowserTest,
                       DoesNotBrowserAutopip_WhenUsingCamera) {
  // Load a page that does not register for autopip and start video playback.
  LoadNotRegisteredPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  // Simulate camera usage.
  ASSERT_TRUE(
      ExecJs(web_contents, "navigator.mediaSession.setCameraActive(true)"));
  WaitForMediaSessionCameraState(web_contents,
                                 media_session::mojom::CameraState::kTurnedOn);

  // Auto-pip should not take place.
  SwitchToNewTabAndDontExpectAutopip();

  // Switch back to tab.
  SwitchToExistingTab(web_contents);

  // Stop camera usage.
  ASSERT_TRUE(
      ExecJs(web_contents, "navigator.mediaSession.setCameraActive(false)"));
  WaitForMediaSessionCameraState(web_contents,
                                 media_session::mojom::CameraState::kTurnedOff);

  // Auto-pip should take place.
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(BrowserInitiatedAutoPictureInPictureBrowserTest,
                       DoesNotBrowserAutopip_WhenUsingMicrophone) {
  // Load a page that does not register for autopip and start video playback.
  LoadNotRegisteredPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  // Simulate microphone usage.
  ASSERT_TRUE(
      ExecJs(web_contents, "navigator.mediaSession.setMicrophoneActive(true)"));
  WaitForMediaSessionMicrophoneState(
      web_contents, media_session::mojom::MicrophoneState::kUnmuted);

  // Auto-pip should not take place.
  SwitchToNewTabAndDontExpectAutopip();

  // Switch back to tab.
  SwitchToExistingTab(web_contents);

  // Stop microphone usage.
  ASSERT_TRUE(ExecJs(web_contents,
                     "navigator.mediaSession.setMicrophoneActive(false)"));
  WaitForMediaSessionMicrophoneState(
      web_contents, media_session::mojom::MicrophoneState::kMuted);

  // Auto-pip should take place.
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(BrowserInitiatedAutoPictureInPictureBrowserTest,
                       DoesNotBrowserAutopip_WhenUsingCameraAndMicrophone) {
  // Load a page that does not register for autopip and start video playback.
  LoadNotRegisteredPage(browser());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  // Simulate camera and microphone usage.
  ASSERT_TRUE(
      ExecJs(web_contents, "navigator.mediaSession.setCameraActive(true)"));
  ASSERT_TRUE(
      ExecJs(web_contents, "navigator.mediaSession.setMicrophoneActive(true)"));
  WaitForMediaSessionCameraState(web_contents,
                                 media_session::mojom::CameraState::kTurnedOn);
  WaitForMediaSessionMicrophoneState(
      web_contents, media_session::mojom::MicrophoneState::kUnmuted);

  // Auto-pip should not take place.
  SwitchToNewTabAndDontExpectAutopip();

  // Switch back to tab.
  SwitchToExistingTab(web_contents);

  // Stop camera usage and microphone.
  ASSERT_TRUE(
      ExecJs(web_contents, "navigator.mediaSession.setCameraActive(false)"));
  ASSERT_TRUE(ExecJs(web_contents,
                     "navigator.mediaSession.setMicrophoneActive(false)"));
  WaitForMediaSessionCameraState(web_contents,
                                 media_session::mojom::CameraState::kTurnedOff);
  WaitForMediaSessionMicrophoneState(
      web_contents, media_session::mojom::MicrophoneState::kMuted);

  // Auto-pip should take place.
  SwitchToNewTabAndBackAndExpectAutopip(/*should_video_pip=*/true,
                                        /*should_document_pip=*/false);
}

IN_PROC_BROWSER_TEST_F(BrowserInitiatedAutoPictureInPictureBrowserTest,
                       PromptResultRecorded_BrowserInitiatedAllowOnce) {
  // Load a page that does not register for autopip and start video playback.
  ASSERT_TRUE(embedded_https_test_server().Start());
  GURL test_page_url = embedded_https_test_server().GetURL(
      "a.com", base::FilePath(FILE_PATH_LITERAL("/"))
                   .Append(kNotRegisteredPage)
                   .MaybeAsASCII());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideo(web_contents);
  WaitForAudioFocusGained();
  WaitForMediaSessionPlaying(web_contents);
  WaitForWasRecentlyAudible(web_contents);
  SetExpectedHasHighEngagement(true);

  // Set content setting to CONTENT_SETTING_ASK to show the prompt.
  SetContentSetting(web_contents, CONTENT_SETTING_ASK);

  base::HistogramTester histograms;
  SwitchToNewTabAndWaitForAutoPip();
  EXPECT_TRUE(web_contents->HasPictureInPictureVideo());

  auto* const overlay_view = GetOverlayViewFromVideoPipWindow();
  ASSERT_TRUE(overlay_view);
  overlay_view->get_view_for_testing()->simulate_button_press_for_testing(
      AutoPipSettingView::UiResult::kAllowOnce);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  auto samples =
      histograms.GetHistogramSamplesSinceCreation(kBrowserInitiatedHistogram);

  // Verify metrics.
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<int>(PromptResult::kAllowOnce)));
  CheckPromptResultUkmRecorded(web_contents->GetLastCommittedURL(),
                               UkmEntry::kBrowserInitiatedName,
                               PromptResult::kAllowOnce);
}
