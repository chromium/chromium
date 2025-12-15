// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ViewportTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManagerTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;

/** Browser test for {@link TopControlsStacker}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION
})
@EnableFeatures({
    ChromeFeatureList.TOP_CONTROLS_REFACTOR,
    ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2
})
public class BrowserControlsPTTest {

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private TopBottomLinksPageStation.TopFacility mTopFacility;
    private ChromeTabbedActivity mActivity;
    private View mControlContainer;
    private ViewportTestUtils mViewportTestUtils;

    @Before
    public void setup() {
        mViewportTestUtils = new ViewportTestUtils(mActivityTestRule);
        mViewportTestUtils.setUpForBrowserControls();

        var blankPage = mActivityTestRule.startOnBlankPage();
        var pair =
                TopBottomLinksPageStation.loadPage(
                        mActivityTestRule.getActivityTestRule(), blankPage);
        mTopFacility = pair.second;
        mActivity = mActivityTestRule.getActivity();
        mControlContainer = mActivity.findViewById(R.id.control_container);

        FullscreenManagerTestUtils.waitForBrowserControlsToBeMoveable(mActivity);
    }

    @Test
    @LargeTest
    public void topControlsScroll() {
        waitForControlsVisibility(true);

        var bottomFacility = mTopFacility.scrollToBottom();
        waitForControlsVisibility(false);

        bottomFacility.scrollToTop();
        waitForControlsVisibility(true);
    }

    @Test
    @LargeTest
    public void topControlsScrollingDisabled() {
        // before test case - set scrolling disabled
        TopControlsStacker topControlsStacker =
                mActivity.getRootUiCoordinatorForTesting().getTopControlsStacker();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    topControlsStacker.setScrollingDisabled(true);
                    topControlsStacker.requestLayerUpdateSync(false);
                });

        mTopFacility.scrollToBottom();
        waitForControlsVisibility(true);

        // after test case - set scrolling back.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    topControlsStacker.setScrollingDisabled(false);
                    topControlsStacker.requestLayerUpdateSync(false);
                });
    }

    private void waitForControlsVisibility(boolean visible) {
        mViewportTestUtils.waitForBrowserControlsState(visible);
        int expectedVisibility = visible ? View.VISIBLE : View.INVISIBLE;
        CriteriaHelper.pollUiThread(() -> mControlContainer.getVisibility() == expectedVisibility);
    }
}
