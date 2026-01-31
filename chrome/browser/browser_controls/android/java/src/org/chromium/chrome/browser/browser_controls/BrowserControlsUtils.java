// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.content.Context;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/** Static utilities related to browser controls interfaces. */
@NullMarked
public class BrowserControlsUtils {

    private static @Nullable Boolean sSyncMinHeightWithTotalHeightForTesting;

    // Disallow top browser controls from scrolling off on large tablets by setting min height
    // equal to overall height.
    // TODO(https://crbug.com/450970998): Replace with doSyncMinHeightWithTotalHeightV2.
    public static boolean doSyncMinHeightWithTotalHeight(Context context) {
        return ChromeFeatureList.sLockTopControlsOnLargeTablets.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnLargeTablet(context);
    }

    /**
     * Disallow top browser controls from scrolling off by setting min height equal to overall
     * height. This method checks the form factors internally.
     */
    // TODO(https://crbug.com/450970998): Move to TopControlsLockCoordinator after removing
    //  reference from BrowserControlsManager.
    public static boolean doSyncMinHeightWithTotalHeightV2(Context context) {
        if (sSyncMinHeightWithTotalHeightForTesting != null) {
            return sSyncMinHeightWithTotalHeightForTesting;
        }

        if (!ChromeFeatureList.sLockTopControlsOnLargeTabletsV2.isEnabled()
                || !ChromeFeatureList.sTopControlsRefactor.isEnabled()) {
            return false;
        }

        return DeviceInfo.isDesktop()
                || DeviceFormFactor.isNonMultiDisplayContextOnLargeTablet(context);
    }

    /** Whether use TopControlsStacker to drive the y offset for top control layers. */
    public static boolean isTopControlsRefactorOffsetEnabled() {
        return ChromeFeatureList.sTopControlsRefactor.isEnabled()
                && ChromeFeatureList.sTopControlsRefactorV2.isEnabled();
    }

    /** Whether force adjusting top chrome height is allowed based on feature flags. */
    public static boolean isForceTopChromeHeightAdjustmentOnStartupEnabled(Context context) {
        // Note: the check for feature doSyncMinHeightWithTotalHeightV2 is not necessary once the
        // feature flag is launched. Once we are ready to cleanup the param
        // sLockTopControlsForceAdjustHeightOnStartup it's safe to assume this method to return
        // true always.
        return isTopControlsRefactorOffsetEnabled()
                && doSyncMinHeightWithTotalHeightV2(context)
                && ChromeFeatureList.sLockTopControlsForceAdjustHeightOnStartup.getValue();
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
     * @return Whether the browser controls should be drawn as a texture.
     */
    public static boolean drawControlsAsTexture(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBrowserControlHiddenRatio() > 0;
    }

    /**
     * TODO(jinsukkim): Move this to CompositorViewHolder.
     * @return {@code true} if browser controls shrink Blink view's size. Note that this
     *         is valid only when the browser controls are in idle state i.e. not scrolling
     *         or animating.
     */
    public static boolean controlsResizeView(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getContentOffset() > stateProvider.getTopControlsMinHeight()
                || getBottomContentOffset(stateProvider)
                        > stateProvider.getBottomControlsMinHeight();
    }

    /**
     * @return The content offset from the bottom of the screen, or the visible height of the bottom
     *         controls, in px.
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

    public static void setsSyncMinHeightWithTotalHeightForTesting(boolean override) {
        sSyncMinHeightWithTotalHeightForTesting = override;
        ResettersForTesting.register(() -> sSyncMinHeightWithTotalHeightForTesting = null);
    }
}
