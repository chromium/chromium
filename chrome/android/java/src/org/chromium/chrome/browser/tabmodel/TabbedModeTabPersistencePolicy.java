// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.Pair;
import android.util.SparseBooleanArray;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

/** Handles the Tabbed mode specific behaviors of tab persistence. */
public class TabbedModeTabPersistencePolicy implements TabPersistencePolicy {
    private static final String TAG = "tabmodel";

    // This shared prefs file was used for storing tab group session counts. It is no longer in use.
    private static final String LEGACY_TAB_GROUP_PREFS_FILE = "tab_group_pref";

    // This shared prefs file was used for storing tab properties to use before tabs had loaded for
    // tab switching surfaces.
    private static final String LEGACY_TAB_ATTRIBUTE_CACHE_FILE = "tab_attribute_cache";

    /** <M53 The name of the file where the old tab metadata file is saved per directory. */
    @VisibleForTesting static final String LEGACY_SAVED_STATE_FILE = "tab_state";

    /** Prevents two copies of the Migration task from being created. */
    private static final Object MIGRATION_LOCK = new Object();

    /**
     * Prevents two clean up tasks from getting created simultaneously. Also protects against
     * incorrectly interleaving create/run/cancel on the task.
     */
    private static final Object CLEAN_UP_TASK_LOCK = new Object();

    /** Tracks whether tabs from two TabPersistentStores tabs are being merged together. */
    // TODO(crbug.com/40131185): Transit AtomicBoolean to an AtomicInteger to keep track the task id
    //        of activity being merged.
    private static final AtomicBoolean MERGE_IN_PROGRESS = new AtomicBoolean();

    private static AsyncTask<Void> sMigrationTask;
    private static AsyncTask<Void> sCleanupTask;

    private final @NonNull String mMetadataFileName;
    private final @Nullable String mOtherMetadataFileName;
    private final boolean mMergeTabsOnStartup;
    private final int mMaxSelectors;

    private TabContentManager mTabContentManager;
    private boolean mDestroyed;

    /**
     * Constructs a persistence policy that handles the Tabbed mode specific logic.
     *
     * @param metadataFileName The state file name to pull and save state to.
     * @param otherMetadataFileName The state file name to use for merging. Must be non-null if
     *     either {@code mergeTabsOnStartup} or {@code tabMergingEnabled} are true and is ignored if
     *     both are false.
     * @param mergeTabsOnStartup Whether this policy should handle merging tabs from all available
     *     tabbed mode files at startup.
     * @param tabMergingEnabled Whether tab merging operation should be done for multi-window/
     *     instance feature in general.
     */
    public TabbedModeTabPersistencePolicy(
            @NonNull String metadataFileName,
            @Nullable String otherMetadataFileName,
            boolean mergeTabsOnStartup,
            boolean tabMergingEnabled) {
        mMetadataFileName = metadataFileName;
        if (mergeTabsOnStartup || tabMergingEnabled) {
            assert otherMetadataFileName != null
                    : "otherMetadataFileName must be non-null if tab merging is enabled.";
            mOtherMetadataFileName = otherMetadataFileName;
        } else {
            mOtherMetadataFileName = null;
        }
        mMergeTabsOnStartup = mergeTabsOnStartup;
        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        mMaxSelectors = tabWindowManager.getMaxSimultaneousSelectors();
    }

    /**
     * Constructs a persistence policy that handles the Tabbed mode specific logic.
     *
     * @param selectorIndex The index that represents which state file to pull and save state to.
     *     This is used when there can be more than one TabModelSelector.
     * @param mergeTabsOnStartup Whether this policy should handle merging tabs from all available
     *     tabbed mode files at startup.
     * @param tabMergingEnabled Whether tab merging operation should be done for multi-window/
     *     instance feature in general.
     */
    public TabbedModeTabPersistencePolicy(
            int selectorIndex, boolean mergeTabsOnStartup, boolean tabMergingEnabled) {
        this(
                getMetadataFileNameForIndex(selectorIndex),
                getMetadataFileNameForIndex(selectorIndex == 0 ? 1 : 0),
                mergeTabsOnStartup,
                tabMergingEnabled);
    }

    @Override
    public File getOrCreateStateDirectory() {
        return TabStateDirectory.getOrCreateTabbedModeStateDirectory();
    }

    @Override
    public @NonNull String getMetadataFileName() {
        return mMetadataFileName;
    }

    @Override
    public boolean shouldMergeOnStartup() {
        return mMergeTabsOnStartup;
    }

    @Override
    public @Nullable String getMetadataFileNameToBeMerged() {
        return mOtherMetadataFileName;
    }

    /**
     * @param selectorIndex The index that represents which state file to pull and save state to.
     * @return The name of the state file.
     */
    public static String getMetadataFileNameForIndex(int selectorIndex) {
        return TabPersistentStore.getMetadataFileName(Integer.toString(selectorIndex));
    }

