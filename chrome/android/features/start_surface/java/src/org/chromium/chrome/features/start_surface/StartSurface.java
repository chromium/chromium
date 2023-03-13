// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.os.SystemClock;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.chrome.features.tasks.TasksView;

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
     * Show the Start surface homepage. Used only when refactor is enabled.
     */
    void show(boolean animate);

    /**
     * Hide the Start surface homepage. Used only when refactor is enabled.
     */
    void hide(boolean animate);

    /**
     * Called when the Start surface is hidden. It hides TasksSurfaces which are created when the
     * Start surface is enabled.
     */
    void onHide();

    /**
     * Called before the tab switcher starts showing.
     */
    void beforeShowTabSwitcherView();

    /**
     * Called before tab switcher starts hiding.
     */
    void beforeHideTabSwitcherView();

    /**
     * An observer that is notified when the start surface internal state, excluding
     * the states notified in {@link TabSwitcherViewObserver}, is changed.
     */
    // TODO(crbug.com/1315679): Replace this observer with LayoutStateObserver after the {@link
    // ChromeFeatureList.START_SURFACE_REFACTOR} is enabled by default.
    @Deprecated
    interface StateObserver {
        /**
         * Called when the internal state is changed.
         * @param startSurfaceState the {@link StartSurfaceState}.
         * @param shouldShowTabSwitcherToolbar Whether or not should show the Tab switcher toolbar.
         */
        void onStateChanged(
                @StartSurfaceState int startSurfaceState, boolean shouldShowTabSwitcherToolbar);
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
     * An observer that is notified when the tab switcher view state changes.
     */
    interface TabSwitcherViewObserver {
        /**
         * Called when tab switcher starts showing.
         */
        void startedShowing();

        /**
         * Called when tab switcher finishes showing.
         */
        void finishedShowing();

        /**
         * Called when tab switcher starts hiding.
         */
        void startedHiding();

        /**
         * Called when tab switcher finishes hiding.
         */
        void finishedHiding();
    }

    /**
     * @param listener Registers {@code listener} for tab switcher status changes.
     */
    void addTabSwitcherViewObserver(TabSwitcherViewObserver listener);

    /**
     * @param listener Unregisters {@code listener} for tab switcher status changes.
     */
    void removeTabSwitcherViewObserver(TabSwitcherViewObserver listener);

    /**
     * Hide the tab switcher view.
     * @param animate Whether we should animate while hiding.
     */
    void hideTabSwitcherView(boolean animate);

    /**
     * Show the overview.
     * @param animate Whether we should animate while showing.
     */
    // TODO(crbug.com/1315676): Rename this function once the Start surface layout and Grid tab
    // switcher layout are decoupled.
    void showOverview(boolean animate);

    /**
     * Sets the state {@link StartSurfaceState} and {@link NewTabPageLaunchOrigin}.
     * @param state The {@link StartSurfaceState} to show.
     * @param launchOrigin The {@link NewTabPageLaunchOrigin} representing what launched the
     *         start surface.
     */
    void setStartSurfaceState(
            @StartSurfaceState int state, @NewTabPageLaunchOrigin int launchOrigin);

    /**
     * Sets the state {@link StartSurfaceState} without changing the existing {@link
     * NewTabPageLaunchOrigin}.
     * @param state The {@link StartSurfaceState} to show.
     */
    void setStartSurfaceState(@StartSurfaceState int state);

    /**
     * Set the launch origin.
     * @param launchOrigin The {@link NewTabPageLaunchOrigin} representing what launched the
     *         start surface.
     */
    void setLaunchOrigin(@NewTabPageLaunchOrigin int launchOrigin);

    /**
     * Resets the scroll position. This is called when Start surface is showing but not via back
     * operations.
     */
    void resetScrollPosition();

    /**
     * Called by the TabSwitcherLayout when the system back button is pressed.
     * @return Whether or not the TabSwitcher consumed the event.
     */
    boolean onBackPressed();

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
     * @return The Tab switcher container view.
     */
    ViewGroup getTabSwitcherContainer();

    /**
     * @return The Tab switcher controller.
     */
    @Nullable
    TabSwitcher.Controller getGridTabSwitcherController();

    /**
     * Sets the parent view for snackbars. If <code>null</code> is given, the original parent
     * view is restored.
     *
     * @param parentView The {@link ViewGroup} to attach snackbars to.
     */
    void setSnackbarParentView(ViewGroup parentView);

    @Deprecated
    /*
     * Returns whether start surface homepage is showing.
     *
     * TODO(1347089): Removes this test after the refactoring is enabled by default. This function
     * is only used by {@link TabSwitcherAndStartSurfaceLayout} which will go away after the
     * refactoring. This API add an additional check of {@link StartSurfaceState#SHOWING_PREVIOUS}
     * to prevent shrinking animation when returns to Start surface from a Tab.
     * See crbug.com/1248680.
     */
    boolean isShowingStartSurfaceHomepage();

    /*
     * Returns whether start surface homepage is showing. Compared with
     * isShowingStartSurfaceHomepage(), this API only checks state
     * {@link StartSurfaceState#SHOWN_HOMEPAGE} when the refactoring is disabled.
     */
    boolean isHomepageShown();

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
    // TODO(crbug.com/1315676): Remove this API after the refactoring is done.
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
     * Returns the primary {@link TasksView} (omnibox, most visited, feed, etc.). Can be null if
     * grid tab switcher is enabled but Start surface is disabled.
     */
    @Nullable
    TasksView getPrimarySurfaceView();

    /**
     * TODO(crbug.com/1315676): Remove this API after the bug is resolved.
     *
     * @return A {@link ObservableSupplier <TabSwitcherCustomViewManager>}.
     */
    @NonNull
    ObservableSupplier<TabSwitcherCustomViewManager> getTabSwitcherCustomViewManagerSupplier();
}
