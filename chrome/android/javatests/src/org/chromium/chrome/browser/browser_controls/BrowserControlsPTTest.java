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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManagerTestUtils;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
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
public class BrowserControlsPTTest {

    @Rule
    public AutoResetCtaTransitTestRule sActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private TopBottomLinksPageStation.TopFacility mTopFacility;
    private ChromeTabbedActivity mActivity;
    private View mControlContainer;

    @Before
    public void setup() {
        var blankPage = sActivityTestRule.startOnBlankPage();
        var pair =
                TopBottomLinksPageStation.loadPage(
                        sActivityTestRule.getActivityTestRule(), blankPage);
        mTopFacility = pair.second;
        mActivity = sActivityTestRule.getActivity();
        mControlContainer = mActivity.findViewById(R.id.control_container);

        FullscreenManagerTestUtils.disableBrowserOverrides();
        ThreadUtils.runOnUiThreadBlocking(
                TabStateBrowserControlsVisibilityDelegate::disablePageLoadDelayForTests);
    }

    @Test
    @LargeTest
    @DisabledTest(message = "Flaky, see crbug.com/455947814")
    public void topControlsScroll() {
        waitForControlsVisibility(true);

        var bottomFacility = mTopFacility.scrollToBottom();
        waitForControlsVisibility(false);

        bottomFacility.scrollToTop();
        waitForControlsVisibility(true);
    }

    @Test
    @LargeTest
    @DisabledTest(message = "Flaky, see crbug.com/455558607")
    public void topControlsScrollingDisabled() {
        // before test case - set scrolling disabled
        TopControlsStacker topControlsStacker =
                mActivity.getRootUiCoordinatorForTesting().getTopControlsStacker();
        ThreadUtils.runOnUiThreadBlocking(() -> topControlsStacker.setScrollingDisabled(true));

        mTopFacility.scrollToBottom();
        waitForControlsVisibility(true);

        // after test case - set scrolling back.
        ThreadUtils.runOnUiThreadBlocking(() -> topControlsStacker.setScrollingDisabled(false));
    }

    private void waitForControlsVisibility(boolean visible) {
        int expectedVisibility = visible ? View.VISIBLE : View.INVISIBLE;
        CriteriaHelper.pollUiThread(() -> mControlContainer.getVisibility() == expectedVisibility);
    }
}
