// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.media.FullscreenVideoPictureInPictureController;
import org.chromium.chrome.browser.media.FullscreenVideoPictureInPictureController.MetricsEndReason;
import org.chromium.chrome.browser.media.FullscreenVideoPictureInPictureController.PipEntered;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.media.MediaSwitches;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for FullscreenVideoPictureInPictureController and related methods. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY
})
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO) // PiP not supported on AAOS.
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/481444525
@EnableFeatures(ChromeFeatureList.FULLSCREEN_VIDEO_PICTURE_IN_PICTURE)
public class FullscreenVideoPictureInPictureControllerTest {
    // TODO(peconn): Add a test for exit on Tab Reparenting.
    private static final String TEST_PATH = "/chrome/test/data/media/bigbuck-player.html";
    private static final String VIDEO_ID = "video";
    private static final long PIP_TIMEOUT_MS = 10000L;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        mActivity = mActivityTestRule.getActivity();
    }

    /** Tests that we can detect when a video is playing fullscreen, a prerequisite for PiP. */
    @Test
    @MediumTest
    public void testFullscreenVideoDetected() throws Throwable {
        enterFullscreen();
    }

    /** Tests that fullscreen detection only applies to playing videos. */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/335305496")
    public void testFullscreenVideoDetectedOnlyWhenPlaying() throws Throwable {
        enterFullscreen();

        DOMUtils.pauseMedia(getWebContents(), VIDEO_ID);
        CriteriaHelper.pollUiThread(() -> !getWebContents().hasActiveEffectivelyFullscreenVideo());
    }

    /** Tests that we can enter PiP. */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/339501283")
    public void testEnterPip() throws Throwable {
        enterFullscreen();
        triggerAutoPiPAndWait();

        // Exit Picture in Picture.
        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> InstrumentationRegistry.getInstrumentation().callActivityOnStop(mActivity));
        CriteriaHelper.pollUiThread(
                AsyncInitializationActivity::wasMoveTaskToBackInterceptedForTesting);
    }

    /** Tests that PiP is not entered when the feature is disabled. */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.FULLSCREEN_VIDEO_PICTURE_IN_PICTURE)
    public void testNoPipWhenDisabled() throws Throwable {
        enterFullscreen();

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        InstrumentationRegistry.getInstrumentation()
                                .callActivityOnUserLeaving(mActivity));

        // Wait a bit to ensure it doesn't enter PiP.
        // If it was going to enter PiP, it would have happened already or shortly after
        // callActivityOnUserLeaving.
        Thread.sleep(1000);
        Assert.assertFalse(ThreadUtils.runOnUiThreadBlocking(mActivity::isInPictureInPictureMode));
    }

    /** Tests that PiP is left when we navigate the main page. */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/40767862/")
    public void testExitPipOnNavigation() throws Throwable {
        testExitOn(
                () ->
                        JavaScriptUtils.executeJavaScript(
                                getWebContents(),
                                "window.location.href = 'https://www.example.com/';"));
    }

    /** Tests that PiP is left when the video leaves fullscreen. */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/348618570")
    public void testExitOnLeaveFullscreen() throws Throwable {
        testExitOn(() -> DOMUtils.exitFullscreen(getWebContents()));
    }

    /** Tests that PiP is left when the active Tab is closed. */
    @Test
    @MediumTest
    @DisabledTest(message = "b/354013006")
    public void testExitOnCloseTab() throws Throwable {
        // We want 2 Tabs so we can close the first without any special behaviour.
        mActivityTestRule.loadUrlInNewTab(mActivityTestRule.getTestServer().getURL(TEST_PATH));

        testExitOn(() -> JavaScriptUtils.executeJavaScript(getWebContents(), "window.close()"));
    }

    /** Tests that PiP is left when the renderer crashes. */
    @Test
    @MediumTest
    public void testExitOnCrash() throws Throwable {
        testExitOn(() -> WebContentsUtils.simulateRendererKilled(getWebContents()));
    }

    /** Tests that PiP is left when a new Tab is created in the foreground. */
    @Test
    @MediumTest
    public void testExitOnNewForegroundTab() throws Throwable {
        testExitOn(
                new Runnable() {
                    @Override
                    public void run() {
                        try {
                            mActivityTestRule.loadUrlInNewTab(
                                    mActivityTestRule.getTestServer().getURL(TEST_PATH));
                        } catch (Exception e) {
                            throw new RuntimeException();
                        }
                    }
                });
    }

    /**
     * Tests that a navigation in an iframe other than the fullscreen one does not exit PiP.
     * TODO(jazzhsu): This test is failing because the navigation observer is no longer observing
     * child frame navigation. Should fix this after the navigation observer can observe child frame
     * navigation.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/40767862/#c10")
    public void testNoExitOnIframeNavigation() throws Throwable {
        // Add a TabObserver so we know when the iFrame navigation has occurred before we check that
        // we are still in PiP.
        final NavigationObserver navigationObserver = new NavigationObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getActivityTab().addObserver(navigationObserver));

        enterFullscreen();
        triggerAutoPiPAndWait();

        JavaScriptUtils.executeJavaScript(
                getWebContents(),
                "document.getElementById('iframe').src = 'https://www.example.com/'");

        CriteriaHelper.pollUiThread(navigationObserver::didNavigationOccur);

        // Wait for isInPictureInPictureMode rather than getLast...ForTesting, since the latter
        // isn't synchronous with navigation occurring.  It has to wait for some back-and-forth with
        // the framework.
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(mActivity::isInPictureInPictureMode));
    }

    /** Tests that we can resume PiP after it has been cancelled. */
    @Test
    @MediumTest
    public void testReenterPip() throws Throwable {
        enterFullscreen();
        triggerAutoPiPAndWait();
        exitPipAndFullscreenAndWait();

        mActivityTestRule.launchMainActivityFromLauncher();

        // Open a new tab and wait for it to load.
        mActivityTestRule.loadUrlInNewTab(mActivityTestRule.getTestServer().getURL(TEST_PATH));

        // Wait for the new tab to be active and ready.
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = mActivityTestRule.getActivityTab();
                    return tab != null && tab.getWebContents() != null && !tab.isClosing();
                },
                "New tab should be active and ready",
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        enterFullscreen(true);
        triggerAutoPiPAndWait();
    }

    private void exitPipAndFullscreenAndWait() throws Throwable {
        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        JavaScriptUtils.executeJavaScript(getWebContents(), "document.exitFullscreen()");

        CriteriaHelper.pollUiThread(
                () -> !getWebContents().hasActiveEffectivelyFullscreenVideo(),
                "Engine should not have fullscreen video",
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        final Tab tab = mActivityTestRule.getActivityTab();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            tab.getWebContents().getFullscreenVideoSize(), Matchers.nullValue());
                },
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        CriteriaHelper.pollUiThread(
                AsyncInitializationActivity::wasMoveTaskToBackInterceptedForTesting,
                "Chrome should have attempted dismissal after exitFullscreen",
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        // Since we intercept moveTaskToBack, the framework won't automatically send the signal.
        mActivity.onPictureInPictureModeChanged(false, mActivity.getResources().getConfiguration());

        // Wait for Chrome to acknowledge PiP exit.
        CriteriaHelper.pollUiThread(
                () -> !mActivity.getLastPictureInPictureModeForTesting(),
                "Chrome should have acknowledged PiP exit",
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getWebContents();
    }

    private void triggerAutoPiPAndWait() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        InstrumentationRegistry.getInstrumentation()
                                .callActivityOnUserLeaving(mActivity));

        // Wait for Chrome to process the callback.
        CriteriaHelper.pollUiThread(
                mActivity::getLastPictureInPictureModeForTesting,
                "Chrome should have acknowledged PiP mode",
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
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
        Assert.assertTrue(
                DOMUtils.clickNode(
                        getWebContents(),
                        "fullscreen",
                        /* goThroughRootAndroidView= */ true,
                        /* shouldScrollIntoView= */ false));

        // We use the web contents fullscreen heuristic.
        CriteriaHelper.pollUiThread(
                getWebContents()::hasActiveEffectivelyFullscreenVideo,
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        // It can take a while for the fullscreen video to register.
        final Tab tab = mActivityTestRule.getActivityTab();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            tab.getWebContents().getFullscreenVideoSize(), Matchers.notNullValue());
                },
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void testExitOn(Runnable runnable) throws Throwable {
        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();

        enterFullscreen();
        triggerAutoPiPAndWait();

        runnable.run();

        CriteriaHelper.pollUiThread(
                AsyncInitializationActivity::wasMoveTaskToBackInterceptedForTesting,
                "Failed to move task to the background.",
                PIP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        // This logic would run if we hadn't intercepted moveTaskToBack (which is how PiP gets
        // exited), so run it now just in case.
        mActivity.onPictureInPictureModeChanged(false, mActivity.getResources().getConfiguration());
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

    /** Tests that we exit PiP whe device is locked. */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/335422062")
    public void testExitPipWhenDeviceLocked() throws Throwable {
        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        enterFullscreen();
        triggerAutoPiPAndWait();

        // Ensure that we entered Picture in Picture.
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(mActivity::isInPictureInPictureMode));

        // Call activity OnStop. This simulates user locking the device.
        ThreadUtils.runOnUiThreadBlocking(
                () -> InstrumentationRegistry.getInstrumentation().callActivityOnStop(mActivity));

        CriteriaHelper.pollUiThread(
                AsyncInitializationActivity::wasMoveTaskToBackInterceptedForTesting);
        // This logic would run if we hadn't intercepted moveTaskToBack (which is how PiP gets
        // exited), we run it now for completion and proceed to ensure we exited Picture in Picture.
        mActivity.onPictureInPictureModeChanged(false, mActivity.getResources().getConfiguration());
        CriteriaHelper.pollUiThread(() -> !mActivity.getLastPictureInPictureModeForTesting());
    }

    @Test
    @MediumTest
    public void testMetricsRecorded() throws Throwable {
        HistogramWatcher enteredWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FullscreenVideoPictureInPictureController.ENTERED_HISTOGRAM,
                                PipEntered.ENTERED)
                        .build();

        enterFullscreen();
        triggerAutoPiPAndWait();

        enteredWatcher.assertExpected();

        HistogramWatcher exitWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FullscreenVideoPictureInPictureController.EXIT_REASON_HISTOGRAM,
                                MetricsEndReason.LEFT_FULLSCREEN)
                        .expectAnyRecord(
                                FullscreenVideoPictureInPictureController.DURATION_HISTOGRAM)
                        .build();

        exitPipAndFullscreenAndWait();

        exitWatcher.assertExpected();
    }
}
