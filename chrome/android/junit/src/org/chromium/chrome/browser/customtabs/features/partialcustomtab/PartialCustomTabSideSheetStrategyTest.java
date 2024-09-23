// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_SIDE_SHEET;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_START;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH_MEDIUM;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.FULL_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.NAVBAR_HEIGHT;

import android.os.Build;
import android.view.View;
import android.view.WindowManager;

import androidx.annotation.Px;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.ui.base.LocalizationUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link PartialCustomTabSideSheetStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {PartialCustomTabTestRule.ShadowSemanticColorUtils.class})
@LooperMode(Mode.PAUSED)
public class PartialCustomTabSideSheetStrategyTest {
    private static final float MINIMAL_WIDTH_RATIO_EXPANDED = 0.33f;
    private static final float MINIMAL_WIDTH_RATIO_MEDIUM = 0.5f;
    private static final boolean RTL = true;
    private static final boolean LTR = false;
    private static final boolean RIGHT = true;
    private static final boolean LEFT = false;
    private boolean mFullscreen;

    @Rule public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabSideSheetStrategy createPcctSideSheetStrategy(@Px int widthPx) {
        return createPcctSideSheetStrategy(
                widthPx,
                ACTIVITY_SIDE_SHEET_POSITION_END,
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);
    }

    private PartialCustomTabSideSheetStrategy createLeftPcctSideSheetStrategy(
            @Px int widthPx, int roundedCornersPosition) {
        return createPcctSideSheetStrategy(
                widthPx,
                ACTIVITY_SIDE_SHEET_POSITION_START,
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                roundedCornersPosition);
    }

    private PartialCustomTabSideSheetStrategy createPcctSideSheetStrategy(
            @Px int widthPx, int position) {
        return createPcctSideSheetStrategy(
                widthPx,
                position,
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);
    }

