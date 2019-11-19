// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.os.StrictMode;
import android.util.Pair;
import android.util.SparseBooleanArray;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutionException;

import javax.inject.Inject;

/**
 * Handles the Custom Tab specific behaviors of tab persistence.
 */
@ActivityScope
public class CustomTabTabPersistencePolicy implements TabPersistencePolicy {
    static final String SAVED_STATE_DIRECTORY = "custom_tabs";

    /** Threshold where old state files should be deleted (30 days). */
    protected static final long STATE_EXPIRY_THRESHOLD = 30L * 24 * 60 * 60 * 1000;

    /** Maximum number of state files before we should start deleting old ones. */
    protected static final int MAXIMUM_STATE_FILES = 30;

    private static final String TAG = "tabmodel";

    /** Prevents two state directories from getting created simultaneously. */
    private static final Object DIR_CREATION_LOCK = new Object();

    /**
     * Prevents two clean up tasks from getting created simultaneously. Also protects against
     * incorrectly interleaving create/run/cancel on the task.
     */
    private static final Object CLEAN_UP_TASK_LOCK = new Object();

    private static File sStateDirectory;
    private static AsyncTask<Void> sCleanupTask;

    /**
     * The folder where the state should be saved to.
     * @return A file representing the directory that contains TabModelSelector states.
     */
    public static File getOrCreateCustomTabModeStateDirectory() {
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

    private final int mTaskId;
    private final boolean mShouldRestore;

    private AsyncTask<Void> mInitializationTask;
    private SequencedTaskRunner mTaskRunner;
    private boolean mDestroyed;

    @Inject
    public CustomTabTabPersistencePolicy(ChromeActivity activity) {
        mTaskId = activity.getTaskId();
        mShouldRestore = (activity.getSavedInstanceState() != null);
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
        return getOrCreateCustomTabModeStateDirectory();
    }

    @Override
    public String getStateFileName() {
        return TabPersistentStore.getStateFileName(Integer.toString(mTaskId));
    }

    @Override
    public boolean shouldMergeOnStartup() {
        return false;
    }

    @Override
    @Nullable
    public List<String> getStateToBeMergedFileNames() {
        return null;
    }

    @Override
    public boolean performInitialization(TaskRunner taskRunner) {
        mInitializationTask = new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                File stateDir = getOrCreateStateDirectory();
                File metadataFile = new File(stateDir, getStateFileName());
                if (metadataFile.exists()) {
                    if (mShouldRestore) {
                        if (!metadataFile.setLastModified(System.currentTimeMillis())) {
                            Log.e(TAG, "Unable to update last modified time: " + metadataFile);
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
    public void cleanupUnusedFiles(Callback<List<String>> filesToDelete) {
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
            sCleanupTask = new CleanUpTabStateDataTask(filesToDelete);
            sCleanupTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    @Override
    public void setTabContentManager(TabContentManager cache) {
    }

    @Override
    public void notifyStateLoaded(int tabCountAtStartup) {
    }

    @Override
    public void destroy() {
        mDestroyed = true;
    }

    @Override
    public void setTaskRunner(SequencedTaskRunner taskRunner) {
        mTaskRunner = taskRunner;
    }

    /**
     * Triggers an async deletion of the tab state metadata file.
     */
    public void deleteMetadataStateFileAsync() {
        assert mTaskRunner != null;
        mTaskRunner.postTask(() -> {
            File stateDir = getOrCreateStateDirectory();
            File metadataFile = new File(stateDir, getStateFileName());
            if (metadataFile.exists() && !metadataFile.delete()) {
                Log.e(TAG, "Failed to delete file: " + metadataFile);
            }
        });
    }

    /**
     * Given a list of metadata files, determine which are applicable for deletion based on the
     * deletion strategy of Custom Tabs.
     *
     * @param currentTimeMillis The current time in milliseconds
     *                          ({@link System#currentTimeMillis()}.
     * @param allMetadataFiles The complete list of all metadata files to check.
     * @return The list of metadata files that are applicable for deletion.
     */
    protected static List<File> getMetadataFilesForDeletion(
            long currentTimeMillis, List<File> allMetadataFiles) {
        Collections.sort(allMetadataFiles, new Comparator<File>() {
            @Override
            public int compare(File lhs, File rhs) {
                long lhsModifiedTime = lhs.lastModified();
                long rhsModifiedTime = rhs.lastModified();

                // Sort such that older files (those with an lower timestamp number) are at the
                // end of the sorted listed.
                return ApiCompatibilityUtils.compareLong(rhsModifiedTime, lhsModifiedTime);
            }
        });

        List<File> stateFilesApplicableForDeletion = new ArrayList<File>();
        for (int i = 0; i < allMetadataFiles.size(); i++) {
            File file = allMetadataFiles.get(i);
            long fileAge = currentTimeMillis - file.lastModified();
            if (i >= MAXIMUM_STATE_FILES || fileAge >= STATE_EXPIRY_THRESHOLD) {
                stateFilesApplicableForDeletion.add(file);
            }
        }
        return stateFilesApplicableForDeletion;
    }

    /**
     * Get all current Tab IDs used by the specified activity.
     *
     * @param activity The activity whose tab IDs are to be collected from.
     * @param tabIds Where the tab IDs should be added to.
     */
    private static void getAllTabIdsForActivity(CustomTabActivity activity, Set<Integer> tabIds) {
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
            if (!(activity instanceof CustomTabActivity)) continue;
            getAllTabIdsForActivity((CustomTabActivity) activity, liveTabIds);
            liveTaskIds.add(activity.getTaskId());
        }
    }

    private class CleanUpTabStateDataTask extends AsyncTask<Void> {
        private final Callback<List<String>> mFilesToDeleteCallback;

        private Set<Integer> mUnreferencedTabIds;
        private List<File> mDeletableMetadataFiles;
        private Map<File, SparseBooleanArray> mTabIdsByMetadataFile;

        CleanUpTabStateDataTask(Callback<List<String>> filesToDelete) {
            mFilesToDeleteCallback = filesToDelete;
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
                if (TabPersistentStore.isStateFile(file.getName())) {
                    metadataFiles.add(file);

                    SparseBooleanArray tabIds = new SparseBooleanArray();
                    mTabIdsByMetadataFile.put(file, tabIds);
                    getTabsFromStateFile(tabIds, file);
                    for (int i = 0; i < tabIds.size(); i++) {
                        allReferencedTabIds.add(tabIds.keyAt(i));
                    }
                    continue;
                }

                Pair<Integer, Boolean> tabInfo = TabState.parseInfoFromFilename(file.getName());
                if (tabInfo == null) continue;
                allTabIds.add(tabInfo.first);
            }

            mUnreferencedTabIds.addAll(allTabIds);
            mUnreferencedTabIds.removeAll(allReferencedTabIds);

            mDeletableMetadataFiles = getMetadataFilesForDeletion(
                    System.currentTimeMillis(), metadataFiles);
            return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            List<String> filesToDelete = new ArrayList<>();
            if (mDestroyed) {
                mFilesToDeleteCallback.onResult(filesToDelete);
                return;
            }

            if (mUnreferencedTabIds.isEmpty() && mDeletableMetadataFiles.isEmpty()) {
                mFilesToDeleteCallback.onResult(filesToDelete);
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
                filesToDelete.add(TabState.getTabStateFilename(unreferencedTabId, false));
            }

            for (int i = 0; i < mDeletableMetadataFiles.size(); i++) {
                File metadataFile = mDeletableMetadataFiles.get(i);
                String id = TabPersistentStore.getStateFileUniqueId(metadataFile.getName());
                try {
                    int taskId = Integer.parseInt(id);

                    // Ignore the metadata file if it belongs to a currently live CustomTabActivity.
                    if (liveTaskIds.contains(taskId)) continue;

                    filesToDelete.add(metadataFile.getName());

                    SparseBooleanArray unusedTabIds = mTabIdsByMetadataFile.get(metadataFile);
                    if (unusedTabIds == null) continue;
                    for (int j = 0; j < unusedTabIds.size(); j++) {
                        filesToDelete.add(TabState.getTabStateFilename(
                                unusedTabIds.keyAt(j), false));
                    }
                } catch (NumberFormatException ex) {
                    assert false : "Unexpected tab metadata file found: " + metadataFile.getName();
                    continue;
                }
            }

            mFilesToDeleteCallback.onResult(filesToDelete);

            synchronized (CLEAN_UP_TASK_LOCK) {
                sCleanupTask = null; // Release static reference to external callback
            }
        }

        private void getTabsFromStateFile(SparseBooleanArray tabIds, File metadataFile) {
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

        @Override
        protected void onCancelled(Void result) {
            super.onCancelled(result);
            synchronized (CLEAN_UP_TASK_LOCK) {
                sCleanupTask = null;
            }
        }
    }
}
