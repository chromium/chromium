// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Interface for the Tab Switcher. */
public interface TabSwitcher {
    @IntDef({TabSwitcherType.GRID, TabSwitcherType.SINGLE, TabSwitcherType.NONE})
    @Retention(RetentionPolicy.SOURCE)
    @interface TabSwitcherType {
        int GRID = 0;
        // int CAROUSEL_DEPRECATED = 1;
        int SINGLE = 2;
        int NONE = 3;
    }

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

    // TODO(crbug.com/40946413): Post AndroidHub launch this will only be used by
    // SingleTabSwitcherCoordinator. Consider deprecating this interface and migrating
    // SingleTabSwitcherCoordinator's usage to be internal to start_surface/.
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

    // TODO(crbug.com/40946413): Post AndroidHub launch this will only be used by
    // SingleTabSwitcherCoordinator. Consider deprecating this interface and migrating
    // SingleTabSwitcherCoordinator's usage to be internal to start_surface/.
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

        /**
         * Hide the tab switcher view.
         *
         * @param animate Whether we should animate while hiding.
         */
        void hideTabSwitcherView(boolean animate);

        /**
         * Show the tab switcher view.
         *
         * @param animate Whether we should animate while showing.
         */
        void showTabSwitcherView(boolean animate);

        /** Returns the tab switcher type. */
        @TabSwitcherType
        int getTabSwitcherType();

        /**
         * Called after the Chrome activity is launched.
         *
         * @param activityCreationTimeMs {@link SystemClock#elapsedRealtime} at activity creation.
         */
        // TODO(crbug.com/40221888): Remove this API when tab switcher and start surface are
        // decoupled.
        void onOverviewShownAtLaunch(long activityCreationTimeMs);
    }

    /**
     * Returns a {@link Controller} implementation that can be used for controlling visibility
     * changes.
     */
    Controller getController();

    /** Returns a {@link Supplier} that provides dialog visibility. */
    Supplier<Boolean> getTabGridDialogVisibilitySupplier();

    /**
     * Returns a {@link TabSwitcherCustomViewManager} that allows to pass custom views to {@link
     * TabSwitcherCoordinator}.
     */
    @Nullable
    default TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        return null;
    }

    /** Returns the number of elements in the tab switcher's tab list model. */
    int getTabSwitcherTabListModelSize();

    /**
     * Set the tab switcher's current RecyclerViewPosition. This is a no-op for tab switcher without
     * a recyclerView.
     */
    default void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition recyclerViewPosition) {}

    /**
     * Show the Quick Delete animation on the tab list.
     *
     * @param onAnimationEnd Runnable that is invoked when the animation is completed.
     * @param tabs The tabs to fade with the animation. These tabs will get closed after the
     *     animation is complete.
     */
    void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs);

    /**
     * Open the invitation modal on top of the tab switcher view when an invitation intent is
     * intercepted.
     *
     * @param invitationId The id of the invitation.
     */
    void openInvitationModal(String invitationId);
}
