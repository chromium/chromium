// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video;

import android.graphics.Rect;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/** Test suite for fullscreen video implementation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY
})
@Batch(Batch.PER_CLASS)
public class FullscreenVideoTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private ChromeActivity mActivity;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    /**
     * Test that when playing a fullscreen video, hitting the back button will let the tab exit
     * fullscreen mode without changing its URL.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "Flaky https://crbug.com/458368 https://crbug.com/1331504")
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testExitFullscreenNotifiesTabObservers() {
        testExitFullscreenNotifiesTabObserversInternal();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky https://crbug.com/458368 https://crbug.com/1331504")
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testExitFullscreenNotifiesTabObservers_backGestureRefactor() {
        testExitFullscreenNotifiesTabObserversInternal();
    }

    private void testExitFullscreenNotifiesTabObserversInternal() {
        String url = launchOnFullscreenMode();

        Espresso.pressBack();

        waitForTabToExitFullscreen();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "URL mismatch after exiting fullscreen video",
                            url,
                            mActivity.getActivityTab().getUrl().getSpec());
                });
    }

    /** Tests that the dimensions of the fullscreen video are propagated correctly. */
    @Test
    @MediumTest
    public void testFullscreenDimensions() throws TimeoutException {
        String url =
                mTestServerRule.getServer().getURL("/content/test/data/media/video-player.html");
        String video = "video";
        Rect expectedSize = new Rect(0, 0, 320, 180);

        mActivityTestRule.loadUrl(url);

        final Tab tab = mActivity.getActivityTab();

        // Start playback to guarantee it's properly loaded.
        WebContents webContents = mActivity.getCurrentWebContents();
        Assert.assertTrue(DOMUtils.isMediaPaused(webContents, video));
        DOMUtils.playMedia(webContents, video);
        DOMUtils.waitForMediaPlay(webContents, video);

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(DOMUtils.clickNode(webContents, "fullscreen"));

        waitForVideoToEnterFullscreen();

        // It can take a while for the fullscreen video to register.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            tab.getWebContents().getFullscreenVideoSize(), Matchers.notNullValue());
                });

        Assert.assertEquals(expectedSize, tab.getWebContents().getFullscreenVideoSize());
    }

    private String launchOnFullscreenMode() {
        String url =
                mTestServerRule
                        .getServer()
                        .getURL("/chrome/test/data/android/media/video-fullscreen.html");
        mActivityTestRule.loadUrl(url);
        final Tab tab = mActivity.getActivityTab();

        TestTouchUtils.singleClickView(
                InstrumentationRegistry.getInstrumentation(), tab.getView(), 500, 500);
        waitForVideoToEnterFullscreen();
        return url;
    }

    void waitForVideoToEnterFullscreen() {
        FullscreenTestUtils.waitForFullscreenFlag(mActivity.getActivityTab(), true, mActivity);
    }

    void waitForTabToExitFullscreen() {
        FullscreenTestUtils.waitForFullscreenFlag(mActivity.getActivityTab(), false, mActivity);
    }
}
