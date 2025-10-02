// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video;

import android.os.Build;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/** Simple tests of html5 video. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class VideoTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mPage;

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
    public void testLoadMediaUrl() throws TimeoutException {
        Tab tab = mPage.getTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, "ready_to_play");
        mActivityTestRule.loadUrl(
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/media/video-play.html"));
        titleObserver.waitForTitleUpdate(5);
        Assert.assertEquals("ready_to_play", ChromeTabUtils.getTitleOnUiThread(tab));
        titleObserver = new TabTitleObserver(tab, "ended");
        DOMUtils.clickNode(tab.getWebContents(), "button1");
        // Now the video will play for 5 secs.
        // Makes sure that the video ends and title was changed.
        titleObserver.waitForTitleUpdate(15);
        Assert.assertEquals("ended", ChromeTabUtils.getTitleOnUiThread(tab));
    }
}
