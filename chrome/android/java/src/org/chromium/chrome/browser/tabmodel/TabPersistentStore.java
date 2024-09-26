// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.SharedPreferences;
import android.os.StrictMode;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.StreamUtil;
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
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.state.PersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabPersistenceFileInfo.TabStateFileInfo;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
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
import java.util.Deque;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ExecutionException;

/** This class handles saving and loading tab state from the persistent storage. */
public class TabPersistentStore {
    public static final String CLIENT_TAG_REGULAR = "Regular";
    public static final String CLIENT_TAG_CUSTOM = "Custom";
    public static final String CLIENT_TAG_ARCHIVED = "Archived";

    private static final String TAG = "tabmodel";
    private static final String TAG_MIGRATION = "fb_migration";

    private static final long INVALID_TIME = -1;

    /**
     * The current version of the saved state file. Version 4: In addition to the tab's ID, save the
     * tab's last URL. Version 5: In addition to the total tab count, save the incognito tab count.
     */
    private static final int SAVED_STATE_VERSION = 5;

    /**
     * The prefix of the name of the file where the metadata is saved. Values returned by {@link
     * #getMetadataFileName(String)} must begin with this prefix.
     */
    @VisibleForTesting static final String SAVED_METADATA_FILE_PREFIX = "tab_state";

    /** Prevents two TabPersistentStores from saving the same file simultaneously. */
    private static final Object SAVE_LIST_LOCK = new Object();

    private static boolean sDeferredStartupComplete;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected static int sMaxMigrationsPerSave = 5;

    private TabModelObserver mTabModelObserver;
    private TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;

    private int mDuplicateTabIdsSeen;

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

    /** Values are recorded in metrics and should not be changed. */
    @IntDef({
        TabRestoreMethod.TAB_STATE,
        TabRestoreMethod.CRITICAL_PERSISTED_TAB_DATA,
        TabRestoreMethod.CREATE_NEW_TAB,
        TabRestoreMethod.FAILED_TO_RESTORE,
        TabRestoreMethod.SKIPPED_NTP,
        TabRestoreMethod.SKIPPED_EMPTY_URL,
        TabRestoreMethod.NUM_ENTRIES
    })
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
        TabStateAttributes.Observer attributesObserver =
                new TabStateAttributes.Observer() {
                    @Override
                    public void onTabStateDirtinessChanged(
                            Tab tab, @TabStateAttributes.DirtinessState int dirtiness) {
                        if (dirtiness == TabStateAttributes.DirtinessState.DIRTY
                                && !tab.isDestroyed()) {
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

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void willCloseAllTabs(boolean incognito) {
                        cancelLoadingTabs(incognito);
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        saveTabListAsynchronously();
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        if (!mTabModelSelector.isIncognitoSelected()) {
                            saveTabListAsynchronously();
                        }
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        saveTabListAsynchronously();
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        saveTabListAsynchronously();
                    }

                    @Override
                    public void didAddTab(
                            Tab tab, int type, int creationState, boolean markedForSelection) {
                        saveTabListAsynchronously();
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        saveTabListAsynchronously();
                    }
                };
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
        mTabModelSelector.getModel(true).addObserver(mTabModelObserver);
    }

    /** Callback interface to use while reading the persisted TabModelSelector info from disk. */
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
        void onDetailsRead(
                int index,
                int id,
                String url,
                Boolean isIncognito,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex);
    }

    /** Alerted at various stages of operation. */
    public abstract static class TabPersistentStoreObserver {
        /**
         * To be called when the file containing the initial information about the TabModels has
         * been loaded.
         * @param tabCountAtStartup How many tabs there are in the TabModels.
         */
        public void onInitialized(int tabCountAtStartup) {}

        /** Called when details about a Tab are read from the metadata file. */
        public void onDetailsRead(
                int index,
                int id,
                String url,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex,
                Boolean isIncognito,
                boolean fromMerge) {}

        /** To be called when the TabStates have all been loaded. */
        public void onStateLoaded() {}

