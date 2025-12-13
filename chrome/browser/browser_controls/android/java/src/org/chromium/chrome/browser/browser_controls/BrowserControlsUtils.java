// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/** Static utilities related to browser controls interfaces. */
@NullMarked
public class BrowserControlsUtils {

    // Disallow top browser controls from scrolling off on large tablets by setting min height
    // equal to overall height.
    // TODO(https://crbug.com/436900619): Converge on and implement long-term solution.
    public static boolean doSyncMinHeightWithTotalHeight(Context context) {
        return ChromeFeatureList.sLockTopControlsOnLargeTablets.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnLargeTablet(context);
    }

    /**
     * Disallow top browser controls from scrolling off by setting min height equal to overall
     * height. This feature does not check form factors.
     */
    public static boolean doSyncMinHeightWithTotalHeightV2() {
        return ChromeFeatureList.sLockTopControlsOnLargeTabletsV2.isEnabled()
                && ChromeFeatureList.sTopControlsRefactor.isEnabled();
    }

    /** Whether use TopControlsStacker to drive the y offset for top control layers. */
    public static boolean isTopControlsRefactorOffsetEnabled() {
        return ChromeFeatureList.sTopControlsRefactor.isEnabled()
                && ChromeFeatureList.sTopControlsRefactorV2.isEnabled();
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
}
