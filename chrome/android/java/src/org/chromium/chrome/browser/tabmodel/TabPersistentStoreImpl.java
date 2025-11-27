// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.SharedPreferences;
import android.os.StrictMode;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseIntArray;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

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
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.state.PersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabPersistenceFileInfo.TabStateFileInfo;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.OnTabStateReadCallback;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelMetadata;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ExecutionException;

/** This class handles saving and loading tab state from the persistent storage. */
@NullMarked
public class TabPersistentStoreImpl implements TabPersistentStore {
    public static final String CLIENT_TAG_REGULAR = "Regular";
    public static final String CLIENT_TAG_CUSTOM = "Custom";
    public static final String CLIENT_TAG_ARCHIVED = "Archived";
    public static final String CLIENT_TAG_HEADLESS = "Headless";
    private static final String TAG = "tabmodel";
    private static final String TAG_MIGRATION = "fb_migration";
    private static final long INVALID_TIME = -1;
    private static final int CLEANUP_LEGACY_TABSTATE_BATCH_SIZE = 5;
    private static final int MAX_FILES_DELETED_PER_SESSION_LEGACY_TABSTATE = 100;

    /**
     * Determined experimentally to balance load speed and UI thread congestion. The optimal value
     * of those tested was 5. The exact optimal value is likely in the range [2, 9].
     */
    private static final int BATCH_RESTORE_SIZE = 5;

    @VisibleForTesting /* package */ static final int MAX_MIGRATIONS_PER_SAVE = 5;