    private PartialCustomTabSideSheetStrategy createPcctSideSheetStrategy(
            @Px int widthPx, int position, int decorationType, int roundedCornersPosition) {
        BrowserServicesIntentDataProvider intentData = mPCCTTestRule.mIntentData;
        when(intentData.getInitialActivityWidth()).thenReturn(widthPx);
        when(intentData.showSideSheetMaximizeButton()).thenReturn(true);
        when(intentData.canInteractWithBackground()).thenReturn(true);
        when(intentData.getSideSheetPosition()).thenReturn(position);
        when(intentData.getSideSheetSlideInBehavior())
                .thenReturn(ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE);
        when(intentData.getActivitySideSheetDecorationType()).thenReturn(decorationType);
        when(intentData.getActivitySideSheetRoundedCornersPosition())
                .thenReturn(roundedCornersPosition);
        PartialCustomTabSideSheetStrategy pcct =
                new PartialCustomTabSideSheetStrategy(
                        mPCCTTestRule.mActivity,
                        mPCCTTestRule.mIntentData,
                        mPCCTTestRule.mOnResizedCallback,
                        mPCCTTestRule.mOnActivityLayoutCallback,
                        mPCCTTestRule.mFullscreenManager,
                        /* isTablet= */ false,
                        /* startMaximized= */ false,
                        mPCCTTestRule.mHandleStrategyFactory);
        pcct.setMockViewForTesting(
                mPCCTTestRule.mCoordinatorLayout,
                mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator);
        return pcct;
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_sideSheetStrategy_Q() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabSideSheetStrategy strategy = createPcctSideSheetStrategy(2000);

        assertEquals(
                "Side-Sheet PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET,
                strategy.getStrategyType());
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_sideSheetStrategy() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabSideSheetStrategy strategy = createPcctSideSheetStrategy(2000);

        assertEquals(
                "Side-Sheet PCCT should be created",
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
        assertEquals(
                "Side-sheet has wrong width",
                DEVICE_WIDTH_LANDSCAPE,
                mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_smallWidthLandscape_Expanded() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(100);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
        assertEquals(
                "Side-sheet has wrong width",
                (int) (DEVICE_WIDTH_LANDSCAPE * MINIMAL_WIDTH_RATIO_EXPANDED),
                mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_smallWidthLandscape_Medium() {
        mPCCTTestRule.configDeviceWidthMedium();
        createPcctSideSheetStrategy(100);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "Side-sheet has wrong width",
                (int) (DEVICE_WIDTH_MEDIUM * MINIMAL_WIDTH_RATIO_MEDIUM),
                mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_deviceWidthLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(DEVICE_WIDTH_LANDSCAPE);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
        assertEquals(
                "Side-sheet has wrong width",
                DEVICE_WIDTH_LANDSCAPE,
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

        strategy.onShowSoftInput(
                () -> {
                    hasRunnableRun.set(true);
                });

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
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));

        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(2000);

        mPCCTTestRule.verifyWindowFlagsSet();
        assertTabIsAtFullLandscapeHeight();
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
        assertEquals(
                "Top margin should be zero for the shadow",
                0,
                mPCCTTestRule.mLayoutParams.topMargin);
    }

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void leftShadowIsVisible() {
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));

        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(2000);

        mPCCTTestRule.verifyWindowFlagsSet();
        assertTabIsAtFullLandscapeHeight();
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
        assertNotEquals(
                "Left margin should be non-zero for the shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
        assertEquals(
                "Right margin should be zero because shadow is on left side",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
    }

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void rightShadowIsVisible() {
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));

        mPCCTTestRule.configLandscapeMode();
        createLeftPcctSideSheetStrategy(2000, ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);

        mPCCTTestRule.verifyWindowFlagsSet();
        assertTabIsAtFullLandscapeHeight();
        assertEquals(
                "Side-sheet has wrong width", 2000, mPCCTTestRule.mAttributeResults.get(0).width);
        assertNotEquals(
                "Right margin should be non-zero for the shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
        assertEquals(
                "Left margin should be zero because shadow is on right side",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Test
    public void roundTopLeftCorner() {
        doReturn(16)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_handle_height));
        mPCCTTestRule.configLandscapeMode();
        var strategy =
                createPcctSideSheetStrategy(
                        2000,
                        ACTIVITY_SIDE_SHEET_POSITION_END,
                        ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                        ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP);
        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);

        assertTabIsAtFullLandscapeHeight();
        assertNotEquals("Drag bar not rounded", 0, mPCCTTestRule.mDragBarLayoutParams.height);
        verify(mPCCTTestRule.mDragBarBackground, times(2))
                .setCornerRadii(eq(new float[] {5, 5, 0, 0, 0, 0, 0, 0}));
    }

    @Test
    public void roundTopRightCorner() {
        doReturn(16)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_handle_height));
        mPCCTTestRule.configLandscapeMode();
        var strategy =
                createLeftPcctSideSheetStrategy(
                        2000, ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP);
        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);

        assertTabIsAtFullLandscapeHeight();
        assertNotEquals("Drag bar not rounded", 0, mPCCTTestRule.mDragBarLayoutParams.height);
        verify(mPCCTTestRule.mDragBarBackground, times(2))
                .setCornerRadii(eq(new float[] {0, 0, 5, 5, 0, 0, 0, 0}));
    }

    @Test
    public void noRoundedCorner() {
        mPCCTTestRule.configLandscapeMode();
        var strategy = createPcctSideSheetStrategy(2000);

        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);

        assertTabIsAtFullLandscapeHeight();
        assertEquals("Drag bar rounded", 0, mPCCTTestRule.mDragBarLayoutParams.height);
        verify(mPCCTTestRule.mDragBarBackground, times(2))
                .setCornerRadii(eq(new float[] {0, 0, 0, 0, 0, 0, 0, 0}));
    }

    @Test
    public void noShadowsFullWidth() {
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));

        mPCCTTestRule.configLandscapeMode();
        createLeftPcctSideSheetStrategy(3000, ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);

        mPCCTTestRule.verifyWindowFlagsSet();
        assertTabIsAtFullLandscapeHeight();
        assertEquals(
                "Side-sheet has wrong width",
                DEVICE_WIDTH_LANDSCAPE,
                mPCCTTestRule.mRealMetrics.widthPixels);
        assertEquals(
                "Right margin should be zero because side sheet is max width",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);

        mPCCTTestRule.configPortraitMode();
        createPcctSideSheetStrategy(2000);
        assertEquals(
                "Side-sheet has wrong width", DEVICE_WIDTH, mPCCTTestRule.mRealMetrics.widthPixels);
        assertEquals(
                "Left margin should be zero because side sheet is max width",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Test
    public void drawDividerLine() {
        doReturn(10)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        mPCCTTestRule.configLandscapeMode();
        var strategy =
                createPcctSideSheetStrategy(
                        2000,
                        ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE,
                        ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER,
                        ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);
        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTrue(strategy.shouldDrawDividerLine());
        assertEquals(
                "Right margin should zero for no shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
        assertEquals(
                "Left margin should be zero for no shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Config(sdk = Build.VERSION_CODES.P)
    @Test
    public void drawDividerLine_OverrideWithOldOS() {
        doReturn(10)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        mPCCTTestRule.configLandscapeMode();
        // Only override instead of shadow. If they set to no decoration, we don't override with
        // divider line
        var strategy =
                createPcctSideSheetStrategy(
                        2000,
                        ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE,
                        ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                        ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);
        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTrue(strategy.shouldDrawDividerLine());
        assertEquals(
                "Right margin should zero for no shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
        assertEquals(
                "Left margin should be zero for no shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Test
    public void noDecoration() {
        doReturn(10)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        mPCCTTestRule.configLandscapeMode();
        var strategy =
                createPcctSideSheetStrategy(
                        2000,
                        ACTIVITY_SIDE_SHEET_POSITION_END,
                        ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE,
                        ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);
        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertFalse(strategy.shouldDrawDividerLine());
        assertEquals(
                "Right margin should zero for no shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
        assertEquals(
                "Left margin should be zero for no shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void enterAndExitHtmlFullscreen() {
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        doReturn(10)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_handle_height));
        var strategy =
                createPcctSideSheetStrategy(
                        1000,
                        ACTIVITY_SIDE_SHEET_POSITION_END,
                        ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                        ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP);
        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
        assertFalse(getWindowAttributes().isFullscreen());
        int height = getWindowAttributes().height;
        int width = getWindowAttributes().width;
        mPCCTTestRule.verifyWindowFlagsSet();

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        assertTrue(getWindowAttributes().isFullscreen());
        assertEquals("Shadow should be removed.", 0, mPCCTTestRule.mLayoutParams.leftMargin);
        assertEquals("Toolbar still present.", 0, mPCCTTestRule.mLayoutParams.topMargin);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(DEVICE_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_FULL_SCREEN));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        mFullscreen = false;
        strategy.onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertFalse(getWindowAttributes().isFullscreen());
        assertEquals(height, getWindowAttributes().height);
        assertEquals(width, getWindowAttributes().width);
        assertNotEquals("Shadow should be restored.", 0, mPCCTTestRule.mLayoutParams.leftMargin);
        assertNotEquals("Toolbar should be restored.", 0, mPCCTTestRule.mLayoutParams.topMargin);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), eq(width));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(DEVICE_WIDTH - width),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void enterAndExitHtmlFullscreen_useDivider() {
        doReturn(10)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_handle_height));
        var strategy =
                createPcctSideSheetStrategy(
                        800,
                        ACTIVITY_SIDE_SHEET_POSITION_END,
                        ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER,
                        ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP);
        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
        mPCCTTestRule.configInsetDrawableBg();
        assertFalse(getWindowAttributes().isFullscreen());
        int height = getWindowAttributes().height;
        int width = getWindowAttributes().width;
        mPCCTTestRule.verifyWindowFlagsSet();

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        assertTrue(getWindowAttributes().isFullscreen());
        assertEquals("Toolbar still present.", 0, mPCCTTestRule.mLayoutParams.topMargin);
        // this line gets called when divider line is removed
        verify(mPCCTTestRule.mCoordinatorLayout).getPaddingRight();
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(DEVICE_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_FULL_SCREEN));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        mFullscreen = false;
        strategy.onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertFalse(getWindowAttributes().isFullscreen());
        assertNotEquals("Toolbar should be restored.", 0, mPCCTTestRule.mLayoutParams.topMargin);
        assertEquals(height, getWindowAttributes().height);
        assertEquals(width, getWindowAttributes().width);
        // called twice because divider line is restored
        verify(mPCCTTestRule.mDragBar, times(2)).setBackground(any());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), eq(width));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(DEVICE_WIDTH - width),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void enterAndExitMaximizeMode() {
        var strategy = createPcctSideSheetStrategy(700);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
        assertFalse(getWindowAttributes().isFullscreen());
        int height = getWindowAttributes().height;
        int width = getWindowAttributes().width;

        strategy.toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        strategy.toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertEquals(height, getWindowAttributes().height);
        assertEquals(width, getWindowAttributes().width);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), eq(width));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(DEVICE_WIDTH - width),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void toggleMaximizeNoAnimation() {
        var strategy = createPcctSideSheetStrategy(700);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        int height = getWindowAttributes().height;
        int width = getWindowAttributes().width;
        strategy.toggleMaximize(/* animation= */ false);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        strategy.toggleMaximize(/* animation= */ false);
        // Even without animation, we still need to wait for task to be idle, since we post
        // the task for size init in |onMaximizeEnd|.
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertEquals(height, getWindowAttributes().height);
        assertEquals(width, getWindowAttributes().width);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), eq(width));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(DEVICE_WIDTH - width),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void maximizeAndFullscreen() {
        doReturn(16)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_handle_height));
        // Ensure maximize -> fullscreen enter/exit -> comes back to maximize mode
        var strategy =
                createPcctSideSheetStrategy(
                        700,
                        ACTIVITY_SIDE_SHEET_POSITION_END,
                        ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                        ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP);
        strategy.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);
        assertNotEquals("Corner not rounded", 0, mPCCTTestRule.mLayoutParams.topMargin);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);
        strategy.toggleMaximize(true);

        PartialCustomTabTestRule.waitForAnimationToFinish();
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), eq(DEVICE_WIDTH));
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED));
        assertEquals("Corner rounded while maximized", 0, mPCCTTestRule.mLayoutParams.topMargin);

        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        assertEquals("Corner rounded in fullscreen", 0, mPCCTTestRule.mLayoutParams.topMargin);
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        // Verify we get a single resize callback invocation when exiting fullscreen and restoring
        // maximize mode.
        mFullscreen = false;
        strategy.onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertEquals("Corner rounded while maximized", 0, mPCCTTestRule.mLayoutParams.topMargin);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void sheetPosition() {
        assertPosition(LEFT, RTL, ACTIVITY_SIDE_SHEET_POSITION_DEFAULT);
        assertPosition(LEFT, RTL, ACTIVITY_SIDE_SHEET_POSITION_END);
        assertPosition(RIGHT, RTL, ACTIVITY_SIDE_SHEET_POSITION_START);
        assertPosition(RIGHT, LTR, ACTIVITY_SIDE_SHEET_POSITION_DEFAULT);
        assertPosition(RIGHT, LTR, ACTIVITY_SIDE_SHEET_POSITION_END);
        assertPosition(LEFT, LTR, ACTIVITY_SIDE_SHEET_POSITION_START);
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

    @Test
    public void maximizeMinimize() {
        mPCCTTestRule.configLandscapeMode();
        var strategy = createPcctSideSheetStrategy(700);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        strategy.toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTrue("Should be in maximized state.", strategy.isMaximized());

        strategy.toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertFalse("Should be in minimized state.", strategy.isMaximized());
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
