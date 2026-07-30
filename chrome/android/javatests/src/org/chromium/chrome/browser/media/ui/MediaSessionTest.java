// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.content.Intent;
import android.media.AudioManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.browser_ui.media.AudioBecomingNoisyReceiver;
import org.chromium.components.browser_ui.media.MediaFeatureList;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.media.MediaSessionHelper;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Tests for checking whether the media are paused when unplugging the headset */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY,
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE
})
@Batch(Batch.PER_CLASS)
public class MediaSessionTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private static final String TEST_PATH = "/content/test/data/media/session/media-session.html";
    private static final String VIDEO_ID = "long-video";

    private static final long LONG_TIMEOUT = 5000L;
    private static final double MIN_PLAYBACK_PROGRESS_SEC = 1.0;
    private static final long DEFAULT_POLL_INTERVAL = 50L;

    private EmbeddedTestServer mTestServer;

    @After
    public void tearDown() {
        MediaNotificationManager.resetForTesting();
    }

    @Test
    @LargeTest
    @DisableFeatures("NoPauseMediaOnHeadphoneUnplug")
    public void testPauseOnHeadsetUnplug() throws IllegalArgumentException, TimeoutException {
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        Assert.assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        simulateHeadsetUnplug();
        DOMUtils.waitForMediaPauseBeforeEnd(tab.getWebContents(), VIDEO_ID);
    }

    @Test
    @LargeTest
    @EnableFeatures("NoPauseMediaOnHeadphoneUnplug")
    public void testNoPauseOnHeadsetUnplug_FeatureEnabled()
            throws IllegalArgumentException, TimeoutException {
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        Assert.assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        double timeBeforeUnplug = getCurrentTime(tab);

        simulateHeadsetUnplug();

        waitForMediaPlayToProgress(tab, timeBeforeUnplug);
    }

    /**
     * Regression test for crbug.com/40141115.
     *
     * <p>Makes sure the notification info is updated after a navigation from a native page to a
     * site with media.
     */
    @Test
    @LargeTest
    public void mediaSessionUrlUpdatedAfterNativePageNavigation() throws Exception {
        mActivityTestRule.startOnBlankPage();

        Tab tab = mActivityTestRule.getActivityTab();
        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        Assert.assertTrue(tab.getNativePage() instanceof NewTabPage);

        String videoPageUrl = mTestServer.getURL(TEST_PATH);
        new TabLoadObserver(tab).fullyLoadUrl(videoPageUrl);

        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MediaNotificationController controller =
                            MediaNotificationManager.getActiveOrFallbackControllerByMediaTypeId(
                                    R.id.media_playback_notification);
                    Assert.assertEquals(
                            UrlFormatter.formatUrlForSecurityDisplay(
                                    videoPageUrl, SchemeDisplay.OMIT_HTTP_AND_HTTPS),
                            controller.mMediaNotificationInfo.origin);
                });
    }

    @Before
    public void setUp() {
        mTestServer = mActivityTestRule.getTestServer();
    }

    private void waitForNotificationReady() {
        // Extended timeout to avoid flakiness https://crbug.com/40833503
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    if (MediaNotificationManager.getActiveOrFallbackControllerByMediaTypeId(
                                    R.id.media_playback_notification)
                            == null) {
                        return false;
                    }

                    MediaNotificationController controller =
                            MediaNotificationManager.getActiveOrFallbackControllerByMediaTypeId(
                                    R.id.media_playback_notification);
                    controller.mPendingIntentActionSwipe =
                            controller.createPendingIntent(
                                    MediaNotificationController.ACTION_SWIPE);

                    // After creating `mPendingIntentActionSwipe`, wait until the throttler exits
                    // the throttled state.
                    return controller.mThrottler.mThrottleTask == null;
                },
                LONG_TIMEOUT,
                DEFAULT_POLL_INTERVAL);
    }

    private void simulateHeadsetUnplug() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent i = new Intent(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
                    AudioBecomingNoisyReceiver.getInstance()
                            .onReceive(ApplicationProvider.getApplicationContext(), i);
                });
    }

    private void simulateScreenOff() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent i = new Intent(Intent.ACTION_SCREEN_OFF);
                    assumeNonNull(MediaSessionHelper.sInstanceForTesting)
                            .getScreenStateObserverForTesting()
                            .onScreenOff(ApplicationProvider.getApplicationContext(), i);
                });
    }

    private void simulateScreenOn() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent i = new Intent(Intent.ACTION_SCREEN_ON);
                    assumeNonNull(MediaSessionHelper.sInstanceForTesting)
                            .getScreenStateObserverForTesting()
                            .onScreenOn(ApplicationProvider.getApplicationContext(), i);
                });
    }

    @Test
    @LargeTest
    @EnableFeatures(MediaFeatureList.PAUSE_MEDIA_ON_SYSTEM_SLEEP_ANDROID)
    public void testPauseOnScreenOff_FeatureEnabled() throws Exception {
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        Assert.assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        simulateScreenOff();
        // Simulate deep sleep discontinuity
        mFakeTimeTestRule.deepSleepMillis(1500);
        simulateScreenOn();
        DOMUtils.waitForMediaPauseBeforeEnd(tab.getWebContents(), VIDEO_ID);

        // Verify that the system sleep pause did not grant user activation.
        String result =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "navigator.userActivation.isActive");
        Assert.assertEquals("false", result);
    }

    @Test
    @LargeTest
    @DisableFeatures(MediaFeatureList.PAUSE_MEDIA_ON_SYSTEM_SLEEP_ANDROID)
    public void testPauseOnScreenOff_FeatureDisabled() throws Exception {
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        Assert.assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        double timeBeforeScreenOff = getCurrentTime(tab);

        simulateScreenOff();
        // Simulate deep sleep discontinuity
        mFakeTimeTestRule.deepSleepMillis(1500);
        simulateScreenOn();

        waitForMediaPlayToProgress(tab, timeBeforeScreenOff);
    }

    private void waitForMediaPlayToProgress(Tab tab, double previousTime) {
        // Verify that playback continues. Since the media is already playing, simply checking
        // isMediaPaused() == false will return true instantly. To verify it *remains* playing
        // and doesn't pause shortly after the event, we poll until the media's clock
        // (currentTime) has progressed past the cached previousTime by at least 1.0 second.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID),
                                Matchers.is(false));
                        Criteria.checkThat(
                                getCurrentTime(tab),
                                Matchers.greaterThan(previousTime + MIN_PLAYBACK_PROGRESS_SEC));
                    } catch (TimeoutException e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                LONG_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private double getCurrentTime(Tab tab) throws TimeoutException {
        String result =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(),
                        "document.getElementById('" + VIDEO_ID + "').currentTime");
        return Double.parseDouble(result);
    }

    @Test
    @LargeTest
    @DisableFeatures("NoPauseMediaOnHeadphoneUnplug")
    public void testNoAudioBecomingNoisyPausedMetricWhenAlreadyPaused() throws Exception {
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        Assert.assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        // Pause media via DOM
        DOMUtils.pauseMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPauseBeforeEnd(tab.getWebContents(), VIDEO_ID);

        // Ensure the media helper state registers the pause
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                var helper = MediaSessionHelper.sInstanceForTesting;
                                return helper != null
                                        && helper.mNotificationInfoBuilder != null
                                        && helper.mNotificationInfoBuilder.build().isPaused;
                            });
                },
                LONG_TIMEOUT,
                DEFAULT_POLL_INTERVAL);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Media.Android.AudioBecomingNoisyPaused")
                        .build();

        simulateHeadsetUnplug();

        // Wait a short time to verify no metric was recorded
        Thread.sleep(500);
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testReceiverUnregisteredWhenMediaSessionHelperDestroyed() throws Exception {
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        // 1. Play media to ensure helper and observer are active.
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        // Verify receiver is registered.
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> AudioBecomingNoisyReceiver.getInstance().isRegisteredForTesting()));

        // 2. Directly destroy the MediaSessionHelper to simulate final cleanup.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MediaSessionTabHelper helper = MediaSessionTabHelper.from(tab);
                    if (helper != null && helper.mMediaSessionHelper != null) {
                        helper.mMediaSessionHelper.destroy();
                    }
                });

        // Verify receiver is unregistered.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    !AudioBecomingNoisyReceiver.getInstance()
                                            .isRegisteredForTesting());
                },
                LONG_TIMEOUT,
                DEFAULT_POLL_INTERVAL);
    }
}
