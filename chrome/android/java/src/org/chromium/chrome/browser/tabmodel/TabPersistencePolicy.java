// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;

import java.io.File;
import java.util.List;

/**
 * Policy that handles the Activity specific behaviors regarding the persistence of tab data.
 */
public interface TabPersistencePolicy {

    /**
     * @return File representing the directory that is used to store Tab state
     *         information.
     */
    File getOrCreateStateDirectory();

    /**
     * @return The filename of the primary state file containing information about the tabs to be
     *         loaded.
     */
    String getStateFileName();

    /**
     * @return Whether a merge needs to be performed on startup.
     */
    boolean shouldMergeOnStartup();

    /**
     * @return The filename list of the state that is to be merged.  If null or empty list, no
     *         merge will be triggered.
     */
    @Nullable
    List<String> getStateToBeMergedFileNames();

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

    /**
     * Cancels any pending cleanups in progress.
     */
    void cancelCleanupInProgress();

    /**
     * Trigger a clean up for any unused files (both individual state files for tabs as well as
     * state files for the models).
     *
     * @param filesToDelete Callback that is triggered with the filenames to delete.  These files
     *                      need to reside in {@link #getOrCreateStateDirectory()}.
     */
    // TODO(tedchoc): Clean up this API.  This does a mixture of file deletion as well as collecting
    //                files to be deleted.  It should either handle all deletions internally or not
    //                do anything but collect files to be deleted.
    void cleanupUnusedFiles(Callback<List<String>> filesToDelete);

    /**
     * Clean up the persistent state for a given instance.
     * @param index ID of an instance whose state will be deleted.
     * @param filesToDelete Callback that is triggered with the filenames to delete.  These files
     *                      need to reside in {@link #getOrCreateStateDirectory()}.
     */
    default void cleanupInstanceState(int index, Callback<List<String>> filesToDelete) {}

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

    /**
     * Notify that persistent store has been destroyed.
     */
    void destroy();

    /**
     * Provide a sequenced task runner for the TabPersistencePolicy to coordinate tasks between
     * {@link TabPersistentStore} and the persistence policy.
     *
     * @param taskRunner The same SequencedTaskRunner as {@link TabPersistentStore} is using.
     */
    default void setTaskRunner(SequencedTaskRunner taskRunner) {}

    /**
     * @return Whether allows to skip loading the first active Tab during Startup when an overview
     *         page is showing.
     */
    default boolean allowSkipLoadingTab() {
        return false;
    }
}
