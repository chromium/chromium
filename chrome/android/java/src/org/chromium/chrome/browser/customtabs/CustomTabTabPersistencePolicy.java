// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.app.Activity;
import android.os.Bundle;
import android.util.Pair;
import android.util.SparseBooleanArray;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistenceFileInfo;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutionException;

import javax.inject.Inject;
import javax.inject.Named;

/** Handles the Custom Tab specific behaviors of tab persistence. */
@ActivityScope
public class CustomTabTabPersistencePolicy implements TabPersistencePolicy {
    private static final String TAG = "tabmodel";

    /**
     * Prevents two clean up tasks from getting created simultaneously. Also protects against
     * incorrectly interleaving create/run/cancel on the task.
     */
    private static final Object CLEAN_UP_TASK_LOCK = new Object();

    private static AsyncTask<Void> sCleanupTask;

    private final int mTaskId;
    private final boolean mShouldRestore;

    private AsyncTask<Void> mInitializationTask;
    private SequencedTaskRunner mTaskRunner;
    private boolean mDestroyed;

    @Inject
    public CustomTabTabPersistencePolicy(
            Activity activity,
            @Named(SAVED_INSTANCE_SUPPLIER) Supplier<Bundle> savedInstanceStateSupplier) {
        mTaskId = activity.getTaskId();
        mShouldRestore = (savedInstanceStateSupplier.get() != null);
    }

    /**
     * Constructor for slightly simplifying testing.
     *
     * @param taskId The task ID that the owning Custom Tab is in.
     * @param shouldRestore Whether an attempt to restore tab state information should be done on
     *                      startup.
     */
    @VisibleForTesting
    CustomTabTabPersistencePolicy(int taskId, boolean shouldRestore) {
        mTaskId = taskId;
        mShouldRestore = shouldRestore;
    }

    @Override
    public File getOrCreateStateDirectory() {
        return TabStateDirectory.getOrCreateCustomTabModeStateDirectory();
    }

    @Override
    public String getMetadataFileName() {
        return TabPersistentStore.getMetadataFileName(Integer.toString(mTaskId));
    }

    @Override
    public boolean shouldMergeOnStartup() {
        return false;
    }

    @Override
    @Nullable
    public String getMetadataFileNameToBeMerged() {
        return null;
    }

    @Override
    public boolean performInitialization(TaskRunner taskRunner) {
        mInitializationTask =
                new BackgroundOnlyAsyncTask<Void>() {
                    @Override
                    protected Void doInBackground() {
                        File stateDir = getOrCreateStateDirectory();
                        File metadataFile = new File(stateDir, getMetadataFileName());
                        if (metadataFile.exists()) {
                            if (mShouldRestore) {
                                if (!metadataFile.setLastModified(System.currentTimeMillis())) {
                                    Log.e(
                                            TAG,
                                            "Unable to update last modified time: " + metadataFile);
                                }
                            } else {
                                if (!metadataFile.delete()) {
                                    Log.e(TAG, "Failed to delete file: " + metadataFile);
                                }
                            }
                        }
                        return null;
                    }
                }.executeOnTaskRunner(taskRunner);

        return true;
    }

    @Override
    public void waitForInitializationToFinish() {
        if (mInitializationTask == null) return;
        try {
            mInitializationTask.get();
        } catch (InterruptedException | ExecutionException e) {
            // Ignore and proceed.
        }
    }

    @Override
    public boolean isMergeInProgress() {
        return false;
    }

    @Override
    public void setMergeInProgress(boolean isStarted) {
        assert false : "Merge not supported in Custom Tabs";
    }

