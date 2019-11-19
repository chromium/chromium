// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.SharedPreferences;
import android.os.StrictMode;
import android.util.Pair;
import android.util.SparseBooleanArray;

import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tab.TabState;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Handles the Tabbed mode specific behaviors of tab persistence.
 */
public class TabbedModeTabPersistencePolicy implements TabPersistencePolicy {

    private static final String TAG = "tabmodel";

    /** <M53 The name of the file where the old tab metadata file is saved per directory. */
    @VisibleForTesting
    static final String LEGACY_SAVED_STATE_FILE = "tab_state";

    @VisibleForTesting
    static final String PREF_HAS_RUN_FILE_MIGRATION =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_RUN_FILE_MIGRATION";

    @VisibleForTesting
    static final String PREF_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore."
            + "HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION";

    /** The name of the directory where the state is saved. */
    @VisibleForTesting
    static final String SAVED_STATE_DIRECTORY = "0";

    /** Prevents two copies of the Migration task from being created. */
    private static final Object MIGRATION_LOCK = new Object();
    /** Prevents two state directories from getting created simultaneously. */
    private static final Object DIR_CREATION_LOCK = new Object();
    /**
     * Prevents two clean up tasks from getting created simultaneously. Also protects against
     * incorrectly interleaving create/run/cancel on the task.
     */
    private static final Object CLEAN_UP_TASK_LOCK = new Object();
    /** Tracks whether tabs from two TabPersistentStores tabs are being merged together. */
    private static final AtomicBoolean MERGE_IN_PROGRESS = new AtomicBoolean();

    private static AsyncTask<Void> sMigrationTask;
    private static AsyncTask<Void> sCleanupTask;

    private static File sStateDirectory;

    private final SharedPreferences mPreferences;
    private final int mSelectorIndex;
    private final int mOtherSelectorIndex;
    private final boolean mMergeTabs;

    private TabContentManager mTabContentManager;
    private boolean mDestroyed;

    /**
     * Constructs a persistence policy that handles the Tabbed mode specific logic.
     * @param selectorIndex The index that represents which state file to pull and save state to.
     *                      This is used when there can be more than one TabModelSelector.
     * @param mergeTabs     Whether this policy should handle merging tabs from all available
     *                      tabbed mode files.
     */
    public TabbedModeTabPersistencePolicy(int selectorIndex, boolean mergeTabs) {
        mPreferences = ContextUtils.getAppSharedPreferences();
        mSelectorIndex = selectorIndex;
        mOtherSelectorIndex = selectorIndex == 0 ? 1 : 0;
        mMergeTabs = mergeTabs;
    }

    @Override
    public File getOrCreateStateDirectory() {
        return getOrCreateTabbedModeStateDirectory();
    }

    @Override
    public String getStateFileName() {
        return getStateFileName(mSelectorIndex);
    }

    @Override
    public boolean shouldMergeOnStartup() {
        return mMergeTabs;
    }

    @Override
    public List<String> getStateToBeMergedFileNames() {
        List<String> mergedFileNames = new ArrayList<>();
        if (FeatureUtilities.isTabModelMergingEnabled()) {
            mergedFileNames.add(getStateFileName(mOtherSelectorIndex));
        }
        // TODO(peconn): Can I clean up this code now that Browser Actions are gone?
        return mergedFileNames;
    }

    /**
     * @param selectorIndex The index that represents which state file to pull and save state to.
     * @return The name of the state file.
     */
    @VisibleForTesting
    public static String getStateFileName(int selectorIndex) {
        return TabPersistentStore.getStateFileName(Integer.toString(selectorIndex));
    }

    /**
     * The folder where the state should be saved to.
     * @return A file representing the directory that contains TabModelSelector states.
     */
    public static File getOrCreateTabbedModeStateDirectory() {
        synchronized (DIR_CREATION_LOCK) {
            if (sStateDirectory == null) {
                sStateDirectory = new File(
                        TabPersistentStore.getOrCreateBaseStateDirectory(), SAVED_STATE_DIRECTORY);
                StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
                try {
                    if (!sStateDirectory.exists() && !sStateDirectory.mkdirs()) {
                        Log.e(TAG, "Failed to create state folder: " + sStateDirectory);
                    }
                } finally {
                    StrictMode.setThreadPolicy(oldPolicy);
                }
            }
        }
        return sStateDirectory;
    }

