// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/mei_preload_component_installer.h"
#include "chrome/browser/media/media_engagement_contents_observer.h"
#include "chrome/browser/media/media_engagement_preloaded_list.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_test_utils.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/common/features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom-test-utils.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

const char* kMediaEngagementTestDataPath = "chrome/test/data/media/engagement";

const std::u16string kReadyTitle = u"Ready";

// Watches WasRecentlyAudible changes on a WebContents, blocking until the
// tab is audible. The audio stream monitor runs at 15Hz so we need have
// a slight delay to ensure it has run.
// TODO: Clean this up to use the callbacks available on
// RecentlyAudibleHelper rather than busy-looping.
class WasRecentlyAudibleWatcher {
 public:
  // |web_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|.
  explicit WasRecentlyAudibleWatcher(content::WebContents* web_contents)
      : audible_helper_(RecentlyAudibleHelper::FromWebContents(web_contents)) {}

  WasRecentlyAudibleWatcher(const WasRecentlyAudibleWatcher&) = delete;
  WasRecentlyAudibleWatcher& operator=(const WasRecentlyAudibleWatcher&) =
      delete;

  ~WasRecentlyAudibleWatcher() = default;

  // Waits until WasRecentlyAudible is true.
  void WaitForWasRecentlyAudible() {
    if (!audible_helper_->WasRecentlyAudible()) {
      timer_.Start(FROM_HERE, base::Microseconds(100),
                   base::BindRepeating(
                       &WasRecentlyAudibleWatcher::TestWasRecentlyAudible,
                       base::Unretained(this)));
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

 private:
  void TestWasRecentlyAudible() {
    if (audible_helper_->WasRecentlyAudible()) {
      run_loop_->Quit();
      timer_.Stop();
    }
  }

  const raw_ptr<RecentlyAudibleHelper, DanglingUntriaged> audible_helper_;

  base::RepeatingTimer timer_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

// Class used to test the Media Engagement service.
class MediaEngagementBrowserTest : public InProcessBrowserTest {
 protected:
  MediaEngagementBrowserTest()
      : task_runner_(new base::TestMockTimeTaskRunner()) {
    http_server_.ServeFilesFromSourceDirectory(kMediaEngagementTestDataPath);
    http_server_origin2_.ServeFilesFromSourceDirectory(
        kMediaEngagementTestDataPath);
  }

  ~MediaEngagementBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(http_server_.Start());
    ASSERT_TRUE(http_server_origin2_.Start());

    scoped_feature_list_.InitWithFeatures({media::kRecordMediaEngagementScores},
                                          disabled_features_);

    InProcessBrowserTest::SetUp();

    injected_clock_ = false;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void LoadTestPage(const GURL& url) {
    // We can't do this in SetUp as the browser isn't ready yet and we
    // need it before the page navigates.
    InjectTimerTaskRunner();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  void LoadTestPageAndWaitForPlay(const GURL& url, bool web_contents_muted) {
    LoadTestPage(url);
    GetWebContents()->SetAudioMuted(web_contents_muted);
    WaitForPlay();
  }

  // TODO(beccahughes,mlamouri): update this to use GURL.
  void LoadTestPageAndWaitForPlayAndAudible(const std::string& page,
                                            bool web_contents_muted) {
    LoadTestPageAndWaitForPlayAndAudible(http_server_.GetURL("/" + page),
                                         web_contents_muted);
  }

  void LoadTestPageAndWaitForPlayAndAudible(const GURL& url,
                                            bool web_contents_muted) {
    LoadTestPageAndWaitForPlay(url, web_contents_muted);
    WaitForWasRecentlyAudible();
  }

  void OpenTab(const GURL& url, ui::PageTransition transition) {
    NavigateParams params(browser(), url, transition);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    // params.opener does not need to be set in the context of this test because
    // it will use the current tab by default.
    Navigate(&params);

    InjectTimerTaskRunner();
    params.navigated_or_inserted_contents->SetAudioMuted(false);
    EXPECT_TRUE(
        content::WaitForLoadStop(params.navigated_or_inserted_contents));
  }

  void OpenTabAsLink(const GURL& url) {
    OpenTab(url, ui::PAGE_TRANSITION_LINK);
  }

  void OpenTabAndWaitForPlayAndAudible(const GURL& url) {
    OpenTabAsLink(url);

    WaitForPlay();
    WaitForWasRecentlyAudible();
  }

  void Advance(base::TimeDelta time) {
    DCHECK(injected_clock_);
    task_runner_->FastForwardBy(time);
    test_clock_.Advance(time);
    base::RunLoop().RunUntilIdle();
  }

  void AdvanceMeaningfulPlaybackTime() {
    Advance(MediaEngagementBrowserTest::kMaxWaitingTime);
  }

  void ExpectScores(int visits, int media_playbacks) {
    ExpectScores(http_server_.base_url(), visits, media_playbacks);
  }

  void ExpectScoresSecondOrigin(int visits, int media_playbacks) {
    ExpectScores(http_server_origin2_.base_url(), visits, media_playbacks);
  }

  void ExpectScores(GURL url, int visits, int media_playbacks) {
    ExpectScores(GetService(), url, visits, media_playbacks);
  }

  void ExpectScores(MediaEngagementService* service,
                    GURL url,
                    int visits,
                    int media_playbacks) {
    MediaEngagementScore score =
        service->CreateEngagementScore(url::Origin::Create(url));
    EXPECT_EQ(visits, score.visits());
    EXPECT_EQ(media_playbacks, score.media_playbacks());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void ExecuteScript(const std::string& script) {
    EXPECT_TRUE(content::ExecJs(GetWebContents(), script));
  }

  void OpenTabAsLink() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("chrome://about"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void CloseTab() {
    const int previous_tab_count = browser()->tab_strip_model()->count();
    browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
    EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());
  }

  void LoadSubFrame(const GURL& url) {
    ExecuteScript("window.open(\"" + url.spec() + "\", \"subframe\")");
  }

  void WaitForPlay() {
    content::TitleWatcher title_watcher(GetWebContents(), kReadyTitle);
    EXPECT_EQ(kReadyTitle, title_watcher.WaitAndGetTitle());
  }

  void WaitForWasRecentlyAudible() {
    WasRecentlyAudibleWatcher watcher(GetWebContents());
    watcher.WaitForWasRecentlyAudible();
  }

  void EraseHistory() {
    history::URLRows urls;
    urls.push_back(history::URLRow(http_server_.GetURL("/")));
    GetService()->OnHistoryDeletions(
        nullptr, history::DeletionInfo::ForUrls(urls, std::set<GURL>()));
  }

  void LoadNewOriginPage() {
    // We can't do this in SetUp as the browser isn't ready yet and we
    // need it before the page navigates.
    InjectTimerTaskRunner();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), http_server_origin2_.GetURL("/engagement_test.html")));
  }

  const net::EmbeddedTestServer& http_server() const { return http_server_; }

  const net::EmbeddedTestServer& http_server_origin2() const {
    return http_server_origin2_;
  }

  void CloseBrowser() { CloseAllBrowsers(); }

  MediaEngagementService* GetService() {
    return MediaEngagementService::Get(browser()->profile());
  }

  // To be used only for a service that wasn't the one created by the test
  // class.
  void InjectTimerTaskRunnerToService(MediaEngagementService* service) {
    service->clock_ = &test_clock_;

    for (auto observer : service->contents_observers_)
      observer.second->SetTaskRunnerForTest(task_runner_);
  }

  std::vector<base::test::FeatureRef> disabled_features_;

 private:
  void InjectTimerTaskRunner() {
    if (!injected_clock_) {
      GetService()->clock_ = &test_clock_;
      injected_clock_ = true;
    }

    for (auto observer : GetService()->contents_observers_)
      observer.second->SetTaskRunnerForTest(task_runner_);
  }

  bool injected_clock_ = false;

  base::SimpleTestClock test_clock_;

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer http_server_origin2_;

  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  const base::TimeDelta kMaxWaitingTime =
      MediaEngagementContentsObserver::kSignificantMediaPlaybackTime +
      base::Seconds(2);
};

// Class used to test the MEI preload component.
class MediaEngagementPreloadBrowserTest : public InProcessBrowserTest {
 public:
  MediaEngagementPreloadBrowserTest() = default;
  ~MediaEngagementPreloadBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({media::kPreloadMediaEngagementData},
                                          {});

    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, RecordEngagement) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html", false);
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(0, 0);
  CloseTab();
  ExpectScores(1, 1);
}

// Flaky tests on CrOS: http://crbug.com/1020131.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_RecordEngagement_AudioOnly DISABLED_RecordEngagement_AudioOnly
#else
#define MAYBE_RecordEngagement_AudioOnly RecordEngagement_AudioOnly
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_RecordEngagement_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html", false);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 1);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_NotTime) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html", false);
  Advance(base::Seconds(1));
  CloseTab();
  ExpectScores(1, 0);
}

