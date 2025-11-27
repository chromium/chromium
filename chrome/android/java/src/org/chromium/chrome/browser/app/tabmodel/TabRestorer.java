// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupVisualDataStore;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Manages the tab restoration process after loading tabs from storage. */
@NullMarked
class TabRestorer {
    @IntDef({State.EMPTY, State.LOADED, State.RESTORING, State.CANCELLED, State.FINISHED})
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        int EMPTY = 0;
        int LOADED = 1;
        int RESTORING = 2;
        int CANCELLED = 3;
        int FINISHED = 4;
    }

    interface TabRestorerDelegate {
        /** Called when the tab restorer is cancelled. */
        void onCancelled();

        /** Called when all tabs have been created. */
        void onFinishedCreatingAllTabs();
    }

    private final TabRestorerDelegate mDelegate;

    // TODO(crbug.com/464029104): Remove after the restore code is migrated from TabStateStore.
    @SuppressWarnings("UnusedVariable")
    private final TabCreatorManager mTabCreatorManager;

    private @State int mState = State.EMPTY;
    private @Nullable StorageLoadedData mData;

    /**
     * @param delegate The delegate to notify when the tab restorer for certain events.
     * @param tabCreatorManager The tab creator manager to use to create tabs.
     */
    TabRestorer(TabRestorerDelegate delegate, TabCreatorManager tabCreatorManager) {
        mDelegate = delegate;
        mTabCreatorManager = tabCreatorManager;
    }

    /**
     * Called when the tab state storage service has finished loading tabs from storage.
     *
     * @param data The data loaded from storage.
     */
    public void onDataLoaded(StorageLoadedData data) {
        mData = data;

        // Special case for when cancellation happened during loading. In this case we cancel as
        // soon as loading has finished.
        if (mState == State.CANCELLED) {
            cancelInternal();
            return;
        }

        assert mState == State.EMPTY;
        mState = State.LOADED;
        if (mData.getLoadedTabStates().length == 0) {
            onFinished();
        }
    }

    /**
     * Start the tab restoration process.
     *
     * <p>This should only be called after {@link #onDataLoaded(StorageLoadedData)} has been called.
     */
    public void start() {
        assert mState != State.EMPTY;
        if (mState != State.LOADED) return;

        mState = State.RESTORING;
        // TODO(crbug.com/464029104): Move the restore code here from TabStateStore.
        onFinished();
    }

    /**
     * Cancels the tab restoration process. If loading has not finished, the cancel logic will run
     * after loading has finished.
     */
    public void cancel() {
        if (mState == State.CANCELLED || mState == State.FINISHED) return;

        mState = State.CANCELLED;
        // This may no-op if the load hasn't finished yet, it will be completed when the load
        // finishes instead so the memory used by the StorageLoadedData can be released.
        cancelInternal();
    }

    private void cancelInternal() {
        if (mData != null) {
            cleanupStorageLoadedData();
            mDelegate.onCancelled();
        }
    }

    /** Called when the tab restoration process has finished. */
    private void onFinished() {
        if (mState == State.CANCELLED || mState == State.FINISHED) return;

        mState = State.FINISHED;
        cleanupStorageLoadedData();
        mDelegate.onFinishedCreatingAllTabs();
    }

    /** Cleans up the {@link StorageLoadedData}. */
    private void cleanupStorageLoadedData() {
        assumeNonNull(mData);
        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            TabGroupVisualDataStore.removeCachedGroups(mData.getGroupsData());
        }
        mData.destroy();
        mData = null;
    }
}
