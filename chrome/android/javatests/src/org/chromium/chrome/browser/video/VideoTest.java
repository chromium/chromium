// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video;

import android.os.Build;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.PageTitleCondition;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.WebPageFacility;
import org.chromium.content_public.common.ContentFeatures;

/** Simple tests of html5 video. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.DisableFeatures({ContentFeatures.ANDROID_DESKTOP_ZOOM_SCALING})
public class VideoTest {
    // test.webm plays for about 5 s; keep the 15 s margin the old TabTitleObserver wait used.
    private static final int VIDEO_PLAY_TIMEOUT_MS = 15000;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mPage;

    private static class VideoPlayPageFacility extends WebPageFacility {
        public final HtmlElement playButton = declareHtmlElement("#button1");
    }

    @Before
    public void setUp() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
    }

    @Test
    @Feature({"Media", "Media-Video", "Main"})
    @LargeTest
    @DisableIf.Build(
            sdk_equals = Build.VERSION_CODES.Q,
            message = "crbug.com/447426928, crashing emulator with --disable-field-trial-config")
    public void testLoadMediaUrl() {
        VideoPlayPageFacility videoPlayFacility = new VideoPlayPageFacility();
        mPage =
                mPage.loadPageProgrammatically(
                        mActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/media/video-play.html"),
                        WebPageStation.newBuilder()
                                .withExpectedTitle("ready_to_play")
                                .withFacility(videoPlayFacility));
        // Now the video will play for 5 secs.
        // Makes sure that the video ends and title was changed.
        videoPlayFacility
                .playButton
                .clickTo()
                .waitFor(
                        new PageTitleCondition("ended", mPage.loadedTabElement)
                                .withTimeout(VIDEO_PLAY_TIMEOUT_MS));
    }
}