    @Override
    public void cancelCleanupInProgress() {
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
        }
    }

    @Override
    public void cleanupUnusedFiles(Callback<TabPersistenceFileInfo> tabDataToDelete) {
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
            sCleanupTask = new CleanUpTabStateDataTask(tabDataToDelete);
            sCleanupTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    @Override
    public void setTabContentManager(TabContentManager cache) {}

    @Override
    public void notifyStateLoaded(int tabCountAtStartup) {}

    @Override
    public void destroy() {
        mDestroyed = true;
    }

    @Override
    public void setTaskRunner(SequencedTaskRunner taskRunner) {
        mTaskRunner = taskRunner;
    }

    /** Triggers an async deletion of the tab state metadata file. */
    public void deleteMetadataStateFileAsync() {
        assert mTaskRunner != null;
        mTaskRunner.execute(
                () -> {
                    File stateDir = getOrCreateStateDirectory();
                    File metadataFile = new File(stateDir, getMetadataFileName());
                    if (metadataFile.exists() && !metadataFile.delete()) {
                        Log.e(TAG, "Failed to delete file: " + metadataFile);
                    }
                });
    }

    /**
     * Get all current Tab IDs used by the specified activity.
     *
     * @param activity The activity whose tab IDs are to be collected from.
     * @param tabIds Where the tab IDs should be added to.
     */
    private static void getAllTabIdsForActivity(
            BaseCustomTabActivity activity, Set<Integer> tabIds) {
        if (activity == null) return;
        TabModelSelector selector = activity.getTabModelSelector();
        if (selector == null) return;
        List<TabModel> models = selector.getModels();
        for (int i = 0; i < models.size(); i++) {
            TabModel model = models.get(i);
            for (int j = 0; j < model.getCount(); j++) {
                tabIds.add(model.getTabAt(j).getId());
            }
        }
    }

    /**
     * Gathers all of the tab IDs and task IDs for all currently live Custom Tabs.
     *
     * @param liveTabIds Where tab IDs will be added.
     * @param liveTaskIds Where task IDs will be added.
     */
    protected static void getAllLiveTabAndTaskIds(
            Set<Integer> liveTabIds, Set<Integer> liveTaskIds) {
        ThreadUtils.assertOnUiThread();

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity instanceof BaseCustomTabActivity customActivity) {
                getAllTabIdsForActivity(customActivity, liveTabIds);
                liveTaskIds.add(customActivity.getTaskId());
            }
        }
    }

    private class CleanUpTabStateDataTask extends AsyncTask<Void> {
        private final Callback<TabPersistenceFileInfo> mTabDataToDeleteCallback;

        private Set<Integer> mUnreferencedTabIds;
        private List<File> mDeletableMetadataFiles;
        private Map<File, SparseBooleanArray> mTabIdsByMetadataFile;

        CleanUpTabStateDataTask(Callback<TabPersistenceFileInfo> storedTabDataToDeleteCallback) {
            mTabDataToDeleteCallback = storedTabDataToDeleteCallback;
        }

        @Override
        protected Void doInBackground() {
            if (mDestroyed) return null;

            mTabIdsByMetadataFile = new HashMap<>();
            mUnreferencedTabIds = new HashSet<>();

            File[] stateFiles = getOrCreateStateDirectory().listFiles();
            if (stateFiles == null) return null;

            Set<Integer> allTabIds = new HashSet<>();
            Set<Integer> allReferencedTabIds = new HashSet<>();
            List<File> metadataFiles = new ArrayList<>();
            for (File file : stateFiles) {
                if (TabPersistentStore.isMetadataFile(file.getName())) {
                    metadataFiles.add(file);

                    SparseBooleanArray tabIds = new SparseBooleanArray();
                    mTabIdsByMetadataFile.put(file, tabIds);
                    getTabsFromStateFile(tabIds, file);
                    for (int i = 0; i < tabIds.size(); i++) {
                        allReferencedTabIds.add(tabIds.keyAt(i));
                    }
                    continue;
                }

                Pair<Integer, Boolean> tabInfo =
                        TabStateFileManager.parseInfoFromFilename(file.getName());
                if (tabInfo == null) continue;
                allTabIds.add(tabInfo.first);
            }

            mUnreferencedTabIds.addAll(allTabIds);
            mUnreferencedTabIds.removeAll(allReferencedTabIds);

            mDeletableMetadataFiles =
                    CustomTabFileUtils.getFilesForDeletion(
                            System.currentTimeMillis(), metadataFiles);
            return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            TabPersistenceFileInfo tabDataToDelete = new TabPersistenceFileInfo();
            if (mDestroyed) {
                mTabDataToDeleteCallback.onResult(tabDataToDelete);
                return;
            }

            if (mUnreferencedTabIds.isEmpty() && mDeletableMetadataFiles.isEmpty()) {
                mTabDataToDeleteCallback.onResult(tabDataToDelete);
                return;
            }

            Set<Integer> liveTabIds = new HashSet<>();
            Set<Integer> liveTaskIds = new HashSet<>();
            getAllLiveTabAndTaskIds(liveTabIds, liveTaskIds);

            for (Integer unreferencedTabId : mUnreferencedTabIds) {
                // Ignore tabs that are referenced by live activities as they might not have been
                // able to write out their state yet.
                if (liveTabIds.contains(unreferencedTabId)) continue;

                // The tab state is not referenced by any current activities or any metadata files,
                // so mark it for deletion.
                tabDataToDelete.addTabStateFileInfo(unreferencedTabId, false);
            }

            for (int i = 0; i < mDeletableMetadataFiles.size(); i++) {
                File metadataFile = mDeletableMetadataFiles.get(i);
                String id = TabPersistentStore.getMetadataFileUniqueTag(metadataFile.getName());
                try {
                    int taskId = Integer.parseInt(id);

                    // Ignore the metadata file if it belongs to a currently live
                    // BaseCustomTabActivity.
                    if (liveTaskIds.contains(taskId)) continue;

                    tabDataToDelete.addMetadataFile(metadataFile.getName());

                    SparseBooleanArray unusedTabIds = mTabIdsByMetadataFile.get(metadataFile);
                    if (unusedTabIds == null) continue;
                    for (int j = 0; j < unusedTabIds.size(); j++) {
                        tabDataToDelete.addTabStateFileInfo(unusedTabIds.keyAt(j), false);
                    }
                } catch (NumberFormatException ex) {
                    assert false : "Unexpected tab metadata file found: " + metadataFile.getName();
                    continue;
                }
            }

            mTabDataToDeleteCallback.onResult(tabDataToDelete);

            synchronized (CLEAN_UP_TASK_LOCK) {
                sCleanupTask = null; // Release static reference to external callback
            }
        }

        private void getTabsFromStateFile(SparseBooleanArray tabIds, File metadataFile) {
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
        // This function is currently only used for PersistedTabData maintenance.
        // PersistedTabData doesn't currently support Custom Tabs.
        assert false : "Not currently supported for Custom Tabs";
    }
}
