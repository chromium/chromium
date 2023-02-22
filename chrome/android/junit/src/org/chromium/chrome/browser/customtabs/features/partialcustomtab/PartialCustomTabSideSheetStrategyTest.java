// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_POSITION_DEFAULT;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_POSITION_END;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_POSITION_START;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_SLIDE_IN_DEFAULT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.FULL_HEIGHT;

import android.os.Build;
import android.view.View;
import android.view.WindowManager;

import androidx.annotation.Px;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.LocalizationUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link PartialCustomTabSideSheetStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET})
public class PartialCustomTabSideSheetStrategyTest {
    private static final float MINIMAL_WIDTH_RATIO = 0.33f;
    private static final boolean RTL = true;
    private static final boolean LTR = false;
    private static final boolean RIGHT = true;
    private static final boolean LEFT = false;
    private boolean mFullscreen;

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabSideSheetStrategy createPcctSideSheetStrategy(@Px int widthPx) {
        PartialCustomTabSideSheetStrategy pcct = new PartialCustomTabSideSheetStrategy(
                mPCCTTestRule.mActivity, widthPx, mPCCTTestRule.mOnResizedCallback,
                mPCCTTestRule.mFullscreenManager, false, true, /*showMaximizedButton=*/true,
                /*startMaximized=*/false, ACTIVITY_SIDE_SHEET_POSITION_DEFAULT,
                ACTIVITY_SIDE_SHEET_SLIDE_IN_DEFAULT, mPCCTTestRule.mHandleStrategyFactory, 0);
        pcct.setMockViewForTesting(mPCCTTestRule.mCoordinatorLayout, mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator);
        return pcct;
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_sideSheetStrategy_Q() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabSideSheetStrategy strategy = createPcctSideSheetStrategy(2000);

        assertEquals("Side-Sheet PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET,
                strategy.getStrategyType());
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_sideSheetStrategy() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabSideSheetStrategy strategy = createPcctSideSheetStrategy(2000);

        assertEquals("Side-Sheet PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET,
                strategy.getStrategyType());
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_largeWidthLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(5000);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
        assertEquals("Side-sheet has wrong width", DEVICE_WIDTH_LANDSCAPE,
                mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_smallWidthLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(100);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
        assertEquals("Side-sheet has wrong width",
                (int) (DEVICE_WIDTH_LANDSCAPE * MINIMAL_WIDTH_RATIO),
                mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_deviceWidthLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(DEVICE_WIDTH_LANDSCAPE);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
        assertEquals("Side-sheet has wrong width", DEVICE_WIDTH_LANDSCAPE,
                mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_widthHalfWindowLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(DEVICE_WIDTH_LANDSCAPE / 2);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "Should only have one attribute result", 1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtFullLandscapeHeight();
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
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
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
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
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
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
        assertNotEquals("Left margin should be non-zero for the shadow", 0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Test
    public void enterAndExitHtmlFullscreen() {
        var strategy = createPcctSideSheetStrategy(2000);
        assertFalse(getWindowAttributes().isFullscreen());
        int height = getWindowAttributes().height;
        int width = getWindowAttributes().width;

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        assertTrue(getWindowAttributes().isFullscreen());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(DEVICE_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);

        mFullscreen = false;
        strategy.onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertFalse(getWindowAttributes().isFullscreen());
        assertEquals(height, getWindowAttributes().height);
        assertEquals(width, getWindowAttributes().width);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), eq(width));
    }

    @Test
    public void enterAndExitMaximizeMode() {
        var strategy = createPcctSideSheetStrategy(2000);
        assertFalse(getWindowAttributes().isFullscreen());
        int height = getWindowAttributes().height;
        int width = getWindowAttributes().width;

        strategy.toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);

        strategy.toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertEquals(height, getWindowAttributes().height);
        assertEquals(width, getWindowAttributes().width);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), eq(width));
    }

    @Test
    public void toggleMaximizeNoAnimation() {
        var strategy = createPcctSideSheetStrategy(2000);
        int height = getWindowAttributes().height;
        int width = getWindowAttributes().width;
        strategy.toggleMaximize(false);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), eq(DEVICE_WIDTH));

        strategy.toggleMaximize(false);
        assertEquals(height, getWindowAttributes().height);
        assertEquals(width, getWindowAttributes().width);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), eq(width));
    }

    @Test
    public void sheetPosition() {
        assertPosition(LEFT, RTL, ACTIVITY_SIDE_SHEET_POSITION_DEFAULT);
        assertPosition(LEFT, RTL, ACTIVITY_SIDE_SHEET_POSITION_END);
        assertPosition(RIGHT, RTL, ACTIVITY_SIDE_SHEET_POSITION_START);
        assertPosition(RIGHT, LTR, ACTIVITY_SIDE_SHEET_POSITION_DEFAULT);
        assertPosition(RIGHT, LTR, ACTIVITY_SIDE_SHEET_POSITION_END);
        assertPosition(LEFT, LTR, ACTIVITY_SIDE_SHEET_POSITION_START);
        LocalizationUtils.setRtlForTesting(false);
    }

    @Test
    public void handleCloseAnimation() {
        var strategy = createPcctSideSheetStrategy(2000);
        strategy.setSheetOnRightForTesting(true);
        var invoked = new ObservableSupplierImpl<Boolean>();

        invoked.set(false);
        assertEquals(0, mPCCTTestRule.getWindowAttributes().x);
        strategy.handleCloseAnimation(() -> invoked.set(true)); // Slide out to right
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTrue(invoked.get());
        assertEquals(DEVICE_WIDTH, mPCCTTestRule.getWindowAttributes().x);

        invoked.set(false);
        strategy = createPcctSideSheetStrategy(2000);
        strategy.setSheetOnRightForTesting(false);
        strategy.setSlideDownAnimationForTesting(true); // Slide down
        strategy.handleCloseAnimation(() -> invoked.set(true));
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTrue(invoked.get());
        assertEquals(DEVICE_HEIGHT, mPCCTTestRule.getWindowAttributes().y);
    }

    private static void assertPosition(boolean isRightSide, boolean isRtl, int position) {
        LocalizationUtils.setRtlForTesting(isRtl);
        String msg = isRightSide ? "Should be on right" : "Should be on left";
        assertEquals(msg, isRightSide, PartialCustomTabSideSheetStrategy.isSheetOnRight(position));
    }

    private void assertTabIsAtFullLandscapeHeight() {
        assertEquals(
                "Should only have one attribute result", 1, mPCCTTestRule.mAttributeResults.size());
        assertEquals(DEVICE_HEIGHT_LANDSCAPE, mPCCTTestRule.mAttributeResults.get(0).height);
    }

    private WindowManager.LayoutParams getWindowAttributes() {
        return mPCCTTestRule.getWindowAttributes();
    }
}
