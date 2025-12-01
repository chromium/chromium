// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.IntDef;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupVisualDataStore;

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
        /** Called when the tab restorer is cancelled. */
        void onCancelled();

        /** Called when all tabs have been created. */
        void onFinished();

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

    private final TabRestorerDelegate mDelegate;
    private final TabCreatorManager mTabCreatorManager;
    private final List<Integer> mTabIdsToIgnore = new ArrayList<>();

    private @State int mState = State.EMPTY;
    private @Nullable StorageLoadedData mData;
    private boolean mRestoreActiveTabImmediately;

    /**
     * Track the index we are restoring the next tab from. This is done globally so that {@link
     * #restoreTabStateForUrl(String)} and {@link #restoreTabStateForId(int)} can exclude tabs that
     * have already been restored.
     */
    private int mIndex;

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

        // Start was already called before the load finished. Start immediately.
        if (mState == State.RESTORE_ONCE_LOADED) {
            mState = State.LOADED;
            start(mRestoreActiveTabImmediately);
            return;
        }

        assert mState == State.EMPTY;
        mState = State.LOADED;
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
     */
    public void restoreTabStateForUrl(String url) {
        restoreTabStateByPredicate(
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
     */
    public void restoreTabStateForId(@TabId int tabId) {
        restoreTabStateByPredicate(loadedTabState -> loadedTabState.tabId == tabId);
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
            cleanupStorageLoadedData();
            mDelegate.onCancelled();
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
        cleanupStorageLoadedData();
        mDelegate.onFinished();
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

    private void restoreTabStateByPredicate(Predicate<LoadedTabState> predicate) {
        if (mData == null
                || mState == State.CANCELLED
                || mState == State.FINISHING
                || mState == State.FINISHED) {
            return;
        }

        LoadedTabState[] loadedTabStates = mData.getLoadedTabStates();
        for (int i = mIndex; i < loadedTabStates.length; i++) {
            LoadedTabState loadedTabState = loadedTabStates[i];
            if (!mTabIdsToIgnore.contains(loadedTabState.tabId) && predicate.test(loadedTabState)) {
                mTabIdsToIgnore.add(loadedTabState.tabId);
                restoreTab(loadedTabState, i, /* isIncognito= */ false, /* isActive= */ false);
                return;
            }
        }
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
        // TODO(https://crbug.com/451614469): Handle incognito.
        restoreTab(
                activeTabState,
                restoredActiveTabIndex,
                /* isIncognito= */ false,
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
     * @param isIncognito Whether the tab is in incognito mode.
     * @param isActive Whether the tab is the active tab.
     */
    private void restoreTab(
            LoadedTabState loadedTabState, int index, boolean isIncognito, boolean isActive) {
        assert mState == State.RESTORING;
        @TabId int tabId = loadedTabState.tabId;
        Tab tab = resolveTab(loadedTabState.tabState, tabId, index, isIncognito);
        if (tab == null) return;

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

        while (batchSize > 0 && mIndex < finalIndex) {
            LoadedTabState loadedTabState = loadedTabStates[mIndex];
            if (!mTabIdsToIgnore.contains(loadedTabState.tabId)) {
                // TODO(https://crbug.com/451614469): Handle incognito.
                restoreTab(loadedTabState, mIndex, /* isIncognito= */ false, /* isActive= */ false);
            }

            mIndex++;
            batchSize--;
        }

        if (mIndex < finalIndex) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::restoreNextBatchOfTabs);
        } else {
            postTaskToFinish();
        }
    }

    private @Nullable Tab resolveTab(
            TabState tabState, @TabId int tabId, int index, boolean isIncognito) {
        if (tabState.contentsState == null || tabState.contentsState.buffer().limit() <= 0) {
            return null;
        }

        return mTabCreatorManager
                .getTabCreator(isIncognito)
                .createFrozenTab(tabState, tabId, index);
    }
}
