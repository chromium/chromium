// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

/**
 * Exposes the current overview mode state as well as a way to listen to overview mode state
 * changes.
 *
 * DEPRECATED, please use {@link org.chromium.chrome.browser.layouts.LayoutStateProvider} instead.
 */
@Deprecated
public interface OverviewModeBehavior {
    /**
     * An observer that is notified when the overview mode state changes.
     *
     * DEPRECATED, please use {@link
     * org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver} instead.
     */
    @Deprecated
    interface OverviewModeObserver {
        /**
         * Called when overview mode starts showing.
         * @param showToolbar Whether or not to show the normal toolbar when animating into overview
         * mode.
         */
        void onOverviewModeStartedShowing(boolean showToolbar);

        /**
         * Called when overview mode finishes showing.
         */
        void onOverviewModeFinishedShowing();

        /**
         * Called when overview mode starts hiding.
         * @param showToolbar    Whether or not to show the normal toolbar when animating out of
         *                       overview mode.
         * @param delayAnimation Whether or not to delay any related overview animations until after
         *                       overview mode is finished hiding.
         */
        void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation);

        /**
         * Called when overview mode finishes hiding.
         */
        void onOverviewModeFinishedHiding();
    }

    /**
     * @return Whether or not the overview {@link Layout} is visible.
     */
    boolean overviewVisible();

    /**
     * @param listener Registers {@code listener} for overview mode status changes.
     */
    void addOverviewModeObserver(OverviewModeObserver listener);

    /**
     * @param listener Unregisters {@code listener} for overview mode status changes.
     */
    void removeOverviewModeObserver(OverviewModeObserver listener);
}
