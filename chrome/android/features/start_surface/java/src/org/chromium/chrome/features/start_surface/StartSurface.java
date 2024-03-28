// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
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

    /** Called when activity is being destroyed. */
    void destroy();

    /** Show the Start surface homepage. Used only when refactor is enabled. */
    void show(boolean animate);

    /** Hide the Start surface homepage. Used only when refactor is enabled. */
    void hide(boolean animate);

    /**
     * Called when the Start surface is hidden. It hides TasksSurfaces which are created when the
     * Start surface is enabled.
     */
    void onHide();

    /**
     * @param onOffsetChangedListener Registers listener for the offset changes on top of the start
     *     surface.
     */
    void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener);

    /**
     * @param onOffsetChangedListener Unregisters listener for the offset changes on top of the
     *     start surface.
     */
    void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener);

    /** Defines an interface to pass out tab selecting event. */
    interface OnTabSelectingListener extends TabSwitcher.OnTabSelectingListener {}

    /**
     * Set the listener to get the {@link Layout#onTabSelecting} event from the Tab Switcher.
     * @param listener The {@link OnTabSelectingListener} to use.
     */
    void setOnTabSelectingListener(OnTabSelectingListener listener);

    /** Called when native initialization is completed. */
    void initWithNative();

    /** An observer that is notified when the tab switcher view state changes. */
    interface TabSwitcherViewObserver {
        /** Called when tab switcher starts showing. */
        void startedShowing();

        /** Called when tab switcher finishes showing. */
        void finishedShowing();

        /** Called when tab switcher starts hiding. */
        void startedHiding();

        /** Called when tab switcher finishes hiding. */
        void finishedHiding();
    }

    /**
     * Hide the tab switcher view.
     * @param animate Whether we should animate while hiding.
     */
    void hideTabSwitcherView(boolean animate);

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

    /*
     * Returns whether start surface homepage is showing. Compared with
     * isShowingStartSurfaceHomepage(), this API only checks state
     * {@link StartSurfaceState#SHOWN_HOMEPAGE} when the refactoring is disabled.
     */
    boolean isHomepageShown();

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
