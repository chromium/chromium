// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.SparseBooleanArray;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.chrome.browser.tab_ui.TabContentManager;

import java.io.File;

/** Policy that handles the Activity specific behaviors regarding the persistence of tab data. */
public interface TabPersistencePolicy {

    /**
     * @return File representing the directory that is used to store Tab state
     *         information.
     */
    File getOrCreateStateDirectory();

    /**
     * Returns the filename of the primary metadata file containing information about the tabs to be
     * loaded.
     */
    @NonNull
    String getMetadataFileName();

    /**
     * @return Whether a merge needs to be performed on startup.
     */
    boolean shouldMergeOnStartup();

    /** Returns the filename of the metadata file that is to be merged. */
    @Nullable
    String getMetadataFileNameToBeMerged();

    /**
     * Performs any necessary initialization required before accessing the tab information.  This
     * can include cleanups or migrations that must occur before the tab state information can be
     * read reliably.
     *
     * @param taskRunner The task runner that any asynchronous tasks should be run on.
     * @return Whether any blocking initialization is necessary.
     */
    boolean performInitialization(TaskRunner taskRunner);

    /**
     * Waits for the any pending initialization to finish.
     *
     * @see #performInitialization(TaskRunner)
     */
    void waitForInitializationToFinish();

    /**
     * @return Whether a merge is currently in progress.
     */
    // TODO(tedchoc): Merging is currently very tabbed mode specific.  Investigate moving more
    //                of the merging logic into this class and out of the main persistence store.
    boolean isMergeInProgress();

    /**
     * Marks whether a merge operation has begun or ended.
     * @param isStarted Whether a merge has been started.
     */
    void setMergeInProgress(boolean isStarted);

    /** Cancels any pending cleanups in progress. */
    void cancelCleanupInProgress();

    /**
     * Trigger a clean up for any unused files (both individual state files for tabs as well as
     * state files for the models).
     *
     * @param tabDataToDelete Callback that is triggered with data to identify stored Tab data to
     *     delete. The stored Tab data must reside in {@link #getOrCreateStateDirectory()}.
     */
    // TODO(tedchoc): Clean up this API.  This does a mixture of file deletion as well as collecting
    //                files to be deleted.  It should either handle all deletions internally or not
    //                do anything but collect files to be deleted.
    void cleanupUnusedFiles(Callback<TabPersistenceFileInfo> tabDataToDelete);

    /**
     * Clean up the persistent state for a given instance.
     *
     * @param index ID of an instance whose state will be deleted.
     * @param tabDataToDelete Callback that is triggered with data to identify stored Tab data to
     *     delete. These stored Tab data must reside in {@link #getOrCreateStateDirectory()}.
     */
    default void cleanupInstanceState(
            int index, Callback<TabPersistenceFileInfo> tabDataToDelete) {}

    /**
     * Sets the {@link TabContentManager} to use.
     * @param cache The {@link TabContentManager} to use.
     */
    void setTabContentManager(TabContentManager cache);

    /**
     * Notified when {@link TabPersistentStore#loadState(boolean)} has completed.
     * @param tabCountAtStartup The number of tabs to be restored at startup.
     */
    void notifyStateLoaded(int tabCountAtStartup);

    /** Notify that persistent store has been destroyed. */
    void destroy();

    /**
     * Provide a sequenced task runner for the TabPersistencePolicy to coordinate tasks between
     * {@link TabPersistentStore} and the persistence policy.
     *
     * @param taskRunner The same SequencedTaskRunner as {@link TabPersistentStore} is using.
     */
    default void setTaskRunner(SequencedTaskRunner taskRunner) {}

    /**
     * Acquire all {@link Tab} identifiers across all windows.
     *
     * @param tabIdsCallback callback to pass {@link Tab} identifiers back in.
     */
    void getAllTabIds(Callback<SparseBooleanArray> tabIdsCallback);
}
