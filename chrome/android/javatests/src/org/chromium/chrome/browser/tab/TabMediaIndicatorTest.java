// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.media.MediaSwitches;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for {@link Tab.MediaState}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY,
    ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
    "enable-features=EnableAudioMonitoringOnAndroid",
})
@Batch(Batch.PER_CLASS)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@EnableFeatures({ChromeFeatureList.MEDIA_INDICATORS_ANDROID})
public class TabMediaIndicatorTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEST_PATH = "/chrome/test/data/media/tab_media_indicator.html";
    private static final String VIDEO_ID = "video";
    private static final String MUTE_VIDEO_ID = "mute";
    private static final String UNMUTE_VIDEO_ID = "unmute";
    private static final String REQUEST_MIC_ID = "request-mic";
    private static final String REQUEST_CAM_ID = "request-cam";
    private static final long WAIT = 2000;

    public TabModel mTabModel;
    public Tab mTab;

    @Before
    public void setUp() throws Exception {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        mTabModel = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        mTab = page.getTab();

        new TabLoadObserver(mTab).fullyLoadUrl(mActivityTestRule.getTestServer().getURL(TEST_PATH));
        DOMUtils.waitForNonZeroNodeBounds(mTab.getWebContents(), VIDEO_ID);
        Assert.assertEquals(Tab.MediaState.NONE, mTab.getMediaState());
    }

    @Test
    @SmallTest
    public void testMediaStateAudible() throws TimeoutException {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.AUDIBLE);
    }

    @Test
    @SmallTest
    public void testMediaStateMuted() throws TimeoutException {
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.MUTED);
    }

    @Test
    @SmallTest
    public void testMediaStateMutedThenUnmute() throws TimeoutException {
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.MUTED);
        setMuteState(false);
        waitForMediaState(Tab.MediaState.AUDIBLE);
    }

    @Test
    @SmallTest
    public void testMediaStateAudibleThenMute() throws TimeoutException {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.AUDIBLE);
        setMuteState(true);
        waitForMediaState(Tab.MediaState.MUTED);
    }

    @Test
    @SmallTest
    public void testMediaStateAudibleMuteWithPause() throws Exception {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.AUDIBLE);

        // Pause video.
        DOMUtils.pauseMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPauseBeforeEnd(mTab.getWebContents(), VIDEO_ID);

        // Wait for recently audible to update.
        Thread.sleep(WAIT);
        waitForMediaState(Tab.MediaState.NONE);

        // Mute video.
        setMuteState(true);
        Assert.assertEquals(Tab.MediaState.NONE, mTab.getMediaState());

        // Play the video again.
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.MUTED);
    }

    @Test
    @SmallTest
    public void testMediaStateWithVideoMutedAndUnmuted() throws Exception {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.AUDIBLE);

        // Mute video element.
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), MUTE_VIDEO_ID);

        // Wait for recently audible to update.
        Thread.sleep(WAIT);
        Assert.assertFalse(DOMUtils.isMediaPaused(mTab.getWebContents(), VIDEO_ID));
        waitForMediaState(Tab.MediaState.NONE);

        // Unmute video element.
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), UNMUTE_VIDEO_ID);
        waitForMediaState(Tab.MediaState.AUDIBLE);
    }

    @Test
    @SmallTest
    public void testMediaStateRecordingMic() throws InterruptedException {
        requestRecording(REQUEST_MIC_ID);
        waitForMediaState(Tab.MediaState.RECORDING);
    }

    @Test
    @SmallTest
    public void testMediaStateRecordingCam() throws InterruptedException {
        requestRecording(REQUEST_CAM_ID);
        waitForMediaState(Tab.MediaState.RECORDING);
    }

    @Test
    @SmallTest
    public void testMediaStatePriority() throws Exception {
        Assert.assertEquals(Tab.MediaState.NONE, mTab.getMediaState());

        // MUTED
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.MUTED);

        // AUDIBLE
        setMuteState(false);
        waitForMediaState(Tab.MediaState.AUDIBLE);

        // RECORDING
        requestRecording(REQUEST_MIC_ID);
        waitForMediaState(Tab.MediaState.RECORDING);
    }

    private void requestRecording(String id) throws InterruptedException {
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), id);
        Thread.sleep(WAIT);
        onViewWaiting(withText("Allow this time")).perform(click());
    }

    private void waitForMediaState(@Tab.MediaState int expectedState) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab media state should be " + expectedState,
                            mTab.getMediaState(),
                            Matchers.is(expectedState));
                });
    }

    private void setMuteState(boolean mute) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.setMuteSetting(List.of(mTab), mute);
                });
    }
}
