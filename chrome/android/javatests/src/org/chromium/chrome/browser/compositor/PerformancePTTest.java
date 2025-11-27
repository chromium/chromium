// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;
import org.chromium.components.feature_engagement.Tracker;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PerformancePTTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tracker mTracker;

    @Before
    public void setUp() {
        // Disable IPH. We want the browser controls to scroll off the screen.
        // An IPH anchored to the toolbar will prevent the browser controls
        // from being scrolled. Sometimes an IPH appears at the location we
        // inject the touch event, which also prevents the controls from being
        // scrolled.
        when(mTracker.shouldTriggerHelpUi(anyString())).thenReturn(false);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    private static class TestFrameRequestObserver
            implements CompositorViewHolder.FrameRequestObserver {
        private int mFrameCount;

        @Override
        public void onFrameRequested() {
            mFrameCount++;
        }

        public int getFrameCount() {
            return mFrameCount;
        }
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/464298925")
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION})
    public void zeroCompositorFramesWhileScrollingBrowserControls() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStateBrowserControlsVisibilityDelegate.disablePageLoadDelayForTests();
                });
        WebPageStation blankPage = mCtaTestRule.startOnBlankPage();
        var topBottomLinkPageAndTop =
                TopBottomLinksPageStation.loadPage(mCtaTestRule.getActivityTestRule(), blankPage);
        TopBottomLinksPageStation topBottomLinkPage = topBottomLinkPageAndTop.first;
        TopBottomLinksPageStation.TopFacility topFacility = topBottomLinkPageAndTop.second;

        CompositorViewHolder compositorViewHolder =
                mCtaTestRule.getActivity().getCompositorViewHolderForTesting();
        TestFrameRequestObserver frameObserver = new TestFrameRequestObserver();

        // Some delayed tasks that are posted in response to page load result in additional frames.
        // Give them time to run before we start counting.
        Thread.sleep(200);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    compositorViewHolder.addFrameRequestObserver(frameObserver);
                });

        TopBottomLinksPageStation.BottomFacility bottomFacility = topFacility.scrollToBottom();
        topFacility = bottomFacility.scrollToTop();
        bottomFacility = topFacility.scrollToBottom();
        topFacility = bottomFacility.scrollToTop();
        bottomFacility = topFacility.scrollToBottom();
        topFacility = bottomFacility.scrollToTop();

        // TODO(https://crbug.com/423953849): We usually get one browser frame from scrolling the
        // webpage the first time after BottomControlsStacker is initialized, but we shouldn't get
        // any more frames afterwards.
        int expectedAtMost = 1;
        int actual = frameObserver.getFrameCount();
        assertTrue(
                "Got "
                        + String.valueOf(actual)
                        + " frame(s) but expected at most "
                        + String.valueOf(expectedAtMost),
                actual <= expectedAtMost);

        TransitAsserts.assertFinalDestination(topBottomLinkPage, topFacility);
    }
}
