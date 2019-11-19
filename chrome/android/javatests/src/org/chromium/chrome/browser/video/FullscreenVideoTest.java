// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video;

import android.graphics.Rect;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.KeyEvent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/**
 * Test suite for fullscreen video implementation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY})
public class FullscreenVideoTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final int TEST_TIMEOUT = 3000;
    private boolean mIsTabFullscreen;
    private ChromeActivity mActivity;

    private class FullscreenTabObserver extends EmptyTabObserver {
        @Override
        public void onEnterFullscreenMode(Tab tab, FullscreenOptions options) {
            mIsTabFullscreen = true;
        }
        @Override
        public void onExitFullscreenMode(Tab tab) {
            mIsTabFullscreen = false;
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    /**
     * Test that when playing a fullscreen video, hitting the back button will let the tab
     * exit fullscreen mode without changing its URL.
     *
     * @MediumTest
     */
    @Test
    @FlakyTest(message = "crbug.com/458368")
    public void testExitFullscreenNotifiesTabObservers() {
        String url = mTestServerRule.getServer().getURL(
                "/chrome/test/data/android/media/video-fullscreen.html");
        mActivityTestRule.loadUrl(url);
        Tab tab = mActivity.getActivityTab();
        FullscreenTabObserver observer = new FullscreenTabObserver();
        tab.addObserver(observer);

        TestTouchUtils.singleClickView(
                InstrumentationRegistry.getInstrumentation(), tab.getView(), 500, 500);
        waitForVideoToEnterFullscreen();
        // Key events have to be dispached on UI thread.
        KeyUtils.singleKeyEventActivity(
                InstrumentationRegistry.getInstrumentation(), mActivity, KeyEvent.KEYCODE_BACK);

        waitForTabToExitFullscreen();
        Assert.assertEquals("URL mismatch after exiting fullscreen video", url,
                mActivity.getActivityTab().getUrl());
    }

    /**
     * Tests that the dimensions of the fullscreen video are propagated correctly.
     */
    @Test
    @MediumTest
    public void testFullscreenDimensions() throws TimeoutException {
        String url =
                mTestServerRule.getServer().getURL("/content/test/data/media/video-player.html");
        String video = "video";
        Rect expectedSize = new Rect(0, 0, 320, 180);

        mActivityTestRule.loadUrl(url);

        final Tab tab = mActivity.getActivityTab();
        FullscreenTabObserver observer = new FullscreenTabObserver();
        tab.addObserver(observer);

        // Start playback to guarantee it's properly loaded.
        WebContents webContents = mActivity.getCurrentWebContents();
        Assert.assertTrue(DOMUtils.isMediaPaused(webContents, video));
        DOMUtils.playMedia(webContents, video);
        DOMUtils.waitForMediaPlay(webContents, video);

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(DOMUtils.clickNode(webContents, "fullscreen"));

        waitForVideoToEnterFullscreen();

        // It can take a while for the fullscreen video to register.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return tab.getWebContents().getFullscreenVideoSize() != null;
            }
        });

        Assert.assertEquals(expectedSize, tab.getWebContents().getFullscreenVideoSize());
    }

    void waitForVideoToEnterFullscreen() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mIsTabFullscreen;
            }
        }, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    void waitForTabToExitFullscreen() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mIsTabFullscreen;
            }
        }, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
