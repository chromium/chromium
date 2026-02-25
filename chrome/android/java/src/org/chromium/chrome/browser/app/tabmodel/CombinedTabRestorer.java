// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.SystemClock;

import androidx.core.util.Supplier;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.TabRestorer.TabRestorerDelegate;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;

/**
 * A thin wrapper around two {@link TabRestorer}s, one for regular tabs and one for incognito tabs.
 * This class has its own delegate interface that only emits signals when both the regular and
 * incognito tab restorers have finished a stage.
 */
@NullMarked
class CombinedTabRestorer {
    private static final long INVALID_TIME = -1;

    /** Delegate for the {@link CombinedTabRestorer}. */
    public interface CombinedTabRestorerDelegate {
        /**
         * Called once both the regular and incognito tab restorer have been loaded.
         *
         * @param loadedTabCount The number of tabs that were loaded.
         */
        default void onLoadFinished(int loadedTabCount) {}

        /** Called once both the regular and incognito tab restorer have been cancelled. */
        default void onCancelled() {}

        /** Called when either the regular or incognito tab restorer have been finished. */
        default void onRestoredForModel(boolean incognito) {}

        /**
         * Called once the active tab has been restored.
         *
         * @param incognito Whether the active tab is incognito.
         */
        default void onActiveTabRestored(boolean incognito) {}

        /** Called once both the regular and incognito tab restorer have been finished. */
        default void onRestoreFinished() {}

        /**
         * Called when the details of a tab have been read {@see
         * TabPersistentStoreObserver#onDetailsRead}.
         */
        default void onDetailsRead(
                int index,
                @TabId int tabId,
                String url,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex,
                boolean isIncognito,
                boolean fromMerge) {}
    }

    private final TabRestorerDelegate mDelegate;
    private final TabRestorer mRegularTabRestorer;
    private final @Nullable TabRestorer mIncognitoTabRestorer;
    private final long mLoadStartTime;

    /**
     * Tracks the state of a tab restorer. This is used to determine when to send the signals to the
     * {@link CombinedTabRestorerDelegate}.
     */
    private static class RestoreState {
        private boolean mIsCancelled;
        private boolean mIsFinished;
        private boolean mIsLoadFinished;

        RestoreState() {}

        void setLoadFinished() {
            mIsLoadFinished = true;
        }

        void setCancelled() {
            mIsCancelled = true;
        }

        void setFinished() {
            mIsFinished = true;
        }

        void setAll() {
            mIsLoadFinished = true;
            mIsCancelled = true;
            mIsFinished = true;
        }

        boolean isLoadFinished() {
            return mIsLoadFinished;
        }

        boolean isCancelled() {
            return mIsCancelled;
        }

        boolean isFinishedOrCancelled() {
            return (mIsFinished || mIsCancelled);
        }
    }

    private class TabRestorerDelegateImpl implements TabRestorerDelegate {
        private final CombinedTabRestorerDelegate mOrchestratorDelegate;
        private final RestoreState mRegularState = new RestoreState();
        private final RestoreState mIncognitoState = new RestoreState();
        private int mRestoredTabCount;

        TabRestorerDelegateImpl(
                CombinedTabRestorerDelegate delegate, boolean restoreIncognitoTabs) {
            mOrchestratorDelegate = delegate;
            if (!restoreIncognitoTabs) {
                mIncognitoState.setAll();
            }
        }

        @Override
        public void onDataLoaded(boolean incognito, int count) {
            mRestoredTabCount += count;
            RestoreState state = incognito ? mIncognitoState : mRegularState;
            state.setLoadFinished();

            if (mRegularState.isLoadFinished() && mIncognitoState.isLoadFinished()) {
                if (mLoadStartTime != INVALID_TIME) {
                long duration = SystemClock.elapsedRealtime() - mLoadStartTime;
                RecordHistogram.recordTimesHistogram(
                        "Tabs.TabStateStore.LoadAllTabsDuration", duration);
                }
                mOrchestratorDelegate.onLoadFinished(mRestoredTabCount);
            }
        }

        @Override
        public void onCancelled(boolean incognito) {
            RestoreState state = incognito ? mIncognitoState : mRegularState;
            assert state.isLoadFinished();
            state.setCancelled();

            if (mRegularState.isCancelled() && mIncognitoState.isCancelled()) {
                mOrchestratorDelegate.onCancelled();
                return;
            }
            // If only one of the tab restorers is cancelled it is a successful load. Either send
            // the finished signal or start the remaining tab restorer.
            sendOnFinishedOrStartRemainingTabRestorer();
        }

        @Override
        public void onActiveTabRestored(boolean incognito) {
            mOrchestratorDelegate.onActiveTabRestored(incognito);
        }

        @Override
        public void onFinished(boolean incognito) {
            RestoreState state = incognito ? mIncognitoState : mRegularState;
            assert state.isLoadFinished();
            state.setFinished();

            mOrchestratorDelegate.onRestoredForModel(incognito);
            sendOnFinishedOrStartRemainingTabRestorer();
        }

