// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ViewportTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.browser_controls.BrowserControlsFacility;
import org.chromium.chrome.test.transit.browser_controls.BrowserControlsOffsetCondition;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

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
@DisableFeatures({ChromeFeatureList.LOCK_TOP_CONTROLS_ON_LARGE_TABLETS})
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class BrowserControlsPTTest {

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private TopBottomLinksPageStation mPage;
    private ChromeTabbedActivity mActivity;
    private ViewportTestUtils mViewportTestUtils;

    @Before
    public void setup() {
        mViewportTestUtils = new ViewportTestUtils(mActivityTestRule);
        mViewportTestUtils.setUpForBrowserControls();

        var blankPage = mActivityTestRule.startOnBlankPage();
        mPage =
                TopBottomLinksPageStation.loadPageNoFacility(
                        mActivityTestRule.getActivityTestRule(), blankPage);
        mActivity = mActivityTestRule.getActivity();
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisabledTest(message = "https://crbug.com/471837397")
    public void topControlsScroll() {
        BrowserControlsFacility.waitForBrowserControlsToBeMoveable(mPage);

        mPage.scrollToBottom(BrowserControlsFacility.scrolledOff(mPage));
        mPage.scrollToTop(BrowserControlsFacility.shown(mPage));
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @EnableFeatures(ChromeFeatureList.LOCK_TOP_CONTROLS_ON_LARGE_TABLETS_V2)
    @DisabledTest(message = "https://crbug.com/471837396")
    public void topControlsScrollingDisabled() {
        mPage.noopTo()
                .waitFor(
                        BrowserControlsOffsetCondition.showAndHeightSynced(
                                mPage.getActivityElement()));
        mPage.scrollToBottom(null);
        mPage.noopTo()
                .waitFor(
                        BrowserControlsOffsetCondition.showAndHeightSynced(
                                mPage.getActivityElement()));
    }
}
