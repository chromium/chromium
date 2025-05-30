// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PerformancePTTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

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
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION})
    @DisableIf.Device(DeviceFormFactor.TABLET) // Disable on tablet (crbug.com/420861061)
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO) // Disable on automotive (crbug.com/420881807)
    public void zeroCompositorFramesWhileScrollingBrowserControls() {
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

        // Sometimes, we get one browser frame from scrolling the webpage for the first time, but
        // we shouldn't get any more frames afterwards.
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