        /** To be called when the TabState from another instance has been merged. */
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

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof TabModelMetadata that)) return false;
            return index == that.index
                    && Objects.equals(ids, that.ids)
                    && Objects.equals(urls, that.urls);
        }

        @Override
        public int hashCode() {
            return Objects.hash(index, ids, urls);
        }
    }

    private final Set<Integer> mSeenTabIds = new HashSet<>();
    private final String mClientTag;
    private final TabPersistencePolicy mPersistencePolicy;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private final TabWindowManager mTabWindowManager;
    private final CipherFactory mCipherFactory;
    private final ObserverList<TabPersistentStoreObserver> mObservers;

    private final Deque<Tab> mTabsToSave;
    private final ArrayDeque<Tab> mTabsToMigrate;
    private final Deque<TabRestoreDetails> mTabsToRestore;
    private final Set<Integer> mTabIdsToRestore;

    private TabLoader mTabLoader;
    private SaveTabTask mSaveTabTask;
    private MigrateTabTask mMigrateTabTask;

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
    private TabModelSelectorMetadata mLastSavedMetadata;

    // Tracks whether this TabPersistentStore's tabs are being loaded.
    private boolean mLoadInProgress;

    private long mTabRestoreStartTime = INVALID_TIME;

    AsyncTask<TabState> mPrefetchTabStateActiveTabTask;

    /**
     * Creates an instance of a TabPersistentStore.
     *
     * @param clientTag The client tag used to record metrics.
     * @param modelSelector The {@link TabModelSelector} to restore to and save from.
     * @param tabCreatorManager The {@link TabCreatorManager} to use.
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     */
    public TabPersistentStore(
            String clientTag,
            TabPersistencePolicy policy,
            TabModelSelector modelSelector,
            TabCreatorManager tabCreatorManager,
            TabWindowManager tabWindowManager,
            CipherFactory cipherFactory) {
        mClientTag = clientTag;
        mPersistencePolicy = policy;
        mTabModelSelector = modelSelector;
        mTabCreatorManager = tabCreatorManager;
        mTabWindowManager = tabWindowManager;
        mCipherFactory = cipherFactory;

        mTabsToSave = new ArrayDeque<>();
        mTabsToMigrate = new ArrayDeque<>();
        mTabsToRestore = new ArrayDeque<>();
        mTabIdsToRestore = new HashSet<>();
        mObservers = new ObserverList<>();
        @TaskTraits int taskTraits = TaskTraits.USER_BLOCKING_MAY_BLOCK;
        mSequencedTaskRunner = PostTask.createSequencedTaskRunner(taskTraits);
        mPrefetchTabListToMergeTasks = new ArrayList<>();
        mMergedFileNames = new HashSet<>();

        assert isMetadataFile(policy.getMetadataFileName()) : "Metadata file name is not valid";
        boolean needsInitialization =
                mPersistencePolicy.performInitialization(mSequencedTaskRunner);

        mPersistencePolicy.setTaskRunner(mSequencedTaskRunner);

        if (mPersistencePolicy.isMergeInProgress()) return;

        // TODO(smaier): We likely can move everything onto the SequencedTaskRunner when the
        //  SERIAL_EXECUTOR path is gone. crbug.com/957735
        TaskRunner taskRunner =
                needsInitialization ? mSequencedTaskRunner : PostTask.createTaskRunner(taskTraits);

        mPrefetchTabListTask =
                startFetchTabListTask(taskRunner, mPersistencePolicy.getMetadataFileName());
        startPrefetchActiveTabTask(taskRunner);

        if (mPersistencePolicy.shouldMergeOnStartup()) {
            String mergedFileName = mPersistencePolicy.getMetadataFileNameToBeMerged();
            assert mergedFileName != null;
            AsyncTask<DataInputStream> task = startFetchTabListTask(taskRunner, mergedFileName);
            mPrefetchTabListToMergeTasks.add(Pair.create(task, mergedFileName));
        }
    }

    /** Waits for the task that migrates all state files to their new location to finish. */
    @VisibleForTesting
    public void waitForMigrationToFinish() {
        mPersistencePolicy.waitForInitializationToFinish();
    }

    public void saveState() {
        // Temporarily allowing disk access. TODO: Fix. See http://b/5518024
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            // Clear out any in-flight migration.
            if (mMigrateTabTask != null) {
                if (mMigrateTabTask.cancel(false) && !mMigrateTabTask.mMigrationComplete) {
                    // The task was successfully cancelled. Re-add Tab to migration queue.
                    Tab cancelledTab = mMigrateTabTask.mTab;
                    mTabsToMigrate.addFirst(cancelledTab);
                }
                mMigrateTabTask = null;
            }
            // Don't want any new save below to trigger new migrations which are unnecessary. Only
            // want to update any migrations for which Tabs have already migrated (so the
            // migrated TabState file is not out of date, which would lead to an old snapshot
            // of the Tab being restored upon restart). If the Tab hasn't migrated yet,
            // the legacy TabState file will be used upon a restart.
            ArrayDeque<Tab> tabsToMigrateCopy = mTabsToMigrate.clone();
            mTabsToMigrate.clear();

            // The list of tabs should be saved first in case our activity is terminated early.
            // Explicitly toss out any existing SaveListTask because they only save the TabModel as
            // it looked when the SaveListTask was first created.
            if (mSaveListTask != null) mSaveListTask.cancel(true);
            try {
                saveListToFile(saveTabMetadata());
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
                                getStateDirectory(), state, id, incognito, mCipherFactory);
                        if (isFlatBufferSchemaEnabled()
                                && TabStateFileManager.isMigrated(
                                        getStateDirectory(), id, incognito)) {
                            // Ensure parity between the FlatBuffer TabState file and legacy.
                            // Otherwise if the user restarts and is in the experiment, they may
                            // have the Tab restored using an out of date FlatBuffer file.
                            TabStateFileManager.migrateTabState(
                                    getStateDirectory(), state, id, incognito, mCipherFactory);
                            // No longer need to migrate the Tab as it was just migrated.
                            tabsToMigrateCopy.remove(tab);
                        }
                    }
                } catch (OutOfMemoryError e) {
                    Log.e(TAG, "Out of memory error while attempting to save tab state.  Erasing.");
                    deleteTabState(id, incognito);
                    if (isFlatBufferSchemaEnabled()) {
                        TabStateFileManager.deleteMigratedFile(getStateDirectory(), id, incognito);
                    }
                }
            }
            // Now all pending saves (and migrations, if applicable) are complete we are ok to
            // resume any migrations which would be triggered by another Tab save.
            for (Tab tab : tabsToMigrateCopy) {
                mTabsToMigrate.add(tab);
            }
            updateMigratedFiles();
            mTabsToSave.clear();
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Migrate any Tabs to the TabState FlatBuffer file which have a FlatBuffer file already
     * written. Otherwise if the user restarts in the experiment they may have their Tab restored
     * using an out of date FlatBuffer file.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void updateMigratedFiles() {
        List<Tab> updatedMigrations = new LinkedList<>();
        for (Tab tab : mTabsToMigrate) {
            int id = tab.getId();
            boolean incognito = tab.isIncognito();
            if (TabStateFileManager.isMigrated(getStateDirectory(), id, incognito)) {
                try {
                    TabState state = TabStateExtractor.from(tab);
                    if (state != null) {
                        TabStateFileManager.migrateTabState(
                                getStateDirectory(), state, id, incognito, mCipherFactory);
                        updatedMigrations.add(tab);
                    }
                } catch (OutOfMemoryError e) {
                    Log.e(
                            TAG,
                            "Out of memory error while attempting to update Migrated TabState file."
                                    + "  Erasing.");
                    TabStateFileManager.deleteMigratedFile(getStateDirectory(), id, incognito);
                }
            }
        }
        // No longer need to migrate Tabs which were just migrated.
        for (Tab migratedTab : updatedMigrations) {
            mTabsToMigrate.remove(migratedTab);
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
            mTabRestoreStartTime = SystemClock.elapsedRealtime();
            assert mTabModelSelector.getModel(true).getCount() == 0;
            assert mTabModelSelector.getModel(false).getCount() == 0;
            checkAndUpdateMaxTabId();
            DataInputStream stream;
            if (mPrefetchTabListTask != null) {
                mTabRestoreStartTime = SystemClock.elapsedRealtime();
                stream = mPrefetchTabListTask.get();

                // Restore the tabs for this TabPersistentStore instance if the tab metadata file
                // exists.
                if (stream != null) {
                    mLoadInProgress = true;
                    readSavedMetadataFile(
                            stream,
                            createOnTabStateReadCallback(
                                    mTabModelSelector.isIncognitoSelected(), false),
                            null);
                } else {
                    mTabRestoreStartTime = INVALID_TIME;
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
                    readSavedMetadataFile(
                            stream,
                            createOnTabStateReadCallback(
                                    mTabModelSelector.isIncognitoSelected(),
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
            mTabRestoreStartTime = INVALID_TIME;
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
        if (mLoadInProgress
                || mPersistencePolicy.isMergeInProgress()
                || !mTabsToRestore.isEmpty()) {
            Log.d(TAG, "Tab load still in progress when merge was attempted.");
            return;
        }

        // Initialize variables.
        initializeRestoreVars(false);

        try {
            // Read the tab state metadata file.
            String mergeFileName = mPersistencePolicy.getMetadataFileNameToBeMerged();
            assert mergeFileName != null
                    : "mergeState called when no metadata file to be merged exists.";
            DataInputStream stream =
                    startFetchTabListTask(mSequencedTaskRunner, mergeFileName).get();
            if (stream != null) {
                mMergedFileNames.add(mergeFileName);
                mPersistencePolicy.setMergeInProgress(true);
                readSavedMetadataFile(
                        stream,
                        createOnTabStateReadCallback(mTabModelSelector.isIncognitoSelected(), true),
                        null);
            }
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing app.
            Log.d(TAG, "mergeState exception: " + e.toString(), e);
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
                loadNextTab(); // Queue up async task to load next tab after we're done here.
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
        // This will no longer be necessary when the TabState schema is replaced with
        // a FlatBuffer approach - go/tabstate-flatbuffer-decision.
        try {
            int restoredTabId =
                    ChromeSharedPreferences.getInstance()
                            .readInt(
                                    ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID,
                                    Tab.INVALID_TAB_ID);
            @Nullable TabState state = maybeRestoreTabState(restoredTabId, tabToRestore);
            restoreTab(tabToRestore, state, setAsActive);
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing the app
            // at startup.
            Log.i(TAG, "loadTabs exception: " + e.toString(), e);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    private @Nullable TabState maybeRestoreTabState(
            int restoredTabId, TabRestoreDetails tabToRestore)
            throws InterruptedException, ExecutionException {
        // If the Tab being restored is the Active Tab and the corresponding TabState prefetch
        // was initiated, use the prefetch result.
        if (restoredTabId == tabToRestore.id && mPrefetchTabStateActiveTabTask != null) {
            return mPrefetchTabStateActiveTabTask.get();
        }
        // Necessary to do on the UI thread as a last resort.
        return TabStateFileManager.restoreTabState(
                getStateDirectory(), tabToRestore.id, mCipherFactory);
    }

    /**
     * Handles restoring an individual tab.
     *
     * @param tabToRestore Meta data about the tab to be restored.
     * @param tabState The previously serialized state of the tab to be restored.
     * @param setAsActive Whether the tab should be set as the active tab as part of the restoration
     *     process.
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
                boolean isNtp = UrlUtilities.isNtpUrl(tabToRestore.url);
                boolean isNtpFromMerge = isNtp && tabToRestore.fromMerge;

                if (!isNtpFromMerge && (!isNtp || !setAsActive || mCancelIncognitoTabLoads)) {
                    Log.i(
                            TAG,
                            "Failed to restore Incognito tab: its TabState could not be restored.");
                    return;
                }
            }
        }
        TabModel model = mTabModelSelector.getModel(isIncognito);

        if (model.isIncognito() != isIncognito) {
            throw new IllegalStateException(
                    "Incognito state mismatch. Restored tab state: "
                            + isIncognito
                            + ". Model: "
                            + model.isIncognito());
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
        if (ChromeFeatureList.sAndroidTabDeclutterDedupeTabIdsKillSwitch.isEnabled()
                && mSeenTabIds.contains(tabId)) {
            mDuplicateTabIdsSeen++;
            return;
        }

        if (tabState != null) {
            if (tabState.contentsState != null) {
                tabState.contentsState.setFallbackUrlForRestorationFailure(tabToRestore.url);
            }

            @TabRestoreMethod int tabRestoreMethod = TabRestoreMethod.TAB_STATE;
            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.TabRestoreMethod", tabRestoreMethod, TabRestoreMethod.NUM_ENTRIES);
            Tab tab =
                    mTabCreatorManager
                            .getTabCreator(isIncognito)
                            .createFrozenTab(tabState, tabToRestore.id, restoredIndex);
            if (tabState.shouldMigrate) {
                mTabsToMigrate.add(tab);
            }

            if (tab != null) {
                RecordHistogram.recordBooleanHistogram(
                        "Tabs.TabRestoreUrlMatch", tabToRestore.url.equals(tab.getUrl().getSpec()));
            }

            mSeenTabIds.add(tabId);
        } else {
            Log.w(TAG, "Failed to restore TabState; creating Tab with last known URL.");
            Tab fallbackTab =
                    mTabCreatorManager
                            .getTabCreator(isIncognito)
                            .createNewTab(
                                    new LoadUrlParams(tabToRestore.url),
                                    TabLaunchType.FROM_RESTORE,
                                    null,
                                    restoredIndex);

            if (fallbackTab == null) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Tabs.TabRestoreMethod",
                        TabRestoreMethod.FAILED_TO_RESTORE,
                        TabRestoreMethod.NUM_ENTRIES);
                return;
            }

            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.TabRestoreMethod",
                    TabRestoreMethod.CREATE_NEW_TAB,
                    TabRestoreMethod.NUM_ENTRIES);

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

        mSequencedTaskRunner.execute(
                new Runnable() {
                    @Override
                    public void run() {
                        File[] baseStateFiles =
                                TabStateDirectory.getOrCreateBaseStateDirectory().listFiles();
                        if (baseStateFiles == null) return;
                        for (File baseStateFile : baseStateFiles) {
                            // In legacy scenarios (prior to migration, state files could reside in
                            // the
                            // root state directory.  So, handle deleting direct child files as well
                            // as those that reside in sub directories.
                            if (!baseStateFile.isDirectory()) {
                                if (!baseStateFile.delete()) {
                                    Log.e(TAG, "Failed to delete file: " + baseStateFile);
                                }
                            } else {
                                File[] files = baseStateFile.listFiles();
                                if (files == null) continue;
                                for (File file : files) {
                                    if (!file.delete()) {
                                        Log.e(TAG, "Failed to delete file: " + file);
                                    }
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
        TabStateAttributes tabStateAttributes = TabStateAttributes.from(tab);
        @TabStateAttributes.DirtinessState
        int dirtinessState = tabStateAttributes.getDirtinessState();
        if (mTabsToSave.contains(tab)
                || dirtinessState == TabStateAttributes.DirtinessState.CLEAN) {
            return;
        }

        if (mSaveTabTask != null && mSaveTabTask.mId == tab.getId()) {
            RecordHistogram.recordCount100Histogram(
                    "Tabs.PotentialDoubleDirty.SaveQueueSize", mTabsToSave.size());
        }

        mTabsToSave.addLast(tab);
    }

    public void removeTabFromQueues(Tab tab) {
        mTabsToSave.remove(tab);
        mTabsToRestore.remove(getTabToRestoreById(tab.getId()));
        mTabsToMigrate.remove(tab);

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

        if (mMigrateTabTask != null && mMigrateTabTask.mId == tab.getId()) {
            mMigrateTabTask.cancel(false);
            int nextNumMigration = mMigrateTabTask.mNumMigration + 1;
            mMigrateTabTask = null;
            migrateNextTabIfApplicable(nextNumMigration);
        }

        // If the tab can't be found in any selector, then cleanup it's data.
        if (mTabWindowManager.canTabStateBeDeleted(tab.getId())) {
            cleanupPersistentData(tab.getId(), tab.isIncognito());
        }
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
        TabStateFileManager.deleteAsync(getStateDirectory(), id, incognito);
        // No need to forward that event to the tab content manager as this is already
        // done as part of the standard tab removal process.
    }

    private TabModelSelectorMetadata saveTabMetadata() throws IOException {
        List<TabRestoreDetails> tabsToRestore = new ArrayList<>();

        // The metadata file may be being written out before all of the Tabs have been restored.
        // Save that information out, as well.
        if (mTabLoader != null) tabsToRestore.add(mTabLoader.mTabToRestore);
        for (TabRestoreDetails details : mTabsToRestore) {
            tabsToRestore.add(details);
        }

        return saveTabModelSelectorMetadata(mTabModelSelector, tabsToRestore);
    }

    /**
     * Records state of {@code selector} into a separate DataStructure to be used for save/restore.
     *
     * @param selector The {@link TabModelSelector} to process.
     * @param tabsBeingRestored Tabs that are in the process of being restored.
     * @return {@link TabModelSelectorMetadata} containing the meta data of {@code selector}.
     */
    @VisibleForTesting
    public static TabModelSelectorMetadata saveTabModelSelectorMetadata(
            TabModelSelector selector, List<TabRestoreDetails> tabsBeingRestored)
            throws IOException {
        ThreadUtils.assertOnUiThread();

        // TODO(crbug.com/40549331): Convert TabModelMetadata to use GURL.
        TabModelMetadata incognitoInfo = metadataFromModel(selector, true);

        TabModel normalModel = selector.getModel(false);
        TabModelMetadata normalInfo = metadataFromModel(selector, false);

        // Cache the active tab id to be pre-loaded next launch.
        int activeTabId = Tab.INVALID_TAB_ID;
        int activeIndex = normalModel.index();
        @ActiveTabState int activeTabState = ActiveTabState.EMPTY;
        if (activeIndex != TabList.INVALID_TAB_INDEX) {
            Tab activeTab = normalModel.getTabAt(activeIndex);
            activeTabId = activeTab.getId();
            activeTabState =
                    UrlUtilities.isNtpUrl(activeTab.getUrl())
                            ? ActiveTabState.NTP
                            : ActiveTabState.OTHER;
        }

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

        Log.d(
                TAG,
                "Recording tab lists; counts: "
                        + normalInfo.ids.size()
                        + ", "
                        + incognitoInfo.ids.size());

        saveTabModelPrefs(normalInfo, incognitoInfo, activeTabId, activeTabState);
        return new TabModelSelectorMetadata(normalInfo, incognitoInfo);
    }

    @VisibleForTesting
    public static void saveTabModelPrefs(
            TabModelMetadata normalInfo,
            TabModelMetadata incognitoInfo,
            int activeTabId,
            int activeTabState) {
        // Always override the existing value in case there is no active tab.
        SharedPreferences.Editor editor = ChromeSharedPreferences.getInstance().getEditor();
        editor.putInt(ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, activeTabId);
        editor.putInt(ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, activeTabState);
        editor.apply();
    }

    /**
     * Creates a TabModelMetadata for the given TabModel mode.
     *
     * @param selector The object of {@link TabModelSelector}
     * @param isIncognito Whether the TabModel is incognito.
     */
    private static TabModelMetadata metadataFromModel(
            TabModelSelector selector, boolean isIncognito) {
        TabModel tabModel = selector.getModel(isIncognito);
        TabModelMetadata modelInfo = new TabModelMetadata(tabModel.index());

        int activeIndex = tabModel.index();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            // This tab has likely just been deleted, and it's possible we're being notified before
            // hand because undo is not allowed. This shouldn't be persisted.
            if (tab.isClosing()) {
                // Select the previous tab if there is one. 0 should be fine even if there are no
                // tabs left.
                if (i == activeIndex) {
                    modelInfo.index = Math.max(0, modelInfo.ids.size() - 1);
                }
                continue;
            }

            if (i == activeIndex) {
                // If any non-active NTPs have been skipped, the serialized tab model index
                // needs to be adjusted.
                modelInfo.index = modelInfo.ids.size();
            } else if (shouldSkipTab(tab)) {
                continue;
            }
            modelInfo.ids.add(tab.getId());
            modelInfo.urls.add(tab.getUrl().getSpec());
        }
        return modelInfo;
    }

    private static boolean shouldSkipTab(@NonNull Tab tab) {
        boolean isNtp = tab.isNativePage() && UrlUtilities.isNtpUrl(tab.getUrl());
        if (!isNtp) return false;

        // Only skip NTP tabs that are not in a tab group.
        return tab.getTabGroupId() == null;
    }

    private void saveListToFile(TabModelSelectorMetadata listData) {
        if (Objects.equals(mLastSavedMetadata, listData)) return;
        // Save the index file containing the list of tabs to restore.
        File metadataFile = new File(getStateDirectory(), mPersistencePolicy.getMetadataFileName());
        saveListToFile(metadataFile, listData);
        mLastSavedMetadata = listData;
    }

    /**
     * Atomically writes the given tab model selector data to disk.
     *
     * @param metadataFile File to save TabModel data into.
     * @param metadata TabModel data in copied types.
     */
    public static void saveListToFile(File metadataFile, TabModelSelectorMetadata metadata) {
        synchronized (SAVE_LIST_LOCK) {
            AtomicFile file = new AtomicFile(metadataFile);
            FileOutputStream output = null;
            try {
                output = file.startWrite();

                int standardCount = metadata.normalModelMetadata.ids.size();
                int incognitoCount = metadata.incognitoModelMetadata.ids.size();

                // Determine how many Tabs there are.
                int numTabsTotal = incognitoCount + standardCount;
                Log.d(TAG, "Persisting tab lists; " + standardCount + ", " + incognitoCount);

                // Save the index file containing the list of tabs to restore. Wrap a
                // BufferedOutputStream to batch/buffer actual writes. Most urls are far smaller
                // than the 8K buffer.
                DataOutputStream stream = new DataOutputStream(new BufferedOutputStream(output));
                stream.writeInt(SAVED_STATE_VERSION);
                stream.writeInt(numTabsTotal);
                stream.writeInt(incognitoCount);
                stream.writeInt(metadata.incognitoModelMetadata.index);
                stream.writeInt(metadata.normalModelMetadata.index + incognitoCount);

                // Save incognito state first, so when we load, if the incognito files are
                // unreadable
                // we can fall back easily onto the standard selected tab.
                for (int i = 0; i < incognitoCount; i++) {
                    stream.writeInt(metadata.incognitoModelMetadata.ids.get(i));
                    stream.writeUTF(metadata.incognitoModelMetadata.urls.get(i));
                }
                for (int i = 0; i < standardCount; i++) {
                    stream.writeInt(metadata.normalModelMetadata.ids.get(i));
                    stream.writeUTF(metadata.normalModelMetadata.urls.get(i));
                }

                stream.flush();
                file.finishWrite(output);

            } catch (IOException e) {
                if (output != null) file.failWrite(output);
            }
        }
    }

    /**
     * @param isIncognitoSelected Whether the tab model is incognito.
     * @return A callback for reading data from tab models.
     */
    private OnTabStateReadCallback createOnTabStateReadCallback(
            final boolean isIncognitoSelected, final boolean fromMerge) {
        return new OnTabStateReadCallback() {
            @Override
            public void onDetailsRead(
                    int index,
                    int id,
                    String url,
                    Boolean isIncognito,
                    boolean isStandardActiveIndex,
                    boolean isIncognitoActiveIndex) {
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

                if (!fromMerge
                        && ((isIncognitoActiveIndex && isIncognitoSelected)
                                || (isStandardActiveIndex && !isIncognitoSelected))) {
                    // Active tab gets loaded first
                    mTabsToRestore.addFirst(details);
                } else {
                    mTabsToRestore.addLast(details);
                }

                for (TabPersistentStoreObserver observer : mObservers) {
                    observer.onDetailsRead(
                            index,
                            id,
                            url,
                            isStandardActiveIndex,
                            isIncognitoActiveIndex,
                            isIncognito,
                            fromMerge);
                }
            }
        };
    }

    /**
     * If a global max tab ID has not been computed and stored before, then check all the state
     * folders and calculate a new global max tab ID to be used. Must be called before any new tabs
     * are created.
     */
    private void checkAndUpdateMaxTabId() throws IOException {
        if (ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.TABMODEL_HAS_COMPUTED_MAX_ID, false)) {
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
                        } else if (isMetadataFile(file.getName())) {
                            DataInputStream stream = null;
                            try {
                                stream =
                                        new DataInputStream(
                                                new BufferedInputStream(new FileInputStream(file)));
                                maxId = Math.max(maxId, readSavedMetadataFile(stream, null, null));
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
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TABMODEL_HAS_COMPUTED_MAX_ID, true);
    }

    /**
     * Extracts the tab information from a given tab state metadata stream.
     *
     * @param stream The stream pointing to the tab state metadata file to be parsed.
     * @param callback A callback to be streamed updates about the tab state information being read.
     * @param tabIds A mapping of tab ID to whether the tab is an off the record tab.
     * @return The next available tab ID based on the maximum ID referenced in this state file.
     */
    public static int readSavedMetadataFile(
            DataInputStream stream,
            @Nullable OnTabStateReadCallback callback,
            @Nullable SparseBooleanArray tabIds)
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
        int standardActiveIndex = stream.readInt();
        if (standardActiveIndex < incognitoCount) {
            // See https://crbug.com/354041918. This is equal to the original standard active index
            // + incognitoCount. If there are not standard tabs, that would be -1 + incognitoCount,
            // which unexpectedly maps to the last incognito tab. Adjust here.
            standardActiveIndex = TabModel.INVALID_TAB_INDEX;
        }
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
                callback.onDetailsRead(
                        i,
                        id,
                        tabUrl,
                        isIncognito,
                        i == standardActiveIndex,
                        i == incognitoActiveIndex);
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
            // Ensure Tab is moved to the front of the migration queue to ensure the two versions
            // of the TabState file are kept in sync.
            mTabsToMigrate.remove(tab);
            mTabsToMigrate.addFirst(tab);
            migrateNextTabIfApplicable(1);
        } else {
            saveTabListAsynchronously();
        }
    }

    private void migrateNextTabIfApplicable(int numMigration) {
        // Only migrate TabState to FlatBuffer format if:
        // - FlatBuffer schema flag is enabled
        // - We haven't hit the limit of sMaxMigrationsPerSave migrations per save yet
        // - Deferred startup is complete (to reduce the risk of Jank).
        if (!isFlatBufferSchemaEnabled()
                || mTabsToMigrate.isEmpty()
                || numMigration > sMaxMigrationsPerSave
                || !sDeferredStartupComplete) {
            return;
        }
        Tab tab = mTabsToMigrate.removeFirst();
        mMigrateTabTask = new MigrateTabTask(tab, numMigration);
        mMigrateTabTask.executeOnTaskRunner(mSequencedTaskRunner);
    }

    /** Kick off an AsyncTask to save the current list of Tabs. */
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
            TabStateAttributes.from(mTab).clearTabStateDirtiness();
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
            mSaveTabTask = null;
            saveNextTab();
        }
    }

    /** Migrate Tab to new FlatBuffer format. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    class MigrateTabTask extends AsyncTask<Void> {
        Tab mTab;
        int mId;
        TabState mState;
        boolean mEncrypted;
        int mNumMigration;
        boolean mMigrationComplete;

        MigrateTabTask(Tab tab, int numMigration) {
            mTab = tab;
            mId = tab.getId();
            mEncrypted = tab.isIncognito();
            mNumMigration = numMigration;
        }

        @Override
        protected void onPreExecute() {
            if (mDestroyed || mTab.isDestroyed() || isCancelled()) return;
            try {
                mState = TabStateExtractor.from(mTab);
            } catch (Exception e) {
                Log.d(TAG_MIGRATION, "Error MigrateTabTask#onPreExecute", e);
                throw e;
            }
        }

        @Override
        protected Void doInBackground() {
            try {
                mMigrationComplete =
                        TabStateFileManager.migrateTabState(
                                getStateDirectory(), mState, mId, mEncrypted, mCipherFactory);
            } catch (Exception e) {
                Log.d(TAG_MIGRATION, "Error MigrateTabTask#doInBackground", e);
                throw e;
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            if (mDestroyed || isCancelled()) return;
            mMigrateTabTask = null;
            migrateNextTabIfApplicable(mNumMigration + 1);
        }
    }

    /** Stores meta data about the TabModelSelector which can be serialized to disk. */
    public static class TabModelSelectorMetadata {
        public final TabModelMetadata normalModelMetadata;
        public final TabModelMetadata incognitoModelMetadata;

        public TabModelSelectorMetadata(
                TabModelMetadata normalModelMetadata, TabModelMetadata incognitoModelMetadata) {
            this.normalModelMetadata = normalModelMetadata;
            this.incognitoModelMetadata = incognitoModelMetadata;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof TabModelSelectorMetadata that)) return false;
            return Objects.equals(normalModelMetadata, that.normalModelMetadata)
                    && Objects.equals(incognitoModelMetadata, that.incognitoModelMetadata);
        }

        @Override
        public int hashCode() {
            return Objects.hash(normalModelMetadata, incognitoModelMetadata);
        }
    }

    private class SaveListTask extends AsyncTask<Void> {
        TabModelSelectorMetadata mMetadata;

        @Override
        protected void onPreExecute() {
            if (mDestroyed || isCancelled()) return;
            try {
                mMetadata = saveTabMetadata();
            } catch (IOException e) {
                mMetadata = null;
            }
        }

        @Override
        protected Void doInBackground() {
            if (mMetadata == null || isCancelled()) return null;
            saveListToFile(mMetadata);
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
        return TabStateFileManager.getTabStateFile(
                getStateDirectory(), tabId, encrypted, /* isFlatBuffer= */ false);
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
            TabStateFileManager.saveState(
                    getStateDirectory(), state, tabId, encrypted, mCipherFactory);
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
     *
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
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT,
                        new Runnable() {
                            @Override
                            public void run() {
                                // This eventually calls saveTabModelSelectorMetadata() which much
                                // be called from the UI thread. #mergeState() starts an async task
                                // in the background that goes through this code path.
                                saveTabListAsynchronously();
                            }
                        });
                for (String mergedFileName : new HashSet<>(mMergedFileNames)) {
                    deleteFileAsync(mergedFileName, true);
                }
                for (TabPersistentStoreObserver observer : mObservers) observer.onStateMerged();
            }

            recordLegacyTabCountMetrics();
            recordTabCountMetrics();
            recordRestoreDuration();
            cleanUpPersistentData();
            onStateLoaded();
            mTabLoader = null;
            Log.d(
                    TAG,
                    "Loaded tab lists; counts: "
                            + mTabModelSelector.getModel(false).getCount()
                            + ","
                            + mTabModelSelector.getModel(true).getCount());

            // If there were any duplicate tab ids seen, then force a write to overwrite tab ids.
            if (ChromeFeatureList.sAndroidTabDeclutterDedupeTabIdsKillSwitch.isEnabled()
                    && mDuplicateTabIdsSeen > 0) {
                recordDuplicateTabIdMetrics();
                saveState();
            }
        } else {
            TabRestoreDetails tabToRestore = mTabsToRestore.removeFirst();
            mTabLoader = new TabLoader(tabToRestore);
            mTabLoader.load();
        }
    }

    private void recordDuplicateTabIdMetrics() {
        RecordHistogram.recordCount1000Histogram(
                "Tabs.Startup.TabCount2." + mClientTag + ".DuplicateTabIds", mDuplicateTabIdsSeen);
    }

    protected void recordLegacyTabCountMetrics() {
        RecordHistogram.recordCount1MHistogram(
                "Tabs.Startup.TabCount.Regular", mTabModelSelector.getModel(false).getCount());
        RecordHistogram.recordCount1MHistogram(
                "Tabs.Startup.TabCount.Incognito", mTabModelSelector.getModel(true).getCount());
    }

    private void recordTabCountMetrics() {
        RecordHistogram.recordCount1MHistogram(
                "Tabs.Startup.TabCount2." + mClientTag + ".Regular",
                mTabModelSelector.getModel(false).getCount());
        RecordHistogram.recordCount1MHistogram(
                "Tabs.Startup.TabCount2." + mClientTag + ".Incognito",
                mTabModelSelector.getModel(true).getCount());
    }

    private void recordRestoreDuration() {
        if (mTabRestoreStartTime == INVALID_TIME) return;

        long duration = SystemClock.elapsedRealtime() - mTabRestoreStartTime;
        RecordHistogram.recordMediumTimesHistogram(
                "Tabs.Startup.RestoreDuration." + mClientTag, duration);
        int tabCount = mTabModelSelector.getTotalTabCount();
        if (tabCount != 0) {
            RecordHistogram.recordTimesHistogram(
                    "Tabs.Startup.RestoreDurationPerTab." + mClientTag,
                    Math.round((float) duration / tabCount));
        }
        mTabRestoreStartTime = INVALID_TIME;
    }

    /**
     * Manages loading of {@link TabState}. Also used to track if a load is in progress and the tab
     * details of that load. TODO(b/298058408) deprecate TabLoader
     */
    private class TabLoader {
        public final TabRestoreDetails mTabToRestore;
        private LoadTabTask mLoadTabTask;

        /**
         * @param tabToRestore details of {@link Tab} which will be read from storage
         */
        TabLoader(TabRestoreDetails tabToRestore) {
            mTabToRestore = tabToRestore;
        }

        /** Load {@link TabState} */
        public void load() {
            mLoadTabTask = new LoadTabTask(mTabToRestore);
            mLoadTabTask.executeOnTaskRunner(mSequencedTaskRunner);
        }

        public void cancel(boolean mayInterruptIfRunning) {
            if (mLoadTabTask != null) {
                mLoadTabTask.cancel(mayInterruptIfRunning);
            }
        }
    }

    /** Asynchronously triggers a cleanup of any unused persistent data. */
    private void cleanUpPersistentData() {
        mPersistencePolicy.cleanupUnusedFiles(
                new Callback<TabPersistenceFileInfo>() {
                    @Override
                    public void onResult(TabPersistenceFileInfo result) {
                        if (result == null) return;
                        for (String metadataFile : result.getMetadataFiles()) {
                            deleteFileAsync(metadataFile, true);
                        }
                        for (TabStateFileInfo tabStateFileInfo : result.getTabStateFileInfos()) {
                            TabStateFileManager.deleteAsync(
                                    getStateDirectory(),
                                    tabStateFileInfo.tabId,
                                    tabStateFileInfo.isEncrypted);
                        }
                    }
                });
        performPersistedTabDataMaintenance(null);
        TabStateFileManager.cleanupUnusedFiles(getStateDirectory());
    }

    @VisibleForTesting
    protected void performPersistedTabDataMaintenance(Runnable onCompleteForTesting) {
        // PersistedTabData currently doesn't support Custom Tabs, so maintenance
        // only needs to be performed for regular Tabs.
        if (mPersistencePolicy instanceof TabbedModeTabPersistencePolicy) {
            mPersistencePolicy.getAllTabIds(
                    (res) -> {
                        List<Integer> allTabIds = new ArrayList<>();
                        for (int i = 0; i < res.size(); i++) {
                            allTabIds.add(res.keyAt(i));
                        }
                        PersistedTabData.performStorageMaintenance(allTabIds);
                        if (onCompleteForTesting != null) {
                            onCompleteForTesting.run();
                        }
                    });
        }
    }

    /**
     * Clean up persistent state for a given instance.
     * @param instanceId Instance ID.
     */
    public void cleanupStateFile(int instanceId) {
        mPersistencePolicy.cleanupInstanceState(
                instanceId,
                new Callback<TabPersistenceFileInfo>() {
                    @Override
                    public void onResult(TabPersistenceFileInfo result) {
                        // Delete the instance state file (tab_stateX) as well.
                        deleteFileAsync(
                                TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(
                                        instanceId),
                                true);

                        // |result| can be null if the task gets cancelled.
                        if (result == null) return;
                        for (String metadataFile : result.getMetadataFiles()) {
                            deleteFileAsync(metadataFile, true);
                        }
                        for (TabStateFileInfo tabStateFileInfo : result.getTabStateFileInfos()) {
                            TabStateFileManager.deleteAsync(
                                    mPersistencePolicy.getOrCreateStateDirectory(),
                                    tabStateFileInfo.tabId,
                                    tabStateFileInfo.isEncrypted);
                        }
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
                    TaskTraits.BEST_EFFORT_MAY_BLOCK,
                    () -> {
                        deleteStateFile(file);
                    });
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

    private class LoadTabTask extends AsyncTask<TabState> {
        private final TabRestoreDetails mTabToRestore;
        private TabState mTabState;

        public LoadTabTask(TabRestoreDetails tabToRestore) {
            mTabToRestore = tabToRestore;
            TraceEvent.startAsync("LoadTabTask", mTabToRestore.id);
            TraceEvent.startAsync("LoadTabState", mTabToRestore.id);
        }

        @Override
        protected TabState doInBackground() {
            if (mDestroyed || isCancelled()) return null;
            try {
                return TabStateFileManager.restoreTabState(
                        getStateDirectory(), mTabToRestore.id, mCipherFactory);
            } catch (Exception e) {
                Log.w(TAG, "Unable to read state: " + e);
                return null;
            }
        }

        @Override
        protected void onPostExecute(TabState tabState) {
            TraceEvent.finishAsync("LoadTabState", mTabToRestore.id);
            mTabState = tabState;

            TraceEvent.finishAsync("LoadTabTask", mTabToRestore.id);
            if (mDestroyed || isCancelled()) {
                return;
            }

            completeLoad(mTabToRestore, mTabState);
        }
    }

    private void completeLoad(TabRestoreDetails tabToRestore, TabState tabState) {
        boolean isIncognito = isIncognitoTabBeingRestored(tabToRestore, tabState);
        boolean isLoadCancelled =
                (isIncognito && mCancelIncognitoTabLoads)
                        || (!isIncognito && mCancelNormalTabLoads);
        if (!isLoadCancelled) {
            restoreTab(tabToRestore, tabState, false);
        }

        loadNextTab();
    }

    /** Provides additional meta data to restore an individual tab. */
    @VisibleForTesting
    protected static final class TabRestoreDetails {
        public final int id;
        public final int originalIndex;
        public final String url;
        public final Boolean isIncognito;
        public final Boolean fromMerge;

        public TabRestoreDetails(
                int id, int originalIndex, Boolean isIncognito, String url, Boolean fromMerge) {
            this.id = id;
            this.originalIndex = originalIndex;
            this.url = url;
            this.isIncognito = isIncognito;
            this.fromMerge = fromMerge;
        }
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
        final int activeTabId =
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, Tab.INVALID_TAB_ID);
        if (activeTabId == Tab.INVALID_TAB_ID) return;
        prefetchActiveTabTask(activeTabId, taskRunner);
    }

    private void prefetchActiveTabTask(int activeTabId, TaskRunner taskRunner) {
        mPrefetchTabStateActiveTabTask =
                new BackgroundOnlyAsyncTask<TabState>() {
                    @Override
                    protected TabState doInBackground() {
                        return TabStateFileManager.restoreTabState(
                                getStateDirectory(), activeTabId, mCipherFactory);
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
     * @param uniqueTag The tag that uniquely identifies this state file. Typically this is an index
     *     or ID.
     * @return The name of the state file.
     */
    public static String getMetadataFileName(String uniqueTag) {
        return SAVED_METADATA_FILE_PREFIX + uniqueTag;
    }

    /**
     * Parses the metadata file name and returns the unique tag encoded into it.
     *
     * @param metadataFileName The state file name to be parsed.
     * @return The unique tag used when generating the file name.
     */
    public static String getMetadataFileUniqueTag(String metadataFileName) {
        assert isMetadataFile(metadataFileName);
        return metadataFileName.substring(SAVED_METADATA_FILE_PREFIX.length());
    }

    /**
     * Returns whether the specified filename matches the expected pattern of the tab metadata
     * files.
     */
    public static boolean isMetadataFile(String fileName) {
        // The .new/.bak suffixes may be added internally by AtomicFile before the file finishes
        // writing. Ignore files in this transitory state.
        return fileName.startsWith(SAVED_METADATA_FILE_PREFIX)
                && !fileName.endsWith(".new")
                && !fileName.endsWith(".bak");
    }

    /**
     * @return The shared pref APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE. This is used when we need to
     *         know the last known tab state before the active tab from the tab state is read.
     */
    public static @ActiveTabState int readLastKnownActiveTabStatePref() {
        return ChromeSharedPreferences.getInstance()
                .readInt(
                        ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE,
                        ActiveTabState.EMPTY);
    }

    public static void onDeferredStartup() {
        sDeferredStartupComplete = true;
    }

    public static void resetDeferredStartupCompleteForTesting() {
        sDeferredStartupComplete = false;
    }

    public MigrateTabTask getMigrateTabTaskForTesting() {
        return mMigrateTabTask;
    }

    protected Deque<Tab> getTabsToSaveForTesting() {
        return mTabsToSave;
    }

    protected boolean isSavingAndMigratingIdleForTesting() {
        // Idle if
        // 1) no save is in progress and the save queue is empty.
        // 2) No migration in progress (it's ok for the migration queue to be non-empty as only
        // sMaxMigrationsPerSave are executed at a time).
        return mSaveTabTask == null && mTabsToSave.isEmpty() && mMigrateTabTask == null;
    }

    public void setMigrateTabTaskForTesting(MigrateTabTask migrateTabTask) {
        mMigrateTabTask = migrateTabTask;
    }

    protected Deque<Tab> getTabsToMigrateForTesting() {
        return mTabsToMigrate;
    }

    private static boolean isFlatBufferSchemaEnabled() {
        return ChromeFeatureList.sTabStateFlatBuffer.isEnabled();
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
}
