// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tabmodel.TabModel;

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
    private boolean mAttached;

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
        assert !tab.hasParentCollection() : "LiveBackgroundTab must not have a parent collection.";
        assert !tab.isDestroyed() : "LiveBackgroundTab must not wrap a destroyed tab.";
        assert !tab.isOffTheRecord() : "LiveBackgroundTab does not support incognito tabs.";
        mPool = pool;
        mTab = tab;
        mPlaceholderTabId = placeholderTabId;
        mTaskId = taskId;
    }

    @Override
    public @TabId int getPlaceholderTabId() {
        return mPlaceholderTabId;
    }

    @Override
    public Tab attachTabImpl(TabModel tabModel, int index) {
        assert !mAttached : "LiveBackgroundTab has already been attached or destroyed.";
        mAttached = true;
        mPool.removeTab(mTab.getId());
        tabModel.addTab(
                mTab, index, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_BACKGROUND);
        return mTab;
    }

    /** Returns the underlying live Tab instance. */
    public Tab getTab() {
        return mTab;
    }

    /** Returns the task ID associated with the background session, or null if none. */
    public @Nullable Integer getTaskId() {
        return mTaskId;
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
