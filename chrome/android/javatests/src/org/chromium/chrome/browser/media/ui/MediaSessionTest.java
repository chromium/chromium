// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.content.Intent;
import android.media.AudioManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Tests for checking whether the media are paused when unplugging the headset */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY,
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE
})
public class MediaSessionTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PATH = "/content/test/data/media/session/media-session.html";
    private static final String VIDEO_ID = "long-video";

    private static final long LONG_TIMEOUT = 5000L;
    private static final long DEFAULT_POLL_INTERVAL = 50L;

    private EmbeddedTestServer mTestServer;

    @Test
    @LargeTest
    public void testPauseOnHeadsetUnplug() throws IllegalArgumentException, TimeoutException {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PATH));
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        simulateHeadsetUnplug();
        DOMUtils.waitForMediaPauseBeforeEnd(tab.getWebContents(), VIDEO_ID);
    }

    /**
     * Regression test for crbug.com/1108038.
     *
     * <p>Makes sure the notification info is updated after a navigation from a native page to a
     * site with media.
     */
    @Test
    @LargeTest
    public void mediaSessionUrlUpdatedAfterNativePageNavigation() throws Exception {
        mActivityTestRule.startMainActivityWithURL("about:blank");

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
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
                            MediaNotificationManager.getController(
                                    R.id.media_playback_notification);
                    Assert.assertEquals(
                            UrlFormatter.formatUrlForSecurityDisplay(
                                    videoPageUrl, SchemeDisplay.OMIT_HTTP_AND_HTTPS),
                            controller.mMediaNotificationInfo.origin);
                });
    }

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
    }

    private void waitForNotificationReady() {
        // Extended timeout to avoid flakiness https://crbug.com/1315419
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return MediaNotificationManager.getController(R.id.media_playback_notification)
                            != null;
                },
                LONG_TIMEOUT,
                DEFAULT_POLL_INTERVAL);
    }

    private void simulateHeadsetUnplug() {
        Intent i =
                new Intent(
                        ApplicationProvider.getApplicationContext(),
                        ChromeMediaNotificationControllerServices.PlaybackListenerService.class);
        i.setAction(AudioManager.ACTION_AUDIO_BECOMING_NOISY);

        ApplicationProvider.getApplicationContext().startService(i);
    }
}