// Flaky tests on CrOS: http://crbug.com/1019671.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_DoNotRecordEngagement_NotTime_AudioOnly \
  DISABLED_DoNotRecordEngagement_NotTime_AudioOnly
#else
#define MAYBE_DoNotRecordEngagement_NotTime_AudioOnly \
  DoNotRecordEngagement_NotTime_AudioOnly
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_DoNotRecordEngagement_NotTime_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html", false);
  Advance(base::Seconds(1));
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_TabMuted) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html", true);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

// Flaky tests on CrOS: http://crbug.com/1019671.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_DoNotRecordEngagement_TabMuted_AudioOnly \
  DISABLED_DoNotRecordEngagement_TabMuted_AudioOnly
#else
#define MAYBE_DoNotRecordEngagement_TabMuted_AudioOnly \
  DoNotRecordEngagement_TabMuted_AudioOnly
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_DoNotRecordEngagement_TabMuted_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html", true);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_PlayerMuted) {
  LoadTestPageAndWaitForPlay(
      http_server().GetURL("/engagement_test_muted.html"), false);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_PlayerMuted_AudioOnly) {
  LoadTestPageAndWaitForPlay(
      http_server().GetURL("/engagement_test_muted.html"), false);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

// TODO(crbug.com/40748282) Re-enable test
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DISABLED_DoNotRecordEngagement_PlaybackStopped) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html", false);
  Advance(base::Seconds(1));
  ExecuteScript("document.getElementById(\"media\").pause();");
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

