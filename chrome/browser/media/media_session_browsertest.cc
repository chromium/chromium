// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_client.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"

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

  void PlayVideoWithMetadata(Browser* browser) {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Navigate to a test page with some media on it.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, embedded_test_server()->GetURL(
                     "/media/session/video-with-metadata.html")));

    auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();

    // Start playback.
    ASSERT_EQ(nullptr, content::EvalJs(web_contents, "play()"));
  }

  media_session::MediaMetadata GetExpectedMetadata() {
    media_session::MediaMetadata expected_metadata;

    expected_metadata.title = u"Big Buck Bunny";
    expected_metadata.source_title = base::ASCIIToUTF16(base::StringPrintf(
        "%s:%u", embedded_test_server()->GetIPLiteralString().c_str(),
        embedded_test_server()->port()));

    expected_metadata.album = u"";
    expected_metadata.artist = u"Blender Foundation";

    return expected_metadata;
  }

  media_session::MediaMetadata GetExpectedHiddenMetadata() {
    media_session::MediaMetadata expected_metadata;

    content::MediaSessionClient* media_session_client =
        content::MediaSessionClient::Get();

    expected_metadata.title = media_session_client->GetTitlePlaceholder();
    expected_metadata.source_title =
        media_session_client->GetSourceTitlePlaceholder();
    expected_metadata.album = media_session_client->GetAlbumPlaceholder();
    expected_metadata.artist = media_session_client->GetArtistPlaceholder();

    return expected_metadata;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       MediaSessionInfoDontHideMetadataByDefault) {
  Browser* test_browser = browser();

  media_session::test::MockMediaSessionMojoObserver observer(
      *content::MediaSession::Get(
          test_browser->tab_strip_model()->GetActiveWebContents()));

  PlayVideoWithMetadata(test_browser);

  observer.WaitForExpectedHideMetadata(false);
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       MediaSessionInfoHideMetadataIfInIncognito) {
  Browser* browser = CreateIncognitoBrowser();

  media_session::test::MockMediaSessionMojoObserver observer(
      *content::MediaSession::Get(
          browser->tab_strip_model()->GetActiveWebContents()));

  PlayVideoWithMetadata(browser);

  observer.WaitForExpectedHideMetadata(true);
}

// We hide the media metadata from CrOS' media controls by replacing the
// metadata in the MediaSessionImpl with some placeholder metadata. These
// changes are gated to only affect ChromeOS, hence why the testing for this is
// also ChromeOS only.
#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       MediaSessionInfoIsHiddenInCrOSIncognito) {
  Browser* browser = CreateIncognitoBrowser();

  media_session::test::MockMediaSessionMojoObserver observer(
      *content::MediaSession::Get(
          browser->tab_strip_model()->GetActiveWebContents()));

  PlayVideoWithMetadata(browser);

  observer.WaitForExpectedMetadata(GetExpectedHiddenMetadata());
}
#else  // !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       MediaSessionInfoIsNotHiddenInNonCrOSIncognito) {
  Browser* browser = CreateIncognitoBrowser();

  media_session::test::MockMediaSessionMojoObserver observer(
      *content::MediaSession::Get(
          browser->tab_strip_model()->GetActiveWebContents()));

  PlayVideoWithMetadata(browser);

  observer.WaitForExpectedMetadata(GetExpectedMetadata());
}
#endif
