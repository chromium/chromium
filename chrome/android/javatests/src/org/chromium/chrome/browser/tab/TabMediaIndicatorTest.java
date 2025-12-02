// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

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
import org.chromium.media.MediaSwitches;

import java.util.List;

/** Tests for {@link Tab.MediaState}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY,
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
    private static final long RECENTLY_AUDIBLE_TIMEOUT = 3000;

    public TabModel mTabModel;
    public Tab mTab;

    // TODO(crbug.com/454045510): Add tests for the other media states (recording, sharing).
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
    public void testMediaStateAudible() throws Exception {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.AUDIBLE);
    }

    @Test
    @SmallTest
    public void testMediaStateMuted() throws Exception {
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.MUTED);
    }

    @Test
    @SmallTest
    public void testMediaStateMutedThenUnmute() throws Exception {
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(Tab.MediaState.MUTED);
        setMuteState(false);
        waitForMediaState(Tab.MediaState.AUDIBLE);
    }

    @Test
    @SmallTest
    public void testMediaStateAudibleThenMute() throws Exception {
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
        Thread.sleep(RECENTLY_AUDIBLE_TIMEOUT);
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
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), "muteButton");

        // Wait for recently audible to update.
        Thread.sleep(RECENTLY_AUDIBLE_TIMEOUT);
        Assert.assertFalse(DOMUtils.isMediaPaused(mTab.getWebContents(), VIDEO_ID));
        waitForMediaState(Tab.MediaState.NONE);

        // Unmute video element.
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), "unmuteButton");
        waitForMediaState(Tab.MediaState.AUDIBLE);
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
