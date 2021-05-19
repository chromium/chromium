// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/prefetch/no_state_prefetch/prerender_test_utils.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/component_updater/component_updater_service.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/prerender_final_status.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

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
  ~WasRecentlyAudibleWatcher() = default;

  // Waits until WasRecentlyAudible is true.
  void WaitForWasRecentlyAudible() {
    if (!audible_helper_->WasRecentlyAudible()) {
      timer_.Start(FROM_HERE, base::TimeDelta::FromMicroseconds(100),
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

  RecentlyAudibleHelper* const audible_helper_;

  base::RepeatingTimer timer_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WasRecentlyAudibleWatcher);
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

    scoped_feature_list_.InitAndEnableFeature(
        media::kRecordMediaEngagementScores);

    InProcessBrowserTest::SetUp();

    injected_clock_ = false;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void LoadTestPage(const GURL& url) {
    // We can't do this in SetUp as the browser isn't ready yet and we
    // need it before the page navigates.
    InjectTimerTaskRunner();

    ui_test_utils::NavigateToURL(browser(), url);
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
    EXPECT_TRUE(content::ExecuteScript(GetWebContents(), script));
  }

  void OpenTabAsLink() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("chrome://about"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void CloseTab() {
    EXPECT_TRUE(browser()->tab_strip_model()->CloseWebContentsAt(0, 0));
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
    GetService()->OnURLsDeleted(
        nullptr, history::DeletionInfo::ForUrls(urls, std::set<GURL>()));
  }

  void LoadNewOriginPage() {
    // We can't do this in SetUp as the browser isn't ready yet and we
    // need it before the page navigates.
    InjectTimerTaskRunner();

    ui_test_utils::NavigateToURL(
        browser(), http_server_origin2_.GetURL("/engagement_test.html"));
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
      base::TimeDelta::FromSeconds(2);
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
  Advance(base::TimeDelta::FromSeconds(1));
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
  Advance(base::TimeDelta::FromSeconds(1));
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

// TODO(crbug.com/1177113) Re-enable test
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DISABLED_DoNotRecordEngagement_PlaybackStopped) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html", false);
  Advance(base::TimeDelta::FromSeconds(1));
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
  Advance(base::TimeDelta::FromSeconds(1));
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

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_MAC)
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

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
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
  Advance(base::TimeDelta::FromSeconds(4));
  CloseTab();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       SessionNewTabNavigateSameURL) {
  const GURL& url = http_server().GetURL("/engagement_test.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  OpenTabAsLink(GURL("about:blank"));
  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  browser()->tab_strip_model()->CloseAllTabs();

  ExpectScores(2, 2);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, SessionNewTabSameURL) {
  const GURL& url = http_server().GetURL("/engagement_test.html");

  LoadTestPageAndWaitForPlayAndAudible(url, false);
  AdvanceMeaningfulPlaybackTime();

  OpenTabAndWaitForPlayAndAudible(url);
  AdvanceMeaningfulPlaybackTime();

  browser()->tab_strip_model()->CloseAllTabs();

  ExpectScores(1, 1);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, SessionNewTabSameOrigin) {
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

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       SessionMultipleTabsClosingParent) {
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

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       SessionNewTabNavigateSameURLWithOpener_Typed) {
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

#if defined(OS_WIN)
#define MAYBE_Ignored DISABLED_Ignored
#else
#define MAYBE_Ignored Ignored
#endif
IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, MAYBE_Ignored) {
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
      no_state_prefetch_manager->AddPrerenderFromOmnibox(url, storage_namespace,
                                                         gfx::Size(640, 480));

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
