// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tabmodel.TabPersistenceUtils.shouldSkipTab;

import androidx.annotation.IntDef;
import androidx.core.util.Supplier;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupVisualDataStore;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;

/** Manages the tab restoration process after loading tabs from storage. */
@NullMarked
class TabRestorer {
    private static final int RESTORE_BATCH_SIZE = 5;

    @IntDef({
        State.EMPTY,
        State.RESTORE_ONCE_LOADED,
        State.LOADED,
        State.RESTORING,
        State.CANCELLED,
        State.FINISHING,
        State.FINISHED
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        // No data to restore tabs has been loaded.
        int EMPTY = 0;
        // Restore once loaded.
        int RESTORE_ONCE_LOADED = 1;
        // Data to restore tabs has been loaded.
        int LOADED = 2;
        // Tab restore is in progress.
        int RESTORING = 3;
        // Tab restore is cancelled.
        int CANCELLED = 4;
        // Tab restore is finished, but the finish signals have not been sent yet.
        int FINISHING = 5;
        // Tab restore is finished and all finish signals have been sent.
        int FINISHED = 6;
    }

    interface TabRestorerDelegate {
        /**
         * Called when all the data is loaded. This is guaranteed to be called before onCancelled or
         * onFinished.
         *
         * @param incognito Whether the data is for incognito tabs.
         * @param restoredTabCount The number of tabs that were restored.
         */
        void onDataLoaded(boolean incognito, int restoredTabCount);

        /**
         * Called when the tab restorer is cancelled. It is guaranteed that only one of onCancelled
         * or onFinished will be called.
         *
         * @param incognito Whether the tab restorer is for incognito tabs.
         */
        void onCancelled(boolean incognito);

        /**
         * Called when all tabs have been created. It is guaranteed that only one of onCancelled or
         * onFinished will be called.
         *
         * @param incognito Whether the tab restorer is for incognito tabs.
         */
        void onFinished(boolean incognito);

        /**
         * Called when the active tab has been restored.
         *
         * @param incognito Whether the active tab is incognito.
         */
        void onActiveTabRestored(boolean incognito);

        /**
         * Called when the details of a tab have been read {@see
         * TabPersistentStoreObserver#onDetailsRead}.
         */
        void onDetailsRead(
                int index,
                @TabId int tabId,
                String url,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex,
                boolean isIncognito,
                boolean fromMerge);
    }

    private final boolean mIncognito;
    private final TabRestorerDelegate mDelegate;
    private final TabCreator mTabCreator;
    private final Supplier<ScopedStorageBatch> mBatchFactory;
    private final List<Integer> mTabIdsToIgnore = new ArrayList<>();

    private @State int mState = State.EMPTY;
    private @Nullable StorageLoadedData mData;
    private boolean mRestoreActiveTabImmediately;
    private int mRestoreFilteredTabCount;

    /**
     * Track the index we are restoring the next tab from. This is done globally so that {@link
     * #restoreTabStateForUrl(String)} and {@link #restoreTabStateForId(int)} can exclude tabs that
     * have already been restored.
     */
    private int mIndex;

    /**
     * @param incognito Whether the tab restorer is for incognito tabs.
     * @param delegate The delegate to notify when the tab restorer for certain events.
     * @param tabCreator The tab creator to use to create tabs.
     * @param batchFactory The factory to create scoped storage batches.
     */
    TabRestorer(
            boolean incognito,
            TabRestorerDelegate delegate,
            TabCreator tabCreator,
            Supplier<ScopedStorageBatch> batchFactory) {
        mIncognito = incognito;
        mDelegate = delegate;
        mTabCreator = tabCreator;
        mBatchFactory = batchFactory;
    }

    /**
     * Called when the tab state storage service has finished loading tabs from storage.
     *
     * @param data The data loaded from storage.
     */
    public void onDataLoaded(StorageLoadedData data) {
        mData = data;
        int restoredTabCount = data.getLoadedTabStates().length;

        // Special case for when cancellation happened during loading. In this case we cancel as
        // soon as loading has finished.
        if (mState == State.CANCELLED) {
            mDelegate.onDataLoaded(mIncognito, restoredTabCount);
            cancelInternal();
            return;
        }

        // Start was already called before the load finished. Start immediately.
        if (mState == State.RESTORE_ONCE_LOADED) {
            mState = State.LOADED;
            mDelegate.onDataLoaded(mIncognito, restoredTabCount);
            start(mRestoreActiveTabImmediately);
            return;
        }

        assert mState == State.EMPTY;
        mState = State.LOADED;
        mDelegate.onDataLoaded(mIncognito, restoredTabCount);
    }

    /**
     * Start the tab restoration process.
     *
     * <p>This should only be called after {@link #onDataLoaded(StorageLoadedData)} has been called.
     *
     * @param restoreActiveTabImmediately Whether the active tab should be restored immediately. If
     *     false another tab may have already been created and activated so this should just restore
     *     the active tab as if it were any other tab.
     */
    public void start(boolean restoreActiveTabImmediately) {
        mRestoreActiveTabImmediately = restoreActiveTabImmediately;

        // If load is not finished yet, schedule restore to start as soon as it finishes.
        if (mState == State.EMPTY) {
            mState = State.RESTORE_ONCE_LOADED;
            return;
        }
        if (mState != State.LOADED) return;

        mState = State.RESTORING;

        // If there are no tabs to restore, we can finish immediately.
        assert mData != null;
        if (mData.getLoadedTabStates().length == 0) {
            // Cleanup as soon as possible rather than posting.
            mState = State.FINISHING; // Skip assert for an invalid transition.
            onFinished();
            return;
        }

        // Synchronously restore the active tab if requested as there is no other tab already open
        // and doing this in a posted task would block user interaction with the app until finished.
        if (restoreActiveTabImmediately) {
            restoreActiveTab();
        } else {
            // Post this task as there is an assumption that another tab is already open and this
            // operation is not blocking user interaction.
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::restoreNextBatchOfTabs);
        }
    }

