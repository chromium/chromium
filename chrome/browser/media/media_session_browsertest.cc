// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"

class MediaSessionBrowserTest : public InProcessBrowserTest {
 public:
  MediaSessionBrowserTest(const MediaSessionBrowserTest&) = delete;
  MediaSessionBrowserTest& operator=(const MediaSessionBrowserTest&) = delete;

 protected:
  MediaSessionBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        media::kHideIncognitoMediaMetadata);
  }

  ~MediaSessionBrowserTest() override = default;

  void PlayVideoAndCheckHideMediaMetadataValue(Browser* browser,
                                               bool expected_hide_metadata) {
    MediaControlsObserver media_controls_observer;
    mojo::Receiver<media_session::mojom::MediaControllerObserver>
        observer_receiver_(&media_controls_observer);
    mojo::Remote<media_session::mojom::MediaControllerManager>
        controller_manager_remote;
    mojo::Remote<media_session::mojom::MediaController> media_controller_remote;
    content::GetMediaSessionService().BindMediaControllerManager(
        controller_manager_remote.BindNewPipeAndPassReceiver());
    controller_manager_remote->CreateActiveMediaController(
        media_controller_remote.BindNewPipeAndPassReceiver());
    media_controller_remote->AddObserver(
        observer_receiver_.BindNewPipeAndPassRemote());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Navigate to a test page with some media on it.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, embedded_test_server()->GetURL(
                     "/media/session/video-with-metadata.html")));

    auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();

    // Start playback.
    ASSERT_EQ(nullptr, content::EvalJs(web_contents, "play()"));

    media_controls_observer.run_loop.Run();

    EXPECT_EQ(media_controls_observer.hide_metadata, expected_hide_metadata);
  }

  class MediaControlsObserver
      : public media_session::mojom::MediaControllerObserver {
   public:
    void MediaSessionInfoChanged(
        media_session::mojom::MediaSessionInfoPtr info) override {
      if (info) {
        hide_metadata = info->hide_metadata;
        if (run_loop.IsRunningOnCurrentThread()) {
          run_loop.Quit();
        }
      }
    }
    void MediaSessionMetadataChanged(
        const absl::optional<media_session::MediaMetadata>& metadata) override {
    }
    void MediaSessionActionsChanged(
        const std::vector<media_session::mojom::MediaSessionAction>& action)
        override {}
    void MediaSessionChanged(
        const absl::optional<base::UnguessableToken>& request_id) override {}
    void MediaSessionPositionChanged(
        const absl::optional<media_session::MediaPosition>& position) override {
    }

    bool hide_metadata;
    base::RunLoop run_loop;
  };

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       MediaSessionInfoDontHideMetadataByDefault) {
  PlayVideoAndCheckHideMediaMetadataValue(browser(), false);
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       MediaSessionInfoHideMetadataIfInIncognito) {
  PlayVideoAndCheckHideMediaMetadataValue(CreateIncognitoBrowser(), true);
}