// Flaky tests on CrOS: http://crbug.com/1019671.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_DoNotRecordEngagement_PlaybackStopped_AudioOnly \
  DISABLED_DoNotRecordEngagement_PlaybackStopped_AudioOnly
#else
#define MAYBE_DoNotRecordEngagement_PlaybackStopped_AudioOnly \
  DoNotRecordEngagement_PlaybackStopped_AudioOnly
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_DoNotRecordEngagement_PlaybackStopped_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html", false);
  Advance(base::Seconds(1));
  ExecuteScript("document.getElementById(\"media\").pause();");
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       RecordEngagement_NotVisible) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html", false);
  OpenTabAsLink();
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 1);
}

// Flaky tests on CrOS: http://crbug.com/1019671.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_RecordEngagement_NotVisible_AudioOnly \
  DISABLED_RecordEngagement_NotVisible_AudioOnly
#else
#define MAYBE_RecordEngagement_NotVisible_AudioOnly \
  RecordEngagement_NotVisible_AudioOnly
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_RecordEngagement_NotVisible_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html", false);
  OpenTabAsLink();
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 1);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_FrameSize) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_small_frame_size.html",
                                       false);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_NoAudioTrack) {
  LoadTestPageAndWaitForPlay(
      http_server().GetURL("/engagement_test_no_audio_track.html"), false);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_SilentAudioTrack) {
  LoadTestPageAndWaitForPlay(
      http_server().GetURL("/engagement_test_silent_audio_track.html"), false);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, RecordVisitOnBrowserClose) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_small_frame_size.html",
                                       false);
  AdvanceMeaningfulPlaybackTime();

  CloseBrowser();
  ExpectScores(1, 0);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