    /**
     * If a tab with the matching URL exists in the remaining tabs to restore this will immediately
     * restore that tab. This is relatively expensive as it requires parsing the entire tab state
     * for each tab. Note that this does not consider already restored tabs. Restoration will resume
     * normally after this, but if a tab was restored by this method it will not be restored a
     * second time.
     *
     * @param url The URL to restore the tab state for.
     * @return Whether a tab was restored.
     */
    public boolean restoreTabStateForUrl(String url) {
        return restoreTabStateByPredicate(
                loadedTabState -> {
                    var contentsState = loadedTabState.tabState.contentsState;
                    return contentsState != null
                            && url.equals(contentsState.getVirtualUrlFromState());
                });
    }

    /**
     * If a tab with the matching ID exists in the remaining tabs to restore this will immediately
     * restore that tab. Note that this does not consider already restored tabs. Restoration will
     * resume normally after this, but if a tab was restored by this method it will not be restored
     * a second time.
     *
     * @param tabId The tab ID to restore the tab state for.
     * @return Whether a tab was restored.
     */
    public boolean restoreTabStateForId(@TabId int tabId) {
        return restoreTabStateByPredicate(loadedTabState -> loadedTabState.tabId == tabId);
    }

    /**
     * Cancels the tab restoration process. If loading has not finished, the cancel logic will run
     * after loading has finished.
     */
    public void cancel() {
        if (mState == State.CANCELLED || mState == State.FINISHING || mState == State.FINISHED) {
            return;
        }

        mState = State.CANCELLED;
        // This may no-op if the load hasn't finished yet, it will be completed when the load
        // finishes instead so the memory used by the StorageLoadedData can be released.
        cancelInternal();
    }

    private void cancelInternal() {
        if (mData != null) {
            // Delegate still needs access to the StorageLoadedData before it is cleaned up.
            mDelegate.onCancelled(mIncognito);
            cleanupStorageLoadedData();
        }
    }

    private void postTaskToFinish() {
        mState = State.FINISHING;
        PostTask.postTask(TaskTraits.UI_DEFAULT, this::onFinished);
    }

