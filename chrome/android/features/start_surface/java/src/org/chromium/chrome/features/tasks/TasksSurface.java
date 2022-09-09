// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;

/**
 * Interface for the Tasks-related Start Surface. The tasks surface displays information related to
 *  task management, such as the tab switcher, most visited tiles, and omnibox. Implemented by
 *  {@link TasksSurfaceCoordinator}.
 */
public interface TasksSurface {
    /**
     * Called to initialize this interface.
     * It should be called before showing.
     * It should not be called in the critical startup process since it will do expensive work.
     * It might be called many times.
     */
    void initialize();

    /**
     * Called to initialize MV tiles.
     * It should be called before MV tiles is showing.
     * It might be called many times.
     */
    void initializeMVTiles();

    /**
     * Set the listener to get the {@link Layout#onTabSelecting} event from the Grid Tab Switcher.
     * @param listener The {@link TabSwitcher.OnTabSelectingListener} to use.
     */
    void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener);

    /**
     * @return Controller implementation for overview observation and visibility changes.
     */
    @Nullable
    TabSwitcher.Controller getController();

    /**
     * @return TabListDelegate implementation to access the tab grid.
     */
    @Nullable
    TabSwitcher.TabListDelegate getTabListDelegate();

    /**
     * @return {@link Supplier} that provides dialog visibility.
     */
    @Nullable
    Supplier<Boolean> getTabGridDialogVisibilitySupplier();

    /**
     * Get the view container {@link ViewGroup} of the tasks surface body.
     * @return The tasks surface body view container {@link ViewGroup}.
     */
    ViewGroup getBodyViewContainer();

    /**
     * Get the view {@link View} of the surface.
     * @return The surface's container {@link View}.
     */
    View getView();

    /**
     * Called when the native initialization is completed. Anything to construct a TasksSurface but
     * require native initialization should be constructed here.
     */
    void onFinishNativeInitialization(Context context, OmniboxStub omniboxStub,
            @Nullable FeedReliabilityLogger feedReliabilityLogger);

    /**
     * @param onOffsetChangedListener Registers listener for the offset changes of the header view.
     */
    void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener);

    /**
     * @param onOffsetChangedListener Unregisters listener for the offset changes of the header
     *         view.
     */
    void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener);

    /**
     * Update the fake search box layout.
     * @param height Current height of the fake search box layout.
     * @param topMargin Current top margin of the fake search box layout.
     * @param endPadding Current end padding of the fake search box layout.
     * @param translationX Current translationX of text view in fake search box layout.
     * @param buttonSize Current height and width of the buttons in fake search box layout.
     * @param lensButtonLeftMargin Current left margin of the lens button in fake search box layout.
     */
    void updateFakeSearchBox(int height, int topMargin, int endPadding, float translationX,
            int buttonSize, int lensButtonLeftMargin);

    /**
     * Called when the Tasks surface is hidden.
     */
    void onHide();

    @VisibleForTesting
    /** Returns whether the cleanup of MV tiles has been done after hiding the Start surface. */
    boolean isMVTilesCleanedUp();

    @VisibleForTesting
    /** Returns whether the MV tiles has been initialized. */
    boolean isMVTilesInitialized();

    /**
     * TODO(crbug.com/1315676): Remove this API after the bug is resolved.
     *
     * @return {@link TabSwitcherCustomViewManager} that allows to pass custom views to {@link
     *         TabSwitcherCoordinator}.
     */
    @Nullable
    TabSwitcherCustomViewManager getTabSwitcherCustomViewManager();
}
