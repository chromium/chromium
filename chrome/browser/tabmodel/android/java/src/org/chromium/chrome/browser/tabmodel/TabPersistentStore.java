// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.base.CallbackUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This class handles saving and loading tab state from the persistent storage. */
@NullMarked
public interface TabPersistentStore {
    /** Alerted at various stages of operation. */
    interface TabPersistentStoreObserver {
        /**
         * To be called when the file containing the initial information about the TabModels has
         * been loaded.
         *
         * @param tabCountAtStartup How many tabs there are in the TabModels.
         */
        default void onInitialized(int tabCountAtStartup) {}

        /** Called when details about a Tab are read from the metadata file. */
        default void onDetailsRead(
                int index,
                int id,
                String url,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex,
                @Nullable Boolean isIncognito,
                boolean fromMerge) {}

        /** To be called when the TabStates have all been loaded. */
        default void onStateLoaded() {}

        /** To be called when the TabState from another instance has been merged. */
        default void onStateMerged() {}

        /**
         * Called when the metadata file has been saved out asynchronously. This currently does not
         * get called when the metadata file is saved out on the UI thread.
         */
        default void onMetadataSavedAsynchronously() {}
    }

    @IntDef({ActiveTabState.OTHER, ActiveTabState.NTP, ActiveTabState.EMPTY})
    @Retention(RetentionPolicy.SOURCE)
    @interface ActiveTabState {
        /** No active tab. */
        int EMPTY = 0;

        /** Active tab is NTP. */
        int NTP = 1;

        /** Active tab is anything other than NTP. */
        int OTHER = 2;
    }

    @IntDef({
        MetadataSaveMode.SAVING_ALLOWED,
        MetadataSaveMode.PAUSED_AND_CLEAN,
        MetadataSaveMode.PAUSED_AND_DIRTY
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface MetadataSaveMode {
        /** Changes to the tab list are allowed to trigger saves. */
        int SAVING_ALLOWED = 0;

        /** Saving has been paused, but no changes have been seen. */
        int PAUSED_AND_CLEAN = 1;

        /**
         * Saving has been paused and changes have been made, a save will be triggered on resume.
         */
        int PAUSED_AND_DIRTY = 2;
    }

    /**
     * Initializes the TabPersistentStore. This must be called after the native library has been
     * loaded.
     */
    void onNativeLibraryReady();

    /**
     * Waits for the task that migrates all state files to their new location to finish. For custom
     * tabs this might also set up the state directory.
     */
    void waitForMigrationToFinish();

    /**
     * Saves the current tab state to disk. This is a blocking call. It should not be called on the
     * UI thread except during shutdown or in other scenarios where immediate persistence is
     * required.
     */
    void saveState();

    /**
     * Restore saved state. Must be called before any tabs are added to the list.
     *
     * <p>This will read the metadata file for the current TabPersistentStore and the metadata file
     * from another TabPersistentStore if applicable. When restoreTabs() is called, tabs from both
     * will be restored into this instance.
     *
     * @param ignoreIncognitoFiles Whether to skip loading incognito tabs.
     */
    void loadState(boolean ignoreIncognitoFiles);

    /**
     * Merge the tabs of the other Chrome instance into this instance by reading its tab metadata
     * file and tab state files.
     *
     * <p>This method should be called after a change in activity state indicates that a merge is
     * necessary. #loadState() will take care of merging states on application cold start if needed.
     *
     * <p>If there is currently a merge or load in progress then this method will return early.
     */
    void mergeState();

    /**
     * Restore tab state. Tab state is loaded asynchronously, other than the active tab which can be
     * forced to load synchronously.
     *
     * @param setActiveTab If true the last active tab given in the saved state is loaded
     *     synchronously and set as the current active tab. If false all tabs are loaded
     *     asynchronously.
     */
    void restoreTabs(boolean setActiveTab);

    /**
     * If a tab is being restored with the given url, then restore the tab in a frozen state
     * synchronously.
     *
     * @param url The URL of the tab to restore.
     */
    void restoreTabStateForUrl(String url);

    /**
     * If a tab is being restored with the given id, then restore the tab in a frozen state
     * synchronously.
     *
     * @param id The ID of the tab to restore.
     */
    void restoreTabStateForId(int id);

    /** Returns the number of restored tabs on cold startup. */
    int getRestoredTabCount();

    /**
     * Deletes all files in the tab state directory. This will delete all files and not just those
     * owned by this TabPersistentStore.
     */
    void clearState();

    /** Cleans up any resources used by the TabPersistentStore. */
    void destroy();

    /**
     * Pauses the async saving of the tab state. Used in cases where there are batch updates to
     * {@link org.chromium.chrome.browser.tabmodel.TabModel}s.
     */
    void pauseSaveTabList();

    /** See {@link #resumeSaveTabList(Runnable)}. */
    default void resumeSaveTabList() {
        resumeSaveTabList(CallbackUtils.emptyRunnable());
    }

    /**
     * Resumes the async saving of the tab state, then kicks off an AsyncTask to save the current
     * list of tabs. Will execute the provided {@link Runnable} once the first save task has
     * completed after resumption.
     *
     * @param onSaveTabListRunnable The {@link Runnable} to execute once the first save task has
     *     completed after resumption.
     */
    void resumeSaveTabList(Runnable onSaveTabListRunnable);

    /**
     * Deletes the state file for the given instance.
     *
     * @param windowId The window ID of the corresponding window to delete the state file for.
     */
    void cleanupStateFile(int windowId);

    /**
     * Adds a {@link TabPersistentStoreObserver}.
     *
     * @param observer The {@link TabPersistentStoreObserver} to add.
     */
    void addObserver(TabPersistentStoreObserver observer);

    /**
     * Removes a {@link TabPersistentStoreObserver}.
     *
     * @param observer The {@link TabPersistentStoreObserver} to remove.
     */
    void removeObserver(TabPersistentStoreObserver observer);
}