    @Override
    public boolean performInitialization(TaskRunner taskRunner) {
        ThreadUtils.assertOnUiThread();

        final boolean hasRunLegacyMigration =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.TABMODEL_HAS_RUN_FILE_MIGRATION, false);
        final boolean hasRunMultiInstanceMigration =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.TABMODEL_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION,
                                false);

        if (hasRunLegacyMigration && hasRunMultiInstanceMigration) return false;

        synchronized (MIGRATION_LOCK) {
            if (sMigrationTask != null) return true;
            sMigrationTask =
                    new BackgroundOnlyAsyncTask<Void>() {
                        @Override
                        protected Void doInBackground() {
                            if (!hasRunLegacyMigration) {
                                performLegacyMigration();
                            }

                            // It's possible that the legacy migration ran in the past but the
                            // preference wasn't set, because the legacy migration hasn't always
                            // set a preference upon completion. If the legacy migration has
                            // already been performed, performLecacyMigration() will exit early
                            // without renaming the metadata file, so the multi-instance migration
                            // is still necessary.
                            if (!hasRunMultiInstanceMigration) {
                                performMultiInstanceMigration();
                            }
                            return null;
                        }
                    }.executeOnTaskRunner(taskRunner);
            return true;
        }
    }

    /**
     * Upgrades users from an old version of Chrome when the state file was still in the root
     * directory.
     */
    @WorkerThread
    private void performLegacyMigration() {
        Log.w(TAG, "Starting to perform legacy migration.");
        File newFolder = getOrCreateStateDirectory();
        File[] newFiles = newFolder.listFiles();
        // Attempt migration if we have no tab state file in the new directory.
        if (newFiles == null || newFiles.length == 0) {
            File oldFolder = ContextUtils.getApplicationContext().getFilesDir();
            File modelFile = new File(oldFolder, LEGACY_SAVED_STATE_FILE);
            if (modelFile.exists()) {
                if (!modelFile.renameTo(new File(newFolder, getMetadataFileName()))) {
                    Log.e(TAG, "Failed to rename file: " + modelFile);
                }
            }

            File[] files = oldFolder.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (TabStateFileManager.parseInfoFromFilename(file.getName()) != null) {
                        if (!file.renameTo(new File(newFolder, file.getName()))) {
                            Log.e(TAG, "Failed to rename file: " + file);
                        }
                    }
                }
            }
        }
        setLegacyFileMigrationPref();
        Log.w(TAG, "Finished performing legacy migration.");
    }

    /**
     * Upgrades users from an older version of Chrome when the state files for multi-instance
     * were each kept in separate subdirectories.
     */
    @WorkerThread
    private void performMultiInstanceMigration() {
        Log.w(TAG, "Starting to perform multi-instance migration.");
        // 0. Do not rename the old metadata file if the new metadata file already exists. This
        //    should not happen, but if it does and the metadata file is overwritten then users
        //    may lose tabs. See crbug.com/649384.
        File stateDir = getOrCreateStateDirectory();
        File newMetadataFile = new File(stateDir, getMetadataFileName());
        File oldMetadataFile = new File(stateDir, LEGACY_SAVED_STATE_FILE);
        if (newMetadataFile.exists()) {
            Log.e(TAG, "New metadata file already exists");
        } else if (oldMetadataFile.exists()) {
            // 1. Rename tab metadata file for tab directory "0".
            if (!oldMetadataFile.renameTo(newMetadataFile)) {
                Log.e(TAG, "Failed to rename file: " + oldMetadataFile);
            }
        }

        // 2. Move files from other state directories.
        for (int i = TabModelSelectorImpl.CUSTOM_TABS_SELECTOR_INDEX; i < mMaxSelectors; i++) {
            // Skip the directory we're migrating to.
            if (i == 0) continue;

            File otherStateDir =
                    new File(
                            TabStateDirectory.getOrCreateBaseStateDirectory(), Integer.toString(i));
            if (otherStateDir == null || !otherStateDir.exists()) continue;

            // Rename tab state file.
            oldMetadataFile = new File(otherStateDir, LEGACY_SAVED_STATE_FILE);
            if (oldMetadataFile.exists()) {
                if (!oldMetadataFile.renameTo(new File(stateDir, getMetadataFileNameForIndex(i)))) {
                    Log.e(TAG, "Failed to rename file: " + oldMetadataFile);
                }
            }

            // Rename tab files.
            File[] files = otherStateDir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (TabStateFileManager.parseInfoFromFilename(file.getName()) != null) {
                        // Custom tabs does not currently use tab files. Delete them rather than
                        // migrating.
                        if (i == TabModelSelectorImpl.CUSTOM_TABS_SELECTOR_INDEX) {
                            if (!file.delete()) {
                                Log.e(TAG, "Failed to delete file: " + file);
                            }
                            continue;
                        }

                        // If the tab was moved between windows in Android N multi-window, the tab
                        // file may exist in both directories. Keep whichever was modified more
                        // recently.
                        File newFileName = new File(stateDir, file.getName());
                        if (newFileName.exists()
                                && newFileName.lastModified() > file.lastModified()) {
                            if (!file.delete()) {
                                Log.e(TAG, "Failed to delete file: " + file);
                            }
                        } else if (!file.renameTo(newFileName)) {
                            Log.e(TAG, "Failed to rename file: " + file);
                        }
                    }
                }
            }

            // Delete other state directory.
            if (!otherStateDir.delete()) {
                Log.e(TAG, "Failed to delete directory: " + otherStateDir);
            }
        }

        setMultiInstanceFileMigrationPref();
        Log.w(TAG, "Finished performing multi-instance migration.");
    }

    private void setLegacyFileMigrationPref() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TABMODEL_HAS_RUN_FILE_MIGRATION, true);
    }

    private void setMultiInstanceFileMigrationPref() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.TABMODEL_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION, true);
    }

    @Override
    public void waitForInitializationToFinish() {
        if (sMigrationTask == null) return;
        try {
            sMigrationTask.get();
        } catch (InterruptedException e) {
        } catch (ExecutionException e) {
        }
    }

    @Override
    public boolean isMergeInProgress() {
        return MERGE_IN_PROGRESS.get();
    }

    @Override
    public void setMergeInProgress(boolean isStarted) {
        MERGE_IN_PROGRESS.set(isStarted);
    }

    @Override
    public void cancelCleanupInProgress() {
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
        }
    }

    /**
     * {@inheritDoc}
     *
     * <p>Creates an asynchronous task to delete persistent data. The task is run using a thread
     * pool and may be executed in parallel with other tasks. The cleanup task use a combination of
     * the current model and the tab state files for other models to determine which tab files
     * should be deleted. The cleanup task should be canceled if a second tab model is created.
     */
    @Override
    public void cleanupUnusedFiles(Callback<TabPersistenceFileInfo> tabDataToDelete) {
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
            sCleanupTask =
                    new CleanUpTabStateDataTask(
                            tabDataToDelete, () -> getOtherTabsId(getMetadataFileName()));
            sCleanupTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    @Override
    public void cleanupInstanceState(int index, Callback<TabPersistenceFileInfo> tabDataToDelete) {
        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance().getTabModelSelectorById(index);
        if (selector != null) {
            // Remove all the tabs from the instance if it is in running state to be able to
            // delete the corresponding tab state file.
            for (int i = 0; i < selector.getModels().size(); i++) {
                TabModel tabModel = selector.getModels().get(i);
                while (tabModel.getCount() > 0) tabModel.removeTab(tabModel.getTabAt(0));
            }
        }
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
            sCleanupTask =
                    new CleanUpTabStateDataTask(
                            tabDataToDelete,
                            () -> getOtherTabsId(getMetadataFileNameForIndex(index)));
            sCleanupTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    /**
     * Gets the IDs of all tabs in the instances other than the specified one. IDs for custom tabs
     * are excluded. IDs for archived tabs are included.
     *
     * @param excludedMetadataFileName The metadata file name that is not included.
     */
    private SparseBooleanArray getOtherTabsId(String excludedMetadataFileName) {
        SparseBooleanArray tabIds = new SparseBooleanArray();
        for (String metadataFileName : getAllMetadataFileNames()) {
            // Although we check all selectors before deleting, we can only be sure that our own
            // selector will not go away between now and then. So, we read from disk all other
            // state files, even if they are already loaded by another selector.
            if (!excludedMetadataFileName.equals(metadataFileName)) {
                getTabsFromMetadataFile(tabIds, metadataFileName);
            }
        }
        return tabIds;
    }

    /**
     * Gets the IDs of all tabs in TabModelSelectors of a given instance.
     *
     * @param tabIds SparseBooleanArray to populate with TabIds.
     * @param metadataFileName The metadata file name for the corresponding instance.
     */
    private void getTabsFromMetadataFile(SparseBooleanArray tabIds, String metadataFileName) {
        File metadataFile = new File(getOrCreateStateDirectory(), metadataFileName);
        if (metadataFile.exists()) {
            DataInputStream stream = null;
            try {
                stream =
                        new DataInputStream(
                                new BufferedInputStream(new FileInputStream(metadataFile)));
                TabPersistentStore.readSavedMetadataFile(stream, null, tabIds);
            } catch (Exception e) {
                Log.e(TAG, "Unable to read state for " + metadataFile.getName() + ": " + e);
            } finally {
                StreamUtil.closeQuietly(stream);
            }
        }
    }

    @Override
    public void setTabContentManager(TabContentManager cache) {
        mTabContentManager = cache;
    }

    @Override
    public void notifyStateLoaded(int tabCountAtStartup) {
        RecordHistogram.recordCount1MHistogram("Tabs.CountAtStartup", tabCountAtStartup);
    }

    @Override
    public void destroy() {
        mTabContentManager = null;
        mDestroyed = true;
    }

    private class CleanUpTabStateDataTask extends AsyncTask<Void> {
        private final Callback<TabPersistenceFileInfo> mTabDataToDelete;

        private String[] mTabFileNames;
        private String[] mThumbnailFileNames;
        private Supplier<SparseBooleanArray> mOtherTabSupplier;
        private SparseBooleanArray mOtherTabIds; // Tab in use by other selectors, not be deleted.

        CleanUpTabStateDataTask(
                Callback<TabPersistenceFileInfo> storedTabDataId,
                Supplier<SparseBooleanArray> otherTabsSupplier) {
            mTabDataToDelete = storedTabDataId;
            mOtherTabSupplier = otherTabsSupplier;
        }

        @Override
        protected Void doInBackground() {
            if (mDestroyed) return null;

            mTabFileNames = getOrCreateStateDirectory().list();
            String thumbnailDirectory = PathUtils.getThumbnailCacheDirectory();
            mThumbnailFileNames = new File(thumbnailDirectory).list();
            mOtherTabIds = mOtherTabSupplier.get();

            ContextUtils.getApplicationContext()
                    .deleteSharedPreferences(LEGACY_TAB_GROUP_PREFS_FILE);
            ContextUtils.getApplicationContext()
                    .deleteSharedPreferences(LEGACY_TAB_ATTRIBUTE_CACHE_FILE);

            return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            if (mDestroyed) return;
            TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();

            TabPersistenceFileInfo storedTabDataToDelete = new TabPersistenceFileInfo();
            if (mTabFileNames != null) {
                for (String fileName : mTabFileNames) {
                    Pair<Integer, Boolean> data =
                            TabStateFileManager.parseInfoFromFilename(fileName);
                    if (data != null) {
                        int tabId = data.first;
                        if (shouldDeleteTabFile(tabId, tabWindowManager)) {
                            storedTabDataToDelete.addTabStateFileInfo(tabId, data.second);
                        }
                    }
                }
            }
            // Invoke the callback even if filesToDelete is empty since it could perform other
            // cleanups.
            mTabDataToDelete.onResult(storedTabDataToDelete);

            if (mTabContentManager != null && mThumbnailFileNames != null) {
                HashSet<Integer> checkedTabIds = new HashSet<>();
                for (String fileName : mThumbnailFileNames) {
                    // Handle *.jpeg.
                    int lastDotIdx = fileName.lastIndexOf(".");
                    if (lastDotIdx != -1) {
                        fileName = fileName.substring(0, lastDotIdx);
                    }
                    try {
                        int tabId = Integer.parseInt(fileName);
                        // Avoid double removal which could be expensive.
                        if (!checkedTabIds.add(tabId)) continue;

                        if (shouldDeleteTabFile(tabId, tabWindowManager)) {
                            mTabContentManager.removeTabThumbnail(tabId);
                        }
                    } catch (NumberFormatException expected) {
                        // This is an unknown file name, we'll leave it there.
                    }
                }
            }

            synchronized (CLEAN_UP_TASK_LOCK) {
                sCleanupTask = null;
            }
        }

        private boolean shouldDeleteTabFile(int tabId, TabWindowManager tabWindowManager) {
            return tabWindowManager.canTabStateBeDeleted(tabId) && !mOtherTabIds.get(tabId);
        }

        @Override
        protected void onCancelled(Void result) {
            super.onCancelled(null);
            synchronized (CLEAN_UP_TASK_LOCK) {
                sCleanupTask = null;
            }
        }
    }

    @Override
    public void getAllTabIds(Callback<SparseBooleanArray> tabIdsCallback) {
        PostTask.postTask(
                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                () -> {
                    SparseBooleanArray tabIds = new SparseBooleanArray();
                    for (String metadataFileName : getAllMetadataFileNames()) {
                        getTabsFromMetadataFile(tabIds, metadataFileName);
                    }
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                tabIdsCallback.onResult(tabIds);
                            });
                });
    }

    /** Get all the state file names excluding archived. */
    private List<String> getAllMetadataFileNames() {
        List<String> metadataFileNames = new ArrayList<>();
        for (int i = 0; i < mMaxSelectors; i++) {
            metadataFileNames.add(getMetadataFileNameForIndex(i));
        }
        return metadataFileNames;
    }

    protected static void resetMigrationTaskForTesting() {
        sMigrationTask = null;
    }
}
