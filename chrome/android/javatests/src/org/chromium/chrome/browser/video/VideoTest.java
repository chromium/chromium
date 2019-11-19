// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 *  Simple tests of html5 video.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class VideoTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Test
    @DisableIf.Build(sdk_is_less_than = 19, message = "crbug.com/582067")
    @Feature({"Media", "Media-Video", "Main"})
    @LargeTest
    @RetryOnFailure
    public void testLoadMediaUrl() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        try {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            TabTitleObserver titleObserver = new TabTitleObserver(tab, "ready_to_play");
            mActivityTestRule.loadUrl(
                    testServer.getURL("/chrome/test/data/android/media/video-play.html"));
            titleObserver.waitForTitleUpdate(5);
            Assert.assertEquals("ready_to_play", tab.getTitle());

            titleObserver = new TabTitleObserver(tab, "ended");
            DOMUtils.clickNode(tab.getWebContents(), "button1");
            // Now the video will play for 5 secs.
            // Makes sure that the video ends and title was changed.
            titleObserver.waitForTitleUpdate(15);
            Assert.assertEquals("ended", tab.getTitle());
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }
}
