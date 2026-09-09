// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.WindowAndroid;

/**
 * Standalone entity representing a live in-memory background Actor tab in {@link
 * BackgroundTabPool}.
 */
@NullMarked
public class LiveBackgroundTab implements BackgroundPoolTab {
    private final BackgroundTabPool mPool;
    private final Tab mTab;
    private final @TabId int mPlaceholderTabId;
    private final @Nullable Integer mTaskId;
    private final int mOriginalTabIndex;
    private boolean mAttached;
    private @Nullable TabObserver mTabObserver;

    /**
     * Constructs a {@link LiveBackgroundTab}.
     *
     * @param pool The {@link BackgroundTabPool} that owns this tab.
     * @param tab The live in-memory {@link Tab} instance.
     * @param placeholderTabId The placeholder tab ID associated with this background tab.
     * @param taskId The task ID associated with the background session, or null if none.
     */
    public LiveBackgroundTab(
            BackgroundTabPool pool,
            Tab tab,
            @TabId int placeholderTabId,
            @Nullable Integer taskId) {
        this(pool, tab, placeholderTabId, taskId, TabModel.INVALID_TAB_INDEX);
    }

    /**
     * Constructs a {@link LiveBackgroundTab} with an original tab index.
     *
     * @param pool The {@link BackgroundTabPool} that owns this tab.
     * @param tab The live in-memory {@link Tab} instance.
     * @param placeholderTabId The placeholder tab ID associated with this background tab.
     * @param taskId The task ID associated with the background session, or null if none.
     * @param originalTabIndex The original tab model index before detachment, or {@link
     *     TabModel#INVALID_TAB_INDEX}.
     */
    public LiveBackgroundTab(
            BackgroundTabPool pool,
            Tab tab,
            @TabId int placeholderTabId,
            @Nullable Integer taskId,
            int originalTabIndex) {
        assert !tab.hasParentCollection() : "LiveBackgroundTab must not have a parent collection.";
        assert !tab.isDestroyed() : "LiveBackgroundTab must not wrap a destroyed tab.";
        assert !tab.isOffTheRecord() : "LiveBackgroundTab does not support incognito tabs.";
        mPool = pool;
        mTab = tab;
        mPlaceholderTabId = placeholderTabId;
        mTaskId = taskId;
        mOriginalTabIndex = originalTabIndex;

        mTabObserver =
                new TabObserver() {
                    @Override
                    public void onDestroyed(Tab tab) {
                        mPool.removeTabById(mTab.getId());
                        removeObserver();
                    }
                };
        mTab.addObserver(mTabObserver);
    }

    @Override
    public @TabId int getPlaceholderTabId() {
        return mPlaceholderTabId;
    }

    @Override
    public Tab attachTabImpl(TabModel tabModel, int index) {
        assert !mAttached : "LiveBackgroundTab has already been attached or destroyed.";
        mAttached = true;
        removeObserver();
        mPool.removeTabById(mTab.getId());
        tabModel.addTab(
                mTab, index, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_BACKGROUND);
        return mTab;
    }

    /**
     * Attaches this live background tab to an active foreground window, halting offscreen
     * rendering, reparenting to the target {@link WindowAndroid}, inserting into {@link TabModel},
     * transferring group and pin properties from the placeholder tab, destroying the placeholder
     * tab, and evicting from the owning pool.
     *
     * @param model The target foreground {@link TabModel}.
     * @param window The target foreground {@link WindowAndroid}.
     * @param tabDelegateFactory The delegate factory for the window.
     * @return The attached {@link Tab} instance.
     */
    public Tab attachToForeground(
            TabModel model, WindowAndroid window, TabDelegateFactory tabDelegateFactory) {
        assert !mAttached : "LiveBackgroundTab already attached or destroyed.";
        mAttached = true;
        removeObserver();

        // 1. Offscreen rendering, window reparenting, model insertion, and group/pin transfer
        ActorTabStateHelper.restoreSessionTabToForeground(
                mTab, mPlaceholderTabId, mOriginalTabIndex, model, window, tabDelegateFactory);

        // 2. Pool eviction
        mPool.removeTabById(mTab.getId());
        return mTab;
    }

    private void removeObserver() {
        if (mTabObserver != null) {
            mTab.removeObserver(mTabObserver);
            mTabObserver = null;
        }
    }

    /** Returns the underlying live Tab instance. */
    public Tab getTab() {
        return mTab;
    }

    /** Returns the task ID associated with the background session, or null if none. */
    public @Nullable Integer getTaskId() {
        return mTaskId;
    }

    /** Returns the original tab index in the tab model before detachment. */
    public int getOriginalTabIndex() {
        return mOriginalTabIndex;
    }

    /** Marks this background tab dirty to trigger disk persistence by BackgroundTabPool. */
    public void markDirty() {
        markDirty(mTab);
    }

    /** Marks the given tab dirty so its storage observer (BackgroundTabPool) saves it to disk. */
    public static void markDirty(Tab tab) {
        TabStateAttributes.setDirty(tab);
    }
}
