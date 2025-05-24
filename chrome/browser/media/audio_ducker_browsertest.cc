// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_ducker.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"

namespace {

constexpr base::FilePath::CharType kTestPage[] =
    FILE_PATH_LITERAL("media/bigbuck-player.html");

constexpr base::FilePath::CharType kTestPageStartWithNoPlayer[] =
    FILE_PATH_LITERAL("media/start-with-no-player.html");

}  // namespace

using media_session::mojom::MediaSessionInfo;

class AudioDuckerBrowserTest : public InProcessBrowserTest {
 public:
  AudioDuckerBrowserTest() {
    feature_list_.InitAndEnableFeature(media::kAudioDucking);
  }
  AudioDuckerBrowserTest(const AudioDuckerBrowserTest&) = delete;
  AudioDuckerBrowserTest& operator=(const AudioDuckerBrowserTest&) = delete;
  ~AudioDuckerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ResetAudioFocusObserver();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void AddVideo(content::WebContents& web_contents) {
    web_contents.GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"addVideo();", base::NullCallback(),
            content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void PlayVideoAndWaitForAudioFocus(content::WebContents& web_contents) {
    ResetAudioFocusObserver();
    web_contents.GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"document.getElementsByTagName('video')[0].play()",
            base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
    WaitForAudioFocusGained();
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

  void WaitForAudioFocusGained() {
    audio_focus_observer_->WaitForGainedEvent();
  }

  void FlushAudioFocusManager() {
    mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
    content::GetMediaSessionService().BindAudioFocusManager(
        audio_focus_remote.BindNewPipeAndPassReceiver());

    base::RunLoop flush_waiter;
    audio_focus_remote->FlushForTesting(flush_waiter.QuitClosure());
    flush_waiter.Run();
  }

  void ExpectMediaSessionState(content::MediaSession& media_session,
                               MediaSessionInfo::SessionState expected_state) {
    base::RunLoop waiter;
    media_session.GetMediaSessionInfo(base::BindOnce(
        [](MediaSessionInfo::SessionState expected_state,
           base::OnceClosure wait_closure,
           media_session::mojom::MediaSessionInfoPtr info) {
          EXPECT_EQ(info->state, expected_state);
          std::move(wait_closure).Run();
        },
        expected_state, waiter.QuitClosure()));
    waiter.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<media_session::test::TestAudioFocusObserver>
      audio_focus_observer_;
};

IN_PROC_BROWSER_TEST_F(AudioDuckerBrowserTest,
                       DucksAudioInOtherTabs_MediaPlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTestPage));

  // Open a test page and start playing a video.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));
  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideoAndWaitForAudioFocus(*web_contents1);

  // Open a second test page and also play a video.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  content::WebContents* web_contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideoAndWaitForAudioFocus(*web_contents2);

  content::MediaSession* media_session1 =
      content::MediaSession::GetIfExists(web_contents1);
  content::MediaSession* media_session2 =
      content::MediaSession::GetIfExists(web_contents2);
  ASSERT_TRUE(media_session1);
  ASSERT_TRUE(media_session2);

  AudioDucker* audio_ducker =
      AudioDucker::GetOrCreateForPage(web_contents2->GetPrimaryPage());

  // Neither page should be ducked.
  ExpectMediaSessionState(*media_session1,
                          MediaSessionInfo::SessionState::kActive);
  ExpectMediaSessionState(*media_session2,
                          MediaSessionInfo::SessionState::kActive);
  EXPECT_EQ(AudioDucker::AudioDuckingState::kNoDucking,
            audio_ducker->GetAudioDuckingState());

  // Tell the AudioDucker associated with the second page to duck other audio.
  EXPECT_TRUE(audio_ducker->StartDuckingOtherAudio());
  FlushAudioFocusManager();

  // The first page should be ducked while the second remains unducked.
  ExpectMediaSessionState(*media_session1,
                          MediaSessionInfo::SessionState::kDucking);
  ExpectMediaSessionState(*media_session2,
                          MediaSessionInfo::SessionState::kActive);
  EXPECT_EQ(AudioDucker::AudioDuckingState::kDucking,
            audio_ducker->GetAudioDuckingState());

  // Tell the AudioDucker to stop ducking.
  EXPECT_TRUE(audio_ducker->StopDuckingOtherAudio());
  FlushAudioFocusManager();

  // Neither page should be ducked.
  ExpectMediaSessionState(*media_session1,
                          MediaSessionInfo::SessionState::kActive);
  ExpectMediaSessionState(*media_session2,
                          MediaSessionInfo::SessionState::kActive);
  EXPECT_EQ(AudioDucker::AudioDuckingState::kNoDucking,
            audio_ducker->GetAudioDuckingState());
}

IN_PROC_BROWSER_TEST_F(AudioDuckerBrowserTest,
                       DucksAudioInOtherTabs_NoMediaPlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTestPage));

  // Open a test page and start playing a video.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));
  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  PlayVideoAndWaitForAudioFocus(*web_contents1);

  // Open a second test page that has no video.
  GURL test_page_no_player_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTestPageStartWithNoPlayer));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page_no_player_url,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  content::WebContents* web_contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::MediaSession* media_session1 =
      content::MediaSession::GetIfExists(web_contents1);
  ASSERT_TRUE(media_session1);
  ASSERT_EQ(nullptr, content::MediaSession::GetIfExists(web_contents2));

  AudioDucker* audio_ducker =
      AudioDucker::GetOrCreateForPage(web_contents2->GetPrimaryPage());

  // The first page should not be ducked.
  ExpectMediaSessionState(*media_session1,
                          MediaSessionInfo::SessionState::kActive);
  EXPECT_EQ(AudioDucker::AudioDuckingState::kNoDucking,
            audio_ducker->GetAudioDuckingState());

  // Tell the AudioDucker associated with the second page to duck other audio.
  EXPECT_TRUE(audio_ducker->StartDuckingOtherAudio());
  FlushAudioFocusManager();

  // The first page should be ducked.
  ExpectMediaSessionState(*media_session1,
                          MediaSessionInfo::SessionState::kDucking);
  EXPECT_EQ(AudioDucker::AudioDuckingState::kDucking,
            audio_ducker->GetAudioDuckingState());

  // Add a video into the second page and play it.
  AddVideo(*web_contents2);
  PlayVideoAndWaitForAudioFocus(*web_contents2);
  content::MediaSession* media_session2 =
      content::MediaSession::GetIfExists(web_contents2);
  ASSERT_TRUE(media_session2);

  // The first page should still be ducked but the second page should not be
  // ducked.
  ExpectMediaSessionState(*media_session1,
                          MediaSessionInfo::SessionState::kDucking);
  ExpectMediaSessionState(*media_session2,
                          MediaSessionInfo::SessionState::kActive);
  EXPECT_EQ(AudioDucker::AudioDuckingState::kDucking,
            audio_ducker->GetAudioDuckingState());

  // Tell the AudioDucker to stop ducking.
  EXPECT_TRUE(audio_ducker->StopDuckingOtherAudio());
  FlushAudioFocusManager();

  // The first page should no longer be ducked.
  ExpectMediaSessionState(*media_session1,
                          MediaSessionInfo::SessionState::kActive);
  ExpectMediaSessionState(*media_session2,
                          MediaSessionInfo::SessionState::kActive);
  EXPECT_EQ(AudioDucker::AudioDuckingState::kNoDucking,
            audio_ducker->GetAudioDuckingState());
}
