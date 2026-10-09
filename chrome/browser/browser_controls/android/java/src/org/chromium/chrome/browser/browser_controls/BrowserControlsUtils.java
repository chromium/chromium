// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.content.Context;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayUtil;

/** Static utilities related to browser controls interfaces. */
@NullMarked
public class BrowserControlsUtils {

    private static final String HISTOGRAM_PERCENTAGE_MAX_HEIGHT =
            "Android.BrowserControls.PercentageOfWindowUsedByBrowserControlsAtMaxHeight";
    private static final String HISTOGRAM_PERCENTAGE_MIN_HEIGHT =
            "Android.BrowserControls.PercentageOfWindowUsedByBrowserControlsAtMinHeight";

    private static @TriState int sSyncMinHeightWithTotalHeightForTesting;

    /**
     * Returns the height of the window in pixels based on the window's display and context
     * configuration screenHeightDp.
     *
     * @param context The Context to retrieve screen configuration from.
     * @param windowAndroid The WindowAndroid to retrieve the DisplayAndroid from.
     * @return The height of the window in pixels.
     */
    public static int getWindowHeight(Context context, WindowAndroid windowAndroid) {
        return DisplayUtil.dpToPx(
                windowAndroid.getDisplay(),
                context.getResources().getConfiguration().screenHeightDp);
    }

    /**
     * Calculates the percentage of the window's total height used by a controls height [0, 100].
     *
     * @param controlsHeight The height of the controls in pixels.
     * @param windowHeight The height of the window in pixels.
     * @return The percentage of the window height used, rounded and clamped to [0, 100].
     */
    public static int calculatePercentageOfWindowUsed(int controlsHeight, int windowHeight) {
        if (windowHeight <= 0 || controlsHeight < 0) return 0;
        return Math.min(100, Math.round(100.f * controlsHeight / windowHeight));
    }

    /**
     * Disallow top browser controls from scrolling off by setting min height equal to overall
     * height. This method checks the form factors internally.
     */
    // TODO(https://crbug.com/450970998): Move to TopControlsLockCoordinator after removing
    //  reference from BrowserControlsManager.
    public static boolean doSyncMinHeightWithTotalHeightV2(Context context) {
        if (sSyncMinHeightWithTotalHeightForTesting != TriState.NOT_SET) {
            return sSyncMinHeightWithTotalHeightForTesting == TriState.TRUE;
        }

        if (!ChromeFeatureList.sLockTopControlsOnLargeTabletsV2.isEnabled()) {
            return false;
        }

        return DeviceInfo.isDesktop()
                || DeviceFormFactor.isNonMultiDisplayContextOnLargeTablet(context);
    }

    /** Whether force adjusting top chrome height is allowed based on feature flags. */
    public static boolean isForceTopChromeHeightAdjustmentOnStartupEnabled(Context context) {
        // Note: the check for feature doSyncMinHeightWithTotalHeightV2 is not necessary once the
        // feature flag is launched. Once we are ready to cleanup the param
        // sLockTopControlsForceAdjustHeightOnStartup it's safe to assume this method to return
        // true always.
        return doSyncMinHeightWithTotalHeightV2(context)
                && ChromeFeatureList.sLockTopControlsForceAdjustHeightOnStartup.getValue();
    }

    /** Returns whether the top-controls hairline needs an extra offset to stay hidden. */
    public static boolean shouldContentOffsetHideTopControlsHairline(
            int contentOffset, int topControlsMinHeight, int topControlsHairlineHeight) {
        return contentOffset >= topControlsMinHeight
                && contentOffset <= topControlsMinHeight + topControlsHairlineHeight;
    }

    /**
     * @return True if the browser controls are completely off screen.
     */
    public static boolean areBrowserControlsOffScreen(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBrowserControlHiddenRatio() == 1.0f;
    }

    /**
     * @return True if the browser controls are currently completely visible.
     */
    public static boolean areBrowserControlsFullyVisible(
            BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBrowserControlHiddenRatio() == 0.f;
    }

    /**
     * @return True if the top browser controls are completely off screen.
     */
    public static boolean areTopControlsOffScreen(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getTopControlHiddenRatio() == 1.0f;
    }

    /**
     * @return True if the top browser controls are currently completely visible.
     */
    public static boolean areTopControlsFullyVisible(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getTopControlHiddenRatio() == 0.f;
    }

    /**
     * @return True if the bottom browser controls are completely off screen.
     */
    public static boolean areBottomControlsOffScreen(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBottomControlHiddenRatio() == 1.0f;
    }

    /**
     * @return True if the bottom browser controls are currently completely visible.
     */
    public static boolean areBottomControlsFullyVisible(
            BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBottomControlHiddenRatio() == 0.f;
    }

    /**
     * @return Whether the browser controls should be drawn as a texture.
     */
    public static boolean drawControlsAsTexture(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBrowserControlHiddenRatio() > 0;
    }

    /**
     * TODO(jinsukkim): Move this to CompositorViewHolder.
     *
     * @return {@code true} if browser controls shrink Blink view's size. Note that this is valid
     *     only when the browser controls are in idle state i.e. not scrolling or animating.
     */
    public static boolean controlsResizeView(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getContentOffset() > stateProvider.getTopControlsMinHeight()
                || getBottomContentOffset(stateProvider)
                        > stateProvider.getBottomControlsMinHeight();
    }

    /**
     * @return The content offset from the bottom of the screen, or the visible height of the bottom
     *     controls, in px.
     */
    public static int getBottomContentOffset(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBottomControlsHeight() - stateProvider.getBottomControlOffset();
    }

    /**
     * @return Whether browser controls are currently idle, i.e. not scrolling or animating.
     */
    public static boolean areBrowserControlsIdle(BrowserControlsStateProvider provider) {
        return (provider.getContentOffset() == provider.getTopControlsMinHeight()
                        || provider.getContentOffset() == provider.getTopControlsHeight())
                && (BrowserControlsUtils.getBottomContentOffset(provider)
                                == provider.getBottomControlsMinHeight()
                        || BrowserControlsUtils.getBottomContentOffset(provider)
                                == provider.getBottomControlsHeight());
    }

    /**
     * Records the percentage of the window's total height used by combined top and bottom browser
     * controls.
     */
    public static void recordCombinedControlsMetrics(
            BrowserControlsStateProvider stateProvider,
            Context context,
            WindowAndroid windowAndroid) {
        int windowHeight = getWindowHeight(context, windowAndroid);
        int totalHeight =
                stateProvider.getTopControlsHeight() + stateProvider.getBottomControlsHeight();
        int minHeight =
                stateProvider.getTopControlsMinHeight()
                        + stateProvider.getBottomControlsMinHeight();
        if (windowHeight <= 0 || totalHeight < 0 || minHeight < 0) return;

        RecordHistogram.recordPercentageHistogram(
                HISTOGRAM_PERCENTAGE_MAX_HEIGHT,
                calculatePercentageOfWindowUsed(totalHeight, windowHeight));
        RecordHistogram.recordPercentageHistogram(
                HISTOGRAM_PERCENTAGE_MIN_HEIGHT,
                calculatePercentageOfWindowUsed(minHeight, windowHeight));
    }

    public static void setsSyncMinHeightWithTotalHeightForTesting(boolean override) {
        sSyncMinHeightWithTotalHeightForTesting = TriStateUtils.from(override);
        ResettersForTesting.register(
                () -> sSyncMinHeightWithTotalHeightForTesting = TriState.NOT_SET);
    }
}