// Flaky timeout. https://crbug.com/1014229
#define MAYBE_RecordSingleVisitOnSameOrigin \
  DISABLED_RecordSingleVisitOnSameOrigin
#else
#define MAYBE_RecordSingleVisitOnSameOrigin RecordSingleVisitOnSameOrigin
#endif

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_RecordSingleVisitOnSameOrigin) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_small_frame_size.html",
                                       false);
  AdvanceMeaningfulPlaybackTime();

  LoadTestPageAndWaitForPlayAndAudible("engagement_test_no_audio_track.html",
                                       false);
  AdvanceMeaningfulPlaybackTime();

  CloseTab();
  ExpectScores(1, 0);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Flaky: https://crbug.com/1115238
#define MAYBE_RecordVisitOnNewOrigin DISABLED_RecordVisitOnNewOrigin
#else
#define MAYBE_RecordVisitOnNewOrigin RecordVisitOnNewOrigin
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_RecordVisitOnNewOrigin) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_small_frame_size.html",
                                       false);
  AdvanceMeaningfulPlaybackTime();

  LoadNewOriginPage();
  ExpectScores(1, 0);
}

// Flaky tests on CrOS: http://crbug.com/1019671.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_DoNotRecordEngagement_SilentAudioTrack_AudioOnly \
  DISABLED_DoNotRecordEngagement_SilentAudioTrack_AudioOnly
#else
#define MAYBE_DoNotRecordEngagement_SilentAudioTrack_AudioOnly \
  DoNotRecordEngagement_SilentAudioTrack_AudioOnly
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_DoNotRecordEngagement_SilentAudioTrack_AudioOnly) {
  LoadTestPageAndWaitForPlay(
      http_server().GetURL("/engagement_test_silent_audio_track_audio.html"),
      false);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, IFrameDelegation) {
  LoadTestPage(http_server().GetURL("/engagement_test_iframe.html"));
  LoadSubFrame(
      http_server_origin2().GetURL("/engagement_test_iframe_child.html"));

  WaitForPlay();
  WaitForWasRecentlyAudible();
  AdvanceMeaningfulPlaybackTime();

  CloseTab();
  ExpectScores(1, 1);
  ExpectScoresSecondOrigin(0, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, IFrameDelegation_AudioOnly) {
  LoadTestPage(http_server().GetURL("/engagement_test_iframe.html"));
  LoadSubFrame(
      http_server_origin2().GetURL("/engagement_test_iframe_audio_child.html"));

  WaitForPlay();
  WaitForWasRecentlyAudible();
  AdvanceMeaningfulPlaybackTime();

  CloseTab();
  ExpectScores(1, 1);
  ExpectScoresSecondOrigin(0, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       ClearBrowsingHistoryBeforePlayback) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html", false);
  EraseHistory();
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 1);
}