        @Override
        public void onDetailsRead(
                int index,
                @TabId int tabId,
                String url,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex,
                boolean isIncognito,
                boolean fromMerge) {
            mOrchestratorDelegate.onDetailsRead(
                    index,
                    tabId,
                    url,
                    isStandardActiveIndex,
                    isIncognitoActiveIndex,
                    isIncognito,
                    fromMerge);
        }

        private void sendOnFinishedOrStartRemainingTabRestorer() {
            // If both tab restorers are done (finished or cancelled) send the signal that the
            // restore is finished. The special case of both being cancelled is handled by the
            // callers of this method.
            if (mRegularState.isFinishedOrCancelled() && mIncognitoState.isFinishedOrCancelled()) {
                mOrchestratorDelegate.onRestoreFinished();
                return;
            }

            if (mRegularState.isFinishedOrCancelled()) {
                assert !mIncognitoState.isFinishedOrCancelled();
                assumeNonNull(mIncognitoTabRestorer);
                mIncognitoTabRestorer.start(/* restoreActiveTabImmediately= */ false);
            } else if (mIncognitoState.isFinishedOrCancelled()) {
                assert !mRegularState.isFinishedOrCancelled();
                mRegularTabRestorer.start(/* restoreActiveTabImmediately= */ false);
            } else {
                assert false : "Not reached.";
            }
        }
    }

    /**
     * @param restoreIncognitoTabs Whether to restore incognito tabs.
     * @param delegate The delegate to be notified of events from the tab restorers.
     * @param tabCreatorManager The tab creator manager to create the tabs.
     * @param batchFactory The factory to create scoped storage batches.
     * @param logRestoreDuration Whether to log the restore duration.
     */
    CombinedTabRestorer(
            boolean restoreIncognitoTabs,
            CombinedTabRestorerDelegate delegate,
            TabCreatorManager tabCreatorManager,
            Supplier<ScopedStorageBatch> batchFactory,
            boolean logRestoreDuration) {
        mDelegate = new TabRestorerDelegateImpl(delegate, restoreIncognitoTabs);
        mRegularTabRestorer =
                new TabRestorer(
                        /* incognito= */ false,
                        mDelegate,
                        tabCreatorManager.getTabCreator(/* incognito= */ false),
                        batchFactory);
        mIncognitoTabRestorer =
                restoreIncognitoTabs
                        ? new TabRestorer(
                                /* incognito= */ true,
                                mDelegate,
                                tabCreatorManager.getTabCreator(/* incognito= */ true),
                                batchFactory)
                        : null;
        mLoadStartTime = logRestoreDuration ? SystemClock.elapsedRealtime() : INVALID_TIME;
    }

    /**
     * Should be called when the data for one of the models has been loaded.
     *
     * @param data The data loaded from storage.
     * @param incognito Whether the data is for the incognito tab restorer.
     */
    void onDataLoaded(StorageLoadedData data, boolean incognito) {
        if (incognito) {
            assumeNonNull(mIncognitoTabRestorer);
            mIncognitoTabRestorer.onDataLoaded(data);
        } else {
            mRegularTabRestorer.onDataLoaded(data);
        }
    }

    /**
     * Starts the restoration of the currently selected model. The other model will be started once
     * the selected model has finished loading. These operations can be cancelled together or
     * separately.
     *
     * @param isIncognitoSelected Whether the incognito tab restorer should be started.
     * @param restoreActiveTabImmediately Whether the active tab should be restored immediately. If
     *     false another tab may have already been created and activated so this should just restore
     *     the active tab as if it were any other tab.
     */
    void start(boolean isIncognitoSelected, boolean restoreActiveTabImmediately) {
        if (isIncognitoSelected && mIncognitoTabRestorer != null) {
            mIncognitoTabRestorer.start(restoreActiveTabImmediately);
        } else {
            mRegularTabRestorer.start(restoreActiveTabImmediately);
        }
    }

    /** Cancels the restoration of both the regular and incognito tab restorers. */
    void cancel() {
        mRegularTabRestorer.cancel();
        if (mIncognitoTabRestorer != null) {
            mIncognitoTabRestorer.cancel();
        }
    }

    /**
     * Cancels the restoration of the tab restorer as specified by the incognito parameter.
     *
     * @param incognito Whether to cancel the incognito or regular tab restorer.
     */
    void cancelLoadingTabs(boolean incognito) {
        if (!incognito) {
            mRegularTabRestorer.cancel();
        } else if (mIncognitoTabRestorer != null) {
            mIncognitoTabRestorer.cancel();
        }
    }

    /**
     * Restores the tab state for the given URL.
     *
     * @param url The URL to restore the tab state for.
     */
    void restoreTabStateForUrl(String url) {
        boolean success = mRegularTabRestorer.restoreTabStateForUrl(url);
        if (!success && mIncognitoTabRestorer != null) {
            mIncognitoTabRestorer.restoreTabStateForUrl(url);
        }
    }

    /**
     * Restores the tab state for the given tab ID.
     *
     * @param tabId The tab ID to restore the tab state for.
     */
    void restoreTabStateForId(@TabId int tabId) {
        boolean success = mRegularTabRestorer.restoreTabStateForId(tabId);
        if (!success && mIncognitoTabRestorer != null) {
            mIncognitoTabRestorer.restoreTabStateForId(tabId);
        }
    }
}
