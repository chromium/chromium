// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.v4.util.AtomicFile;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Deque;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * This class handles saving and loading tab state from the persistent storage.
 */
public class TabPersistentStore extends TabPersister {
    private static final String TAG = "tabmodel";

    /**
     * The current version of the saved state file.
     * Version 4: In addition to the tab's ID, save the tab's last URL.
     * Version 5: In addition to the total tab count, save the incognito tab count.
     */
    private static final int SAVED_STATE_VERSION = 5;

    private static final String BASE_STATE_FOLDER = "tabs";

    /** The name of the directory where the state is saved. */
    @VisibleForTesting
    static final String SAVED_STATE_DIRECTORY = "0";

    @VisibleForTesting
    static final String PREF_ACTIVE_TAB_ID =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.ACTIVE_TAB_ID";

    private static final String PREF_HAS_COMPUTED_MAX_ID =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_COMPUTED_MAX_ID";

    /** Prevents two TabPersistentStores from saving the same file simultaneously. */
    private static final Object SAVE_LIST_LOCK = new Object();

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
                boolean isIncognitoActiveIndex) {}

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
        public final int index;
        public final List<Integer> ids;
        public final List<String> urls;

        TabModelMetadata(int selectedIndex) {
            index = selectedIndex;
            ids = new ArrayList<>();
            urls = new ArrayList<>();
        }
    }

    private static class BaseStateDirectoryHolder {
        // Not final for tests.
        private static File sDirectory;

        static {
            sDirectory = ContextUtils.getApplicationContext()
                    .getDir(BASE_STATE_FOLDER, Context.MODE_PRIVATE);
        }
    }

    private final TabPersistencePolicy mPersistencePolicy;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private ObserverList<TabPersistentStoreObserver> mObservers;

    private final Deque<Tab> mTabsToSave;
    private final Deque<TabRestoreDetails> mTabsToRestore;
    private final Set<Integer> mTabIdsToRestore;

    private LoadTabTask mLoadTabTask;
    private SaveTabTask mSaveTabTask;
    private SaveListTask mSaveListTask;

    private boolean mDestroyed;
    private boolean mCancelNormalTabLoads;
    private boolean mCancelIncognitoTabLoads;

    // Keys are the original tab indexes, values are the tab ids.
    private SparseIntArray mNormalTabsRestored;
    private SparseIntArray mIncognitoTabsRestored;

    private SharedPreferences mPreferences;
    private AsyncTask<DataInputStream> mPrefetchTabListTask;
    private List<Pair<AsyncTask<DataInputStream>, String>> mPrefetchTabListToMergeTasks;
    // A set of filenames which are tracked to merge.
    private Set<String> mMergedFileNames;
    private byte[] mLastSavedMetadata;

    // Tracks whether this TabPersistentStore's tabs are being loaded.
    private boolean mLoadInProgress;
    // The number of tabs being merged. Used for logging time to restore per tab.
    private int mMergeTabCount;
    // Set when restoreTabs() is called during a non-cold-start merge. Used for logging time to
    // restore per tab.
    private long mRestoreMergedTabsStartTime;

    @VisibleForTesting
    AsyncTask<TabState> mPrefetchActiveTabTask;

    /**
     * Creates an instance of a TabPersistentStore.
     * @param modelSelector The {@link TabModelSelector} to restore to and save from.
     * @param tabCreatorManager The {@link TabCreatorManager} to use.
     * @param observer      Notified when the TabPersistentStore has completed tasks.
     */
    public TabPersistentStore(TabPersistencePolicy policy, TabModelSelector modelSelector,
            TabCreatorManager tabCreatorManager, TabPersistentStoreObserver observer) {
        mPersistencePolicy = policy;
        mTabModelSelector = modelSelector;
        mTabCreatorManager = tabCreatorManager;
        mTabsToSave = new ArrayDeque<>();
        mTabsToRestore = new ArrayDeque<>();
        mTabIdsToRestore = new HashSet<>();
        mObservers = new ObserverList<>();
        mObservers.addObserver(observer);
        mPreferences = ContextUtils.getAppSharedPreferences();

        mPrefetchTabListToMergeTasks = new ArrayList<>();
        mMergedFileNames = new HashSet<>();

        assert isStateFile(policy.getStateFileName()) : "State file name is not valid";
        boolean needsInitialization = mPersistencePolicy.performInitialization(
                AsyncTask.SERIAL_EXECUTOR);

        if (mPersistencePolicy.isMergeInProgress()) return;

        // TODO: create a state controller to sequence initializations rather than relying on
        // the SERIAL_EXECUTOR. http://crbug.com/776554
        Executor executor = needsInitialization
                ? AsyncTask.SERIAL_EXECUTOR : AsyncTask.THREAD_POOL_EXECUTOR;

        mPrefetchTabListTask =
                startFetchTabListTask(executor, mPersistencePolicy.getStateFileName());
        startPrefetchActiveTabTask(executor);

        if (mPersistencePolicy.shouldMergeOnStartup()) {
            for (String mergedFileName : mPersistencePolicy.getStateToBeMergedFileNames()) {
                AsyncTask<DataInputStream> task = startFetchTabListTask(executor, mergedFileName);
                mPrefetchTabListToMergeTasks.add(Pair.create(task, mergedFileName));
            }
        }
    }

    @Override
    protected File getStateDirectory() {
        return mPersistencePolicy.getOrCreateStateDirectory();
    }

    /**
     * Waits for the task that migrates all state files to their new location to finish.
     */
    @VisibleForTesting
    public void waitForMigrationToFinish() {
        mPersistencePolicy.waitForInitializationToFinish();
    }

    /**
     * Sets the {@link TabContentManager} to use.
     * @param cache The {@link TabContentManager} to use.
     */
    public void setTabContentManager(TabContentManager cache) {
        mPersistencePolicy.setTabContentManager(cache);
    }

    private static void logExecutionTime(String name, long time) {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordHistogram.recordTimesHistogram("Android.StrictMode.TabPersistentStore." + name,
                    SystemClock.uptimeMillis() - time, TimeUnit.MILLISECONDS);
        }
    }

    public void saveState() {
        // Temporarily allowing disk access. TODO: Fix. See http://b/5518024
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            long saveStateStartTime = SystemClock.uptimeMillis();
            // The list of tabs should be saved first in case our activity is terminated early.
            // Explicitly toss out any existing SaveListTask because they only save the TabModel as
            // it looked when the SaveListTask was first created.
            if (mSaveListTask != null) mSaveListTask.cancel(true);
            try {
                saveListToFile(serializeTabMetadata().listData);
            } catch (IOException e) {
                Log.w(TAG, "Error while saving tabs state; will attempt to continue...", e);
            }
            logExecutionTime("SaveListTime", saveStateStartTime);

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

            long saveTabsStartTime = SystemClock.uptimeMillis();
            // Synchronously save any remaining unsaved tabs (hopefully very few).
            for (Tab tab : mTabsToSave) {
                int id = tab.getId();
                boolean incognito = tab.isIncognito();
                try {
                    TabState state = tab.getState();
                    if (state != null) {
                        TabState.saveState(getTabStateFile(id, incognito), state, incognito);
                    }
                } catch (OutOfMemoryError e) {
                    Log.e(TAG, "Out of memory error while attempting to save tab state.  Erasing.");
                    deleteTabState(id, incognito);
                }
            }
            mTabsToSave.clear();
            logExecutionTime("SaveTabsTime", saveTabsStartTime);
            logExecutionTime("SaveStateTime", saveStateStartTime);
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
        long time = SystemClock.uptimeMillis();

        // If a cleanup task is in progress, cancel it before loading state.
        mPersistencePolicy.cancelCleanupInProgress();

        waitForMigrationToFinish();
        logExecutionTime("LoadStateTime", time);

        initializeRestoreVars(ignoreIncognitoFiles);

        try {
            long timeLoadingState = SystemClock.uptimeMillis();
            assert mTabModelSelector.getModel(true).getCount() == 0;
            assert mTabModelSelector.getModel(false).getCount() == 0;
            checkAndUpdateMaxTabId();
            DataInputStream stream;
            if (mPrefetchTabListTask != null) {
                long timeWaitingForPrefetch = SystemClock.uptimeMillis();
                stream = mPrefetchTabListTask.get();

                // Restore the tabs for this TabPeristentStore instance if the tab metadata file
                // exists.
                if (stream != null) {
                    logExecutionTime("LoadStateInternalPrefetchTime", timeWaitingForPrefetch);
                    mLoadInProgress = true;
                    readSavedStateFile(
                            stream,
                            createOnTabStateReadCallback(mTabModelSelector.isIncognitoSelected(),
                                    false),
                            null,
                            false);
                    logExecutionTime("LoadStateInternalTime", timeLoadingState);
                }
            }

            // Restore the tabs for the other TabPeristentStore instance if its tab metadata file
            // exists.
            if (mPrefetchTabListToMergeTasks.size() > 0) {
                for (Pair<AsyncTask<DataInputStream>, String> mergeTask :
                        mPrefetchTabListToMergeTasks) {
                    time = SystemClock.uptimeMillis();
                    AsyncTask<DataInputStream> task = mergeTask.first;
                    stream = task.get();
                    if (stream == null) continue;

                    logExecutionTime("MergeStateInternalFetchTime", time);
                    mMergedFileNames.add(mergeTask.second);
                    mPersistencePolicy.setMergeInProgress(true);
                    readSavedStateFile(stream,
                            createOnTabStateReadCallback(mTabModelSelector.isIncognitoSelected(),
                                    mTabsToRestore.size() == 0 ? false : true),
                            null, true);
                    logExecutionTime("MergeStateInternalTime", time);
                }
                if (!mMergedFileNames.isEmpty()) {
                    RecordUserAction.record("Android.MergeState.ColdStart");
                }
                mPrefetchTabListToMergeTasks.clear();
            }
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing app on startup.
            Log.d(TAG, "loadState exception: " + e.toString(), e);
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
            Log.i(TAG, "Tab load still in progress when merge was attempted.");
            return;
        }

        // Initialize variables.
        initializeRestoreVars(false);

        try {
            // Read the tab state metadata file.
            for (String mergeFileName : mPersistencePolicy.getStateToBeMergedFileNames()) {
                long time = SystemClock.uptimeMillis();
                DataInputStream stream =
                        startFetchTabListTask(AsyncTask.SERIAL_EXECUTOR, mergeFileName).get();
                if (stream == null) continue;

                logExecutionTime("MergeStateInternalFetchTime", time);
                mMergedFileNames.add(mergeFileName);
                mPersistencePolicy.setMergeInProgress(true);
                readSavedStateFile(stream,
                        createOnTabStateReadCallback(mTabModelSelector.isIncognitoSelected(), true),
                        null, true);
                logExecutionTime("MergeStateInternalTime", time);
            }
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing app.
            Log.d(TAG, "meregeState exception: " + e.toString(), e);
        }

        // Restore the tabs from the second activity asynchronously.
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                mMergeTabCount = mTabsToRestore.size();
                mRestoreMergedTabsStartTime = SystemClock.uptimeMillis();
                restoreTabs(false);
                return null;
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
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
                TabRestoreDetails tabToRestore = mTabsToRestore.removeFirst();
                restoreTab(tabToRestore, true);
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
        if (mLoadTabTask != null) {
            if ((url == null && mLoadTabTask.mTabToRestore.id == id)
                    || (url != null && TextUtils.equals(mLoadTabTask.mTabToRestore.url, url))) {
                // Steal the task of restoring the tab from the active load tab task.
                mLoadTabTask.cancel(false);
                tabToRestore = mLoadTabTask.mTabToRestore;
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
        try {
            long time = SystemClock.uptimeMillis();
            TabState state;
            int restoredTabId = mPreferences.getInt(PREF_ACTIVE_TAB_ID, Tab.INVALID_TAB_ID);
            if (restoredTabId == tabToRestore.id && mPrefetchActiveTabTask != null) {
                long timeWaitingForPrefetch = SystemClock.uptimeMillis();
                state = mPrefetchActiveTabTask.get();
                logExecutionTime("RestoreTabPrefetchTime", timeWaitingForPrefetch);
            } else {
                // Necessary to do on the UI thread as a last resort.
                state = TabState.restoreTabState(getStateDirectory(), tabToRestore.id);
            }
            logExecutionTime("RestoreTabTime", time);
            restoreTab(tabToRestore, state, setAsActive);
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing the app
            // at startup.
            Log.d(TAG, "loadTabs exception: " + e.toString(), e);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Handles restoring an individual tab.
     *
     * @param tabToRestore Meta data about the tab to be restored.
     * @param tabState     The previously serialized state of the tab to be restored.
     * @param setAsActive  Whether the tab should be set as the active tab as part of the
     *                     restoration process.
     */
    @VisibleForTesting
    protected void restoreTab(
            TabRestoreDetails tabToRestore, TabState tabState, boolean setAsActive) {
        // If we don't have enough information about the Tab, bail out.
        boolean isIncognito = isIncognitoTabBeingRestored(tabToRestore, tabState);
        if (tabState == null) {
            if (tabToRestore.isIncognito == null) {
                Log.w(TAG, "Failed to restore tab: not enough info about its type was available.");
                return;
            } else if (isIncognito) {
                boolean isNtp = NewTabPage.isNTPUrl(tabToRestore.url);
                boolean isNtpFromMerge = isNtp && tabToRestore.fromMerge;

                if (!isNtpFromMerge && (!isNtp || !setAsActive || mCancelIncognitoTabLoads)) {
                    Log.i(TAG,
                            "Failed to restore Incognito tab: its TabState could not be restored.");
                    return;
                }
            }
        }

        TabModel model = mTabModelSelector.getModel(isIncognito);
        SparseIntArray restoredTabs = isIncognito ? mIncognitoTabsRestored : mNormalTabsRestored;
        int restoredIndex = 0;
        if (tabToRestore.fromMerge) {
            // Put any tabs being merged into this list at the end.
            // TODO(ltian): need to figure out a way to add merged tabs before Browser Actions tabs
            // when tab restore and Browser Actions tab merging happen at the same time.
            restoredIndex = mTabModelSelector.getModel(isIncognito).getCount();
        } else if (restoredTabs.size() > 0
                && tabToRestore.originalIndex > restoredTabs.keyAt(restoredTabs.size() - 1)) {
            // If the tab's index is too large, restore it at the end of the list.
            restoredIndex = restoredTabs.size();
        } else {
             // Otherwise try to find the tab we should restore before, if any.
            for (int i = 0; i < restoredTabs.size(); i++) {
                if (restoredTabs.keyAt(i) > tabToRestore.originalIndex) {
                    Tab nextTabByIndex = TabModelUtils.getTabById(model, restoredTabs.valueAt(i));
                    restoredIndex = nextTabByIndex != null ? model.indexOf(nextTabByIndex) : -1;
                    break;
                }
            }
        }

        int tabId = tabToRestore.id;
        if (tabState != null) {
            mTabCreatorManager.getTabCreator(isIncognito).createFrozenTab(
                    tabState, tabToRestore.id, restoredIndex);
        } else {
            if (NewTabPage.isNTPUrl(tabToRestore.url) && !setAsActive && !tabToRestore.fromMerge) {
                Log.i(TAG, "Skipping restore of non-selected NTP.");
                return;
            }

            Log.w(TAG, "Failed to restore TabState; creating Tab with last known URL.");
            Tab fallbackTab = mTabCreatorManager.getTabCreator(isIncognito).createNewTab(
                    new LoadUrlParams(tabToRestore.url), TabModel.TabLaunchType.FROM_RESTORE, null);

            if (fallbackTab == null) return;

            tabId = fallbackTab.getId();
            model.moveTab(tabId, restoredIndex);
        }

        // If the tab is being restored from a merge and its index is 0, then the model being
        // merged into doesn't contain any tabs. Select the first tab to avoid having no tab
        // selected. TODO(twellington): The first tab will always be selected. Instead, the tab that
        // was selected in the other model before the merge should be selected after the merge.
        if (setAsActive || (tabToRestore.fromMerge && restoredIndex == 0)) {
            boolean wasIncognitoTabModelSelected = mTabModelSelector.isIncognitoSelected();
            int selectedModelTabCount = mTabModelSelector.getCurrentModel().getCount();

            TabModelUtils.setIndex(model, TabModelUtils.getTabIndexById(model, tabId));
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

        AsyncTask.SERIAL_EXECUTOR.execute(new Runnable() {
            @Override
            public void run() {
                File[] baseStateFiles = getOrCreateBaseStateDirectory().listFiles();
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
        if (tab == null) return;
        if (mTabsToSave.contains(tab) || !tab.isTabStateDirty() || isTabUrlContentScheme(tab)) {
            return;
        }

        if (NewTabPage.isNTPUrl(tab.getUrl()) && !tab.canGoBack() && !tab.canGoForward()) {
            return;
        }
        mTabsToSave.addLast(tab);
    }

    public void removeTabFromQueues(Tab tab) {
        mTabsToSave.remove(tab);
        mTabsToRestore.remove(getTabToRestoreById(tab.getId()));

        if (mLoadTabTask != null && mLoadTabTask.mTabToRestore.id == tab.getId()) {
            mLoadTabTask.cancel(false);
            mLoadTabTask = null;
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
        mPersistencePolicy.destroy();
        if (mLoadTabTask != null) mLoadTabTask.cancel(true);
        mTabsToSave.clear();
        mTabsToRestore.clear();
        if (mSaveTabTask != null) mSaveTabTask.cancel(false);
        if (mSaveListTask != null) mSaveListTask.cancel(true);
    }

    private void cleanupPersistentData(int id, boolean incognito) {
        deleteFileAsync(TabState.getTabStateFilename(id, incognito));
        // No need to forward that event to the tab content manager as this is already
        // done as part of the standard tab removal process.
    }

    private TabModelSelectorMetadata serializeTabMetadata() throws IOException {
        List<TabRestoreDetails> tabsToRestore = new ArrayList<>();

        // The metadata file may be being written out before all of the Tabs have been restored.
        // Save that information out, as well.
        if (mLoadTabTask != null) tabsToRestore.add(mLoadTabTask.mTabToRestore);
        for (TabRestoreDetails details : mTabsToRestore) {
            tabsToRestore.add(details);
        }

        return serializeTabModelSelector(mTabModelSelector, tabsToRestore);
    }

    /**
     * Serializes {@code selector} to a byte array, copying out the data pertaining to tab ordering
     * and selected indices.
     * @param selector          The {@link TabModelSelector} to serialize.
     * @param tabsBeingRestored Tabs that are in the process of being restored.
     * @return                  {@link TabModelSelectorMetadata} containing the meta data and
     * serialized state of {@code selector}.
     */
    @VisibleForTesting
    public static TabModelSelectorMetadata serializeTabModelSelector(TabModelSelector selector,
            List<TabRestoreDetails> tabsBeingRestored) throws IOException {
        ThreadUtils.assertOnUiThread();

        TabModel incognitoModel = selector.getModel(true);
        TabModelMetadata incognitoInfo = new TabModelMetadata(incognitoModel.index());
        for (int i = 0; i < incognitoModel.getCount(); i++) {
            incognitoInfo.ids.add(incognitoModel.getTabAt(i).getId());
            incognitoInfo.urls.add(incognitoModel.getTabAt(i).getUrl());
        }

        TabModel normalModel = selector.getModel(false);
        TabModelMetadata normalInfo = new TabModelMetadata(normalModel.index());
        for (int i = 0; i < normalModel.getCount(); i++) {
            normalInfo.ids.add(normalModel.getTabAt(i).getId());
            normalInfo.urls.add(normalModel.getTabAt(i).getUrl());
        }

        // Cache the active tab id to be pre-loaded next launch.
        int activeTabId = Tab.INVALID_TAB_ID;
        int activeIndex = normalModel.index();
        if (activeIndex != TabList.INVALID_TAB_INDEX) {
            activeTabId = normalModel.getTabAt(activeIndex).getId();
        }
        // Always override the existing value in case there is no active tab.
        ContextUtils.getAppSharedPreferences().edit().putInt(
                PREF_ACTIVE_TAB_ID, activeTabId).apply();

        byte[] listData = serializeMetadata(normalInfo, incognitoInfo, tabsBeingRestored);
        return new TabModelSelectorMetadata(listData, normalInfo, incognitoInfo, tabsBeingRestored);
    }

    /**
     * Serializes data from a {@link TabModelSelector} into a byte array.
     * @param standardInfo      Info about the regular {@link TabModel}.
     * @param incognitoInfo     Info about the Incognito {@link TabModel}.
     * @param tabsBeingRestored Tabs that are in the process of being restored.
     * @return                  {@code byte[]} containing the serialized state of {@code selector}.
     */
    public static byte[] serializeMetadata(TabModelMetadata standardInfo,
            TabModelMetadata incognitoInfo, @Nullable List<TabRestoreDetails> tabsBeingRestored)
            throws IOException {
        ThreadUtils.assertOnUiThread();

        int standardCount = standardInfo.ids.size();
        int incognitoCount = incognitoInfo.ids.size();

        // Determine how many Tabs there are, including those not yet been added to the TabLists.
        int numAlreadyLoaded = incognitoCount + standardCount;
        int numStillBeingLoaded = tabsBeingRestored == null ? 0 : tabsBeingRestored.size();
        int numTabsTotal = numStillBeingLoaded + numAlreadyLoaded;

        // Save the index file containing the list of tabs to restore.
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream stream = new DataOutputStream(output);
        stream.writeInt(SAVED_STATE_VERSION);
        stream.writeInt(numTabsTotal);
        stream.writeInt(incognitoCount);
        stream.writeInt(incognitoInfo.index);
        stream.writeInt(standardInfo.index + incognitoCount);
        Log.d(TAG, "Serializing tab lists; counts: " + standardCount
                + ", " + incognitoCount
                + ", " + (tabsBeingRestored == null ? 0 : tabsBeingRestored.size()));

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

        // Write out information about the tabs that haven't finished being loaded.
        // We shouldn't have to worry about Tab duplication because the tab details are processed
        // only on the UI Thread.
        if (tabsBeingRestored != null) {
            for (TabRestoreDetails details : tabsBeingRestored) {
                stream.writeInt(details.id);
                stream.writeUTF(details.url);
            }
        }

        stream.close();
        return output.toByteArray();
    }

    private void saveListToFile(byte[] listData) {
        if (Arrays.equals(mLastSavedMetadata, listData)) return;

        saveListToFile(getStateDirectory(), mPersistencePolicy.getStateFileName(), listData);
        mLastSavedMetadata = listData;
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordHistogram.recordCountHistogram(
                    "Android.TabPersistentStore.MetadataFileSize", listData.length);
        }
    }

    /**
     * Atomically writes the given serialized data out to disk.
     * @param stateDirectory Directory to save TabModel data into.
     * @param stateFileName  File name to save TabModel data into.
     * @param listData       TabModel data in the form of a serialized byte array.
     */
    public static void saveListToFile(File stateDirectory, String stateFileName, byte[] listData) {
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
                    observer.onDetailsRead(
                            index, id, url, isStandardActiveIndex, isIncognitoActiveIndex);
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
        if (mPreferences.getBoolean(PREF_HAS_COMPUTED_MAX_ID, false)) return;

        int maxId = 0;
        // Calculation of the max tab ID is done only once per user and is stored in
        // SharedPreferences afterwards.  This is done on the UI thread because it is on the
        // critical patch to initializing the TabIdManager with the correct max tab ID.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            File[] subDirectories = getOrCreateBaseStateDirectory().listFiles();
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
                                TabState.parseInfoFromFilename(file.getName());
                        if (tabStateInfo != null) {
                            maxId = Math.max(maxId, tabStateInfo.first);
                        } else if (isStateFile(file.getName())) {
                            DataInputStream stream = null;
                            try {
                                stream = new DataInputStream(
                                        new BufferedInputStream(new FileInputStream(file)));
                                maxId = Math.max(
                                        maxId, readSavedStateFile(stream, null, null, false));
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
        mPreferences.edit().putBoolean(PREF_HAS_COMPUTED_MAX_ID, true).apply();
    }

    /**
     * Extracts the tab information from a given tab state stream.
     *
     * @param stream   The stream pointing to the tab state file to be parsed.
     * @param callback A callback to be streamed updates about the tab state information being read.
     * @param tabIds   A mapping of tab ID to whether the tab is an off the record tab.
     * @param forMerge Whether this state file was read as part of a merge.
     * @return The next available tab ID based on the maximum ID referenced in this state file.
     */
    public static int readSavedStateFile(
            DataInputStream stream, @Nullable OnTabStateReadCallback callback,
            @Nullable SparseBooleanArray tabIds, boolean forMerge) throws IOException {
        if (stream == null) return 0;
        long time = SystemClock.uptimeMillis();
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

        if (forMerge) {
            logExecutionTime("ReadMergedStateTime", time);
            int tabCount = count + ((incognitoCount > 0) ? incognitoCount : 0);
            RecordHistogram.recordLinearCountHistogram(
                    "Android.TabPersistentStore.MergeStateTabCount",
                    tabCount, 1, 200, 200);
        }

        logExecutionTime("ReadSavedStateTime", time);

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
            mSaveTabTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
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
        mSaveListTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
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
            mState = mTab.getState();
        }

        @Override
        protected Void doInBackground() {
            mStateSaved = saveTabState(mId, mEncrypted, mState);
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            if (mDestroyed || isCancelled()) return;
            if (mStateSaved) mTab.setIsTabStateDirty(false);
            mSaveTabTask = null;
            saveNextTab();
        }
    }

    /** Stores meta data about the TabModelSelector which has been serialized to disk. */
    public static class TabModelSelectorMetadata {
        public final byte[] listData;
        public final TabModelMetadata normalModelMetadata;
        public final TabModelMetadata incognitoModelMetadata;
        public final List<TabRestoreDetails> tabsBeingRestored;

        public TabModelSelectorMetadata(byte[] listData, TabModelMetadata normalModelMetadata,
                TabModelMetadata incognitoModelMetadata,
                List<TabRestoreDetails> tabsBeingRestored) {
            this.listData = listData;
            this.normalModelMetadata = normalModelMetadata;
            this.incognitoModelMetadata = incognitoModelMetadata;
            this.tabsBeingRestored = tabsBeingRestored;
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

    private void onStateLoaded() {
        for (TabPersistentStoreObserver observer : mObservers) {
            // mergeState() starts an AsyncTask to call this and this calls
            // onTabStateInitialized which should be called from the UI thread.
            ThreadUtils.runOnUiThread(() -> observer.onStateLoaded());
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
                if (mMergeTabCount != 0) {
                    long timePerTab = (SystemClock.uptimeMillis() - mRestoreMergedTabsStartTime)
                            / mMergeTabCount;
                    RecordHistogram.recordTimesHistogram(
                            "Android.TabPersistentStore.MergeStateTimePerTab",
                            timePerTab,
                            TimeUnit.MILLISECONDS);
                }

                ThreadUtils.postOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // This eventually calls serializeTabModelSelector() which much be called
                        // from the UI thread. #mergeState() starts an async task in the background
                        // that goes through this code path.
                        saveTabListAsynchronously();
                    }
                });
                for (String mergedFileName : new HashSet<String>(mMergedFileNames)) {
                    deleteFileAsync(mergedFileName);
                }
                for (TabPersistentStoreObserver observer : mObservers) observer.onStateMerged();
            }

            cleanUpPersistentData();
            onStateLoaded();
            mLoadTabTask = null;
            Log.d(TAG, "Loaded tab lists; counts: " + mTabModelSelector.getModel(false).getCount()
                    + "," + mTabModelSelector.getModel(true).getCount());
        } else {
            TabRestoreDetails tabToRestore = mTabsToRestore.removeFirst();
            mLoadTabTask = new LoadTabTask(tabToRestore);
            mLoadTabTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
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
                    deleteFileAsync(result.get(i));
                }
            }
        });
    }

    /**
     * File mutations (e.g. saving & deleting) are explicitly serialized to ensure that they occur
     * in the correct order.
     *
     * @param file Name of file under the state directory to be deleted.
     */
    private void deleteFileAsync(final String file) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
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
                return null;
            }
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        // TODO(twellington): delete tab files using the thread pool rather than the serial
        // executor.
    }

    private class LoadTabTask extends AsyncTask<TabState> {
        public final TabRestoreDetails mTabToRestore;

        public LoadTabTask(TabRestoreDetails tabToRestore) {
            mTabToRestore = tabToRestore;
        }

        @Override
        protected TabState doInBackground() {
            if (mDestroyed || isCancelled()) return null;
            try {
                return TabState.restoreTabState(getStateDirectory(), mTabToRestore.id);
            } catch (Exception e) {
                Log.w(TAG, "Unable to read state: " + e);
                return null;
            }
        }

        @Override
        protected void onPostExecute(TabState tabState) {
            if (mDestroyed || isCancelled()) return;

            boolean isIncognito = isIncognitoTabBeingRestored(mTabToRestore, tabState);
            boolean isLoadCancelled = (isIncognito && mCancelIncognitoTabLoads)
                    || (!isIncognito && mCancelNormalTabLoads);
            if (!isLoadCancelled) restoreTab(mTabToRestore, tabState, false);

            loadNextTab();
        }
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
        String url = tab.getUrl();
        return url != null && url.startsWith(UrlConstants.CONTENT_SCHEME);
    }

    /**
     * Determines if a Tab being restored is definitely an Incognito Tab.
     *
     * This function can fail to determine if a Tab is incognito if not enough data about the Tab
     * was successfully saved out.
     *
     * @return True if the tab is definitely Incognito, false if it's not or if it's undecideable.
     */
    private boolean isIncognitoTabBeingRestored(TabRestoreDetails tabDetails, TabState tabState) {
        if (tabState != null) {
            // The Tab's previous state was completely restored.
            return tabState.isIncognito();
        } else if (tabDetails.isIncognito != null) {
            // The TabState couldn't be restored, but we have some information about the tab.
            return tabDetails.isIncognito;
        } else {
            // The tab's type is undecideable.
            return false;
        }
    }

    private AsyncTask<DataInputStream> startFetchTabListTask(
            Executor executor, final String stateFileName) {
        return new AsyncTask<DataInputStream>() {
            @Override
            protected DataInputStream doInBackground() {
                Log.i(TAG, "Starting to fetch tab list for " + stateFileName);
                File stateFile = new File(getStateDirectory(), stateFileName);
                if (!stateFile.exists()) {
                    Log.i(TAG, "State file does not exist.");
                    return null;
                }
                if (LibraryLoader.getInstance().isInitialized()) {
                    RecordHistogram.recordCountHistogram(
                            "Android.TabPersistentStore.MergeStateMetadataFileSize",
                            (int) stateFile.length());
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
                Log.i(TAG, "Finished fetching tab list.");
                return new DataInputStream(new ByteArrayInputStream(data));
            }
        }
                .executeOnExecutor(executor);
    }

    private void startPrefetchActiveTabTask(Executor executor) {
        final int activeTabId = mPreferences.getInt(PREF_ACTIVE_TAB_ID, Tab.INVALID_TAB_ID);
        if (activeTabId == Tab.INVALID_TAB_ID) return;
        mPrefetchActiveTabTask = new AsyncTask<TabState>() {
            @Override
            protected TabState doInBackground() {
                return TabState.restoreTabState(getStateDirectory(), activeTabId);
            }
        }.executeOnExecutor(executor);
    }

    @VisibleForTesting
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
     * Directory containing all data for TabModels.  Each subdirectory stores info about different
     * TabModelSelectors, including metadata about each TabModel and TabStates for each of their
     * tabs.
     *
     * @return The parent state directory.
     */
    @VisibleForTesting
    public static File getOrCreateBaseStateDirectory() {
        return BaseStateDirectoryHolder.sDirectory;
    }

    /**
     * @param uniqueId The ID that uniquely identifies this state file.
     * @return The name of the state file.
     */
    @VisibleForTesting
    public static String getStateFileName(String uniqueId) {
        return TabPersistencePolicy.SAVED_STATE_FILE_PREFIX + uniqueId;
    }

    /**
     * Parses the state file name and returns the unique ID encoded into it.
     * @param stateFileName The state file name to be parsed.
     * @return The unique ID used when generating the file name.
     */
    public static String getStateFileUniqueId(String stateFileName) {
        assert isStateFile(stateFileName);
        return stateFileName.substring(TabPersistencePolicy.SAVED_STATE_FILE_PREFIX.length());
    }

    /**
     * @return Whether the specified filename matches the expected pattern of the tab state files.
     */
    public static boolean isStateFile(String fileName) {
        return fileName.startsWith(TabPersistencePolicy.SAVED_STATE_FILE_PREFIX);
    }

    /**
     * Sets where the base state directory is in tests.
     */
    @VisibleForTesting
    public static void setBaseStateDirectoryForTests(File directory) {
        BaseStateDirectoryHolder.sDirectory = directory;
    }
}