// Flaky tests on CrOS: http://crbug.com/1019671.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_MultipleElements DISABLED_MultipleElements
#else
#define MAYBE_MultipleElements MultipleElements
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, MAYBE_MultipleElements) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_multiple.html", false);
  AdvanceMeaningfulPlaybackTime();
  CloseTab();
  ExpectScores(1, 1);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       RecordAudibleBasedOnShortTime) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html", false);
  Advance(base::Seconds(4));
  CloseTab();
  ExpectScores(1, 0);
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1222896
#define MAYBE_SessionNewTabNavigateSameURL DISABLED_SessionNewTabNavigateSameURL
#else
#define MAYBE_SessionNewTabNavigateSameURL SessionNewTabNavigateSameURL
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_SessionNewTabNavigateSameURL) {
  const GURL& url = http_server().GetURL("/engagement_test.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  OpenTabAsLink(GURL("about:blank"));
  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  browser()->tab_strip_model()->CloseAllTabs();

  ExpectScores(2, 2);
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1222896
#define MAYBE_SessionNewTabSameURL DISABLED_SessionNewTabSameURL
#else
#define MAYBE_SessionNewTabSameURL SessionNewTabSameURL
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, MAYBE_SessionNewTabSameURL) {
  const GURL& url = http_server().GetURL("/engagement_test.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  OpenTabAndWaitForPlayAndAudible(url);
  AdvanceMeaningfulPlaybackTime();

  browser()->tab_strip_model()->CloseAllTabs();

  ExpectScores(1, 1);
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1222896
#define MAYBE_SessionNewTabSameOrigin DISABLED_SessionNewTabSameOrigin
#else
#define MAYBE_SessionNewTabSameOrigin SessionNewTabSameOrigin
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_SessionNewTabSameOrigin) {
  const GURL& url = http_server().GetURL("/engagement_test.html");
  const GURL& other_url = http_server().GetURL("/engagement_test_audio.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  OpenTabAndWaitForPlayAndAudible(other_url);
  AdvanceMeaningfulPlaybackTime();

  browser()->tab_strip_model()->CloseAllTabs();

  ExpectScores(1, 1);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, SessionNewTabCrossOrigin) {
  const GURL& url = http_server().GetURL("/engagement_test.html");
  const GURL& other_url = http_server_origin2().GetURL("/engagement_test.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  OpenTabAndWaitForPlayAndAudible(other_url);
  AdvanceMeaningfulPlaybackTime();

  browser()->tab_strip_model()->CloseAllTabs();

  ExpectScores(http_server().base_url(), 1, 1);
  ExpectScores(http_server_origin2().base_url(), 1, 1);
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40939686) Flaky on Mac.
#define MAYBE_SessionMultipleTabsClosingParent \
  DISABLED_SessionMultipleTabsClosingParent
#else
#define MAYBE_SessionMultipleTabsClosingParent SessionMultipleTabsClosingParent
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_SessionMultipleTabsClosingParent) {
  const GURL& url = http_server().GetURL("/engagement_test.html");
  const GURL& other_url = http_server().GetURL("/engagement_test_audio.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  OpenTabAndWaitForPlayAndAudible(other_url);
  AdvanceMeaningfulPlaybackTime();

  CloseTab();
  ASSERT_EQ(other_url, GetWebContents()->GetLastCommittedURL());

  OpenTabAndWaitForPlayAndAudible(url);
  AdvanceMeaningfulPlaybackTime();

  browser()->tab_strip_model()->CloseAllTabs();

  ExpectScores(1, 1);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementPreloadBrowserTest,
                       EnsureSingletonListIsLoaded) {
  base::RunLoop run_loop;
  component_updater::RegisterMediaEngagementPreloadComponent(
      g_browser_process->component_updater(), run_loop.QuitClosure());
  run_loop.Run();

  // The list should be loaded now.
  EXPECT_TRUE(MediaEngagementPreloadedList::GetInstance()->loaded());
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1222896
#define MAYBE_SessionNewTabNavigateSameURLWithOpener_Typed \
  DISABLED_SessionNewTabNavigateSameURLWithOpener_Typed
#else
#define MAYBE_SessionNewTabNavigateSameURLWithOpener_Typed \
  SessionNewTabNavigateSameURLWithOpener_Typed
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       MAYBE_SessionNewTabNavigateSameURLWithOpener_Typed) {
  const GURL& url = http_server().GetURL("/engagement_test.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  OpenTab(url, ui::PAGE_TRANSITION_TYPED);
  WaitForPlay();
  WaitForWasRecentlyAudible();
  AdvanceMeaningfulPlaybackTime();

  browser()->tab_strip_model()->CloseAllTabs();

  // The new tab should only count as the same visit if we visited that tab
  // through a link or reload (duplicate tab).
  ExpectScores(2, 2);
}

class MediaEngagementPreThirdPartyCookieDeprecationBrowserTest
    : public MediaEngagementBrowserTest {
 public:
  MediaEngagementPreThirdPartyCookieDeprecationBrowserTest() {
    disabled_features_.push_back(
        content_settings::features::kTrackingProtection3pcd);
  }
};

#if BUILDFLAG(IS_WIN)
#define MAYBE_Ignored DISABLED_Ignored
#else
#define MAYBE_Ignored Ignored
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementPreThirdPartyCookieDeprecationBrowserTest,
                       MAYBE_Ignored) {
  const GURL& url = http_server().GetURL("/engagement_test.html");

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(no_state_prefetch_manager);

  prerender::test_utils::TestNoStatePrefetchContentsFactory*
      no_state_prefetch_contents_factory =
          new prerender::test_utils::TestNoStatePrefetchContentsFactory();
  no_state_prefetch_manager->SetNoStatePrefetchContentsFactoryForTest(
      no_state_prefetch_contents_factory);

  content::SessionStorageNamespace* storage_namespace =
      GetWebContents()->GetController().GetDefaultSessionStorageNamespace();
  ASSERT_TRUE(storage_namespace);

  std::unique_ptr<prerender::test_utils::TestPrerender> test_prerender =
      no_state_prefetch_contents_factory->ExpectNoStatePrefetchContents(
          prerender::FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  std::unique_ptr<prerender::NoStatePrefetchHandle> no_state_prefetch_handle =
      no_state_prefetch_manager->AddSameOriginSpeculation(
          url, storage_namespace, gfx::Size(640, 480),
          url::Origin::Create(url));

  ASSERT_EQ(no_state_prefetch_handle->contents(), test_prerender->contents());

  EXPECT_EQ(nullptr, GetService()->GetContentsObserverFor(
                         test_prerender->contents()->web_contents()));

  test_prerender->WaitForStop();

  ExpectScores(0, 0);
}

class MediaEngagementSessionRestoreBrowserTest
    : public MediaEngagementBrowserTest {
 public:
  Browser* QuitBrowserAndRestore() {
    Profile* profile = browser()->profile();

    SessionStartupPref::SetStartupPref(
        profile, SessionStartupPref(SessionStartupPref::LAST));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    SessionServiceTestHelper helper(profile);
    helper.SetForceBrowserNotAliveWithNoWindows(true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    std::unique_ptr<ScopedKeepAlive> keep_alive(new ScopedKeepAlive(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED));
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive(
        new ScopedProfileKeepAlive(profile,
                                   ProfileKeepAliveOrigin::kBrowserWindow));
    CloseBrowserSynchronously(browser());

    chrome::NewEmptyWindow(profile);
    SessionRestoreTestHelper().Wait();
    return BrowserList::GetInstance()->GetLastActive();
  }

  void WaitForTabsToLoad(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* web_contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      web_contents->GetController().LoadIfNecessary();
      ASSERT_TRUE(content::WaitForLoadStop(web_contents));
    }
  }
};

IN_PROC_BROWSER_TEST_F(MediaEngagementSessionRestoreBrowserTest,
                       RestoredSession_NoPlayback_NoMEI) {
  const GURL& url = http_server().GetURL("/engagement_test_iframe.html");

  LoadTestPage(url);

  Browser* new_browser = QuitBrowserAndRestore();
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));

  new_browser->tab_strip_model()->CloseAllTabs();

  ExpectScores(MediaEngagementService::Get(new_browser->profile()), url, 1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementSessionRestoreBrowserTest,
                       RestoredSession_Playback_MEI) {
  const GURL& url = http_server().GetURL("/engagement_test.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  Browser* new_browser = QuitBrowserAndRestore();

  MediaEngagementService* new_service =
      MediaEngagementService::Get(new_browser->profile());
  InjectTimerTaskRunnerToService(new_service);

  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));

  WasRecentlyAudibleWatcher watcher(
      new_browser->tab_strip_model()->GetActiveWebContents());
  watcher.WaitForWasRecentlyAudible();

  AdvanceMeaningfulPlaybackTime();

  new_browser->tab_strip_model()->CloseAllTabs();

  ExpectScores(new_service, url, 2, 2);
}

class MockAutoplayConfigurationClient
    : public blink::mojom::AutoplayConfigurationClientInterceptorForTesting {
 public:
  MockAutoplayConfigurationClient() = default;
  ~MockAutoplayConfigurationClient() override = default;

  MockAutoplayConfigurationClient(const MockAutoplayConfigurationClient&) =
      delete;
  MockAutoplayConfigurationClient& operator=(
      const MockAutoplayConfigurationClient&) = delete;

  AutoplayConfigurationClient* GetForwardingInterface() override {
    return this;
  }

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<
            blink::mojom::AutoplayConfigurationClient>(std::move(handle)));
  }

  MOCK_METHOD2(AddAutoplayFlags, void(const url::Origin&, const int32_t));

 private:
  mojo::AssociatedReceiver<blink::mojom::AutoplayConfigurationClient> receiver_{
      this};
};

class MediaEngagementContentsObserverMPArchBrowserTest
    : public MediaEngagementBrowserTest {
 public:
  MediaEngagementContentsObserverMPArchBrowserTest() = default;
  ~MediaEngagementContentsObserverMPArchBrowserTest() override = default;
  MediaEngagementContentsObserverMPArchBrowserTest(
      const MediaEngagementContentsObserverMPArchBrowserTest&) = delete;

  MediaEngagementContentsObserverMPArchBrowserTest& operator=(
      const MediaEngagementContentsObserverMPArchBrowserTest&) = delete;

  void OverrideInterface(content::RenderFrameHost* render_frame_host,
                         MockAutoplayConfigurationClient* client) {
    render_frame_host->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            blink::mojom::AutoplayConfigurationClient::Name_,
            base::BindRepeating(&MockAutoplayConfigurationClient::BindReceiver,
                                base::Unretained(client)));
  }

  void SetScores(const url::Origin& origin, int visits, int media_playbacks) {
    MediaEngagementScore score = GetService()->CreateEngagementScore(origin);
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.Commit();
  }
};

