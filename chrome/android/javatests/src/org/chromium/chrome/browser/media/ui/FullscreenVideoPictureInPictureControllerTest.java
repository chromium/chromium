// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.os.Build;
import android.support.test.InstrumentationRegistry;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for FullscreenVideoPictureInPictureController and related methods.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY})
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@RequiresApi(Build.VERSION_CODES.O)
public class FullscreenVideoPictureInPictureControllerTest {
    // TODO(peconn): Add a test for exit on Tab Reparenting.
    private static final String TEST_PATH = "/chrome/test/data/media/bigbuck-player.html";
    private static final String VIDEO_ID = "video";

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
        CriteriaHelper.pollUiThread(() -> !getWebContents().hasActiveEffectivelyFullscreenVideo());
    }

    /** Tests that we can enter PiP. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testEnterPip() throws Throwable {
        enterFullscreen();
        triggerAutoPiPAndWait();
    }

    /** Tests that PiP is left when we navigate the main page. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @DisabledTest(message = "https://crbug.com/1211930/")
    public void testExitPipOnNavigation() throws Throwable {
        testExitOn(()
                           -> JavaScriptUtils.executeJavaScript(getWebContents(),
                                   "window.location.href = 'https://www.example.com/';"));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=Portals"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testExitPipOnPortalActivation() throws Throwable {
        testExitOn(()
                           -> JavaScriptUtils.executeJavaScript(getWebContents(),
                                   "document.querySelector('portal').activate();"));
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
        testExitOn(() -> WebContentsUtils.simulateRendererKilled(getWebContents()));
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

    /**
     * Tests that a navigation in an iframe other than the fullscreen one does not exit PiP.
     * TODO(jazzhsu): This test is failing because the navigation observer is no longer observing
     * child frame navigation. Should fix this after the navigation observer can observe child
     * frame navigation.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @DisabledTest(message = "https://crbug.com/1211930/#c10")
    public void testNoExitOnIframeNavigation() throws Throwable {
        // Add a TabObserver so we know when the iFrame navigation has occurred before we check that
        // we are still in PiP.
        final NavigationObserver navigationObserver = new NavigationObserver();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getActivityTab().addObserver(navigationObserver));

        enterFullscreen();
        triggerAutoPiPAndWait();

        JavaScriptUtils.executeJavaScript(getWebContents(),
                "document.getElementById('iframe').src = 'https://www.example.com/'");

        CriteriaHelper.pollUiThread(navigationObserver::didNavigationOccur);

        // Wait for isInPictureInPictureMode rather than getLast...ForTesting, since the latter
        // isn't synchronous with navigation occurring.  It has to wait for some back-and-forth with
        // the framework.
        Assert.assertTrue(
                TestThreadUtils.runOnUiThreadBlocking(mActivity::isInPictureInPictureMode));
    }

    /** Tests that we can resume PiP after it has been cancelled. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testReenterPip() throws Throwable {
        enterFullscreen();
        triggerAutoPiPAndWait();

        mActivityTestRule.resumeMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(() -> !mActivity.getLastPictureInPictureModeForTesting());

        enterFullscreen(false);
        triggerAutoPiPAndWait();
    }

    private WebContents getWebContents() {
        return mActivity.getCurrentWebContents();
    }

    private void triggerAutoPiPAndWait() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> InstrumentationRegistry.getInstrumentation().callActivityOnUserLeaving(
                                mActivity));
        CriteriaHelper.pollUiThread(mActivity::getLastPictureInPictureModeForTesting);
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
        CriteriaHelper.pollUiThread(getWebContents()::hasActiveEffectivelyFullscreenVideo);
    }

    private void testExitOn(Runnable runnable) throws Throwable {
        enterFullscreen();
        triggerAutoPiPAndWait();

        runnable.run();

        CriteriaHelper.pollUiThread(() -> !mActivity.getLastPictureInPictureModeForTesting());
    }

    /** A TabObserver that tracks whether a navigation has occurred. */
    private static class NavigationObserver extends EmptyTabObserver {
        private boolean mNavigationOccurred;

        public boolean didNavigationOccur() {
            return mNavigationOccurred;
        }

        @Override
        public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
            mNavigationOccurred = true;
        }
    }
}
