// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.os.SystemClock;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;

/** Interface to communicate with the start surface. */
public interface StartSurface {
    /**
     * Called to initialize this interface.
     * It should be called before showing.
     * It should not be called in the critical startup process since it will do expensive work.
     * It might be called many times.
     */
    void initialize();

    /**
     * An observer that is notified when the start surface internal state, excluding
     * the states notified in {@link OverviewModeObserver}, is changed.
     */
    interface StateObserver {
        /**
         * Called when the internal state is changed.
         * @param overviewModeState the {@link OverviewModeState}.
         * @param shouldShowTabSwitcherToolbar Whether or not should show the Tab switcher toolbar.
         */
        void onStateChanged(
                @OverviewModeState int overviewModeState, boolean shouldShowTabSwitcherToolbar);
    }

    /**
     * Set the given {@link StateObserver}.
     * Note that this will override the previous observer.
     * @param observer The given observer.
     */
    void setStateChangeObserver(StateObserver observer);

    /**
     * Defines an interface to pass out tab selecting event.
     */
    interface OnTabSelectingListener extends TabSwitcher.OnTabSelectingListener {}

    /**
     * Set the listener to get the {@link Layout#onTabSelecting} event from the Tab Switcher.
     * @param listener The {@link OnTabSelectingListener} to use.
     */
    void setOnTabSelectingListener(OnTabSelectingListener listener);

    /**
     * Called when native initialization is completed.
     */
    void initWithNative();

    /**
     * An observer that is notified when the StartSurface view state changes.
     */
    interface OverviewModeObserver {
        /**
         * Called when overview mode starts showing.
         */
        void startedShowing();

        /**
         * Called when overview mode finishes showing.
         */
        void finishedShowing();

        /**
         * Called when overview mode starts hiding.
         */
        void startedHiding();

        /**
         * Called when overview mode finishes hiding.
         */
        void finishedHiding();
    }

    /**
     * Interface to control the StartSurface.
     */
    interface Controller {
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

        /**
         * Hide the overview.
         * @param animate Whether we should animate while hiding.
         */
        void hideOverview(boolean animate);

        /**
         * Show the overview.
         * @param animate Whether we should animate while showing.
         */
        void showOverview(boolean animate);

        /**
         * Sets the state {@link OverviewModeState}.
         * @param state the {@link OverviewModeState} to show.
         */
        void setOverviewState(@OverviewModeState int state);

        /**
         * Called by the TabSwitcherLayout when the system back button is pressed.
         * @return Whether or not the TabSwitcher consumed the event.
         */
        boolean onBackPressed();

        /**
         * Enable recording the first meaningful paint event of StartSurface.
         * @param activityCreateTimeMs {@link SystemClock#elapsedRealtime} at activity creation.
         */
        void enableRecordingFirstMeaningfulPaint(long activityCreateTimeMs);
    }

    /**
     * @return Controller implementation that can be used for controlling
     *         visibility changes.
     */
    Controller getController();

    /**
     * @return TabListDelegate implementation that can be used to access the Tab List.
     */
    TabSwitcher.TabListDelegate getTabListDelegate();

    /**
     * @return {@link Supplier} that provides dialog visibility.
     */
    Supplier<Boolean> getTabGridDialogVisibilitySupplier();

    /**
     * Called after the Chrome activity is launched. This is only called if the StartSurface is
     * shown when Chrome is launched from cold start.
     * @param activityCreationTimeMs {@link SystemClock#elapsedRealtime} at activity creation.
     */
    void onOverviewShownAtLaunch(final long activityCreationTimeMs);
}
