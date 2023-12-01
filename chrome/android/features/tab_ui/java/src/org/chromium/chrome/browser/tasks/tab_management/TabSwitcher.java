// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.util.Size;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/** Interface for the Tab Switcher. */
public interface TabSwitcher {
    /** Defines an interface to pass out tab selecting event. */
    interface OnTabSelectingListener {
        /**
         * Called when a tab is getting selected. This will select the tab and exit the layout.
         *
         * @param tabId The ID of selected {@link Tab}.
         */
        void onTabSelecting(int tabId);
    }

    /**
     * Set the listener to receive tab selection events from the Tab Switcher. This should typically
     * trigger a tab selection and hide via a Layout.
     *
     * @param listener The {@link OnTabSelectingListener} to use.
     */
    void setOnTabSelectingListener(OnTabSelectingListener listener);

    /** Called when native initialization is completed. */
    void initWithNative();

    // TODO(crbug/1322733): Remove the following interfaces when we find a better way to notify the
    // layout that the GTS animation is finished.
    /** An observer that is notified when the TabSwitcher view state changes. */
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

    /** Interface to control the TabSwitcher. */
    interface Controller extends BackPressHandler {
        /**
         * @param listener Registers {@code listener} for tab switcher status changes.
         */
        void addTabSwitcherViewObserver(TabSwitcherViewObserver listener);

        /**
         * @param listener Unregisters {@code listener} for tab switcher status changes.
         */
        void removeTabSwitcherViewObserver(TabSwitcherViewObserver listener);

        /** Before tab switcher starts hiding. */
        void prepareHideTabSwitcherView();

        /**
         * Hide the tab switcher view.
         * @param animate Whether we should animate while hiding.
         */
        void hideTabSwitcherView(boolean animate);

        /** Before tab switcher starts showing. */
        default void prepareShowTabSwitcherView() {}

        /**
         * Show the tab switcher view.
         * @param animate Whether we should animate while showing.
         */
        void showTabSwitcherView(boolean animate);

        /**
         * Called by the StartSurfaceLayout when the system back button is pressed.
         * @return Whether or not the TabSwitcher consumed the event.
         */
        boolean onBackPressed();

        /** Returns whether any dialog is opened. */
        boolean isDialogVisible();

        /**
         * Returns an {@link ObservableSupplier<Boolean>} which yields true if any dialog is opened.
         */
        ObservableSupplier<Boolean> isDialogVisibleSupplier();

        /** Returns the tab switcher type. */
        @TabSwitcherType
        int getTabSwitcherType();

        /** Called when start surface is showing or hiding. */
        // TODO(crbug.com/1315676): Remove this API when tab switcher and start surface are
        // decoupled.
        void onHomepageChanged();

        /**
         * Called after the Chrome activity is launched.
         *
         * @param activityCreationTimeMs {@link SystemClock#elapsedRealtime} at activity creation.
         */
        // TODO(crbug.com/1315676): Remove this API when tab switcher and start surface are
        // decoupled.
        void onOverviewShownAtLaunch(long activityCreationTimeMs);

        /**
         * Sets the parent view for snackbars. If <code>null</code> is given, the original parent
         * view is restored.
         *
         * @param parentView The {@link ViewGroup} to attach snackbars to.
         */
        default void setSnackbarParentView(ViewGroup parentView) {}

        /** Returns the Tab switcher container view. */
        default ViewGroup getTabSwitcherContainer() {
            return null;
        }
    }

    /**
     * Returns a {@link Controller} implementation that can be used for controlling visibility
     * changes.
     */
    Controller getController();

    /** Interface to access the Tab List. */
    interface TabListDelegate {
        /** Returns the dynamic resource ID of the TabSwitcher RecyclerView. */
        int getResourceId();

        /**
         * Call before showing the Grid Tab Switcher from Start Surface with refactor disabled to
         * properly register the layout changed listener.
         */
        void prepareTabGridView();

        /**
         * Before calling {@link Controller#showTabSwitcherView} to start showing the
         * TabSwitcher {@link TabListRecyclerView}, call this to populate it without making it
         * visible.
         * @return Whether the {@link TabListRecyclerView} can be shown quickly.
         */
        boolean prepareTabSwitcherView();

        /**
         * This is called after the compositor animation is done, for potential clean-up work.
         * {@link TabSwitcherViewObserver#finishedHiding} happens after
         * the Android View animation, but before the compositor animation.
         */
        void postHiding();

        /**
         * Returns the {@link Rect} of the thumbnail of the current tab, relative to the TabSwitcher
         * {@link TabListRecyclerView} coordinates.
         */
        @NonNull
        Rect getThumbnailLocationOfCurrentTab();

        /** Returns the {@link Size} of a thumbnail. */
        @NonNull
        Size getThumbnailSize();

        /**
         * Returns the {@link Rect} of the {@link TabListRecyclerView} relative to its container.
         */
        Rect getRecyclerViewLocation();

        /**
         * Returns the top offset from top toolbar to tab list. Used to adjust the animations for
         * tab switcher.
         */
        int getTabListTopOffset();

        /** Request accessibility focus for the currently selected tab. */
        default void requestFocusOnCurrentTab() {}

        /**
         * @param r Runnable executed on next layout pass to run a show animation.
         */
        void runAnimationOnNextLayout(Runnable r);

        /**
         * Set a hook to receive all the {@link Bitmap}s returned by
         * {@link TabListMediator.ThumbnailFetcher} for testing.
         * @param callback The callback to send bitmaps through.
         */
        void setBitmapCallbackForTesting(Callback<Bitmap> callback);

        /** Returns the number of thumbnail fetching for testing. */
        int getBitmapFetchCountForTesting();

        /** Reset the current count of thumbnail fetches for testing. */
        default void resetBitmapFetchCountForTesting() {}

        /** Returns the mode of the list of Tabs. */
        int getListModeForTesting();
    }

    /** Returns the {@link TabListDelegate}. */
    TabListDelegate getTabListDelegate();

    /** Returns a {@link Supplier} that provides dialog visibility. */
    Supplier<Boolean> getTabGridDialogVisibilitySupplier();

    /**
     * Returns a {@link TabSwitcherCustomViewManager} that allows to pass custom views to {@link
     * TabSwitcherCoordinator}.
     */
    @Nullable
    TabSwitcherCustomViewManager getTabSwitcherCustomViewManager();

    /** Returns the number of elements in the tab switcher's tab list model. */
    int getTabSwitcherTabListModelSize();

    /** Set the tab switcher's current RecyclerViewPosition. */
    void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition recyclerViewPosition);
}