class MediaEngagementContentsObserverPrerenderBrowserTest
    : public MediaEngagementContentsObserverMPArchBrowserTest {
 public:
  MediaEngagementContentsObserverPrerenderBrowserTest() = default;
  ~MediaEngagementContentsObserverPrerenderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    prerender_helper_->RegisterServerRequestMonitor(embedded_test_server());
    MediaEngagementContentsObserverMPArchBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaEngagementContentsObserverMPArchBrowserTest::SetUpCommandLine(
        command_line);
    // |prerender_helper_| has a ScopedFeatureList so we needed to delay its
    // creation until now because MediaEngagementBrowserTest also uses a
    // ScopedFeatureList and initialization order matters.
    prerender_helper_ = std::make_unique<
        content::test::PrerenderTestHelper>(base::BindRepeating(
        &MediaEngagementContentsObserverPrerenderBrowserTest::GetWebContents,
        base::Unretained(this)));
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_;
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Flaky on Linux: http://crbug.com/325530046
#if BUILDFLAG(IS_LINUX)
#define MAYBE_DoNotSendEngagementLevelToRenderFrameInPrerendering \
  DISABLED_DoNotSendEngagementLevelToRenderFrameInPrerendering
#else
#define MAYBE_DoNotSendEngagementLevelToRenderFrameInPrerendering \
  DoNotSendEngagementLevelToRenderFrameInPrerendering
#endif
IN_PROC_BROWSER_TEST_F(
    MediaEngagementContentsObserverPrerenderBrowserTest,
    MAYBE_DoNotSendEngagementLevelToRenderFrameInPrerendering) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL& initial_url = embedded_test_server()->GetURL("/empty.html");
  SetScores(url::Origin::Create(initial_url), 24, 20);

  content::TestNavigationManager navigation_manager(GetWebContents(),
                                                    initial_url);

  content::NavigationController::LoadURLParams params(initial_url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id =
      GetWebContents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  GetWebContents()->GetController().LoadURLWithParams(params);

  EXPECT_TRUE(navigation_manager.WaitForResponse());

  MockAutoplayConfigurationClient client;
  OverrideInterface(
      navigation_manager.GetNavigationHandle()->GetRenderFrameHost(), &client);
  // AddAutoplayFlags should be called once after navigating |initial_url| in
  // the main frame.
  EXPECT_CALL(client, AddAutoplayFlags(testing::_, testing::_)).Times(1);

  navigation_manager.ResumeNavigation();
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Loads a page in a prerendered page.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  const content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  MockAutoplayConfigurationClient prerendered_client;
  OverrideInterface(prerender_rfh, &prerendered_client);
  // AddAutoplayFlags should not be called in prerendering, but it should be
  // called when the prerendered page is activated.
  EXPECT_CALL(prerendered_client, AddAutoplayFlags(testing::_, testing::_))
      .Times(1);

  // Activate the prerendered page.
  prerender_helper().NavigatePrimaryPage(prerender_url);

  base::RunLoop().RunUntilIdle();
}

class MediaEngagementContentsObserverFencedFrameBrowserTest
    : public MediaEngagementContentsObserverMPArchBrowserTest {
 public:
  MediaEngagementContentsObserverFencedFrameBrowserTest() = default;
  ~MediaEngagementContentsObserverFencedFrameBrowserTest() override = default;
  MediaEngagementContentsObserverFencedFrameBrowserTest(
      const MediaEngagementContentsObserverFencedFrameBrowserTest&) = delete;

  MediaEngagementContentsObserverFencedFrameBrowserTest& operator=(
      const MediaEngagementContentsObserverFencedFrameBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    MediaEngagementContentsObserverMPArchBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaEngagementBrowserTest::SetUpCommandLine(command_line);
    // |fenced_frame_helper_| has a ScopedFeatureList so we needed to delay its
    // creation until now because MediaEngagementBrowserTest also uses a
    // ScopedFeatureList and initialization order matters.
    fenced_frame_helper_ =
        std::make_unique<content::test::FencedFrameTestHelper>();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return *fenced_frame_helper_;
  }

 private:
  std::unique_ptr<content::test::FencedFrameTestHelper> fenced_frame_helper_;
};

// TODO(crbug.com/349253812): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_SendEngagementLevelToRenderFrameOnFencedFrame \
  DISABLED_SendEngagementLevelToRenderFrameOnFencedFrame
#else
#define MAYBE_SendEngagementLevelToRenderFrameOnFencedFrame \
  SendEngagementLevelToRenderFrameOnFencedFrame
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementContentsObserverFencedFrameBrowserTest,
                       MAYBE_SendEngagementLevelToRenderFrameOnFencedFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL& initial_url =
      embedded_test_server()->GetURL("a.com", "/empty.html");
  SetScores(url::Origin::Create(initial_url), 24, 20);
  content::TestNavigationManager navigation_manager(GetWebContents(),
                                                    initial_url);

  content::NavigationController::LoadURLParams params(initial_url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id =
      GetWebContents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  GetWebContents()->GetController().LoadURLWithParams(params);

  EXPECT_TRUE(navigation_manager.WaitForResponse());

  MockAutoplayConfigurationClient client;
  OverrideInterface(
      navigation_manager.GetNavigationHandle()->GetRenderFrameHost(), &client);
  // AddAutoplayFlags should be called once after navigating |initial_url| in
  // the main frame.
  EXPECT_CALL(client, AddAutoplayFlags(testing::_, testing::_)).Times(1);

  navigation_manager.ResumeNavigation();
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Create a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);

  // AddAutoplayFlags should be called on the fenced frame.
  GURL fenced_frame_navigate_url =
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title2.html");
  content::TestNavigationManager navigation_manager2(GetWebContents(),
                                                     fenced_frame_navigate_url);
  EXPECT_TRUE(ExecJs(
      fenced_frame_host,
      content::JsReplace("location.href = $1;", fenced_frame_navigate_url)));

  EXPECT_TRUE(navigation_manager2.WaitForResponse());
  MockAutoplayConfigurationClient fenced_frame_client;
  OverrideInterface(
      navigation_manager2.GetNavigationHandle()->GetRenderFrameHost(),
      &fenced_frame_client);
  // AddAutoplayFlags should be called once after navigating |initial_url| in
  // the main frame.
  base::RunLoop run_loop;
  EXPECT_CALL(fenced_frame_client,
              AddAutoplayFlags(url::Origin::Create(fenced_frame_navigate_url),
                               testing::_))
      .Times(1)
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  navigation_manager2.ResumeNavigation();
  EXPECT_TRUE(navigation_manager2.WaitForNavigationFinished());
  run_loop.Run();
}
