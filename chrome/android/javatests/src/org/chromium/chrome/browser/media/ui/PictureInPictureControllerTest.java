// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for PictureInPictureController and related methods.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY})
@TargetApi(Build.VERSION_CODES.O)
public class PictureInPictureControllerTest {
    // TODO(peconn): Add a test for exit on Tab Reparenting.
    private static final String TEST_PATH = "/chrome/test/data/media/bigbuck-player.html";
    private static final String VIDEO_ID = "video";

    @Rule
    public UiThreadTestRule mUiThreadTestRule = new UiThreadTestRule();
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PATH));
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /** Tests that we can detect when a video is playing fullscreen, a prerequisite for PiP. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testFullscreenVideoDetected() throws Throwable {
        enterFullscreen();
    }

    /** Tests that fullscreen detection only applies to playing videos. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testFullscreenVideoDetectedOnlyWhenPlaying() throws Throwable {
        enterFullscreen();

        DOMUtils.pauseMedia(getWebContents(), VIDEO_ID);
        CriteriaHelper.pollUiThread(
                Criteria.equals(false, getWebContents()::hasActiveEffectivelyFullscreenVideo));
    }

    /** Tests that we can enter PiP. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testEnterPip() throws Throwable {
        enterFullscreen();
        triggerAutoPiP();

        CriteriaHelper.pollUiThread(Criteria.equals(true, mActivity::isInPictureInPictureMode));
    }

    /** Tests that PiP is left when we navigate the main page. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testExitPipOnNavigation() throws Throwable {
        testExitOn(() -> JavaScriptUtils.executeJavaScript(getWebContents(),
                "window.location.href = 'https://www.example.com/';"));
    }

    /** Tests that PiP is left when the video leaves fullscreen. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testExitOnLeaveFullscreen() throws Throwable {
        testExitOn(() -> DOMUtils.exitFullscreen(getWebContents()));
    }

    /** Tests that PiP is left when the active Tab is closed. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testExitOnCloseTab() throws Throwable {
        // We want 2 Tabs so we can close the first without any special behaviour.
        mActivityTestRule.loadUrlInNewTab(mTestServer.getURL(TEST_PATH));

        testExitOn(() -> JavaScriptUtils.executeJavaScript(getWebContents(), "window.close()"));
    }

    /** Tests that PiP is left when the renderer crashes. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testExitOnCrash() throws Throwable {
        testExitOn(() -> WebContentsUtils.simulateRendererKilled(getWebContents(), false));
    }

    /** Tests that PiP is left when a new Tab is created in the foreground. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testExitOnNewForegroundTab() throws Throwable {
        testExitOn(new Runnable() {
            @Override
            public void run() {
                try {
                    mActivityTestRule.loadUrlInNewTab("https://www.example.com/");
                } catch (Exception e) {
                    throw new RuntimeException();
                }
            }
        });
    }

    /** Tests that a navigation in an iframe other than the fullscreen one does not exit PiP. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testNoExitOnIframeNavigation() throws Throwable {
        // Add a TabObserver so we know when the iFrame navigation has occurred before we check that
        // we are still in PiP.
        final NavigationObserver navigationObserver = new NavigationObserver();
        mActivity.getActivityTab().addObserver(navigationObserver);

        enterFullscreen();
        triggerAutoPiP();
        CriteriaHelper.pollUiThread(Criteria.equals(true, mActivity::isInPictureInPictureMode));

        JavaScriptUtils.executeJavaScript(getWebContents(),
                "document.getElementById('iframe').src = 'https://www.example.com/'");

        CriteriaHelper.pollUiThread(Criteria.equals(true, navigationObserver::didNavigationOccur));

        Assert.assertTrue(
                TestThreadUtils.runOnUiThreadBlocking(mActivity::isInPictureInPictureMode));
    }

    /** Tests that we can resume PiP after it has been cancelled. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testReenterPip() throws Throwable {
        enterFullscreen();
        triggerAutoPiP();
        CriteriaHelper.pollUiThread(Criteria.equals(true, mActivity::isInPictureInPictureMode));

        mActivityTestRule.startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(Criteria.equals(false, mActivity::isInPictureInPictureMode));

        enterFullscreen(false);
        triggerAutoPiP();
        CriteriaHelper.pollUiThread(Criteria.equals(true, mActivity::isInPictureInPictureMode));
    }

    private WebContents getWebContents() {
        return mActivity.getCurrentWebContents();
    }

    private void triggerAutoPiP() throws Throwable{
        mUiThreadTestRule.runOnUiThread(
                () -> InstrumentationRegistry.getInstrumentation().callActivityOnUserLeaving(
                                mActivity));
    }

    private void enterFullscreen() throws Throwable {
        enterFullscreen(true);
    }

    private void enterFullscreen(boolean firstPlay) throws Throwable {
        // Start playback to guarantee it's properly loaded.
        if (firstPlay) Assert.assertTrue(DOMUtils.isMediaPaused(getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(getWebContents(), VIDEO_ID);

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(DOMUtils.clickNode(getWebContents(), "fullscreen",
                true /* goThroughRootAndroidView */, false /* shouldScrollIntoView */));

        // We use the web contents fullscreen heuristic.
        CriteriaHelper.pollUiThread(
                Criteria.equals(true, getWebContents()::hasActiveEffectivelyFullscreenVideo));
    }

    private void testExitOn(Runnable runnable) throws Throwable {
        enterFullscreen();
        triggerAutoPiP();
        CriteriaHelper.pollUiThread(Criteria.equals(true, mActivity::isInPictureInPictureMode));

        runnable.run();

        CriteriaHelper.pollUiThread(Criteria.equals(false, mActivity::isInPictureInPictureMode));
    }

    /** A TabObserver that tracks whether a navigation has occurred. */
    private static class NavigationObserver extends EmptyTabObserver {
        private boolean mNavigationOccurred;

        public boolean didNavigationOccur() {
            return mNavigationOccurred;
        }

        @Override
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
            mNavigationOccurred = true;
        }
    }
}
