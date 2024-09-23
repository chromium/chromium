// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.MULTIWINDOW_HEIGHT;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;

/** Tests for {@link PartialCustomTabFullSizeStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
public class PartialCustomTabFullSizeStrategyTest {
    private boolean mFullscreen;

    @Rule public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabFullSizeStrategy createPcctFullSizeStrategy() {
        BrowserServicesIntentDataProvider intentData = mPCCTTestRule.mIntentData;
        when(intentData.canInteractWithBackground()).thenReturn(true);
        PartialCustomTabFullSizeStrategy pcct =
                new PartialCustomTabFullSizeStrategy(
                        mPCCTTestRule.mActivity,
                        mPCCTTestRule.mIntentData,
                        mPCCTTestRule.mOnResizedCallback,
                        mPCCTTestRule.mOnActivityLayoutCallback,
                        mPCCTTestRule.mFullscreenManager,
                        /* isTablet= */ false,
                        mPCCTTestRule.mHandleStrategyFactory);
        pcct.setMockViewForTesting(
                mPCCTTestRule.mCoordinatorLayout,
                mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator);
        return pcct;
    }

    @Test
    public void create_fullSizeStrategyInMultiWindowLandscape() {
        mPCCTTestRule.configLandscapeMode();
        mPCCTTestRule.setupDisplayMetricsInMultiWindowMode();
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        PartialCustomTabFullSizeStrategy strategy = createPcctFullSizeStrategy();

        assertEquals(
                "Full-Size PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.FULL_SIZE,
                strategy.getStrategyType());
        assertEquals(
                "Full-Size has wrong height",
                MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).height);
        assertEquals(
                "Full-Size has wrong width",
                MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).width);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH_LANDSCAPE),
                        eq(MULTIWINDOW_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_FULL_SCREEN));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void create_fullSizeStrategyInMultiWindowPortrait() {
        mPCCTTestRule.configPortraitMode();
        mPCCTTestRule.setupDisplayMetricsInMultiWindowMode();
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        PartialCustomTabFullSizeStrategy strategy = createPcctFullSizeStrategy();

        assertEquals(
                "Full-Size PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.FULL_SIZE,
                strategy.getStrategyType());
        assertEquals(
                "Full-Size has wrong height",
                MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).height);
        assertEquals(
                "Full-Size has wrong width",
                MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).width);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(MULTIWINDOW_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_FULL_SCREEN));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void create_fullSizeStrategyInPortrait_HeightNotSet_WidthSet() {
        when(mPCCTTestRule.mIntentData.getInitialActivityWidth()).thenReturn(300);
        mPCCTTestRule.configPortraitMode();

        PartialCustomTabFullSizeStrategy strategy = createPcctFullSizeStrategy();

        assertEquals(Math.round(mPCCTTestRule.mCoordinatorLayout.getElevation()), 0);
        assertEquals(
                "Full-Size PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.FULL_SIZE,
                strategy.getStrategyType());
        assertEquals(
                "Full-Size has wrong height",
                MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).height);
        assertEquals(
                "Full-Size has wrong width",
                MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).width);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_FULL_SCREEN));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }
}