    /** Called when the tab restoration process has finished. */
    private void onFinished() {
        if (mState == State.CANCELLED || mState == State.FINISHED) return;

        assert mState == State.FINISHING;
        mState = State.FINISHED;

        // Delegate still needs access to the StorageLoadedData before it is cleaned up.
        mDelegate.onFinished(mIncognito);
        cleanupStorageLoadedData();

        RecordHistogram.recordCount1000Histogram(
                "Tabs.TabStateStore.FilteredTabCount", mRestoreFilteredTabCount);
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

    private boolean restoreTabStateByPredicate(Predicate<LoadedTabState> predicate) {
        if (mData == null
                || mState == State.CANCELLED
                || mState == State.FINISHING
                || mState == State.FINISHED) {
            return false;
        }

        LoadedTabState[] loadedTabStates = mData.getLoadedTabStates();
        for (int i = mIndex; i < loadedTabStates.length; i++) {
            LoadedTabState loadedTabState = loadedTabStates[i];
            if (!mTabIdsToIgnore.contains(loadedTabState.tabId) && predicate.test(loadedTabState)) {
                mTabIdsToIgnore.add(loadedTabState.tabId);
                restoreTab(loadedTabState, i, /* isActive= */ false);
                return true;
            }
        }
        return false;
    }

    /**
     * Restores the active tab from {@code data}. Will post a task to restore the next batch if
     * there are more tabs to restore otherwise will signal the end of restoration.
     */
    private void restoreActiveTab() {
        if (mState == State.CANCELLED) return;
        assert mState == State.RESTORING;

        assert mData != null;
        LoadedTabState[] loadedTabStates = mData.getLoadedTabStates();
        assert loadedTabStates.length > 0;

        int activeTabIndex = mData.getActiveTabIndex();
        int restoredActiveTabIndex =
                (activeTabIndex >= 0 && activeTabIndex < loadedTabStates.length)
                        ? activeTabIndex
                        : 0;
        LoadedTabState activeTabState = loadedTabStates[restoredActiveTabIndex];
        restoreTab(
                activeTabState,
                restoredActiveTabIndex,
                /* isActive= */ true);

        if (loadedTabStates.length == 1) {
            postTaskToFinish();
            return;
        }
        mTabIdsToIgnore.add(activeTabState.tabId);
        PostTask.postTask(TaskTraits.UI_DEFAULT, this::restoreNextBatchOfTabs);
    }

    /**
     * Restores a single tab.
     *
     * @param loadedTabState The tab state to restore.
     * @param index The index of the tab to restore.
     * @param isActive Whether the tab is the active tab.
     */
    private void restoreTab(LoadedTabState loadedTabState, int index, boolean isActive) {
        assert mState == State.RESTORING;
        @TabId int tabId = loadedTabState.tabId;
        Tab tab = resolveTab(loadedTabState.tabState, tabId, index);
        if (tab == null) {
            WebContentsState state = loadedTabState.tabState.contentsState;
            if (state != null) state.destroy();
            return;
        }

        boolean isIncognito = mIncognito;
        mDelegate.onDetailsRead(
                index,
                tabId,
                tab.getUrl().getSpec(),
                /* isStandardActiveIndex= */ !isIncognito && isActive,
                /* isIncognitoActiveIndex= */ isIncognito && isActive,
                /* isIncognito= */ isIncognito,
                /* fromMerge= */ false);
    }

    /**
     * Restores the next batch of tabs from {@code mData}. Will post a task to restore the next
     * batch if there are more tabs to restore otherwise will signal the end of restoration.
     */
    private void restoreNextBatchOfTabs() {
        if (mState == State.CANCELLED) return;
        assert mState == State.RESTORING;

        int batchSize = RESTORE_BATCH_SIZE;

        assert mData != null;
        LoadedTabState[] loadedTabStates = mData.getLoadedTabStates();
        int finalIndex = loadedTabStates.length;

        try (ScopedStorageBatch batch = mBatchFactory.get()) {
            while (batchSize > 0 && mIndex < finalIndex) {
                LoadedTabState loadedTabState = loadedTabStates[mIndex];
                if (!mTabIdsToIgnore.contains(loadedTabState.tabId)) {
                    restoreTab(loadedTabState, mIndex, /* isActive= */ false);
                }

                mIndex++;
                batchSize--;
            }
        }

        if (mIndex < finalIndex) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::restoreNextBatchOfTabs);
        } else {
            postTaskToFinish();
        }
    }

    private @Nullable Tab resolveTab(TabState tabState, @TabId int tabId, int index) {
        assert mData != null;
        boolean isActiveTab = mData.getActiveTabIndex() == index;
        if (!isActiveTab && shouldSkipTab(tabState)) {
            mRestoreFilteredTabCount++;
            return null;
        }

        @Nullable Tab tab = null;
        if (isActiveTab && tabState.contentsState == null && tabState.url != null) {
            // Use fallback url if no contents state is available.
            tab =
                    mTabCreator.createNewTab(
                            new LoadUrlParams(tabState.url),
                            TabLaunchType.FROM_RESTORE,
                            null,
                            index);
        } else {
            tab = mTabCreator.createFrozenTab(tabState, tabId, index);
        }

        if (isActiveTab && tab != null) {
            mDelegate.onActiveTabRestored(mIncognito);
        }
        return tab;
    }
}