    private static boolean sDeferredStartupComplete;

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
    /* package */ @interface TabRestoreMethod {
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

    /** Provides additional meta data to restore an individual tab. */
    @VisibleForTesting
    public static final class TabRestoreDetails {
        public final int id;
        public final int originalIndex;
        public final String url;
        public final @Nullable Boolean isIncognito;
        public final Boolean fromMerge;

        public TabRestoreDetails(
                int id,
                int originalIndex,
                @Nullable Boolean isIncognito,
                String url,
                Boolean fromMerge) {
            this.id = id;
            this.originalIndex = originalIndex;
            this.url = url;
            this.isIncognito = isIncognito;
            this.fromMerge = fromMerge;
        }
    }

    private final Set<Integer> mSeenTabIds = new HashSet<>();
    // Counts distinct URLs.
    private final Map<String, Integer> mSeenTabUrlMap = new HashMap<>();
    private final String mClientTag;
    private final TabPersistencePolicy mPersistencePolicy;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private final TabWindowManager mTabWindowManager;
    private final CipherFactory mCipherFactory;
    private final ObserverList<TabPersistentStoreObserver> mObservers;
    private final Deque<Tab> mTabsToSave;
    private final ArrayDeque<Tab> mTabsToMigrate;
    private final ArrayDeque<File> mLegacyTabStateFilesToDelete;
    private final Deque<TabRestoreDetails> mTabsToRestore;
    private final Set<Integer> mTabIdsToRestore;
    private final List<Pair<AsyncTask<@Nullable DataInputStream>, String>>
            mPrefetchTabListToMergeTasks;
    // A set of filenames which are tracked to merge.
    private final Set<String> mMergedFileNames;

    private SequencedTaskRunner mSequencedTaskRunner;
    private TabModelObserver mTabModelObserver;
    private TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private int mDuplicateTabIdsSeen;
    private @MetadataSaveMode int mMetadataSaveMode;
    private @Nullable TabBatchLoader mTabBatchLoader;
    private @Nullable SaveTabTask mSaveTabTask;
    private @Nullable MigrateTabTask mMigrateTabTask;
    private @Nullable SaveListTask mSaveListTask;
    private boolean mDestroyed;
    private boolean mCancelNormalTabLoads;
    private boolean mCancelIncognitoTabLoads;
    // Keys are the original tab indexes, values are the tab ids.
    private @Nullable SparseIntArray mNormalTabsRestored;
    private @Nullable SparseIntArray mIncognitoTabsRestored;
    private @Nullable AsyncTask<@Nullable DataInputStream> mPrefetchTabListTask;
    private @Nullable TabModelSelectorMetadata mLastSavedMetadata;
    // Tracks whether this TabPersistentStore's tabs are being loaded.
    private boolean mLoadInProgress;
    private long mTabRestoreStartTime = INVALID_TIME;
    @Nullable AsyncTask<@Nullable TabState> mPrefetchTabStateActiveTabTask;

    /**
     * Creates an instance of a TabPersistentStore.
     *
     * @param clientTag The client tag used to record metrics.
     * @param policy Abstraction around activity specific behaviors.
     * @param modelSelector The {@link TabModelSelector} to observe changes in. Regardless of the
     *     mode this store is in, this will be the real selector with real models. This should be
     *     treated as a read only object, no modifications should go through it.
     * @param tabCreatorManager Used to create new tabs on initial load. This may return real
     *     creators, or faked out creators if in non-authoritative mode.
     * @param tabWindowManager Used to avoid deleting archived tab state files.
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     */
    public TabPersistentStoreImpl(
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
        mLegacyTabStateFilesToDelete = new ArrayDeque<>();
        mTabsToRestore = new ArrayDeque<>();
        mTabIdsToRestore = new HashSet<>();
        mObservers = new ObserverList<>();
        @TaskTraits int taskTraits = TaskTraits.USER_BLOCKING_MAY_BLOCK;
        mSequencedTaskRunner = PostTask.createSequencedTaskRunner(taskTraits);
        mPrefetchTabListToMergeTasks = new ArrayList<>();
        mMergedFileNames = new HashSet<>();

        assert TabMetadataFileManager.isMetadataFile(policy.getMetadataFileName())
                : "Metadata file name is not valid";
        boolean needsInitialization =
                mPersistencePolicy.performInitialization(mSequencedTaskRunner);

        mPersistencePolicy.setTaskRunner(mSequencedTaskRunner);

        if (mPersistencePolicy.isMergeInProgress()) return;

        // TODO(smaier): We likely can move everything onto the SequencedTaskRunner when the
        // SERIAL_EXECUTOR path is gone. crbug.com/957735
        TaskRunner taskRunner =
                needsInitialization ? mSequencedTaskRunner : PostTask.getTaskRunner(taskTraits);

        mPrefetchTabListTask =
                startFetchTabListTask(taskRunner, mPersistencePolicy.getMetadataFileName());
        startPrefetchActiveTabTask(taskRunner);

        if (mPersistencePolicy.shouldMergeOnStartup()) {
            String mergedFileName = mPersistencePolicy.getMetadataFileNameToBeMerged();
            assert mergedFileName != null;
            AsyncTask<@Nullable DataInputStream> task =
                    startFetchTabListTask(taskRunner, mergedFileName);
            mPrefetchTabListToMergeTasks.add(Pair.create(task, mergedFileName));
        }
    }

    @Initializer
    @Override
    public void onNativeLibraryReady() {
        TabStateAttributes.Observer attributesObserver =
                (Tab tab, @DirtinessState int dirtiness) -> {
                    if (dirtiness == DirtinessState.DIRTY && !tab.isDestroyed()) {
                        addTabToSaveQueue(tab);
                    }
                };
        mTabRegistrationObserver = new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                new TabModelSelectorTabRegistrationObserver.Observer() {
                    @Override
                    public void onTabRegistered(Tab tab) {
                        TabStateAttributes attributes = TabStateAttributes.from(tab);
                        assumeNonNull(attributes);
                        if (attributes.addObserver(attributesObserver) == DirtinessState.DIRTY) {
                            addTabToSaveQueue(tab);
                        }
                    }

                    @Override
                    public void onTabUnregistered(Tab tab) {
                        if (!tab.isDestroyed()) {
                            assumeNonNull(TabStateAttributes.from(tab))
                                    .removeObserver(attributesObserver);
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
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        // Initialization will create the current tab and select it, this isn't a
                        // meaningful change that needs to be saved.
                        if (ChromeFeatureList.sTabModelInitFixes.isEnabled()
                                && !mTabModelSelector.isTabStateInitialized()
                                && lastId == TabList.INVALID_TAB_INDEX) {
                            return;
                        }

                        saveTabListAsynchronously();
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        saveTabListAsynchronously();
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        // Ignore all tabs being restored as part of init, they're all already on
                        // disk.
                        if (ChromeFeatureList.sTabModelInitFixes.isEnabled()
                                && !mTabModelSelector.isTabStateInitialized()
                                && type == TabLaunchType.FROM_RESTORE) {
                            return;
                        }

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

    @VisibleForTesting
    @Override
    public void waitForMigrationToFinish() {
        mPersistencePolicy.waitForInitializationToFinish();
    }

    @Override
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

            // The list of tabs should be saved first in case our activity is terminated early.
            // Explicitly toss out any existing SaveListTask because they only save the TabModel as
            // it looked when the SaveListTask was first created.
            if (mSaveListTask != null) mSaveListTask.cancel(true);
            try {
                RecordHistogram.recordBooleanHistogram(
                        "Tabs.Metadata.SyncSave." + mClientTag, true);
                saveListToFile(extractTabMetadata());
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
                // wrote the state to disk. That's why we have to check mStateSaved here.
                if (mSaveTabTask.cancel(false) && !mSaveTabTask.mStateSaved) {
                    // The task was successfully cancelled. We should try to save this state again.
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
                    if (ChromeFeatureList.sTabModelInitFixes.isEnabled()) {
                        TabStateAttributes attributes = TabStateAttributes.from(tab);
                        if (attributes != null) {
                            attributes.clearTabStateDirtiness();
                        }
                    }
                    TabState state = TabStateExtractor.from(tab);
                    if (state != null) {
                        TabStateFileManager.saveState(
                                getStateDirectory(), state, id, incognito, mCipherFactory);
                    }
                } catch (OutOfMemoryError e) {
                    Log.e(TAG, "Out of memory error while attempting to save tab state. Erasing.");
                    deleteTabState(id, incognito);
                    TabStateFileManager.deleteMigratedFile(getStateDirectory(), id, incognito);
                }
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
    @VisibleForTesting
    protected void updateMigratedFiles() {
        List<Tab> updatedMigrations = new ArrayList<>();
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
    /* package */ void initializeRestoreVars(boolean ignoreIncognitoFiles) {
        mCancelNormalTabLoads = false;
        mCancelIncognitoTabLoads = ignoreIncognitoFiles;
        mNormalTabsRestored = new SparseIntArray();
        mIncognitoTabsRestored = new SparseIntArray();
    }

    @Override
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
                    TabMetadataFileManager.readSavedMetadataFile(
                            stream,
                            createOnTabStateReadCallback(
                                    mTabModelSelector.isIncognitoSelected(), false),
                            /* tabIds= */ null);
                } else {
                    mTabRestoreStartTime = INVALID_TIME;
                }
            }

            // Restore the tabs for the other TabPersistentStore instance if its tab metadata file
            // exists.
            if (!mPrefetchTabListToMergeTasks.isEmpty()) {
                for (Pair<AsyncTask<DataInputStream>, String> mergeTask :
                        mPrefetchTabListToMergeTasks) {
                    AsyncTask<DataInputStream> task = mergeTask.first;
                    stream = task.get();
                    if (stream == null) continue;
                    mMergedFileNames.add(mergeTask.second);
                    mPersistencePolicy.setMergeInProgress(true);
                    TabMetadataFileManager.readSavedMetadataFile(
                            stream,
                            createOnTabStateReadCallback(
                                    mTabModelSelector.isIncognitoSelected(),
                                    !mTabsToRestore.isEmpty()),
                            /* tabIds= */ null);
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

    @Override
    public void mergeState() {
        if (mLoadInProgress
                || mTabBatchLoader != null
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
                TabMetadataFileManager.readSavedMetadataFile(
                        stream,
                        createOnTabStateReadCallback(mTabModelSelector.isIncognitoSelected(), true),
                        /* tabIds= */ null);
            }
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing app.
            Log.d(TAG, "mergeState exception: " + e.toString(), e);
        }

        // Restore the tabs from the second activity asynchronously.
        loadNextTabs();
    }

    @Override
    public void restoreTabs(boolean setActiveTab) {
        if (setActiveTab) {
            // Restore and select the active tab, which is first in the restore list.
            // If the active tab can't be restored, restore and select another tab. Otherwise, the
            // tab model won't have a valid index and the UI will break. http://crbug.com/261378
            while (!mTabsToRestore.isEmpty()
                    && assumeNonNull(mNormalTabsRestored).size() == 0
                    && assumeNonNull(mIncognitoTabsRestored).size() == 0) {
                try (TraceEvent e = TraceEvent.scoped("LoadFirstTabState")) {
                    TabRestoreDetails tabToRestore = mTabsToRestore.removeFirst();
                    restoreTab(tabToRestore, true);
                }
            }
        }
        loadNextTabs();
    }

    @Override
    public void restoreTabStateForUrl(String url) {
        restoreTabStateInternal(url, Tab.INVALID_TAB_ID);
    }

    @Override
    public void restoreTabStateForId(int id) {
        restoreTabStateInternal(null, id);
    }

    private void restoreTabStateInternal(@Nullable String url, int id) {
        TabRestoreDetails tabToRestore = null;
        if (mTabBatchLoader != null) {
            // Steal the task of restoring the tab from the active load tab task.
            tabToRestore = mTabBatchLoader.getTabByUrlOrId(url, id);
            if (tabToRestore != null) {
                // Exclude restoring `tabToRestore` since it will get restored now.
                restartBatch(tabToRestore.id);
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
            Log.i(TAG, "loadTabs exception: " + e, e);
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
            TabRestoreDetails tabToRestore, @Nullable TabState tabState, boolean setAsActive) {
        // If we don't have enough information about the Tab, bail out.
        boolean isIncognito = isIncognitoTabBeingRestored(tabToRestore, tabState);

        if (tabState == null) {
            if (tabToRestore.isIncognito == null) {
                Log.w(TAG, "Failed to restore tab: not enough info about its type was available.");
                return;
            } else if (isIncognito) {
                boolean isNtp = UrlUtilities.isNtpUrl(tabToRestore.url);
                boolean isNtpFromMerge = isNtp && tabToRestore.fromMerge;
                boolean isFromReparenting =
                        AsyncTabParamsManagerSingleton.getInstance()
                                .hasParamsForTabId(tabToRestore.id);

                if (!isNtpFromMerge
                        && !isFromReparenting
                        && (!isNtp || !setAsActive || mCancelIncognitoTabLoads)) {
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
        assumeNonNull(restoredTabs);
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
        // Track duplicate tab ids, and don't restore tab ids which already exist in the tab model.
        int tabId = tabToRestore.id;
        if (ChromeFeatureList.sAndroidTabDeclutterDedupeTabIdsKillSwitch.isEnabled()
                && mSeenTabIds.contains(tabId)) {
            mDuplicateTabIdsSeen++;
            return;
        }
        // Track duplicate tab urls in the regular tab models.
        if (!isIncognito) {
            mSeenTabUrlMap.put(
                    tabToRestore.url, mSeenTabUrlMap.getOrDefault(tabToRestore.url, 0) + 1);
        }

        if (tabState != null) {
            if (tabState.contentsState != null) {
                tabState.contentsState.setFallbackUrlForRestorationFailure(tabToRestore.url);
            }

            if (tabState.legacyFileToDelete != null
                    && mLegacyTabStateFilesToDelete.size()
                            < MAX_FILES_DELETED_PER_SESSION_LEGACY_TABSTATE) {
                mLegacyTabStateFilesToDelete.add(tabState.legacyFileToDelete);
                tabState.legacyFileToDelete = null;
            }

            @TabRestoreMethod int tabRestoreMethod = TabRestoreMethod.TAB_STATE;
            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.TabRestoreMethod", tabRestoreMethod, TabRestoreMethod.NUM_ENTRIES);
            Tab tab =
                    mTabCreatorManager
                            .getTabCreator(isIncognito)
                            .createFrozenTab(tabState, tabToRestore.id, restoredIndex);
            if (tab == null) return;

            if (tabState.shouldMigrate) {
                mTabsToMigrate.add(tab);
            }

            if (!isIncognito) {
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

            // TODO(https://crbug.com/445197903): This is a modification to the tab model that
            // may not be correct if this store isn't authoritative. Move this into an observer.
            TabModelUtils.setIndex(model, TabModelUtils.getTabIndexById(model, tabId));
            boolean isIncognitoTabModelSelected = mTabModelSelector.isIncognitoSelected();

            // Setting the index will cause the tab's model to be selected. Set it back to the model
            // that was selected before setting the index if the index is being set during a merge
            // unless the previously selected model is empty (e.g. showing the empty background
            // view on tablets).
            if (tabToRestore.fromMerge
                    && wasIncognitoTabModelSelected != isIncognitoTabModelSelected
                    && selectedModelTabCount != 0) {
                // TODO(https://crbug.com/445197903): This is a modification to the tab model that
                // may not be correct if this store isn't authoritative. Move this into an observer.
                mTabModelSelector.selectModel(wasIncognitoTabModelSelected);
            }
        }
        restoredTabs.put(tabToRestore.originalIndex, tabId);
    }

    @Override
    public int getRestoredTabCount() {
        return mTabsToRestore.size();
    }

    @Override
    public void clearState() {
        mPersistencePolicy.cancelCleanupInProgress();

        mSequencedTaskRunner.execute(
                () -> {
                    File[] baseStateFiles =
                            TabStateDirectory.getOrCreateBaseStateDirectory().listFiles();
                    if (baseStateFiles == null) return;
                    for (File baseStateFile : baseStateFiles) {
                        // In legacy scenarios (prior to migration, state files could reside in
                        // the root state directory. So, handle deleting direct child files as
                        // well as those that reside in sub directories.
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
                });

        onStateLoaded();
    }

    private void cancelLoadingTabs(boolean incognito) {
        if (incognito) {
            mCancelIncognitoTabLoads = true;
        } else {
            mCancelNormalTabLoads = true;
        }
    }

    @VisibleForTesting
    /* package */ void addTabToSaveQueue(@Nullable Tab tab) {
        addTabToSaveQueueIfApplicable(tab);
        saveNextTab();
    }

    /**
     * @return Whether the specified tab is in any pending save operations.
     */
    @VisibleForTesting
    /* package */ boolean isTabPendingSave(Tab tab) {
        return (mSaveTabTask != null && mSaveTabTask.mTab.equals(tab)) || mTabsToSave.contains(tab);
    }

    private void addTabToSaveQueueIfApplicable(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return;
        TabStateAttributes tabStateAttributes = assumeNonNull(TabStateAttributes.from(tab));
        @DirtinessState int dirtinessState = tabStateAttributes.getDirtinessState();
        if (mTabsToSave.contains(tab) || dirtinessState == DirtinessState.CLEAN) {
            return;
        }

        if (mSaveTabTask != null && mSaveTabTask.mId == tab.getId()) {
            RecordHistogram.recordCount100Histogram(
                    "Tabs.PotentialDoubleDirty.SaveQueueSize", mTabsToSave.size());
        }

        mTabsToSave.addLast(tab);
    }

    @VisibleForTesting
    void removeTabFromQueues(Tab tab) {
        mTabsToSave.remove(tab);
        mTabsToRestore.remove(getTabToRestoreById(tab.getId()));
        mTabsToMigrate.remove(tab);

        if (mTabBatchLoader != null) {
            // If the tab is in the restoring batch drop it from the batch and restart the batch.
            TabRestoreDetails tabRestoreDetailsToDrop =
                    mTabBatchLoader.getTabByUrlOrId(null, tab.getId());
            if (tabRestoreDetailsToDrop != null) {
                restartBatch(tabRestoreDetailsToDrop.id);
            }
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

    private @Nullable TabRestoreDetails getTabToRestoreByUrl(String url) {
        for (TabRestoreDetails tabBeingRestored : mTabsToRestore) {
            if (TextUtils.equals(tabBeingRestored.url, url)) {
                return tabBeingRestored;
            }
        }
        return null;
    }

    private @Nullable TabRestoreDetails getTabToRestoreById(int id) {
        for (TabRestoreDetails tabBeingRestored : mTabsToRestore) {
            if (tabBeingRestored.id == id) {
                return tabBeingRestored;
            }
        }
        return null;
    }

    @SuppressWarnings("NullAway")
    @Override
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
        if (mTabBatchLoader != null) {
            mTabBatchLoader.cancel(true);
            mTabBatchLoader = null;
        }
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

    private TabModelSelectorMetadata extractTabMetadata() throws IOException {
        List<TabRestoreDetails> tabsToRestore = new ArrayList<>();

        // The metadata file may be being written out before all of the Tabs have been restored.
        // Save that information out, as well.
        if (mTabBatchLoader != null) tabsToRestore.addAll(mTabBatchLoader.getTabsInBatch());
        tabsToRestore.addAll(mTabsToRestore);

        return extractTabMetadataFromSelector(mTabModelSelector, tabsToRestore);
    }

    private void saveListToFile(TabModelSelectorMetadata listData) {
        if (Objects.equals(mLastSavedMetadata, listData)) return;
        // Save the index file containing the list of tabs to restore.
        File metadataFile = new File(getStateDirectory(), mPersistencePolicy.getMetadataFileName());
        TabMetadataFileManager.saveListToFile(metadataFile, listData);
        mLastSavedMetadata = listData;
    }

    /**
     * @param isIncognitoSelected Whether the tab model is incognito.
     * @return A callback for reading data from tab models.
     */
    private OnTabStateReadCallback createOnTabStateReadCallback(
            final boolean isIncognitoSelected, final boolean fromMerge) {
        return (int index,
                int id,
                String url,
                @Nullable Boolean isIncognito,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex) -> {
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
        // SharedPreferences afterwards. This is done on the UI thread because it is on the
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
                        } else if (TabMetadataFileManager.isMetadataFile(file.getName())) {
                            DataInputStream stream = null;
                            try {
                                stream =
                                        new DataInputStream(
                                                new BufferedInputStream(new FileInputStream(file)));
                                int nextId =
                                        TabMetadataFileManager.readSavedMetadataFile(
                                                stream, /* callback= */ null, /* tabIds= */ null);
                                maxId = Math.max(maxId, nextId);
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
     * Triggers the next save tab task. Clients do not need to call this as it will be triggered
     * automatically by calling {@link #addTabToSaveQueue(Tab)}.
     */
    @VisibleForTesting
    void saveNextTab() {
        if (mSaveTabTask != null) return;
        if (!mTabsToSave.isEmpty()) {
            Tab tab = mTabsToSave.removeFirst();
            mSaveTabTask = new SaveTabTask(tab);
            mSaveTabTask.executeOnTaskRunner(mSequencedTaskRunner);
            migrateNextTabIfApplicable(1);
            deleteLegacyTabStateFilesIfApplicable();
        } else {
            saveTabListAsynchronously();
        }
    }

    private void migrateNextTabIfApplicable(int numMigration) {
        // Only migrate TabState to FlatBuffer format if:
        // - FlatBuffer schema flag is enabled
        // - We haven't hit the limit of sMaxMigrationsPerSave migrations per save yet
        // - Deferred startup is complete (to reduce the risk of jank).
        if (mTabsToMigrate.isEmpty()
                || numMigration > MAX_MIGRATIONS_PER_SAVE
                || !sDeferredStartupComplete) {
            return;
        }
        Tab tab = mTabsToMigrate.removeFirst();
        mMigrateTabTask = new MigrateTabTask(tab, numMigration);
        mMigrateTabTask.executeOnTaskRunner(mSequencedTaskRunner);
    }

    private void deleteLegacyTabStateFilesIfApplicable() {
        if (mLegacyTabStateFilesToDelete.isEmpty()) {
            return;
        }
        List<File> filesToDelete = new ArrayList<>();
        for (int i = 0;
                !mLegacyTabStateFilesToDelete.isEmpty() && i < CLEANUP_LEGACY_TABSTATE_BATCH_SIZE;
                i++) {
            filesToDelete.add(mLegacyTabStateFilesToDelete.poll());
        }
        PostTask.runOrPostTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    ThreadUtils.assertOnBackgroundThread();
                    for (File fileToDelete : filesToDelete) {
                        if (fileToDelete.exists() && !fileToDelete.delete()) {
                            Log.e(TAG, "Error deleting " + fileToDelete);
                        }
                    }
                });
    }

    private void saveTabListAsynchronously() {
        if (ChromeFeatureList.sAndroidTabSkipSaveTabsKillswitch.isEnabled()
                && mMetadataSaveMode != MetadataSaveMode.SAVING_ALLOWED) {
            if (mMetadataSaveMode == MetadataSaveMode.PAUSED_AND_CLEAN) {
                mMetadataSaveMode = MetadataSaveMode.PAUSED_AND_DIRTY;
            }
            return;
        }
        if (mSaveListTask != null) mSaveListTask.cancel(true);
        mSaveListTask = new SaveListTask();
        mSaveListTask.executeOnTaskRunner(mSequencedTaskRunner);
    }

    @Override
    public void pauseSaveTabList() {
        if (mMetadataSaveMode == MetadataSaveMode.SAVING_ALLOWED) {
            mMetadataSaveMode = MetadataSaveMode.PAUSED_AND_CLEAN;
        }
    }

    @Override
    public void resumeSaveTabList(Runnable onSaveTabListRunnable) {
        boolean shouldTriggerSave =
                !ChromeFeatureList.sTabModelInitFixes.isEnabled()
                        || mMetadataSaveMode == MetadataSaveMode.PAUSED_AND_DIRTY;
        mMetadataSaveMode = MetadataSaveMode.SAVING_ALLOWED;
        if (shouldTriggerSave) {
            addObserver(
                    new TabPersistentStoreObserver() {
                        @Override
                        public void onMetadataSavedAsynchronously() {
                            onSaveTabListRunnable.run();
                            removeObserver(this);
                        }
                    });
            saveTabListAsynchronously();
        } else {
            onSaveTabListRunnable.run();
        }
    }

    private class SaveTabTask extends AsyncTask<Void> {
        final Tab mTab;
        final int mId;
        final boolean mEncrypted;

        @Nullable TabState mState;
        boolean mStateSaved;

        SaveTabTask(Tab tab) {
            mTab = tab;
            mId = tab.getId();
            mEncrypted = tab.isIncognito();
        }

        @Override
        protected void onPreExecute() {
            if (mDestroyed || isCancelled()) return;
            assumeNonNull(TabStateAttributes.from(mTab)).clearTabStateDirtiness();
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
    @VisibleForTesting
    public class MigrateTabTask extends AsyncTask<Void> {
        final Tab mTab;
        final int mId;
        final boolean mEncrypted;
        final int mNumMigration;

        @Nullable TabState mState;
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
                                getStateDirectory(),
                                assumeNonNull(mState),
                                mId,
                                mEncrypted,
                                mCipherFactory);
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

    private class SaveListTask extends AsyncTask<Void> {
        @Nullable TabModelSelectorMetadata mMetadata;

        @Override
        protected void onPreExecute() {
            if (mDestroyed || isCancelled()) return;
            try {
                mMetadata = extractTabMetadata();
            } catch (IOException e) {
                mMetadata = null;
            }
        }

        @Override
        protected Void doInBackground() {
            if (mMetadata == null || isCancelled()) return null;
            RecordHistogram.recordBooleanHistogram("Tabs.Metadata.SyncSave." + mClientTag, false);
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
                    observer.onMetadataSavedAsynchronously();
                }
                mMetadata = null;
            }
        }
    }

    @VisibleForTesting
    File getStateDirectory() {
        return mPersistencePolicy.getOrCreateStateDirectory();
    }

    /**
     * Returns a file pointing at the TabState corresponding to the given Tab.
     *
     * @param tabId ID of the TabState to locate.
     * @param encrypted Whether or not the tab is encrypted.
     * @return File pointing at the TabState for the Tab.
     */
    public File getTabStateFileForTesting(int tabId, boolean encrypted) {
        return TabStateFileManager.getTabStateFile(
                getStateDirectory(), tabId, encrypted, /* isFlatbuffer= */ true);
    }

    /**
     * Saves the TabState with the given ID.
     *
     * @param tabId ID of the Tab.
     * @param encrypted Whether or not the TabState is encrypted.
     * @param state TabState for the Tab.
     */
    private boolean saveTabState(int tabId, boolean encrypted, @Nullable TabState state) {
        if (state == null) return false;

        try {
            TabStateFileManager.saveState(
                    getStateDirectory(), state, tabId, encrypted, mCipherFactory);
            return true;
        } catch (OutOfMemoryError e) {
            Log.e(TAG, "Out of memory error while attempting to save tab state. Erasing.");
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
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, observer::onStateLoaded);
        }
    }

    private void loadNextTabs() {
        if (mDestroyed) return;

        if (mTabBatchLoader != null) return;

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
                    deleteFileAsync(mergedFileName);
                }
                for (TabPersistentStoreObserver observer : mObservers) observer.onStateMerged();
            }

            recordLegacyTabCountMetrics();
            recordTabCountMetrics();
            recordPinnedTabCountMetrics();
            recordRestoreDuration();
            recordUniqueTabUrlMetrics();
            cleanUpPersistentData();
            onStateLoaded();
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
            ArrayList<TabRestoreDetails> details = new ArrayList<>();
            for (int i = 0; i < BATCH_RESTORE_SIZE && !mTabsToRestore.isEmpty(); i++) {
                details.add(mTabsToRestore.removeFirst());
            }
            mTabBatchLoader = new TabBatchLoader(details);
            mTabBatchLoader.load();
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

    private void recordPinnedTabCountMetrics() {
        RecordHistogram.recordCount1MHistogram(
                "Tabs.Startup.PinnedTabCount." + mClientTag + ".Regular",
                mTabModelSelector.getModel(false).getPinnedTabsCount());
        RecordHistogram.recordCount1MHistogram(
                "Tabs.Startup.PinnedTabCount." + mClientTag + ".Incognito",
                mTabModelSelector.getModel(true).getPinnedTabsCount());
    }

    private void recordRestoreDuration() {
        if (mTabRestoreStartTime == INVALID_TIME) return;

        long duration = SystemClock.elapsedRealtime() - mTabRestoreStartTime;
        RecordHistogram.deprecatedRecordMediumTimesHistogram(
                "Tabs.Startup.RestoreDuration." + mClientTag, duration);
        int tabCount = mTabModelSelector.getTotalTabCount();
        if (tabCount != 0) {
            RecordHistogram.recordTimesHistogram(
                    "Tabs.Startup.RestoreDurationPerTab." + mClientTag,
                    Math.round((float) duration / tabCount));
        }
        mTabRestoreStartTime = INVALID_TIME;
    }

    private void recordUniqueTabUrlMetrics() {
        for (Entry<String, Integer> entry : mSeenTabUrlMap.entrySet()) {
            RecordHistogram.recordCount1000Histogram(
                    "Tabs.Startup.UniqueUrlCount." + mClientTag, entry.getValue());
        }
        mSeenTabUrlMap.clear();
    }

    /**
     * Manages loading of {@link TabState}. Also used to track if a load is in progress and the tab
     * details of that load. TODO(b/298058408) deprecate TabLoader
     */
    private class TabBatchLoader {
        private final List<TabRestoreDetails> mBatchedTabsToRestore;
        private @Nullable LoadTabsTask mLoadTabsTask;
        private boolean mCancelled;

        /**
         * @param tabsToRestore details of {@link Tab}s which will be read from storage
         */
        TabBatchLoader(List<TabRestoreDetails> tabsToRestore) {
            mBatchedTabsToRestore = tabsToRestore;
        }

        /** Returns the list of tab to be processed in this batch. */
        public List<TabRestoreDetails> getTabsInBatch() {
            return mBatchedTabsToRestore;
        }

        /** Loads {@link TabState} for the batch. */
        public void load() {
            if (mCancelled) return;

            mLoadTabsTask = new LoadTabsTask(mBatchedTabsToRestore);
            mLoadTabsTask.executeOnTaskRunner(mSequencedTaskRunner);
        }

        /** Cancels restoring the batch of tabs. */
        public void cancel(boolean mayInterruptIfRunning) {
            mCancelled = true;
            if (mLoadTabsTask != null) {
                mLoadTabsTask.cancel(mayInterruptIfRunning);
            }
        }

        /**
         * Gets the first tab that matches the id or url from the batch.
         *
         * @param url The URL of the tab to try to remove, may be null.
         * @param id The id of the tab to try and remove.
         * @return The matching tab's {@link TabRestoreDetails} or null if not found.
         */
        public @Nullable TabRestoreDetails getTabByUrlOrId(@Nullable String url, int id) {
            for (int i = 0; i < mBatchedTabsToRestore.size(); i++) {
                TabRestoreDetails details = mBatchedTabsToRestore.get(i);
                if ((url == null && details.id == id)
                        || (url != null && TextUtils.equals(details.url, url))) {
                    return details;
                }
            }
            return null;
        }

        /**
         * Restore the remaining tabs in the batch to the front of the queue to be processed again.
         *
         * @param excludedId The id of a tab to not restore. Use {@link Tab.INVALID_TAB_ID} if not
         *     applicable.
         */
        public void restoreTabsToQueue(int excludedId) {
            for (int i = mBatchedTabsToRestore.size() - 1; i >= 0; i--) {
                TabRestoreDetails details = mBatchedTabsToRestore.get(i);
                if (details.id == excludedId) continue;
                mTabsToRestore.addFirst(details);
            }
        }
    }

    private void restartBatch(int excludedId) {
        assumeNonNull(mTabBatchLoader);
        mTabBatchLoader.cancel(false);
        mTabBatchLoader.restoreTabsToQueue(excludedId);
        mTabBatchLoader = null;
        loadNextTabs(); // Queue up async task to load next tabs after we're done here.
    }

    /** Asynchronously triggers a cleanup of any unused persistent data. */
    private void cleanUpPersistentData() {
        mPersistencePolicy.cleanupUnusedFiles(
                result -> {
                    if (result == null) return;
                    for (String metadataFile : result.getMetadataFiles()) {
                        deleteFileAsync(metadataFile);
                    }
                    for (TabStateFileInfo tabStateFileInfo : result.getTabStateFileInfos()) {
                        TabStateFileManager.deleteAsync(
                                getStateDirectory(),
                                tabStateFileInfo.tabId,
                                tabStateFileInfo.isEncrypted);
                    }
                });
        performPersistedTabDataMaintenance(null);
    }

    @VisibleForTesting
    protected void performPersistedTabDataMaintenance(@Nullable Runnable onCompleteForTesting) {
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

    @Override
    public void cleanupStateFile(int windowId) {
        mPersistencePolicy.cleanupInstanceState(
                windowId,
                (TabPersistenceFileInfo result) -> {
                    // Delete the instance state file (tab_stateX) as well.
                    deleteFileAsync(
                            TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(windowId));

                    // |result| can be null if the task gets cancelled.
                    if (result == null) return;
                    for (String metadataFile : result.getMetadataFiles()) {
                        deleteFileAsync(metadataFile);
                    }
                    for (TabStateFileInfo tabStateFileInfo : result.getTabStateFileInfos()) {
                        TabStateFileManager.deleteAsync(
                                mPersistencePolicy.getOrCreateStateDirectory(),
                                tabStateFileInfo.tabId,
                                tabStateFileInfo.isEncrypted);
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
        new BackgroundOnlyAsyncTask<>() {
            @Override
            protected Void doInBackground() {
                deleteStateFile(file);
                return null;
            }
        }.executeOnTaskRunner(mSequencedTaskRunner);
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

    private class LoadTabsTask extends AsyncTask<@Nullable List<@Nullable TabState>> {
        private final List<TabRestoreDetails> mBatchedTabsToRestore;
        private final int mId;

        public LoadTabsTask(List<TabRestoreDetails> tabsToRestore) {
            mBatchedTabsToRestore = tabsToRestore;
            mId = tabsToRestore.get(0).id;
            TraceEvent.startAsync("LoadTabTask", mId);
            TraceEvent.startAsync("LoadTabState", mId);
        }

        @Override
        protected @Nullable List<@Nullable TabState> doInBackground() {
            if (mDestroyed || isCancelled()) return null;

            List<@Nullable TabState> tabStates = new ArrayList<>(mBatchedTabsToRestore.size());
            for (TabRestoreDetails details : mBatchedTabsToRestore) {
                tabStates.add(restoreTabState(details));
            }
            return tabStates;
        }

        private @Nullable TabState restoreTabState(TabRestoreDetails details) {
            try {
                return TabStateFileManager.restoreTabState(
                        getStateDirectory(), details.id, mCipherFactory);
            } catch (Exception e) {
                Log.w(TAG, "Unable to read state: " + e);
                return null;
            }
        }

        @Override
        protected void onPostExecute(@Nullable List<@Nullable TabState> tabStates) {
            TraceEvent.finishAsync("LoadTabState", mId);

            TraceEvent.finishAsync("LoadTabTask", mId);
            if (mDestroyed || isCancelled()) {
                return;
            }
            assumeNonNull(tabStates);
            completeLoad(mBatchedTabsToRestore, tabStates);
        }
    }

    private void completeLoad(
            List<TabRestoreDetails> tabsToRestore, List<@Nullable TabState> tabStates) {
        assert tabsToRestore.size() == tabStates.size();
        for (int i = 0; i < tabsToRestore.size(); i++) {
            TabRestoreDetails tabToRestore = tabsToRestore.get(i);
            TabState tabState = tabStates.get(i);

            boolean isIncognito = isIncognitoTabBeingRestored(tabToRestore, tabState);
            boolean isLoadCancelled =
                    (isIncognito && mCancelIncognitoTabLoads)
                            || (!isIncognito && mCancelNormalTabLoads);
            if (!isLoadCancelled) {
                restoreTab(tabToRestore, tabState, false);
            }
        }

        mTabBatchLoader = null;
        loadNextTabs();
    }

    /**
     * Determines if a Tab being restored is definitely an Incognito Tab.
     *
     * <p>This function can fail to determine if a Tab is incognito if not enough data about the Tab
     * was successfully saved out.
     *
     * @return True if the tab is definitely Incognito, false if it's not or if it's undecidable.
     */
    private boolean isIncognitoTabBeingRestored(
            TabRestoreDetails tabDetails, @Nullable TabState tabState) {
        if (tabState != null) {
            // The Tab's previous state was completely restored.
            return tabState.isIncognito;
        } else if (tabDetails.isIncognito != null) {
            // The TabState couldn't be restored, but we have some information about the tab.
            return tabDetails.isIncognito;
        } else {
            // The tab's type is undecidable.
            return false;
        }
    }

    @SuppressWarnings("NullAway") // executeOnTaskRunner() drops null information.
    private AsyncTask<@Nullable DataInputStream> startFetchTabListTask(
            TaskRunner taskRunner, final String stateFileName) {
        return new BackgroundOnlyAsyncTask<@Nullable DataInputStream>() {
            @Override
            protected @Nullable DataInputStream doInBackground() {
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
                    int size = (int) stateFile.length();
                    int sizeInKb = size / ConversionUtils.BYTES_PER_KILOBYTE;
                    RecordHistogram.recordMemoryKBHistogram(
                            "Tabs.Metadata.FileSizeOnRead." + mClientTag, sizeInKb);
                    data = new byte[size];
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

    @SuppressWarnings("NullAway") // executeOnTaskRunner() drops null information.
    private void prefetchActiveTabTask(int activeTabId, TaskRunner taskRunner) {
        mPrefetchTabStateActiveTabTask =
                new BackgroundOnlyAsyncTask<@Nullable TabState>() {
                    @Override
                    protected @Nullable TabState doInBackground() {
                        return TabStateFileManager.restoreTabState(
                                getStateDirectory(), activeTabId, mCipherFactory);
                    }
                }.executeOnTaskRunner(taskRunner);
    }

    @Override
    public void addObserver(TabPersistentStoreObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabPersistentStoreObserver observer) {
        mObservers.removeObserver(observer);
    }

    // Static functions:

    /**
     * Records state of {@code selector} into a separate DataStructure to be used for save/restore.
     *
     * @param selector The {@link TabModelSelector} to process.
     * @param tabsBeingRestored Tabs that are in the process of being restored.
     * @return {@link TabModelSelectorMetadata} containing the meta data of {@code selector}.
     */
    @VisibleForTesting
    public static TabModelSelectorMetadata extractTabMetadataFromSelector(
            TabModelSelector selector, @Nullable List<TabRestoreDetails> tabsBeingRestored) {
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
            Tab activeTab = normalModel.getTabAtChecked(activeIndex);
            activeTabId = activeTab.getId();
            activeTabState =
                    UrlUtilities.isNtpUrl(activeTab.getUrl())
                            ? ActiveTabState.NTP
                            : ActiveTabState.OTHER;
        }

        // Add information about the tabs that haven't finished being loaded. We shouldn't have to
        // worry about Tab duplication because the tab details are processed only on the UI Thread.
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

        // TODO(https://crbug.com/445197903): This is a modification to shared prefs that may not be
        // correct if this store isn't authoritative. Move this into an observer.
        saveTabModelPrefs(activeTabId, activeTabState);
        return new TabModelSelectorMetadata(normalInfo, incognitoInfo);
    }

    @VisibleForTesting
    public static void saveTabModelPrefs(
            @TabId int activeTabId, @ActiveTabState int activeTabState) {
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
        int index = -1;
        for (Tab tab : tabModel) {
            index++;
            // This tab has likely just been deleted, and it's possible we're being notified before
            // hand because undo is not allowed. This shouldn't be persisted.
            if (tab.isClosing()) {
                // Select the previous tab if there is one. 0 should be fine even if there are no
                // tabs left.
                if (index == activeIndex) {
                    modelInfo.index = Math.max(0, modelInfo.ids.size() - 1);
                }
                continue;
            }

            if (index == activeIndex) {
                // If any non-active NTPs have been skipped, the serialized tab model index
                // needs to be adjusted.
                modelInfo.index = modelInfo.ids.size();
            } else if (TabPersistenceUtils.shouldSkipTab(tab)) {
                continue;
            }
            modelInfo.ids.add(tab.getId());
            modelInfo.urls.add(tab.getUrl().getSpec());
        }
        return modelInfo;
    }

    /**
     * @return The shared pref APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE. This is used when we need to
     *     know the last known tab state before the active tab from the tab state is read.
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

    // Static and instanced ForTest/Testing functions:

    public static void resetDeferredStartupCompleteForTesting() {
        sDeferredStartupComplete = false;
    }

    public @Nullable MigrateTabTask getMigrateTabTaskForTesting() {
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

    public void setMigrateTabTaskForTesting(AsyncTask<Void> migrateTabTask) {
        mMigrateTabTask = (MigrateTabTask) migrateTabTask;
    }

    protected Deque<Tab> getTabsToMigrateForTesting() {
        return mTabsToMigrate;
    }

    public SequencedTaskRunner getTaskRunnerForTesting() {
        return mSequencedTaskRunner;
    }

    public void addTabToRestoreForTesting(TabRestoreDetails tabDetails) {
        mTabsToRestore.add(tabDetails);
    }

    public TabPersistencePolicy getTabPersistencePolicyForTesting() {
        return mPersistencePolicy;
    }

    public @Nullable List<Pair<AsyncTask<@Nullable DataInputStream>, String>>
            getTabListToMergeTasksForTesting() {
        return mPrefetchTabListToMergeTasks;
    }

    public @Nullable AsyncTask<@Nullable TabState> getPrefetchTabStateActiveTabTaskForTesting() {
        return mPrefetchTabStateActiveTabTask;
    }

    public void setSequencedTaskRunnerForTesting(SequencedTaskRunner sequencedTaskRunner) {
        mSequencedTaskRunner = sequencedTaskRunner;
    }
}
