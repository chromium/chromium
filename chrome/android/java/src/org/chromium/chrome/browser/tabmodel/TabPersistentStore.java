// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.StrictMode;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.StreamUtil;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.FilePersistedTabDataStorage;
import org.chromium.chrome.browser.tab.state.PersistedTabData;
import org.chromium.chrome.browser.tab.state.SerializedCriticalPersistedTabData;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Deque;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ExecutionException;

/**
 * This class handles saving and loading tab state from the persistent storage.
 */
public class TabPersistentStore {
    private static final String TAG = "tabmodel";

    /**
     * The current version of the saved state file.
     * Version 4: In addition to the tab's ID, save the tab's last URL.
     * Version 5: In addition to the total tab count, save the incognito tab count.
     */
    private static final int SAVED_STATE_VERSION = 5;

    /**
     * The prefix of the name of the file where the state is saved.  Values returned by
     * {@link #getStateFileName(String)} must begin with this prefix.
     */
    @VisibleForTesting
    static final String SAVED_STATE_FILE_PREFIX = "tab_state";

    /** Prevents two TabPersistentStores from saving the same file simultaneously. */
    private static final Object SAVE_LIST_LOCK = new Object();
    private static final int MIGRATE_TO_CRITICAL_PERSISTED_TAB_DATA_DEFAULT_BATCH_SIZE = 5;
    private static final String MIGRATE_TO_CRITICAL_PERSISTED_TAB_DATA_BATCH_SIZE_PARAM =
            "migrate_to_critical_persisted_tab_data_batch_size";
    private static final String CRITICAL_PERSISTED_TAB_DATA_SAVE_ONLY =
            "critical_persisted_tab_data_save_only";
    public static final BooleanCachedFieldTrialParameter
            CRITICAL_PERSISTED_TAB_DATA_SAVE_ONLY_PARAM = new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA,
                    CRITICAL_PERSISTED_TAB_DATA_SAVE_ONLY, false);

    private TabModelObserver mTabModelObserver;
    private TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private boolean mSkipSavingNonActiveNtps;

    @IntDef({ActiveTabState.OTHER, ActiveTabState.NTP, ActiveTabState.EMPTY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActiveTabState {
        /** No active tab. */
        int EMPTY = 0;
        /** Active tab is NTP. */
        int NTP = 1;
        /** Active tab is anything other than NTP. */
        int OTHER = 2;
    }

    /**
     * Values are recorded in metrics and should not be changed.
     */
    @IntDef({TabRestoreMethod.TAB_STATE, TabRestoreMethod.CRITICAL_PERSISTED_TAB_DATA,
            TabRestoreMethod.CREATE_NEW_TAB, TabRestoreMethod.FAILED_TO_RESTORE,
            TabRestoreMethod.SKIPPED_NTP, TabRestoreMethod.SKIPPED_EMPTY_URL,
            TabRestoreMethod.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    protected @interface TabRestoreMethod {
        /** Tab restored using TabState. */
        int TAB_STATE = 0;
        /** Tab restored using CriticalPersistedTabData. */
        int CRITICAL_PERSISTED_TAB_DATA = 1;
        /** Tab restored by creating a new Tab from Tab metadata file. */
        int CREATE_NEW_TAB = 2;
        /** Failed to restore Tab using any of the above methods. */
        int FAILED_TO_RESTORE = 3;
        /** In some situations the NTP is skipped when we re-create the Tab as a fallback. */
        int SKIPPED_NTP = 4;
        /** The URL was empty so restoration was skipped. */
        int SKIPPED_EMPTY_URL = 5;
        int NUM_ENTRIES = 6;
    }

    public void onNativeLibraryReady() {
        TabStateAttributes.Observer attributesObserver = new TabStateAttributes.Observer() {
            @Override
            public void onTabStateDirtinessChanged(
                    Tab tab, @TabStateAttributes.DirtinessState int dirtiness) {
                if (dirtiness == TabStateAttributes.DirtinessState.DIRTY && !tab.isDestroyed()) {
                    addTabToSaveQueue(tab);
                }
            }
        };
        mTabRegistrationObserver = new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                new TabModelSelectorTabRegistrationObserver.Observer() {
                    @Override
                    public void onTabRegistered(Tab tab) {
                        TabStateAttributes attributes = TabStateAttributes.from(tab);
                        if (attributes.addObserver(attributesObserver)
                                == TabStateAttributes.DirtinessState.DIRTY) {
                            addTabToSaveQueue(tab);
                        }
                    }

                    @Override
                    public void onTabUnregistered(Tab tab) {
                        if (!tab.isDestroyed()) {
                            TabStateAttributes.from(tab).removeObserver(attributesObserver);
                        }
                        if (tab.isClosing()) {
                            PersistedTabData.onTabClose(tab);
                            removeTabFromQueues(tab);
                        }
                    }
                });

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void willCloseAllTabs(boolean incognito) {
                cancelLoadingTabs(incognito);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                saveTabListAsynchronously();
            }

            @Override
            public void onFinishingMultipleTabClosure(List<Tab> tabs) {
                if (!mTabModelSelector.isIncognitoSelected()
                        && ChromeFeatureList.sCloseTabSaveTabList.isEnabled()) {
                    saveTabListAsynchronously();
                }
            }
        };
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
        mTabModelSelector.getModel(true).addObserver(mTabModelObserver);
    }

    /**
     * Callback interface to use while reading the persisted TabModelSelector info from disk.
     */
    public static interface OnTabStateReadCallback {
        /**
         * To be called as the details about a persisted Tab are read from the TabModelSelector's
         * persisted data.
         * @param index                  The index out of all tabs for the current tab read.
         * @param id                     The id for the current tab read.
         * @param url                    The url for the current tab read.
         * @param isIncognito            Whether the Tab is definitely Incognito, or null if it
         *                               couldn't be determined because we didn't know how many
         *                               Incognito tabs were saved out.
         * @param isStandardActiveIndex  Whether the current tab read is the normal active tab.
         * @param isIncognitoActiveIndex Whether the current tab read is the incognito active tab.
         */
        void onDetailsRead(int index, int id, String url, Boolean isIncognito,
                boolean isStandardActiveIndex, boolean isIncognitoActiveIndex);
    }

    /**
     * Alerted at various stages of operation.
     */
    public abstract static class TabPersistentStoreObserver {
        /**
         * To be called when the file containing the initial information about the TabModels has
         * been loaded.
         * @param tabCountAtStartup How many tabs there are in the TabModels.
         */
        public void onInitialized(int tabCountAtStartup) {}

        /**
         * Called when details about a Tab are read from the metadata file.
         */
        public void onDetailsRead(int index, int id, String url, boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex, Boolean isIncognito, boolean fromMerge) {}

        /**
         * To be called when the TabStates have all been loaded.
         */
        public void onStateLoaded() {}

        /**
         * To be called when the TabState from another instance has been merged.
         */
        public void onStateMerged() {}

        /**
         * Called when the metadata file has been saved out asynchronously.
         * This currently does not get called when the metadata file is saved out on the UI thread.
         * @param modelSelectorMetadata The saved metadata of current tab model selector.
         */
        public void onMetadataSavedAsynchronously(TabModelSelectorMetadata modelSelectorMetadata) {}
    }

    /** Stores information about a TabModel. */
    public static class TabModelMetadata {
        public int index;
        public final List<Integer> ids;
        public final List<String> urls;

        @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
        public TabModelMetadata(int selectedIndex) {
            index = selectedIndex;
            ids = new ArrayList<>();
            urls = new ArrayList<>();
        }
    }

    private final TabPersistencePolicy mPersistencePolicy;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private final ObserverList<TabPersistentStoreObserver> mObservers;

    private final Deque<Tab> mTabsToSave;
    // Tabs to migrate to CriticalPersistedTabData (i.e. save a CriticalPersistedTabData
    // file for the Tab). When the CriticalPersistedTabData flag is on,
    // CriticalPersistedTabData files are saved upon the first successful
    // save of a TabState for a Tab. However, this only happens if a Tab attribute
    // changes e.g. change of URL or move the Tab to a different group (changing the root
    // id). Stale Tabs (i.e. Tabs which sit uninteracted with) stay unmigrated. This queue is used
    // to migrate an additional MIGRATE_TO_CRITICAL_PERSISTED_TAB_DATA_BATCH_SIZE Tabs for every 1
    // save of a TabState. Considerations (especially in light of users who have a lot of Tabs):
    // - Tabs should not be migrated directly on startup, as this may introduce a startup
    //   metric regression. After a TabState save should be a time of relatively low
    //   resource utilization.
    // - MIGRATE_TO_CRITICAL_PERSISTED_TAB_DATA_BATCH_SIZE should not be too large or
    //   the system will be flooded with saves.
    private final Deque<Tab> mTabsToMigrate;
    private final Deque<TabRestoreDetails> mTabsToRestore;
    private final Set<Integer> mTabIdsToRestore;

    private TabLoader mTabLoader;
    private SaveTabTask mSaveTabTask;
    private SaveListTask mSaveListTask;

    private boolean mDestroyed;
    private boolean mCancelNormalTabLoads;
    private boolean mCancelIncognitoTabLoads;

    // Keys are the original tab indexes, values are the tab ids.
    private SparseIntArray mNormalTabsRestored;
    private SparseIntArray mIncognitoTabsRestored;

    private SequencedTaskRunner mSequencedTaskRunner;
    private AsyncTask<DataInputStream> mPrefetchTabListTask;
    private List<Pair<AsyncTask<DataInputStream>, String>> mPrefetchTabListToMergeTasks;
    // A set of filenames which are tracked to merge.
    private Set<String> mMergedFileNames;
    private byte[] mLastSavedMetadata;

    // Tracks whether this TabPersistentStore's tabs are being loaded.
    private boolean mLoadInProgress;

    AsyncTask<TabState> mPrefetchTabStateActiveTabTask;

    AsyncTask<SerializedCriticalPersistedTabData> mPrefetchCriticalPersistedTabDataActiveTabTask;

    /**
     * Creates an instance of a TabPersistentStore.
     * @param modelSelector The {@link TabModelSelector} to restore to and save from.
     * @param tabCreatorManager The {@link TabCreatorManager} to use.
     */
    public TabPersistentStore(TabPersistencePolicy policy, TabModelSelector modelSelector,
            TabCreatorManager tabCreatorManager) {
        mPersistencePolicy = policy;
        mTabModelSelector = modelSelector;
        mTabCreatorManager = tabCreatorManager;
        mTabsToSave = new ArrayDeque<>();
        mTabsToMigrate = new ArrayDeque<>();
        mTabsToRestore = new ArrayDeque<>();
        mTabIdsToRestore = new HashSet<>();
        mObservers = new ObserverList<>();
        @TaskTraits
        int taskTraits = TaskTraits.USER_BLOCKING_MAY_BLOCK;
        mSequencedTaskRunner = PostTask.createSequencedTaskRunner(taskTraits);
        mPrefetchTabListToMergeTasks = new ArrayList<>();
        mMergedFileNames = new HashSet<>();

        assert isStateFile(policy.getStateFileName()) : "State file name is not valid";
        boolean needsInitialization =
                mPersistencePolicy.performInitialization(mSequencedTaskRunner);

        mPersistencePolicy.setTaskRunner(mSequencedTaskRunner);

        if (mPersistencePolicy.isMergeInProgress()) return;

        // TODO(smaier): We likely can move everything onto the SequencedTaskRunner when the
        //  SERIAL_EXECUTOR path is gone. crbug.com/957735
        TaskRunner taskRunner =
                needsInitialization ? mSequencedTaskRunner : PostTask.createTaskRunner(taskTraits);

        mPrefetchTabListTask =
                startFetchTabListTask(taskRunner, mPersistencePolicy.getStateFileName());
        startPrefetchActiveTabTask(taskRunner);

        if (mPersistencePolicy.shouldMergeOnStartup()) {
            for (String mergedFileName : mPersistencePolicy.getStateToBeMergedFileNames()) {
                AsyncTask<DataInputStream> task = startFetchTabListTask(taskRunner, mergedFileName);
                mPrefetchTabListToMergeTasks.add(Pair.create(task, mergedFileName));
            }
        }
        cleanupCriticalPersistedTabData();
    }

    private void cleanupCriticalPersistedTabData() {
        if (isCriticalPersistedTabDataSaveOnlyEnabled()
                || isCriticalPersistedTabDataSaveAndRestoreEnabled()) {
            return;
        }
        for (boolean isIncognito : new boolean[] {false, true}) {
            int numTabs = mTabModelSelector.getModel(isIncognito).getCount();
            for (int i = 0; i < numTabs; i++) {
                CriticalPersistedTabData.from(mTabModelSelector.getModel(isIncognito).getTabAt(i))
                        .delete();
            }
        }
    }

    /**
     * Waits for the task that migrates all state files to their new location to finish.
     */
    @VisibleForTesting
    public void waitForMigrationToFinish() {
        mPersistencePolicy.waitForInitializationToFinish();
    }

    public void saveState() {
        // Temporarily allowing disk access. TODO: Fix. See http://b/5518024
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            // The list of tabs should be saved first in case our activity is terminated early.
            // Explicitly toss out any existing SaveListTask because they only save the TabModel as
            // it looked when the SaveListTask was first created.
            if (mSaveListTask != null) mSaveListTask.cancel(true);
            try {
                saveListToFile(serializeTabMetadata().listData);
            } catch (IOException e) {
                Log.w(TAG, "Error while saving tabs state; will attempt to continue...", e);
            }

            // Add current tabs to save because they did not get a save signal yet.
            Tab currentStandardTab = TabModelUtils.getCurrentTab(mTabModelSelector.getModel(false));
            addTabToSaveQueueIfApplicable(currentStandardTab);

            Tab currentIncognitoTab = TabModelUtils.getCurrentTab(mTabModelSelector.getModel(true));
            addTabToSaveQueueIfApplicable(currentIncognitoTab);

            // Wait for the current tab to save.
            if (mSaveTabTask != null) {
                // Cancel calls get() to wait for this to finish internally if it has to.
                // The issue is it may assume it cancelled the task, but the task still actually
                // wrote the state to disk.  That's why we have to check mStateSaved here.
                if (mSaveTabTask.cancel(false) && !mSaveTabTask.mStateSaved) {
                    // The task was successfully cancelled.  We should try to save this state again.
                    Tab cancelledTab = mSaveTabTask.mTab;
                    addTabToSaveQueueIfApplicable(cancelledTab);
                }

                mSaveTabTask = null;
            }

            // Synchronously save any remaining unsaved tabs (hopefully very few).
            for (Tab tab : mTabsToSave) {
                int id = tab.getId();
                boolean incognito = tab.isIncognito();
                try {
                    TabState state = TabStateExtractor.from(tab);
                    if (state != null) {
                        TabStateFileManager.saveState(
                                getTabStateFile(id, incognito), state, incognito);
                    }
                } catch (OutOfMemoryError e) {
                    Log.e(TAG, "Out of memory error while attempting to save tab state.  Erasing.");
                    deleteTabState(id, incognito);
                }
            }
            mTabsToSave.clear();
            if (isCriticalPersistedTabDataSaveOnlyEnabled()
                    || isCriticalPersistedTabDataSaveAndRestoreEnabled()) {
                if (currentStandardTab != null) {
                    CriticalPersistedTabData.from(currentStandardTab).save();
                }
                if (currentIncognitoTab != null) {
                    CriticalPersistedTabData.from(currentIncognitoTab).save();
                }
                PersistedTabData.onShutdown();
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @VisibleForTesting
    void initializeRestoreVars(boolean ignoreIncognitoFiles) {
        mCancelNormalTabLoads = false;
        mCancelIncognitoTabLoads = ignoreIncognitoFiles;
        mNormalTabsRestored = new SparseIntArray();
        mIncognitoTabsRestored = new SparseIntArray();
    }

    /**
     * Restore saved state. Must be called before any tabs are added to the list.
     *
     * This will read the metadata file for the current TabPersistentStore and the metadata file
     * from another TabPersistentStore if applicable. When restoreTabs() is called, tabs from both
     * will be restored into this instance.
     *
     * @param ignoreIncognitoFiles Whether to skip loading incognito tabs.
     */
    public void loadState(boolean ignoreIncognitoFiles) {
        // If a cleanup task is in progress, cancel it before loading state.
        mPersistencePolicy.cancelCleanupInProgress();

        waitForMigrationToFinish();

        initializeRestoreVars(ignoreIncognitoFiles);

        try {
            assert mTabModelSelector.getModel(true).getCount() == 0;
            assert mTabModelSelector.getModel(false).getCount() == 0;
            checkAndUpdateMaxTabId();
            DataInputStream stream;
            if (mPrefetchTabListTask != null) {
                stream = mPrefetchTabListTask.get();

                // Restore the tabs for this TabPersistentStore instance if the tab metadata file
                // exists.
                if (stream != null) {
                    mLoadInProgress = true;
                    readSavedStateFile(stream,
                            createOnTabStateReadCallback(
                                    mTabModelSelector.isIncognitoSelected(), false),
                            null);
                }
            }

            // Restore the tabs for the other TabPersistentStore instance if its tab metadata file
            // exists.
            if (mPrefetchTabListToMergeTasks.size() > 0) {
                for (Pair<AsyncTask<DataInputStream>, String> mergeTask :
                        mPrefetchTabListToMergeTasks) {
                    AsyncTask<DataInputStream> task = mergeTask.first;
                    stream = task.get();
                    if (stream == null) continue;
                    mMergedFileNames.add(mergeTask.second);
                    mPersistencePolicy.setMergeInProgress(true);
                    readSavedStateFile(stream,
                            createOnTabStateReadCallback(mTabModelSelector.isIncognitoSelected(),
                                    mTabsToRestore.size() != 0),
                            null);
                }
                if (!mMergedFileNames.isEmpty()) {
                    RecordUserAction.record("Android.MergeState.ColdStart");
                }
                mPrefetchTabListToMergeTasks.clear();
            }
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing app on startup.
            Log.i(TAG, "loadState exception: " + e.toString(), e);
        }

        mPersistencePolicy.notifyStateLoaded(mTabsToRestore.size());
        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onInitialized(mTabsToRestore.size());
        }
    }

    /**
     * Merge the tabs of the other Chrome instance into this instance by reading its tab metadata
     * file and tab state files.
     *
     * This method should be called after a change in activity state indicates that a merge is
     * necessary. #loadState() will take care of merging states on application cold start if needed.
     *
     * If there is currently a merge or load in progress then this method will return early.
     */
    public void mergeState() {
        if (mLoadInProgress || mPersistencePolicy.isMergeInProgress()
                || !mTabsToRestore.isEmpty()) {
            Log.d(TAG, "Tab load still in progress when merge was attempted.");
            return;
        }

        // Initialize variables.
        initializeRestoreVars(false);

        try {
            // Read the tab state metadata file.
            for (String mergeFileName : mPersistencePolicy.getStateToBeMergedFileNames()) {
                DataInputStream stream =
                        startFetchTabListTask(mSequencedTaskRunner, mergeFileName).get();
                if (stream == null) continue;
                mMergedFileNames.add(mergeFileName);
                mPersistencePolicy.setMergeInProgress(true);
                readSavedStateFile(stream,
                        createOnTabStateReadCallback(mTabModelSelector.isIncognitoSelected(), true),
                        null);
            }
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing app.
            Log.d(TAG, "meregeState exception: " + e.toString(), e);
        }

        // Restore the tabs from the second activity asynchronously.
        loadNextTab();
    }

    /**
     * Restore tab state.  Tab state is loaded asynchronously, other than the active tab which
     * can be forced to load synchronously.
     *
     * @param setActiveTab If true the last active tab given in the saved state is loaded
     *                     synchronously and set as the current active tab. If false all tabs are
     *                     loaded asynchronously.
     */
    public void restoreTabs(boolean setActiveTab) {
        if (setActiveTab) {
            // Restore and select the active tab, which is first in the restore list.
            // If the active tab can't be restored, restore and select another tab. Otherwise, the
            // tab model won't have a valid index and the UI will break. http://crbug.com/261378
            while (!mTabsToRestore.isEmpty()
                    && mNormalTabsRestored.size() == 0
                    && mIncognitoTabsRestored.size() == 0) {
                try (TraceEvent e = TraceEvent.scoped("LoadFirstTabState")) {
                    TabRestoreDetails tabToRestore = mTabsToRestore.removeFirst();
                    restoreTab(tabToRestore, true);
                }
            }
        }
        loadNextTab();
    }

    /**
     * If a tab is being restored with the given url, then restore the tab in a frozen state
     * synchronously.
     */
    public void restoreTabStateForUrl(String url) {
        restoreTabStateInternal(url, Tab.INVALID_TAB_ID);
    }

    /**
     * If a tab is being restored with the given id, then restore the tab in a frozen state
     * synchronously.
     */
    public void restoreTabStateForId(int id) {
        restoreTabStateInternal(null, id);
    }

    private void restoreTabStateInternal(String url, int id) {
        TabRestoreDetails tabToRestore = null;
        if (mTabLoader != null) {
            if ((url == null && mTabLoader.mTabToRestore.id == id)
                    || (url != null && TextUtils.equals(mTabLoader.mTabToRestore.url, url))) {
                // Steal the task of restoring the tab from the active load tab task.
                mTabLoader.cancel(false);
                tabToRestore = mTabLoader.mTabToRestore;
                loadNextTab();  // Queue up async task to load next tab after we're done here.
            }
        }

        if (tabToRestore == null) {
            if (url == null) {
                tabToRestore = getTabToRestoreById(id);
            } else {
                tabToRestore = getTabToRestoreByUrl(url);
            }
        }

        if (tabToRestore != null) {
            mTabsToRestore.remove(tabToRestore);
            restoreTab(tabToRestore, false);
        }
    }

    private void restoreTab(TabRestoreDetails tabToRestore, boolean setAsActive) {
        // As we do this in startup, and restoring the active tab's state is critical, we permit
        // this read in the event that the prefetch task is not available. Either:
        // 1. The user just upgraded, has not yet set the new active tab id pref yet. Or
        // 2. restoreTab is used to preempt async queue and restore immediately on the UI thread.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        // As we add more field to TabState, we are crossing the 10 operation counts threshold to
        // enforce the detection of unbuffered input/output operations, which results in
        // https://crbug.com/1276907. After evaluating the performance impact, here we disabled the
        // detection of unbuffered input/output operations.
        // TabState is on a deprecation path and the intention is to replace with
        // CriticalPersistedTabData. So this workaround should be temporary.
        try (StrictModeContext ignored = StrictModeContext.allowUnbufferedIo()) {
            int restoredTabId = SharedPreferencesManager.getInstance().readInt(
                    ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, Tab.INVALID_TAB_ID);
            // If the CriticalPersistedTabData flag is on, we try to restore
            // CriticalPersistedTabData.
            @Nullable
            SerializedCriticalPersistedTabData serializedCriticalPersistedTabData =
                    maybeRestoreCriticalPersistedTabData(restoredTabId, tabToRestore);
            // If the CriticalPersistedTabData flag is off or we failed to read
            // CriticalPersistedTabData we fall back to TabState.
            @Nullable
            TabState state = maybeRestoreTabState(
                    serializedCriticalPersistedTabData, restoredTabId, tabToRestore);
            restoreTab(tabToRestore, state, serializedCriticalPersistedTabData, setAsActive);
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing the app
            // at startup.
            Log.i(TAG, "loadTabs exception: " + e.toString(), e);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    private @Nullable SerializedCriticalPersistedTabData maybeRestoreCriticalPersistedTabData(
            int restoredTabId, TabRestoreDetails tabToRestore)
            throws InterruptedException, ExecutionException {
        if (!isCriticalPersistedTabDataSaveAndRestoreEnabled()) return null;
        // If Tab being restored is the active Tab and the CriticalPersistedTabData prefetch
        // was initiated, use the prefetch result.
        if (restoredTabId == tabToRestore.id
                && mPrefetchCriticalPersistedTabDataActiveTabTask != null) {
            return mPrefetchCriticalPersistedTabDataActiveTabTask.get();
        }
        Boolean isIncognito = isIncognitoWithCPTDFallback(tabToRestore);
        if (isIncognito == null) {
            return null;
        }
        // Fetch CriticalPersistedTabData on the UI thread as a last resort.
        return CriticalPersistedTabData.restore(tabToRestore.id, isIncognito);
    }

    private @Nullable TabState maybeRestoreTabState(
            SerializedCriticalPersistedTabData serializedCriticalPersistedTabData,
            int restoredTabId, TabRestoreDetails tabToRestore)
            throws InterruptedException, ExecutionException {
        // If CriticalPersistedTabData flag is on and CriticalPersistedTabData was retrieved, no
        // need to attempt to retrieve TabState.
        if (isCriticalPersistedTabDataSaveAndRestoreEnabled()
                && !CriticalPersistedTabData.isEmptySerialization(
                        serializedCriticalPersistedTabData)) {
            return null;
        }
        // If the Tab being restored is the Active Tab and the corresponding TabState prefetch
        // was initiated, use the prefetch result.
        if (restoredTabId == tabToRestore.id && mPrefetchTabStateActiveTabTask != null) {
            return mPrefetchTabStateActiveTabTask.get();
        }
        // Necessary to do on the UI thread as a last resort.
        return TabStateFileManager.restoreTabState(getStateDirectory(), tabToRestore.id);
    }

    /**
     * Handles restoring an individual tab.
     *
     * @param tabToRestore Meta data about the tab to be restored.
     * @param tabState     The previously serialized state of the tab to be restored.
     * @param serializedCriticalPersistedTabData serialized {@link CriticalPersistedTabData}
     * @param setAsActive  Whether the tab should be set as the active tab as part of the
     *                     restoration process.
     */
    @VisibleForTesting
    protected void restoreTab(TabRestoreDetails tabToRestore, TabState tabState,
            SerializedCriticalPersistedTabData serializedCriticalPersistedTabData,
            boolean setAsActive) {
        // If we don't have enough information about the Tab, bail out.
        boolean isIncognito = isIncognitoTabBeingRestored(
                tabToRestore, tabState, serializedCriticalPersistedTabData);

        if (tabState == null
                && CriticalPersistedTabData.isEmptySerialization(
                        serializedCriticalPersistedTabData)) {
            if (tabToRestore.isIncognito == null) {
                Log.w(TAG, "Failed to restore tab: not enough info about its type was available.");
                return;
            } else if (isIncognito) {
                boolean isNtp = UrlUtilities.isNTPUrl(tabToRestore.url);
                boolean isNtpFromMerge = isNtp && tabToRestore.fromMerge;

                if (!isNtpFromMerge && (!isNtp || !setAsActive || mCancelIncognitoTabLoads)) {
                    Log.i(TAG,
                            "Failed to restore Incognito tab: its TabState could not be restored.");
                    return;
                }
            }
        }
        TabModel model = mTabModelSelector.getModel(isIncognito);

        if (model.isIncognito() != isIncognito) {
            throw new IllegalStateException("Incognito state mismatch. Restored tab state: "
                    + isIncognito + ". Model: " + model.isIncognito());
        }
        SparseIntArray restoredTabs = isIncognito ? mIncognitoTabsRestored : mNormalTabsRestored;
        int restoredIndex = 0;
        if (tabToRestore.fromMerge) {
            // Put any tabs being merged into this list at the end.
            // TODO(ltian): need to figure out a way to add merged tabs before Browser Actions tabs
            // when tab restore and Browser Actions tab merging happen at the same time.
            restoredIndex = model.getCount();
        } else if (restoredTabs.size() > 0
                && tabToRestore.originalIndex > restoredTabs.keyAt(restoredTabs.size() - 1)) {
            // If the tab's index is too large, restore it at the end of the list.
            restoredIndex = Math.min(model.getCount(), restoredTabs.size());
        } else {
             // Otherwise try to find the tab we should restore before, if any.
            for (int i = 0; i < restoredTabs.size(); i++) {
                if (restoredTabs.keyAt(i) > tabToRestore.originalIndex) {
                    restoredIndex = TabModelUtils.getTabIndexById(model, restoredTabs.valueAt(i));
                    break;
                }
            }
        }

        int tabId = tabToRestore.id;
        boolean useTabState = tabState != null;
        boolean useCriticalPersistedTabData =
                !CriticalPersistedTabData.isEmptySerialization(serializedCriticalPersistedTabData);
        if (useTabState || useCriticalPersistedTabData) {
            assert useTabState
                    == !useCriticalPersistedTabData
                : "Must only restore using TabState or CriticalPersistedTabData";
            @TabRestoreMethod
            int tabRestoreMethod = useTabState ? TabRestoreMethod.TAB_STATE
                                               : TabRestoreMethod.CRITICAL_PERSISTED_TAB_DATA;
            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.TabRestoreMethod", tabRestoreMethod, TabRestoreMethod.NUM_ENTRIES);
            Tab tab = mTabCreatorManager.getTabCreator(isIncognito)
                              .createFrozenTab(tabState, serializedCriticalPersistedTabData,
                                      tabToRestore.id, isIncognito, restoredIndex);
            if (useTabState
                    && (isCriticalPersistedTabDataSaveOnlyEnabled()
                            || isCriticalPersistedTabDataSaveAndRestoreEnabled())) {
                mTabsToMigrate.add(tab);
            }
        } else {
            if (!mSkipSavingNonActiveNtps && UrlUtilities.isNTPUrl(tabToRestore.url) && !setAsActive
                    && !tabToRestore.fromMerge) {
                Log.i(TAG, "Skipping restore of non-selected NTP.");
                RecordHistogram.recordEnumeratedHistogram("Tabs.TabRestoreMethod",
                        TabRestoreMethod.SKIPPED_NTP, TabRestoreMethod.NUM_ENTRIES);
                return;
            }

            Log.w(TAG, "Failed to restore TabState; creating Tab with last known URL.");
            Tab fallbackTab = mTabCreatorManager.getTabCreator(isIncognito)
                                      .createNewTab(new LoadUrlParams(tabToRestore.url),
                                              TabLaunchType.FROM_RESTORE, null, restoredIndex);

            if (fallbackTab == null) {
                RecordHistogram.recordEnumeratedHistogram("Tabs.TabRestoreMethod",
                        TabRestoreMethod.FAILED_TO_RESTORE, TabRestoreMethod.NUM_ENTRIES);
                return;
            }
            if (isCriticalPersistedTabDataSaveOnlyEnabled()
                    || isCriticalPersistedTabDataSaveAndRestoreEnabled()) {
                mTabsToMigrate.add(fallbackTab);
            }

            RecordHistogram.recordEnumeratedHistogram("Tabs.TabRestoreMethod",
                    TabRestoreMethod.CREATE_NEW_TAB, TabRestoreMethod.NUM_ENTRIES);

            // restoredIndex might not be the one used in createNewTab so update accordingly.
            tabId = fallbackTab.getId();
            restoredIndex = model.indexOf(fallbackTab);
        }

        // If the tab is being restored from a merge and its index is 0, then the model being
        // merged into doesn't contain any tabs. Select the first tab to avoid having no tab
        // selected. TODO(twellington): The first tab will always be selected. Instead, the tab that
        // was selected in the other model before the merge should be selected after the merge.
        if (setAsActive || (tabToRestore.fromMerge && restoredIndex == 0)) {
            boolean wasIncognitoTabModelSelected = mTabModelSelector.isIncognitoSelected();
            int selectedModelTabCount = mTabModelSelector.getCurrentModel().getCount();

            // TODO(https://crbug.com/1428552): Don't use static function
            // StartSurfaceUserData#getUnusedTabRestoredAtStartup().
            TabModelUtils.setIndex(model, TabModelUtils.getTabIndexById(model, tabId),
                    mPersistencePolicy.allowSkipLoadingTab()
                            && StartSurfaceUserData.getInstance().getUnusedTabRestoredAtStartup());
            boolean isIncognitoTabModelSelected = mTabModelSelector.isIncognitoSelected();

            // Setting the index will cause the tab's model to be selected. Set it back to the model
            // that was selected before setting the index if the index is being set during a merge
            // unless the previously selected model is empty (e.g. showing the empty background
            // view on tablets).
            if (tabToRestore.fromMerge
                    && wasIncognitoTabModelSelected != isIncognitoTabModelSelected
                    && selectedModelTabCount != 0) {
                mTabModelSelector.selectModel(wasIncognitoTabModelSelected);
            }
        }
        restoredTabs.put(tabToRestore.originalIndex, tabId);
    }

    /**
     * @return Number of restored tabs on cold startup.
     */
    public int getRestoredTabCount() {
        return mTabsToRestore.size();
    }

    /**
     * Deletes all files in the tab state directory.  This will delete all files and not just those
     * owned by this TabPersistentStore.
     */
    public void clearState() {
        mPersistencePolicy.cancelCleanupInProgress();

        mSequencedTaskRunner.postTask(new Runnable() {
            @Override
            public void run() {
                File[] baseStateFiles =
                        TabStateDirectory.getOrCreateBaseStateDirectory().listFiles();
                if (baseStateFiles == null) return;
                for (File baseStateFile : baseStateFiles) {
                    // In legacy scenarios (prior to migration, state files could reside in the
                    // root state directory.  So, handle deleting direct child files as well as
                    // those that reside in sub directories.
                    if (!baseStateFile.isDirectory()) {
                        if (!baseStateFile.delete()) {
                            Log.e(TAG, "Failed to delete file: " + baseStateFile);
                        }
                    } else {
                        File[] files = baseStateFile.listFiles();
                        if (files == null) continue;
                        for (File file : files) {
                            if (!file.delete()) Log.e(TAG, "Failed to delete file: " + file);
                        }
                    }
                }
            }
        });

        onStateLoaded();
    }

    /**
     * Cancels loading of {@link Tab}s from disk from saved state. This is useful if the user
     * does an action which impacts all {@link Tab}s, not just the ones currently loaded into
     * the model. For example, if the user tries to close all {@link Tab}s, we need don't want
     * to restore old {@link Tab}s anymore.
     *
     * @param incognito Whether or not to ignore incognito {@link Tab}s or normal
     *                  {@link Tab}s as they are being restored.
     */
    public void cancelLoadingTabs(boolean incognito) {
        if (incognito) {
            mCancelIncognitoTabLoads = true;
        } else {
            mCancelNormalTabLoads = true;
        }
    }

    public void addTabToSaveQueue(Tab tab) {
        addTabToSaveQueueIfApplicable(tab);
        saveNextTab();
    }

    /**
     * @return Whether the specified tab is in any pending save operations.
     */
    @VisibleForTesting
    boolean isTabPendingSave(Tab tab) {
        return (mSaveTabTask != null && mSaveTabTask.mTab.equals(tab)) || mTabsToSave.contains(tab);
    }

    private void addTabToSaveQueueIfApplicable(Tab tab) {
        if (tab == null || tab.isDestroyed()) return;
        if (mTabsToSave.contains(tab)
                || TabStateAttributes.from(tab).getDirtinessState()
                        == TabStateAttributes.DirtinessState.CLEAN
                || isTabUrlContentScheme(tab)) {
            return;
        }

        if (UrlUtilities.isNTPUrl(tab.getUrl()) && !tab.canGoBack() && !tab.canGoForward()) {
            return;
        }
        mTabsToSave.addLast(tab);
    }

    public void removeTabFromQueues(Tab tab) {
        mTabsToSave.remove(tab);
        mTabsToRestore.remove(getTabToRestoreById(tab.getId()));

        if (isCriticalPersistedTabDataSaveOnlyEnabled()
                || isCriticalPersistedTabDataSaveAndRestoreEnabled()) {
            mTabsToMigrate.remove(tab);
        }

        if (mTabLoader != null && mTabLoader.mTabToRestore.id == tab.getId()) {
            mTabLoader.cancel(false);
            mTabLoader = null;
            loadNextTab();
        }

        if (mSaveTabTask != null && mSaveTabTask.mId == tab.getId()) {
            mSaveTabTask.cancel(false);
            mSaveTabTask = null;
            saveNextTab();
        }
        cleanupPersistentData(tab.getId(), tab.isIncognito());
    }

    private TabRestoreDetails getTabToRestoreByUrl(String url) {
        for (TabRestoreDetails tabBeingRestored : mTabsToRestore) {
            if (TextUtils.equals(tabBeingRestored.url, url)) {
                return tabBeingRestored;
            }
        }
        return null;
    }

    private TabRestoreDetails getTabToRestoreById(int id) {
        for (TabRestoreDetails tabBeingRestored : mTabsToRestore) {
            if (tabBeingRestored.id == id) {
                return tabBeingRestored;
            }
        }
        return null;
    }

    public void destroy() {
        mDestroyed = true;
        if (mTabModelObserver != null) {
            mTabModelSelector.getModel(false).removeObserver(mTabModelObserver);
            mTabModelSelector.getModel(true).removeObserver(mTabModelObserver);
            mTabModelObserver = null;
        }
        if (mTabRegistrationObserver != null) {
            mTabRegistrationObserver.destroy();
        }
        mPersistencePolicy.destroy();
        if (mTabLoader != null) mTabLoader.cancel(true);
        mTabsToSave.clear();
        mTabsToRestore.clear();
        if (mSaveTabTask != null) mSaveTabTask.cancel(false);
        if (mSaveListTask != null) mSaveListTask.cancel(true);
    }

    private void cleanupPersistentData(int id, boolean incognito) {
        deleteFileAsync(TabStateFileManager.getTabStateFilename(id, incognito), false);
        // No need to forward that event to the tab content manager as this is already
        // done as part of the standard tab removal process.
    }

    private TabModelSelectorMetadata serializeTabMetadata() throws IOException {
        List<TabRestoreDetails> tabsToRestore = new ArrayList<>();

        // The metadata file may be being written out before all of the Tabs have been restored.
        // Save that information out, as well.
        if (mTabLoader != null) tabsToRestore.add(mTabLoader.mTabToRestore);
        for (TabRestoreDetails details : mTabsToRestore) {
            tabsToRestore.add(details);
        }

        return serializeTabModelSelector(
                mTabModelSelector, tabsToRestore, mSkipSavingNonActiveNtps);
    }

    /**
     * Serializes {@code selector} to a byte array, copying out the data pertaining to tab ordering
     * and selected indices.
     * @param selector          The {@link TabModelSelector} to serialize.
     * @param tabsBeingRestored Tabs that are in the process of being restored.
     * @param skipNonActiveNtps Whether to skip saving non active Ntps.
     * @return                  {@link TabModelSelectorMetadata} containing the meta data and
     * serialized state of {@code selector}.
     */
    @VisibleForTesting
    public static TabModelSelectorMetadata serializeTabModelSelector(TabModelSelector selector,
            List<TabRestoreDetails> tabsBeingRestored, boolean skipNonActiveNtps)
            throws IOException {
        ThreadUtils.assertOnUiThread();

        // TODO(crbug/783819): Convert TabModelMetadata to use GURL.
        TabModelMetadata incognitoInfo = metadataFromModel(selector, true, skipNonActiveNtps);

        TabModel normalModel = selector.getModel(false);
        TabModelMetadata normalInfo = metadataFromModel(selector, false, skipNonActiveNtps);

        // Cache the active tab id to be pre-loaded next launch.
        int activeTabId = Tab.INVALID_TAB_ID;
        int activeIndex = normalModel.index();
        @ActiveTabState
        int activeTabState = ActiveTabState.EMPTY;
        if (activeIndex != TabList.INVALID_TAB_INDEX) {
            Tab activeTab = normalModel.getTabAt(activeIndex);
            activeTabId = activeTab.getId();
            activeTabState = UrlUtilities.isNTPUrl(activeTab.getUrl()) ? ActiveTabState.NTP
                                                                       : ActiveTabState.OTHER;
        }
        // Always override the existing value in case there is no active tab.
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, activeTabId);

        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, activeTabState);

        // Add information about the tabs that haven't finished being loaded.
        // We shouldn't have to worry about Tab duplication because the tab details are processed
        // only on the UI Thread.
        if (tabsBeingRestored != null) {
            for (TabRestoreDetails details : tabsBeingRestored) {
                // isIncognito was added in M61 (see https://crbug.com/485217), so it is extremely
                // unlikely that isIncognito will be null. But if it is, assume that the tab is
                // incognito so that #restoreTab() will require a tab state file on disk to
                // restore. If a tab state file exists and the tab is not actually incognito, it
                // will be restored in the normal tab model. If a tab state file does not exist,
                // the tab will not be restored.
                if (details.isIncognito == null || details.isIncognito) {
                    incognitoInfo.ids.add(details.id);
                    incognitoInfo.urls.add(details.url);
                } else {
                    normalInfo.ids.add(details.id);
                    normalInfo.urls.add(details.url);
                }
            }
        }

        byte[] listData = serializeMetadata(normalInfo, incognitoInfo);
        return new TabModelSelectorMetadata(listData, normalInfo, incognitoInfo);
    }

    /**
     * Creates a TabModelMetadata for the given TabModel mode.
     * @param selector The object of {@link TabModelSelector}
     * @param isIncognito Whether the TabModel is incognito.
     * @param skipNonActiveNtps Whether to skip non active NTPs.
     */
    private static TabModelMetadata metadataFromModel(
            TabModelSelector selector, boolean isIncognito, boolean skipNonActiveNtps) {
        TabModel tabModel = selector.getModel(isIncognito);
        TabModelMetadata modelInfo = new TabModelMetadata(tabModel.index());

        int activeIndex = tabModel.index();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            if (skipNonActiveNtps) {
                if (i == activeIndex) {
                    // If any non-active NTPs have been skipped, the serialized tab model index
                    // needs to be adjusted.
                    modelInfo.index = modelInfo.ids.size();
                } else if (tab.isNativePage() && UrlUtilities.isNTPUrl(tab.getUrl())) {
                    // Skips saving the non-selected Ntps.
                    continue;
                }
            }
            modelInfo.ids.add(tab.getId());
            modelInfo.urls.add(tab.getUrl().getSpec());
        }
        return modelInfo;
    }

    /**
     * Serializes data from a {@link TabModelSelector} into a byte array.
     * @param standardInfo      Info about the regular {@link TabModel}.
     * @param incognitoInfo     Info about the Incognito {@link TabModel}.
     * @return                  {@code byte[]} containing the serialized state of {@code selector}.
     */
    @VisibleForTesting
    public static byte[] serializeMetadata(
            TabModelMetadata standardInfo, TabModelMetadata incognitoInfo) throws IOException {
        int standardCount = standardInfo.ids.size();
        int incognitoCount = incognitoInfo.ids.size();

        // Determine how many Tabs there are.
        int numTabsTotal = incognitoCount + standardCount;

        // Save the index file containing the list of tabs to restore.
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream stream = new DataOutputStream(output);
        stream.writeInt(SAVED_STATE_VERSION);
        stream.writeInt(numTabsTotal);
        stream.writeInt(incognitoCount);
        stream.writeInt(incognitoInfo.index);
        stream.writeInt(standardInfo.index + incognitoCount);
        Log.d(TAG, "Serializing tab lists; counts: " + standardCount + ", " + incognitoCount);

        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.REGULAR_TAB_COUNT, standardCount);
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.INCOGNITO_TAB_COUNT, incognitoCount);

        // Save incognito state first, so when we load, if the incognito files are unreadable
        // we can fall back easily onto the standard selected tab.
        for (int i = 0; i < incognitoCount; i++) {
            stream.writeInt(incognitoInfo.ids.get(i));
            stream.writeUTF(incognitoInfo.urls.get(i));
        }
        for (int i = 0; i < standardCount; i++) {
            stream.writeInt(standardInfo.ids.get(i));
            stream.writeUTF(standardInfo.urls.get(i));
        }

        stream.close();
        return output.toByteArray();
    }

    private void saveListToFile(byte[] listData) {
        if (Arrays.equals(mLastSavedMetadata, listData)) return;

        saveListToFile(getStateDirectory(), mPersistencePolicy.getStateFileName(), listData);
        mLastSavedMetadata = listData;
    }

    /**
     * Atomically writes the given serialized data out to disk.
     * @param stateDirectory Directory to save TabModel data into.
     * @param stateFileName  File name to save TabModel data into.
     * @param listData       TabModel data in the form of a serialized byte array.
     */
    private static void saveListToFile(File stateDirectory, String stateFileName, byte[] listData) {
        synchronized (SAVE_LIST_LOCK) {
            // Save the index file containing the list of tabs to restore.
            File metadataFile = new File(stateDirectory, stateFileName);

            AtomicFile file = new AtomicFile(metadataFile);
            FileOutputStream stream = null;
            try {
                stream = file.startWrite();
                stream.write(listData, 0, listData.length);
                file.finishWrite(stream);
            } catch (IOException e) {
                if (stream != null) file.failWrite(stream);
                Log.e(TAG, "Failed to write file: " + metadataFile.getAbsolutePath());
            }
        }
    }

    /**
     * @param isIncognitoSelected Whether the tab model is incognito.
     * @return A callback for reading data from tab models.
     */
    private OnTabStateReadCallback createOnTabStateReadCallback(final boolean isIncognitoSelected,
            final boolean fromMerge) {
        return new OnTabStateReadCallback() {
            @Override
            public void onDetailsRead(int index, int id, String url, Boolean isIncognito,
                    boolean isStandardActiveIndex, boolean isIncognitoActiveIndex) {
                if (mLoadInProgress) {
                    // If a load and merge are both in progress, that means two metadata files
                    // are being read. If a merge was previously started and interrupted due to the
                    // app dying, the two metadata files may contain duplicate IDs. Skip tabs with
                    // duplicate IDs.
                    if (mPersistencePolicy.isMergeInProgress() && mTabIdsToRestore.contains(id)) {
                        return;
                    }

                    mTabIdsToRestore.add(id);
                }

                // Note that incognito tab may not load properly so we may need to use
                // the current tab from the standard model.
                // This logic only works because we store the incognito indices first.
                TabRestoreDetails details =
                        new TabRestoreDetails(id, index, isIncognito, url, fromMerge);

                if (!fromMerge && ((isIncognitoActiveIndex && isIncognitoSelected)
                        || (isStandardActiveIndex && !isIncognitoSelected))) {
                    // Active tab gets loaded first
                    mTabsToRestore.addFirst(details);
                } else {
                    mTabsToRestore.addLast(details);
                }

                for (TabPersistentStoreObserver observer : mObservers) {
                    observer.onDetailsRead(index, id, url, isStandardActiveIndex,
                            isIncognitoActiveIndex, isIncognito, fromMerge);
                }
            }
        };
    }

    /**
     * If a global max tab ID has not been computed and stored before, then check all the state
     * folders and calculate a new global max tab ID to be used. Must be called before any new tabs
     * are created.
     *
     * @throws IOException
     */
    private void checkAndUpdateMaxTabId() throws IOException {
        if (SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.TABMODEL_HAS_COMPUTED_MAX_ID, false)) {
            return;
        }

        int maxId = 0;
        // Calculation of the max tab ID is done only once per user and is stored in
        // SharedPreferences afterwards.  This is done on the UI thread because it is on the
        // critical patch to initializing the TabIdManager with the correct max tab ID.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            File[] subDirectories = TabStateDirectory.getOrCreateBaseStateDirectory().listFiles();
            if (subDirectories != null) {
                for (File subDirectory : subDirectories) {
                    if (!subDirectory.isDirectory()) {
                        assert false
                                : "Only directories should exist below the base state directory";
                        continue;
                    }
                    File[] files = subDirectory.listFiles();
                    if (files == null) continue;

                    for (File file : files) {
                        Pair<Integer, Boolean> tabStateInfo =
                                TabStateFileManager.parseInfoFromFilename(file.getName());
                        if (tabStateInfo != null) {
                            maxId = Math.max(maxId, tabStateInfo.first);
                        } else if (isStateFile(file.getName())) {
                            DataInputStream stream = null;
                            try {
                                stream = new DataInputStream(
                                        new BufferedInputStream(new FileInputStream(file)));
                                maxId = Math.max(maxId, readSavedStateFile(stream, null, null));
                            } finally {
                                StreamUtil.closeQuietly(stream);
                            }
                        }
                    }
                }
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        TabIdManager.getInstance().incrementIdCounterTo(maxId);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.TABMODEL_HAS_COMPUTED_MAX_ID, true);
    }

    /**
     * Extracts the tab information from a given tab state stream.
     *
     * @param stream   The stream pointing to the tab state file to be parsed.
     * @param callback A callback to be streamed updates about the tab state information being read.
     * @param tabIds   A mapping of tab ID to whether the tab is an off the record tab.
     * @return The next available tab ID based on the maximum ID referenced in this state file.
     */
    public static int readSavedStateFile(DataInputStream stream,
            @Nullable OnTabStateReadCallback callback, @Nullable SparseBooleanArray tabIds)
            throws IOException {
        if (stream == null) return 0;
        int nextId = 0;
        boolean skipUrlRead = false;
        boolean skipIncognitoCount = false;
        final int version = stream.readInt();
        if (version != SAVED_STATE_VERSION) {
            // We don't support restoring Tab data from before M18.
            if (version < 3) return 0;
            // Older versions are missing newer data.
            if (version < 5) skipIncognitoCount = true;
            if (version < 4) skipUrlRead = true;
        }

        final int count = stream.readInt();
        final int incognitoCount = skipIncognitoCount ? -1 : stream.readInt();
        final int incognitoActiveIndex = stream.readInt();
        final int standardActiveIndex = stream.readInt();
        if (count < 0 || incognitoActiveIndex >= count || standardActiveIndex >= count) {
            throw new IOException();
        }

        for (int i = 0; i < count; i++) {
            int id = stream.readInt();
            String tabUrl = skipUrlRead ? "" : stream.readUTF();
            if (id >= nextId) nextId = id + 1;
            if (tabIds != null) tabIds.append(id, true);

            Boolean isIncognito = (incognitoCount < 0) ? null : i < incognitoCount;

            if (callback != null) {
                callback.onDetailsRead(i, id, tabUrl, isIncognito,
                        i == standardActiveIndex, i == incognitoActiveIndex);
            }
        }
        return nextId;
    }

    /**
     * Triggers the next save tab task.  Clients do not need to call this as it will be triggered
     * automatically by calling {@link #addTabToSaveQueue(Tab)}.
     */
    @VisibleForTesting
    void saveNextTab() {
        if (mSaveTabTask != null) return;
        if (!mTabsToSave.isEmpty()) {
            Tab tab = mTabsToSave.removeFirst();
            mSaveTabTask = new SaveTabTask(tab);
            mSaveTabTask.executeOnTaskRunner(mSequencedTaskRunner);
        } else {
            saveTabListAsynchronously();
        }
    }

    /**
     * Kick off an AsyncTask to save the current list of Tabs.
     */
    public void saveTabListAsynchronously() {
        if (mSaveListTask != null) mSaveListTask.cancel(true);
        mSaveListTask = new SaveListTask();
        mSaveListTask.executeOnTaskRunner(mSequencedTaskRunner);
    }

    private class SaveTabTask extends AsyncTask<Void> {
        Tab mTab;
        int mId;
        TabState mState;
        boolean mEncrypted;
        boolean mStateSaved;

        SaveTabTask(Tab tab) {
            mTab = tab;
            mId = tab.getId();
            mEncrypted = tab.isIncognito();
        }

        @Override
        protected void onPreExecute() {
            if (mDestroyed || isCancelled()) return;
            mState = TabStateExtractor.from(mTab);
        }

        @Override
        protected Void doInBackground() {
            mStateSaved = saveTabState(mId, mEncrypted, mState);
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            if (mDestroyed || isCancelled()) return;
            if (mStateSaved) {
                if (!mTab.isDestroyed()) TabStateAttributes.from(mTab).clearTabStateDirtiness();
                mTab.setIsTabSaveEnabled(isCriticalPersistedTabDataSaveOnlyEnabled()
                        || isCriticalPersistedTabDataSaveAndRestoreEnabled());
                migrateSomeRemainingTabsToCriticalPersistedTabData();
            }
            mSaveTabTask = null;
            saveNextTab();
        }
    }

    /** Stores meta data about the TabModelSelector which has been serialized to disk. */
    public static class TabModelSelectorMetadata {
        public final byte[] listData;
        public final TabModelMetadata normalModelMetadata;
        public final TabModelMetadata incognitoModelMetadata;

        public TabModelSelectorMetadata(byte[] listData, TabModelMetadata normalModelMetadata,
                TabModelMetadata incognitoModelMetadata) {
            this.listData = listData;
            this.normalModelMetadata = normalModelMetadata;
            this.incognitoModelMetadata = incognitoModelMetadata;
        }
    }

    private class SaveListTask extends AsyncTask<Void> {
        TabModelSelectorMetadata mMetadata;

        @Override
        protected void onPreExecute() {
            if (mDestroyed || isCancelled()) return;
            try {
                mMetadata = serializeTabMetadata();
            } catch (IOException e) {
                mMetadata = null;
            }
        }

        @Override
        protected Void doInBackground() {
            if (mMetadata == null || isCancelled()) return null;
            saveListToFile(mMetadata.listData);
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            if (mDestroyed || isCancelled()) {
                mMetadata = null;
                return;
            }

            if (mSaveListTask == this) {
                mSaveListTask = null;
                for (TabPersistentStoreObserver observer : mObservers) {
                    observer.onMetadataSavedAsynchronously(mMetadata);
                }
                mMetadata = null;
            }
        }
    }

    private File getStateDirectory() {
        return mPersistencePolicy.getOrCreateStateDirectory();
    }

    /**
     * Returns a file pointing at the TabState corresponding to the given Tab.
     * @param tabId ID of the TabState to locate.
     * @param encrypted Whether or not the tab is encrypted.
     * @return File pointing at the TabState for the Tab.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public File getTabStateFile(int tabId, boolean encrypted) {
        return TabStateFileManager.getTabStateFile(getStateDirectory(), tabId, encrypted);
    }

    /**
     * Saves the TabState with the given ID.
     * @param tabId ID of the Tab.
     * @param encrypted Whether or not the TabState is encrypted.
     * @param state TabState for the Tab.
     */
    private boolean saveTabState(int tabId, boolean encrypted, TabState state) {
        if (state == null) return false;

        try {
            TabStateFileManager.saveState(getTabStateFile(tabId, encrypted), state, encrypted);
            return true;
        } catch (OutOfMemoryError e) {
            android.util.Log.e(
                    TAG, "Out of memory error while attempting to save tab state.  Erasing.");
            deleteTabState(tabId, encrypted);
        }
        return false;
    }

    /**
     * Deletes the TabState corresponding to the given Tab.
     * @param id ID of the TabState to delete.
     * @param encrypted Whether or not the tab is encrypted.
     */
    private void deleteTabState(int id, boolean encrypted) {
        TabStateFileManager.deleteTabState(getStateDirectory(), id, encrypted);
    }

    private void onStateLoaded() {
        for (TabPersistentStoreObserver observer : mObservers) {
            // mergeState() starts an AsyncTask to call this and this calls
            // onTabStateInitialized which should be called from the UI thread.
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> observer.onStateLoaded());
        }
    }

    private void loadNextTab() {
        if (mDestroyed) return;

        if (mTabsToRestore.isEmpty()) {
            mNormalTabsRestored = null;
            mIncognitoTabsRestored = null;
            mLoadInProgress = false;

            // If tabs are done being merged into this instance, save the tab metadata file for this
            // TabPersistentStore and delete the metadata file for the other instance, then notify
            // observers.
            if (mPersistencePolicy.isMergeInProgress()) {
                PostTask.postTask(TaskTraits.UI_DEFAULT, new Runnable() {
                    @Override
                    public void run() {
                        // This eventually calls serializeTabModelSelector() which much be called
                        // from the UI thread. #mergeState() starts an async task in the background
                        // that goes through this code path.
                        saveTabListAsynchronously();
                    }
                });
                for (String mergedFileName : new HashSet<String>(mMergedFileNames)) {
                    deleteFileAsync(mergedFileName, true);
                }
                for (TabPersistentStoreObserver observer : mObservers) observer.onStateMerged();
            }

            cleanUpPersistentData();
            onStateLoaded();
            mTabLoader = null;
            RecordHistogram.recordCount1MHistogram(
                    "Tabs.Startup.TabCount.Regular", mTabModelSelector.getModel(false).getCount());
            RecordHistogram.recordCount1MHistogram(
                    "Tabs.Startup.TabCount.Incognito", mTabModelSelector.getModel(true).getCount());
            Log.d(TAG,
                    "Loaded tab lists; counts: " + mTabModelSelector.getModel(false).getCount()
                            + "," + mTabModelSelector.getModel(true).getCount());
        } else {
            TabRestoreDetails tabToRestore = mTabsToRestore.removeFirst();
            mTabLoader = new TabLoader(tabToRestore);
            mTabLoader.load();
        }
    }

    /**
     * Determine if tab is incognito based on field in TabRestoreDetails. If unknown in
     * TabRestoreDetails, try to determine if Tab is incognito or not based on existence
     * of CriticalPersistedTabData files.
     */
    private static Boolean isIncognitoWithCPTDFallback(TabRestoreDetails tabToRestore) {
        return tabToRestore.isIncognito == null
                ? FilePersistedTabDataStorage.isIncognito(tabToRestore.id)
                : tabToRestore.isIncognito;
    }

    /**
     * Manages loading of {@link TabState} and {@link CriticalPersistedTabData} (TabState
     * replacement) stored tab metadata. Also used to track if a load is in progress and the tab
     * details of that load.
     */
    private class TabLoader {
        public final TabRestoreDetails mTabToRestore;
        private LoadTabTask mLoadTabTask;
        private CallbackController mCallbackController = new CallbackController();

        /**
         * @param tabToRestore details of {@link Tab} which will be read from storage
         */
        TabLoader(TabRestoreDetails tabToRestore) {
            mTabToRestore = tabToRestore;
        }

        /**
         * Load serialized {@link CriticalPersistedTabData} from storage if CriticalPersistedTabData
         * is enabled or {@link TabState} if CriticalPersistedTabData is not enabled.
         * Fall back to {@link TabState} if no {@link CriticalPersistedTabData} file exists.
         */
        public void load() {
            if (isCriticalPersistedTabDataSaveAndRestoreEnabled()) {
                Boolean isIncognito = isIncognitoWithCPTDFallback(mTabToRestore);
                if (isIncognito == null) {
                    loadTabState();
                } else {
                    TraceEvent.startAsync("LoadCriticalPersistedTabData", mTabToRestore.id);
                    long startTime = SystemClock.elapsedRealtime();
                    CriticalPersistedTabData.restore(mTabToRestore.id, isIncognito,
                            mCallbackController.makeCancelable((res) -> {
                                TraceEvent.finishAsync(
                                        "LoadCriticalPersistedTabData", mTabToRestore.id);
                                RecordHistogram.recordTimesHistogram(
                                        String.format(Locale.US,
                                                "Tabs.SavedTabLoadTime.CriticalPersistedTabData.%s",
                                                res == null ? "Null" : "Exists"),
                                        SystemClock.elapsedRealtime() - startTime);
                                if (CriticalPersistedTabData.isEmptySerialization(res)) {
                                    loadTabState();
                                } else {
                                    completeLoad(mTabToRestore, null, res);
                                }
                            }));
                }
            } else {
                loadTabState();
            }
        }

        private void loadTabState() {
            mLoadTabTask = new LoadTabTask(mTabToRestore);
            mLoadTabTask.executeOnTaskRunner(mSequencedTaskRunner);
        }

        public void cancel(boolean mayInterruptIfRunning) {
            if (mLoadTabTask != null) {
                mLoadTabTask.cancel(mayInterruptIfRunning);
            }
            mCallbackController.destroy();
        }
    }

    /**
     * Asynchronously triggers a cleanup of any unused persistent data.
     */
    private void cleanUpPersistentData() {
        mPersistencePolicy.cleanupUnusedFiles(new Callback<List<String>>() {
            @Override
            public void onResult(List<String> result) {
                if (result == null) return;
                for (int i = 0; i < result.size(); i++) {
                    deleteFileAsync(result.get(i), true);
                }
            }
        });
        // TODO(crbug.com/1237620) Make sure maintenance works correctly in multi window case.
        PersistedTabData.performStorageMaintenance(
                TabModelUtils.getRegularTabIds(mTabModelSelector));
    }

    /**
     * Clean up persistent state for a given instance.
     * @param instanceId Instance ID.
     */
    public void cleanupStateFile(int instanceId) {
        mPersistencePolicy.cleanupInstanceState(instanceId, new Callback<List<String>>() {
            @Override
            public void onResult(List<String> result) {
                // Delete the instance state file (tab_stateX) as well.
                deleteFileAsync(TabbedModeTabPersistencePolicy.getStateFileName(instanceId), true);

                // |result| can be null if the task gets cancelled.
                if (result == null) return;
                for (int i = 0; i < result.size(); i++) deleteFileAsync(result.get(i), true);
            }
        });
    }

    /**
     * File mutations (e.g. saving & deleting) are explicitly serialized to ensure that they occur
     * in the correct order.
     *
     * @param file Name of file under the state directory to be deleted.
     * @param useSerialExecution true if serial executor will be used
     */
    private void deleteFileAsync(final String file, boolean useSerialExecution) {
        if (useSerialExecution) {
            new BackgroundOnlyAsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    deleteStateFile(file);
                    return null;
                }
            }.executeOnTaskRunner(mSequencedTaskRunner);
        } else {
            PostTask.runOrPostTask(
                    TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> { deleteStateFile(file); });
        }
    }

    private void deleteStateFile(String file) {
        ThreadUtils.assertOnBackgroundThread();
        File stateFile = new File(getStateDirectory(), file);
        if (stateFile.exists()) {
            if (!stateFile.delete()) Log.e(TAG, "Failed to delete file: " + stateFile);

            // The merge isn't completely finished until the other TabPersistentStores'
            // metadata files are deleted.
            boolean wasMergeFile = mMergedFileNames.remove(file);
            if (wasMergeFile && mMergedFileNames.isEmpty()) {
                mPersistencePolicy.setMergeInProgress(false);
            }
        }
    }

    private static boolean isCriticalPersistedTabDataSaveAndRestoreEnabled() {
        return ChromeFeatureList.sCriticalPersistedTabData.isEnabled()
                && !CRITICAL_PERSISTED_TAB_DATA_SAVE_ONLY_PARAM.getValue();
    }

    private static boolean isCriticalPersistedTabDataSaveOnlyEnabled() {
        return ChromeFeatureList.sCriticalPersistedTabData.isEnabled()
                && CRITICAL_PERSISTED_TAB_DATA_SAVE_ONLY_PARAM.getValue();
    }

    private class LoadTabTask extends AsyncTask<TabState> {
        private final TabRestoreDetails mTabToRestore;
        private TabState mTabState;
        private long mStartTime;

        public LoadTabTask(TabRestoreDetails tabToRestore) {
            mTabToRestore = tabToRestore;
            TraceEvent.startAsync("LoadTabTask", mTabToRestore.id);
            TraceEvent.startAsync("LoadTabState", mTabToRestore.id);
            mStartTime = SystemClock.elapsedRealtime();
        }

        @Override
        protected TabState doInBackground() {
            if (mDestroyed || isCancelled()) return null;
            try {
                return TabStateFileManager.restoreTabState(getStateDirectory(), mTabToRestore.id);
            } catch (Exception e) {
                Log.w(TAG, "Unable to read state: " + e);
                return null;
            }
        }

        @Override
        protected void onPostExecute(TabState tabState) {
            TraceEvent.finishAsync("LoadTabState", mTabToRestore.id);
            RecordHistogram.recordTimesHistogram(
                    String.format(Locale.US, "Tabs.SavedTabLoadTime.TabState.%s",
                            tabState == null ? "Null" : "Exists"),
                    SystemClock.elapsedRealtime() - mStartTime);
            mTabState = tabState;

            TraceEvent.finishAsync("LoadTabTask", mTabToRestore.id);
            if (mDestroyed || isCancelled()) {
                return;
            }

            completeLoad(mTabToRestore, mTabState, null);
        }
    }

    private void completeLoad(TabRestoreDetails tabToRestore, TabState tabState,
            SerializedCriticalPersistedTabData serializedCriticalPersistedTabData) {
        boolean isIncognito = isIncognitoTabBeingRestored(
                tabToRestore, tabState, serializedCriticalPersistedTabData);
        boolean isLoadCancelled = (isIncognito && mCancelIncognitoTabLoads)
                || (!isIncognito && mCancelNormalTabLoads);
        if (!isLoadCancelled) {
            restoreTab(tabToRestore, tabState, serializedCriticalPersistedTabData, false);
        }

        loadNextTab();
    }

    /**
     * Provides additional meta data to restore an individual tab.
     */
    @VisibleForTesting
    protected static final class TabRestoreDetails {
        public final int id;
        public final int originalIndex;
        public final String url;
        public final Boolean isIncognito;
        public final Boolean fromMerge;

        public TabRestoreDetails(int id, int originalIndex, Boolean isIncognito, String url,
                Boolean fromMerge) {
            this.id = id;
            this.originalIndex = originalIndex;
            this.url = url;
            this.isIncognito = isIncognito;
            this.fromMerge = fromMerge;
        }
    }

    private boolean isTabUrlContentScheme(Tab tab) {
        GURL url = tab.getUrl();
        return url != null && url.getScheme().equals(UrlConstants.CONTENT_SCHEME);
    }

    /**
     * Determines if a Tab being restored is definitely an Incognito Tab.
     *
     * This function can fail to determine if a Tab is incognito if not enough data about the Tab
     * was successfully saved out.
     *
     * @return True if the tab is definitely Incognito, false if it's not or if it's undecideable.
     */
    private boolean isIncognitoTabBeingRestored(TabRestoreDetails tabDetails, TabState tabState,
            SerializedCriticalPersistedTabData serializedCriticalPersistedTabData) {
        if (tabState != null) {
            // The Tab's previous state was completely restored.
            return tabState.isIncognito();
        } else if (tabDetails.isIncognito != null) {
            // The TabState couldn't be restored, but we have some information about the tab.
            return tabDetails.isIncognito;
        } else if (!CriticalPersistedTabData.isEmptySerialization(
                           serializedCriticalPersistedTabData)) {
            return FilePersistedTabDataStorage.isIncognito(tabDetails.id);
        } else {
            // The tab's type is undecidable.
            return false;
        }
    }

    private AsyncTask<DataInputStream> startFetchTabListTask(
            TaskRunner taskRunner, final String stateFileName) {
        return new BackgroundOnlyAsyncTask<DataInputStream>() {
            @Override
            protected DataInputStream doInBackground() {
                Log.d(TAG, "Starting to fetch tab list for " + stateFileName);
                File stateFile = new File(getStateDirectory(), stateFileName);
                if (!stateFile.exists()) {
                    Log.d(TAG, "State file does not exist.");
                    return null;
                }
                FileInputStream stream = null;
                byte[] data;
                try {
                    stream = new FileInputStream(stateFile);
                    data = new byte[(int) stateFile.length()];
                    stream.read(data);
                } catch (IOException exception) {
                    Log.e(TAG, "Could not read state file.", exception);
                    return null;
                } finally {
                    StreamUtil.closeQuietly(stream);
                }
                Log.d(TAG, "Finished fetching tab list.");
                return new DataInputStream(new ByteArrayInputStream(data));
            }
        }.executeOnTaskRunner(taskRunner);
    }

    private void startPrefetchActiveTabTask(TaskRunner taskRunner) {
        final int activeTabId = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, Tab.INVALID_TAB_ID);
        if (activeTabId == Tab.INVALID_TAB_ID) return;
        // If the CriticalPersistedTabData flag is on, try to prefetch
        // CriticalPersistedTabData and fallback to prefetching TabState if there
        // is no CriticalPersistedTabData.
        if (isCriticalPersistedTabDataSaveAndRestoreEnabled()) {
            // This is an equivalent of the hack in TabStateFileManager whereby it is determined
            // if the Tab is regular or incognito by the presence of the corresponding TabState
            // file.
            Boolean isIncognito = FilePersistedTabDataStorage.isIncognito(activeTabId);
            mPrefetchCriticalPersistedTabDataActiveTabTask =
                    new BackgroundOnlyAsyncTask<SerializedCriticalPersistedTabData>() {
                        @Override
                        protected SerializedCriticalPersistedTabData doInBackground() {
                            if (isIncognito == null) {
                                prefetchActiveTabTask(activeTabId, taskRunner);
                                return null;
                            }
                            SerializedCriticalPersistedTabData res =
                                    CriticalPersistedTabData.restore(activeTabId, isIncognito);
                            if (CriticalPersistedTabData.isEmptySerialization(res)) {
                                prefetchActiveTabTask(activeTabId, taskRunner);
                                return null;
                            }
                            return res;
                        }
                    }.executeOnTaskRunner(taskRunner);
        } else {
            prefetchActiveTabTask(activeTabId, taskRunner);
        }
    }

    private void prefetchActiveTabTask(int activeTabId, TaskRunner taskRunner) {
        mPrefetchTabStateActiveTabTask = new BackgroundOnlyAsyncTask<TabState>() {
            @Override
            protected TabState doInBackground() {
                return TabStateFileManager.restoreTabState(getStateDirectory(), activeTabId);
            }
        }.executeOnTaskRunner(taskRunner);
    }

    public void addObserver(TabPersistentStoreObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes a {@link TabPersistentStoreObserver}.
     * @param observer The {@link TabPersistentStoreObserver} to remove.
     */
    public void removeObserver(TabPersistentStoreObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @param uniqueId The ID that uniquely identifies this state file.
     * @return The name of the state file.
     */
    public static String getStateFileName(String uniqueId) {
        return SAVED_STATE_FILE_PREFIX + uniqueId;
    }

    /**
     * Parses the state file name and returns the unique ID encoded into it.
     * @param stateFileName The state file name to be parsed.
     * @return The unique ID used when generating the file name.
     */
    public static String getStateFileUniqueId(String stateFileName) {
        assert isStateFile(stateFileName);
        return stateFileName.substring(SAVED_STATE_FILE_PREFIX.length());
    }

    /**
     * @return Whether the specified filename matches the expected pattern of the tab state files.
     */
    public static boolean isStateFile(String fileName) {
        // The .new/.bak suffixes may be added internally by AtomicFile before the file finishes
        // writing. Ignore files in this transitory state.
        return fileName.startsWith(SAVED_STATE_FILE_PREFIX) && !fileName.endsWith(".new")
                && !fileName.endsWith(".bak");
    }

    /**
     * @return The shared pref APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE. This is used when we need to
     *         know the last known tab state before the active tab from the tab state is read.
     */
    public static @ActiveTabState int readLastKnownActiveTabStatePref() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.EMPTY);
    }

    private static int getMigrateToCriticalPersistedTabDataBatchSize() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA,
                    MIGRATE_TO_CRITICAL_PERSISTED_TAB_DATA_BATCH_SIZE_PARAM,
                    MIGRATE_TO_CRITICAL_PERSISTED_TAB_DATA_DEFAULT_BATCH_SIZE);
        }
        return MIGRATE_TO_CRITICAL_PERSISTED_TAB_DATA_DEFAULT_BATCH_SIZE;
    }

    SequencedTaskRunner getTaskRunnerForTests() {
        return mSequencedTaskRunner;
    }

    void addTabToRestoreForTesting(TabRestoreDetails tabDetails) {
        mTabsToRestore.add(tabDetails);
    }

    public TabPersistencePolicy getTabPersistencePolicyForTesting() {
        return mPersistencePolicy;
    }

    public List<Pair<AsyncTask<DataInputStream>, String>> getTabListToMergeTasksForTesting() {
        return mPrefetchTabListToMergeTasks;
    }

    public AsyncTask<TabState> getPrefetchTabStateActiveTabTaskForTesting() {
        return mPrefetchTabStateActiveTabTask;
    }

    @VisibleForTesting
    public AsyncTask<SerializedCriticalPersistedTabData>
    getPrefetchCriticalPersistedTabDataActiveTabTaskForTesting() {
        return mPrefetchCriticalPersistedTabDataActiveTabTask;
    }

    private void migrateSomeRemainingTabsToCriticalPersistedTabData() {
        if (!isCriticalPersistedTabDataSaveOnlyEnabled()
                && !isCriticalPersistedTabDataSaveAndRestoreEnabled()) {
            return;
        }
        int numMigrated = 0;
        while (numMigrated < getMigrateToCriticalPersistedTabDataBatchSize()
                && !mTabsToMigrate.isEmpty()) {
            Tab tabToMigrate = mTabsToMigrate.pollFirst();
            if (tabToMigrate != null && !tabToMigrate.isDestroyed()) {
                // Not all Tab metadata changes result in a Tab save. There is
                // throttling via setShouldSave. To ensure an un-migrated Tab is
                // saved, the shouldSave flag should be set.
                CriticalPersistedTabData.from(tabToMigrate).setShouldSave();
                tabToMigrate.setIsTabSaveEnabled(true);
            }
            numMigrated++;
        }
    }

    /**
     * Sets whether to skip saving all of the non-active Ntps when serializing the Tab model meta
     * data.
     */
    public void setSkipSavingNonActiveNtps(boolean skipSavingNonActiveNtps) {
        mSkipSavingNonActiveNtps = skipSavingNonActiveNtps;
    }
}
