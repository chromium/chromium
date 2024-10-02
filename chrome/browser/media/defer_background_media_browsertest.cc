// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"

class DeferredMediaBrowserTest : public InProcessBrowserTest {
 public:
  DeferredMediaBrowserTest() {
    feature_list_.InitAndEnableFeature(
        tab_groups::kDeferMediaLoadInBackgroundTab);
  }

  ~DeferredMediaBrowserTest() override = default;

 protected:
  // Verify that media doesn't autoplay on a background tab.
  void VerifyMediaDoesNotAutoPlayInBackground(
      content::WebContents* background_contents) {
    // Verify the tab is in background
    EXPECT_NE(background_contents,
              browser()->tab_strip_model()->GetActiveWebContents());

    // If autoplay for background tabs isn't deferred the play event listener
    // will be attached too late to catch the event, and subsequently the test
    // will hit the ended event before the play event.
    EXPECT_TRUE(
        content::ExecJs(background_contents,
                        "var video = document.querySelector('video');"
                        "video.addEventListener('ended', function(event) {"
                        "  document.title = 'ended';"
                        "}, false);"
                        "video.addEventListener('play', function(event) {"
                        "  document.title = 'playing';"
                        "}, false);"));
    // Make the background tab w/ our video the active tab.
    browser()->tab_strip_model()->SelectNextTab();
    EXPECT_EQ(background_contents,
              browser()->tab_strip_model()->GetActiveWebContents());

    // If everything worked, we should see "playing" and not "ended".
    const std::u16string playing_str = u"playing";
    const std::u16string ended_str = u"ended";
    content::TitleWatcher watcher(background_contents, playing_str);
    watcher.AlsoWaitForTitle(ended_str);
    EXPECT_EQ(playing_str, watcher.WaitAndGetTitle());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeferredMediaBrowserTest, BackgroundMediaIsDeferred) {
  // Navigate to a video file, which would autoplay in the foreground, but won't
  // in the background due to deferred media loading for hidden tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      content::GetFileUrlWithQuery(
          media::GetTestDataFilePath("bear-640x360.webm"), ""),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* background_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(content::WaitForRenderFrameReady(
      background_contents->GetPrimaryMainFrame()));
  EXPECT_NE(background_contents,
            browser()->tab_strip_model()->GetActiveWebContents());

  VerifyMediaDoesNotAutoPlayInBackground(background_contents);
}

// Test that even if a RenderFrame has played media, media is still
// deferred if the URL is loaded from sync.
IN_PROC_BROWSER_TEST_F(
    DeferredMediaBrowserTest,
    HasPlayedMediaBefore_BackgroundMediaIsDeferredOnSyncedURL) {
  // Navigate to a video file.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      content::GetFileUrlWithQuery(
          media::GetTestDataFilePath("bear-640x360.webm"), ""),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  // Upon loading the video, autoplay should happen. Verify that media has
  // played on the page.
  EXPECT_TRUE(
      content::ExecJs(web_contents,
                      "var video = document.querySelector('video');"
                      "video.addEventListener('playing', function(event) {"
                      "  document.title = 'hasPlayed';"
                      "}, false);"
                      "video.addEventListener('play', function(event) {"
                      "  document.title = 'hasPlayed';"
                      "}, false);"
                      "video.addEventListener('ended', function(event) {"
                      "  document.title = 'hasPlayed';"
                      "}, false);"));
  const std::u16string has_played_str = u"hasPlayed";
  content::TitleWatcher watcher(web_contents, has_played_str);
  EXPECT_EQ(has_played_str, watcher.WaitAndGetTitle());

  // Navigate to a new blank tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Simulate a URL load from sync on the background tab.
  TabGroupSyncTabState::Create(web_contents);
  web_contents->GetController().Reload(content::ReloadType::NORMAL,
                                       /*check_for_repost=*/true);
  content::WaitForLoadStop(web_contents);

  // The video is not auto played on the background tab.
  VerifyMediaDoesNotAutoPlayInBackground(web_contents);
}
