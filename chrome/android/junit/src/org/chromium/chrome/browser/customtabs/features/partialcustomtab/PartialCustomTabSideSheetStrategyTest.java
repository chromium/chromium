// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH_LANDSCAPE;

import android.view.View;

import androidx.annotation.Px;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.Features;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link PartialCustomTabSideSheetStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class PartialCustomTabSideSheetStrategyTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabSideSheetStrategy createPcctSideSheetStrategy(@Px int heightPx) {
        PartialCustomTabSideSheetStrategy pcct = new PartialCustomTabSideSheetStrategy(
                mPCCTTestRule.mActivity, heightPx, false, mPCCTTestRule.mOnResizedCallback,
                mPCCTTestRule.mFullscreenManager, false, true);
        pcct.setMockViewForTesting(mPCCTTestRule.mToolbarView, mPCCTTestRule.mToolbarCoordinator);
        return pcct;
    }

    @Test
    public void create_sideSheetStrategy() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabSideSheetStrategy strategy = createPcctSideSheetStrategy(2000);

        assertEquals("Side-Sheet PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET,
                strategy.getStrategyType());
    }

    @Test
    public void create_largeHeightLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(5000);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
    }

    @Test
    public void create_smallHeightLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(100);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
    }

    @Test
    public void create_deviceHeightLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(DEVICE_HEIGHT_LANDSCAPE);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
    }

    @Test
    public void create_widthHalfWindowLandscape() {
        // TODO(crbug.com/1406104): This test case will be redone once we support configurable width
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(DEVICE_HEIGHT_LANDSCAPE);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "Should only have one attribute result", 1, mPCCTTestRule.mAttributeResults.size());
        assertEquals(DEVICE_WIDTH_LANDSCAPE / 2, mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void onShowSoftInputRunsRunnable() {
        mPCCTTestRule.configLandscapeMode();
        var strategy = createPcctSideSheetStrategy(2000);
        AtomicBoolean hasRunnableRun = new AtomicBoolean(false);

        strategy.onShowSoftInput(() -> { hasRunnableRun.set(true); });

        assertEquals("The runnable should be run", true, hasRunnableRun.get());
        assertTabIsAtFullLandscapeHeight();
    }

    @Test
    public void dragHandlebarInvisible() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(2000);

        mPCCTTestRule.verifyWindowFlagsSet();
        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtFullLandscapeHeight();
        verify(mPCCTTestRule.mDragHandlebar).setVisibility(View.GONE);
    }

    @Test
    public void noTopShadow() {
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(org.chromium.chrome.R.dimen.custom_tabs_shadow_offset));

        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(2000);

        mPCCTTestRule.verifyWindowFlagsSet();
        assertTabIsAtFullLandscapeHeight();
        assertEquals("Top margin should be zero for the shadow", 0,
                mPCCTTestRule.mLayoutParams.topMargin);
    }

    @Test
    public void leftShadowIsVisible() {
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(org.chromium.chrome.R.dimen.custom_tabs_shadow_offset));

        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(2000);

        mPCCTTestRule.verifyWindowFlagsSet();
        assertTabIsAtFullLandscapeHeight();
        assertNotEquals("Left margin should be non-zero for the shadow", 0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    private void assertTabIsAtFullLandscapeHeight() {
        assertEquals(
                "Should only have one attribute result", 1, mPCCTTestRule.mAttributeResults.size());
        assertEquals(DEVICE_HEIGHT_LANDSCAPE, mPCCTTestRule.mAttributeResults.get(0).height);
    }
}
