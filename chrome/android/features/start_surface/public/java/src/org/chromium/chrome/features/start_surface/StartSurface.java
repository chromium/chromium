// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.os.SystemClock;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.tasks.TasksSurface;
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
     * Called when activity is being destroyed.
     */
    void destroy();

    /**
     * Called when the Start surface is hidden. It hides TasksSurfaces which are created when the
     * Start surface is enabled.
     */
    void onHide();

    /**
     * An observer that is notified when the start surface internal state, excluding
     * the states notified in {@link OverviewModeObserver}, is changed.
     *
     * TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
     * two different things, let's audit the usage of this observer.
     */
    interface StateObserver {
        /**
         * Called when the internal state is changed.
         * @param overviewModeState the {@link StartSurfaceState}.
         * @param shouldShowTabSwitcherToolbar Whether or not should show the Tab switcher toolbar.
         */
        void onStateChanged(
                @StartSurfaceState int overviewModeState, boolean shouldShowTabSwitcherToolbar);
    }

    /**
     * @param onOffsetChangedListener Registers listener for the offset changes on top of the start
     *         surface.
     */
    void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener);

    /**
     * @param onOffsetChangedListener Unregisters listener for the offset changes on top of the
     *         start surface.
     */
    void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener);

    /**
     * @param observer Registers {@code observer} for the {@link StartSurfaceState} changes.
     */
    void addStateChangeObserver(StateObserver observer);

    /**
     * @param observer Unregisters {@code observer} for the {@link StartSurfaceState} changes.
     */
    void removeStateChangeObserver(StateObserver observer);

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
         * Sets the state {@link StartSurfaceState} and {@link NewTabPageLaunchOrigin}.
         * @param state The {@link StartSurfaceState} to show.
         * @param launchOrigin The {@link NewTabPageLaunchOrigin} representing what launched the
         *         start surface.
         */
        void setOverviewState(
                @StartSurfaceState int state, @NewTabPageLaunchOrigin int launchOrigin);

        /**
         * Sets the state {@link StartSurfaceState} without changing the existing {@link
         * NewTabPageLaunchOrigin}.
         * @param state The {@link StartSurfaceState} to show.
         */
        void setOverviewState(@StartSurfaceState int state);

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

        /**
         * @return The current {@link StartSurfaceState}.
         */
        @StartSurfaceState
        int getStartSurfaceState();

        /**
         * @return The previous {@link StartSurfaceState}.
         */
        @StartSurfaceState
        int getPreviousStartSurfaceState();

        /**
         * @return Whether the Start surface or the Tab switcher is shown or showing.
         */
        boolean inShowState();

        /**
         * @return The Tab switcher container view.
         */
        ViewGroup getTabSwitcherContainer();

        /**
         * Sets the parent view for snackbars. If <code>null</code> is given, the original parent
         * view is restored.
         *
         * @param parentView The {@link ViewGroup} to attach snackbars to.
         */
        void setSnackbarParentView(ViewGroup parentView);

        /*
         * Returns whether start surface homepage is showing.
         */
        boolean isShowingStartSurfaceHomepage();
    }

    /**
     * @return Controller implementation that can be used for controlling
     *         visibility changes.
     */
    Controller getController();

    /**
     * Returns the TabListDelegate implementation that can be used to access the Tab list of the
     * grid tab switcher surface.
     */
    TabSwitcher.TabListDelegate getGridTabListDelegate();

    /**
     * Returns the TabListDelegate implementation that can be used to access the Tab list of the
     * carousel/single tab switcher when start surface is enabled; when start surface is disabled,
     * null should be returned.
     */
    TabSwitcher.TabListDelegate getCarouselOrSingleTabListDelegate();

    /**
     * @return {@link Supplier} that provides dialog visibility.
     */
    Supplier<Boolean> getTabGridDialogVisibilitySupplier();

    /**
     * Called after the Chrome activity is launched.
     * @param isOverviewShownOnStartup Whether the StartSurace is shown when Chrome is launched from
     *                                 cold start.
     * @param activityCreationTimeMs {@link SystemClock#elapsedRealtime} at activity creation.
     */
    void onOverviewShownAtLaunch(
            boolean isOverviewShownOnStartup, final long activityCreationTimeMs);

    /**
     * Returns the primary {@link TasksSurface} (omnibox, most visited, feed, etc.). Can be null if
     * grid tab switcher is enabled but Start surface is disabled.
     */
    @Nullable
    TasksSurface getPrimaryTasksSurface();
}