    @Override
    public boolean performInitialization(TaskRunner taskRunner) {
        ThreadUtils.assertOnUiThread();

        final boolean hasRunLegacyMigration =
                mPreferences.getBoolean(PREF_HAS_RUN_FILE_MIGRATION, false);
        final boolean hasRunMultiInstanceMigration =
                mPreferences.getBoolean(PREF_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION, false);

        if (hasRunLegacyMigration && hasRunMultiInstanceMigration) return false;

        synchronized (MIGRATION_LOCK) {
            if (sMigrationTask != null) return true;
            sMigrationTask = new BackgroundOnlyAsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    if (!hasRunLegacyMigration) {
                        performLegacyMigration();
                    }

                    // It's possible that the legacy migration ran in the past but the preference
                    // wasn't set, because the legacy migration hasn't always set a preference upon
                    // completion. If the legacy migration has already been performed,
                    // performLecacyMigration() will exit early without renaming the metadata file,
                    // so the multi-instance migration is still necessary.
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
                if (!modelFile.renameTo(new File(newFolder, getStateFileName()))) {
                    Log.e(TAG, "Failed to rename file: " + modelFile);
                }
            }

            File[] files = oldFolder.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (TabState.parseInfoFromFilename(file.getName()) != null) {
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
        File newMetadataFile = new File(stateDir, getStateFileName());
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
        for (int i = TabModelSelectorImpl.CUSTOM_TABS_SELECTOR_INDEX;
                i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
            // Skip the directory we're migrating to.
            if (i == 0) continue;

            File otherStateDir = new File(
                    TabPersistentStore.getOrCreateBaseStateDirectory(), Integer.toString(i));
            if (otherStateDir == null || !otherStateDir.exists()) continue;

            // Rename tab state file.
            oldMetadataFile = new File(otherStateDir, LEGACY_SAVED_STATE_FILE);
            if (oldMetadataFile.exists()) {
                if (!oldMetadataFile.renameTo(new File(stateDir, getStateFileName(i)))) {
                    Log.e(TAG, "Failed to rename file: " + oldMetadataFile);
                }
            }

            // Rename tab files.
            File[] files = otherStateDir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (TabState.parseInfoFromFilename(file.getName()) != null) {
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
        mPreferences.edit().putBoolean(PREF_HAS_RUN_FILE_MIGRATION, true).apply();
    }

    private void setMultiInstanceFileMigrationPref() {
        mPreferences.edit().putBoolean(PREF_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION, true).apply();
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
     * <p>
     * Creates an asynchronous task to delete persistent data. The task is run using a thread pool
     * and may be executed in parallel with other tasks. The cleanup task use a combination of the
     * current model and the tab state files for other models to determine which tab files should
     * be deleted. The cleanup task should be canceled if a second tab model is created.
     */
    @Override
    public void cleanupUnusedFiles(Callback<List<String>> filesToDelete) {
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
            sCleanupTask = new CleanUpTabStateDataTask(filesToDelete);
            sCleanupTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    @Override
    public void setTabContentManager(TabContentManager cache) {
        mTabContentManager = cache;
    }

    @Override
    public void notifyStateLoaded(int tabCountAtStartup) {
        RecordHistogram.recordCountHistogram("Tabs.CountAtStartup", tabCountAtStartup);
    }

    @Override
    public void destroy() {
        mDestroyed = true;
    }

    private class CleanUpTabStateDataTask extends AsyncTask<Void> {
        private final Callback<List<String>> mFilesToDeleteCallback;

        private String[] mTabFileNames;
        private String[] mThumbnailFileNames;
        private SparseBooleanArray mOtherTabIds;

        CleanUpTabStateDataTask(Callback<List<String>> filesToDelete) {
            mFilesToDeleteCallback = filesToDelete;
        }

        @Override
        protected Void doInBackground() {
            if (mDestroyed) return null;

            mTabFileNames = getOrCreateStateDirectory().list();
            String thumbnailDirectory = PathUtils.getThumbnailCacheDirectory();
            mThumbnailFileNames = new File(thumbnailDirectory).list();

            mOtherTabIds = new SparseBooleanArray();
            getTabsFromOtherStateFiles(mOtherTabIds);
            return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            if (mDestroyed) return;
            TabWindowManager tabWindowManager = TabWindowManager.getInstance();

            if (mTabFileNames != null) {
                List<String> filesToDelete = new ArrayList<>();
                for (String fileName : mTabFileNames) {
                    Pair<Integer, Boolean> data = TabState.parseInfoFromFilename(fileName);
                    if (data != null) {
                        int tabId = data.first;
                        if (shouldDeleteTabFile(tabId, tabWindowManager)) {
                            filesToDelete.add(fileName);
                        }
                    }
                }
                mFilesToDeleteCallback.onResult(filesToDelete);
            }
            if (mTabContentManager != null && mThumbnailFileNames != null) {
                for (String fileName : mThumbnailFileNames) {
                    try {
                        int tabId = Integer.parseInt(fileName);
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
            return !tabWindowManager.tabExistsInAnySelector(tabId) && !mOtherTabIds.get(tabId);
        }

        /**
         * Gets the IDs of all tabs in TabModelSelectors other than the currently selected one. IDs
         * for custom tabs are excluded.
         * @param tabIds SparseBooleanArray to populate with TabIds.
         */
        private void getTabsFromOtherStateFiles(SparseBooleanArray tabIds) {
            for (int i = 0; i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
                // Although we check all selectors before deleting, we can only be sure that our own
                // selector will not go away between now and then. So, we read from disk all other
                // state files, even if they are already loaded by another selector.
                if (i == mSelectorIndex) continue;

                File metadataFile = new File(getOrCreateStateDirectory(), getStateFileName(i));
                if (metadataFile.exists()) {
                    DataInputStream stream = null;
                    try {
                        stream = new DataInputStream(
                                new BufferedInputStream(new FileInputStream(metadataFile)));
                        TabPersistentStore.readSavedStateFile(stream, null, tabIds, false);
                    } catch (Exception e) {
                        Log.e(TAG, "Unable to read state for " + metadataFile.getName() + ": " + e);
                    } finally {
                        StreamUtil.closeQuietly(stream);
                    }
                }
            }
        }

        @Override
        protected void onCancelled(Void result) {
            super.onCancelled(result);
            synchronized (CLEAN_UP_TASK_LOCK) {
                sCleanupTask = null;
            }
        }
    }
}
