// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.StrictMode;
import android.util.Pair;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.document.IncognitoDocumentActivity;
import org.chromium.chrome.browser.incognito.IncognitoNotificationManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelMetadata;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegateImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.StorageDelegate;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.FeatureUtilities;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.util.HashSet;
import java.util.Set;

/**
 * Divorces Chrome's tabs from Android's Overview menu.  Assumes native libraries are unavailable.
 *
 * Migration from document mode to tabbed mode occurs in two main phases:
 *
 * 1) NON-DESTRUCTIVE MIGRATION:
 *    TabState files for the normal DocumentTabModel are copied from the document mode directories
 *    into the tabbed mode directory.  Incognito tabs are silently dropped, as with the previous
 *    migration pathway.
 *
 *    Once all TabState files are copied, a TabModel metadata file is written out for the tabbed
 *    mode {@link TabModelImpl} to read out.  Because the native library is not available, the file
 *    will be incomplete but usable; it will be corrected by the TabModelImpl when it loads it and
 *    all of the TabState files up.  See {@link #writeTabModelMetadata} for details.
 *
 * 2) CLEANUP OF ALL DOCUMENT-RELATED THINGS:
 *    DocumentActivity tasks in Android's Recents are removed, TabState files in the document mode
 *    directory are deleted, and document mode preferences are cleared.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class DocumentModeAssassin {
    private static DocumentTabModelSelector sDocumentTabModelSelector;

    /**
     * Returns the singleton instance of the DocumentTabModelSelector.
     * @return The DocumentTabModelSelector for the application.
     */
    private static DocumentTabModelSelector getDocumentTabModelSelector() {
        ThreadUtils.assertOnUiThread();
        if (sDocumentTabModelSelector == null) {
            ActivityDelegateImpl activityDelegate = new ActivityDelegateImpl(
                    DocumentActivity.class, IncognitoDocumentActivity.class);
            sDocumentTabModelSelector = new DocumentTabModelSelector(activityDelegate,
                    new StorageDelegate(), new TabDelegate(false), new TabDelegate(true));
        }
        return sDocumentTabModelSelector;
    }

    /** Alerted about progress along the migration pipeline. */
    public static class DocumentModeAssassinObserver {
        /**
         * Called on the UI thread when the DocumentModeAssassin has progressed along its pipeline,
         * and when the DocumentModeAssasssinObserver is first added to the DocumentModeAssassin.
         *
         * @param newStage New stage of the pipeline.
         */
        public void onStageChange(int newStage) {
        }

        /**
         * Called on the background thread when a TabState file has been copied from the document
         * to tabbed mode directory.
         *
         * @param copiedId ID of the Tab whose TabState file was copied.
         */
        public void onTabStateFileCopied(int copiedId) {
        }
    }

    /** Stages of the pipeline.  Each stage is blocked off by a STARTED and DONE pair. */
    static final int STAGE_UNINITIALIZED = 0;
    static final int STAGE_INITIALIZED = 1;
    static final int STAGE_COPY_TAB_STATES_STARTED = 2;
    static final int STAGE_COPY_TAB_STATES_DONE = 3;
    static final int STAGE_WRITE_TABMODEL_METADATA_STARTED = 4;
    static final int STAGE_WRITE_TABMODEL_METADATA_DONE = 5;
    static final int STAGE_CHANGE_SETTINGS_STARTED = 6;
    static final int STAGE_CHANGE_SETTINGS_DONE = 7;
    static final int STAGE_DELETION_STARTED = 8;
    public static final int STAGE_DONE = 9;

    static final String PREF_NUM_MIGRATION_ATTEMPTS =
            "org.chromium.chrome.browser.tabmodel.NUM_MIGRATION_ATTEMPTS";
    static final int MAX_MIGRATION_ATTEMPTS_BEFORE_FAILURE = 3;
    private static final String TAG = "DocumentModeAssassin";

    /** Which TabModelSelectorImpl to copy files into during migration. */
    private static final int TAB_MODEL_INDEX = 0;

    /** SharedPreference values to determine whether user had document mode turned on. */
    static final String OPT_OUT_STATE = "opt_out_state";
    private static final int OPT_IN_TO_DOCUMENT_MODE = 0;
    private static final int OPT_OUT_STATE_UNSET = -1;
    static final int OPTED_OUT_OF_DOCUMENT_MODE = 2;

    /**
     * Preference that denotes that Chrome has attempted to migrate from tabbed mode to document
     * mode. Indicates that the user may be in document mode.
     */
    static final String MIGRATION_ON_UPGRADE_ATTEMPTED = "migration_on_upgrade_attempted";

    /** Creates and holds the Singleton. */
    private static class LazyHolder {
        private static final DocumentModeAssassin INSTANCE = new DocumentModeAssassin();
    }

    /** Returns the Singleton instance. */
    public static DocumentModeAssassin getInstance() {
        return LazyHolder.INSTANCE;
    }

    /** IDs of Tabs that have had their TabState files copied between directories successfully. */
    private final Set<Integer> mMigratedTabIds = new HashSet<>();

    /** Observers of the migration pipeline. */
    private final ObserverList<DocumentModeAssassinObserver> mObservers = new ObserverList<>();

    /** Current stage of the migration. */
    private int mStage = STAGE_UNINITIALIZED;

    /** Whether or not startStage is allowed to progress along the migration pipeline. */
    private boolean mIsPipelineActive;

    /** Migrates the user from document mode to tabbed mode if necessary. */
    @VisibleForTesting
    public final void migrateFromDocumentToTabbedMode() {
        ThreadUtils.assertOnUiThread();

        // Migration is already underway.
        if (mStage != STAGE_UNINITIALIZED) return;

        // If migration isn't necessary, don't do anything.
        if (!isMigrationNecessary()) {
            setStage(STAGE_UNINITIALIZED, STAGE_DONE);
            return;
        }

        // If we've crashed or failed too many times, send them to tabbed mode without their data.
        // - Any incorrect or invalid files in the tabbed mode directory will be wiped out by the
        //   TabPersistentStore when the ChromeTabbedActivity starts.
        //
        // - If it crashes in the step after being migrated, then the user will simply be left
        //   with a bunch of inaccessible document mode data instead of being stuck trying to
        //   migrate, which is a lesser evil.  This case will be caught by the check above to see if
        //   migration is even necessary.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        int numMigrationAttempts = prefs.getInt(PREF_NUM_MIGRATION_ATTEMPTS, 0);
        if (numMigrationAttempts >= MAX_MIGRATION_ATTEMPTS_BEFORE_FAILURE) {
            Log.e(TAG, "Too many failures.  Migrating user to tabbed mode without data.");
            setStage(STAGE_UNINITIALIZED, STAGE_WRITE_TABMODEL_METADATA_DONE);
            return;
        }

        // Kick off the migration pipeline.
        // Using apply() instead of commit() seems to save the preference just fine, even if Chrome
        // crashes immediately afterward.
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(PREF_NUM_MIGRATION_ATTEMPTS, numMigrationAttempts + 1);
        editor.apply();
        setStage(STAGE_UNINITIALIZED, STAGE_INITIALIZED);
    }

    /**
     * Makes copies of {@link TabState} files in the document mode directory and places them in the
     * tabbed mode directory.  Only non-Incognito tabs are transferred.
     *
     * If the user is out of space on their device, this plows through the migration pathway.
     * TODO(dfalcantara): Should we do something about this?  A user can have at most 16 tabs in
     *                    Android's Recents menu.
     *
     * @param selectedTabId             ID of the last viewed non-Incognito tab.
     */
    final void copyTabStateFiles(final int selectedTabId) {
        ThreadUtils.assertOnUiThread();
        if (!setStage(STAGE_INITIALIZED, STAGE_COPY_TAB_STATES_STARTED)) return;

        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                File documentDirectory = getDocumentDataDirectory();
                File tabbedDirectory = getTabbedDataDirectory();

                Log.d(TAG, "Copying TabState files from document to tabbed mode directory.");
                assert mMigratedTabIds.size() == 0;

                File[] allTabStates = documentDirectory.listFiles();
                if (allTabStates != null) {
                    // If we know what tab the user was last viewing, copy just that TabState file
                    // before all the other ones to mitigate storage issues for devices with limited
                    // available storage.
                    if (selectedTabId != Tab.INVALID_TAB_ID) {
                        copyTabStateFilesInternal(
                                allTabStates, tabbedDirectory, selectedTabId, true);
                    }

                    // Copy over everything else.
                    copyTabStateFilesInternal(allTabStates, tabbedDirectory, selectedTabId, false);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                Log.d(TAG, "Finished copying files.");
                setStage(STAGE_COPY_TAB_STATES_STARTED, STAGE_COPY_TAB_STATES_DONE);
            }

            /**
             * Copies the files from the document mode directory to the tabbed mode directory.
             *
             * @param allTabStates        Listing of all files in the document mode directory.
             * @param tabbedDirectory     Directory for the tabbed mode files.
             * @param selectedTabId       ID of the non-Incognito tab the user last viewed.  May be
             *                            {@link Tab#INVALID_TAB_ID} if the ID is unknown.
             * @param copyOnlySelectedTab Copy only the TabState file for the selectedTabId.
             */
            private void copyTabStateFilesInternal(File[] allTabStates, File tabbedDirectory,
                    int selectedTabId, boolean copyOnlySelectedTab) {
                assert !ThreadUtils.runningOnUiThread();
                for (int i = 0; i < allTabStates.length; i++) {
                    // Trawl the directory for non-Incognito TabState files.
                    String fileName = allTabStates[i].getName();
                    Pair<Integer, Boolean> tabInfo = TabState.parseInfoFromFilename(fileName);
                    if (tabInfo == null || tabInfo.second) continue;

                    // Ignore any files that are not relevant for the current pass.
                    int tabId = tabInfo.first;
                    if (selectedTabId != Tab.INVALID_TAB_ID) {
                        if (copyOnlySelectedTab && tabId != selectedTabId) continue;
                        if (!copyOnlySelectedTab && tabId == selectedTabId) continue;
                    }

                    // Copy the file over.
                    File oldFile = allTabStates[i];
                    File newFile = new File(tabbedDirectory, fileName);
                    FileInputStream inputStream = null;
                    FileOutputStream outputStream = null;

                    try {
                        inputStream = new FileInputStream(oldFile);
                        outputStream = new FileOutputStream(newFile);

                        FileChannel inputChannel = inputStream.getChannel();
                        FileChannel outputChannel = outputStream.getChannel();
                        inputChannel.transferTo(0, inputChannel.size(), outputChannel);
                        mMigratedTabIds.add(tabId);

                        for (DocumentModeAssassinObserver observer : mObservers) {
                            observer.onTabStateFileCopied(tabId);
                        }
                    } catch (IOException e) {
                        Log.e(TAG, "Failed to copy: " + oldFile.getName() + " to "
                                + newFile.getName());
                    } finally {
                        StreamUtil.closeQuietly(inputStream);
                        StreamUtil.closeQuietly(outputStream);
                    }
                }
            }
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }


    /**
     * Converts information about the normal {@link DocumentTabModel} into info for
     * {@link TabModelImpl}, then persists it to storage.  Incognito is intentionally not migrated.
     *
     * Because the native library is not available, we have no way of getting the URL from the
     * {@link TabState} files.  Instead, the TabModel metadata file this function writes out
     * writes out inaccurate URLs for each tab:
     * - When the TabState for a Tab is available, a URL of "" is saved out because the
     *   {@link TabPersistentStore} ignores it and restores it from the TabState, anyway.
     *
     * - If a TabState isn't available, we fall back to using the initial URL that was used to spawn
     *   a document mode Tab.
     *
     * These tradeoffs are deemed acceptable because the URL from the metadata file isn't commonly
     * used immediately:
     *
     * 1) {@link TabPersistentStore} uses the URL to allow reusing already open tabs for Home screen
     *    Intents.  If a Tab doesn't match the Intent's URL, a new Tab is created.  This is already
     *    the case when a cold start launches into document mode because the data is unavailable at
     *    startup.
     *
     * 2) {@link TabModelImpl} uses the URL when it fails to load a Tab's persisted TabState.  This
     *    means that the user loses some navigation history, but it's not a case document mode would
     *    have been able to recover from anyway because the TabState stores the URL data.
     *
     * @param migratedTabIds    IDs of Tabs whose TabState files were copied successfully.
     */
    final void writeTabModelMetadata(final Set<Integer> migratedTabIds) {
        ThreadUtils.assertOnUiThread();
        if (!setStage(STAGE_COPY_TAB_STATES_DONE, STAGE_WRITE_TABMODEL_METADATA_STARTED)) return;

        new AsyncTask<Boolean>() {
            private byte[] mSerializedMetadata;

            @Override
            protected void onPreExecute() {
                Log.d(TAG, "Beginning to write tabbed mode metadata files.");

                // Collect information about all the normal tabs on the UI thread.
                final DocumentTabModel normalTabModel = getNormalDocumentTabModel();
                TabModelMetadata normalMetadata = new TabModelMetadata(normalTabModel.index());
                for (int i = 0; i < normalTabModel.getCount(); i++) {
                    int tabId = normalTabModel.getTabAt(i).getId();
                    normalMetadata.ids.add(tabId);

                    if (migratedTabIds.contains(tabId)) {
                        // Don't save a URL because it's in the TabState.
                        normalMetadata.urls.add("");
                    } else {
                        // The best that can be done is to fall back to the initial URL for the Tab.
                        Log.e(TAG, "Couldn't restore state for #" + tabId + "; using initial URL.");
                        normalMetadata.urls.add(normalTabModel.getInitialUrlForDocument(tabId));
                    }
                }

                // Incognito tabs are dropped.
                TabModelMetadata incognitoMetadata =
                        new TabModelMetadata(TabModel.INVALID_TAB_INDEX);

                try {
                    mSerializedMetadata = TabPersistentStore.serializeMetadata(
                            normalMetadata, incognitoMetadata, null);
                } catch (IOException e) {
                    Log.e(TAG, "Failed to serialize the TabModel.", e);
                    mSerializedMetadata = null;
                }
            }

            @Override
            protected Boolean doInBackground() {
                if (mSerializedMetadata != null) {
                    // If an old tab state file still exists when we run migration in TPS, then it
                    // will overwrite the new tab state file that our document tabs migrated to.
                    File oldMetadataFile = new File(
                            getTabbedDataDirectory(),
                            TabbedModeTabPersistencePolicy.LEGACY_SAVED_STATE_FILE);
                    if (oldMetadataFile.exists() && !oldMetadataFile.delete()) {
                        Log.e(TAG, "Failed to delete old tab state file: " + oldMetadataFile);
                    }

                    TabPersistentStore.saveListToFile(
                            getTabbedDataDirectory(),
                            TabbedModeTabPersistencePolicy.getStateFileName(TAB_MODEL_INDEX),
                            mSerializedMetadata);
                    return true;
                } else {
                    return false;
                }
            }

            @Override
            protected void onPostExecute(Boolean result) {
                // TODO(dfalcantara): What do we do if the metadata file failed to be written out?
                Log.d(TAG, "Finished writing tabbed mode metadata file.");
                setStage(STAGE_WRITE_TABMODEL_METADATA_STARTED, STAGE_WRITE_TABMODEL_METADATA_DONE);
            }
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /** Moves the user to tabbed mode by opting them out and removing all the tasks. */
    final void switchToTabbedMode() {
        ThreadUtils.assertOnUiThread();
        if (!setStage(STAGE_WRITE_TABMODEL_METADATA_DONE, STAGE_CHANGE_SETTINGS_STARTED)) return;

        // Record that the user has opted-out of document mode now that their data has been
        // safely copied to the other directory.
        Log.d(TAG, "Setting tabbed mode preference.");
        setOptedOutState(OPTED_OUT_OF_DOCUMENT_MODE);

        // Remove all the {@link DocumentActivity} tasks from Android's Recents list.  Users
        // viewing Recents during migration will continue to see their tabs until they exit.
        // Reselecting a migrated tab will kick the user to the launcher without crashing.
        // TODO(dfalcantara): Confirm that the different Android flavors work the same way.
        createActivityDelegate().finishAllDocumentActivities();

        // Dismiss the "Close all incognito tabs" notification.
        IncognitoNotificationManager.dismissIncognitoNotification();

        setStage(STAGE_CHANGE_SETTINGS_STARTED, STAGE_CHANGE_SETTINGS_DONE);
    }

    /** Deletes all remnants of the document mode directory and preferences. */
    final void deleteDocumentModeData() {
        ThreadUtils.assertOnUiThread();
        if (!setStage(STAGE_CHANGE_SETTINGS_DONE, STAGE_DELETION_STARTED)) return;

        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                Log.d(TAG, "Starting to delete document mode data.");

                // Delete the old tab state directory.
                FileUtils.recursivelyDeleteFile(getDocumentDataDirectory());

                // Clean up the {@link DocumentTabModel} shared preferences.
                SharedPreferences prefs = getContext().getSharedPreferences(
                        DocumentTabModelImpl.PREF_PACKAGE, Context.MODE_PRIVATE);
                SharedPreferences.Editor editor = prefs.edit();
                editor.clear();
                editor.apply();
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                Log.d(TAG, "Finished deleting document mode data.");
                setStage(STAGE_DELETION_STARTED, STAGE_DONE);
            }
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Updates the stage of the migration.
     * @param expectedStage Stage of the pipeline that is currently expected.
     * @param newStage      Stage of the pipeline that is being activated.
     * @return Whether or not the stage was updated.
     */
    private final boolean setStage(int expectedStage, int newStage) {
        ThreadUtils.assertOnUiThread();

        if (mStage != expectedStage) {
            Log.e(TAG, "Wrong stage encountered: expected " + expectedStage + " but in " + mStage);
            return false;
        }
        mStage = newStage;

        for (DocumentModeAssassinObserver callback : mObservers) callback.onStageChange(newStage);
        startStage(newStage);
        return true;
    }

    /**
     * Kicks off tasks for the new state of the pipeline.
     *
     * We don't wait for the DocumentTabModel to finish parsing its metadata file before proceeding
     * with migration because it doesn't have actionable information:
     *
     * 1) WE DON'T NEED TO RE-POPULATE THE "RECENTLY CLOSED" LIST:
     *    The metadata file contains a list of tabs Chrome knew about before it died, which
     *    could differ from the list of tabs in Android Overview.  The canonical list of
     *    live tabs, however, has always been the ones displayed by the Android Overview.
     *
     * 2) RETARGETING MIGRATED TABS FROM THE HOME SCREEN IS A CORNER CASE:
     *    The only downside here is that Chrome ends up creating a new tab for a home screen
     *    shortcut the first time they start Chrome after migration.  This was already
     *    broken for document mode during cold starts, anyway.
     */
    private final void startStage(int newStage) {
        ThreadUtils.assertOnUiThread();
        if (!mIsPipelineActive) return;

        if (newStage == STAGE_INITIALIZED) {
            Log.d(TAG, "Migrating user into tabbed mode.");
            int selectedTabId = DocumentUtils.getLastShownTabIdFromPrefs(getContext(), false);
            copyTabStateFiles(selectedTabId);
        } else if (newStage == STAGE_COPY_TAB_STATES_DONE) {
            Log.d(TAG, "Writing tabbed mode metadata file.");
            writeTabModelMetadata(mMigratedTabIds);
        } else if (newStage == STAGE_WRITE_TABMODEL_METADATA_DONE) {
            Log.d(TAG, "Changing user preference.");
            switchToTabbedMode();
        } else if (newStage == STAGE_CHANGE_SETTINGS_DONE) {
            Log.d(TAG, "Cleaning up document mode data.");
            deleteDocumentModeData();
        }
    }

    /** @return the current stage of the pipeline. */
    public final int getStage() {
        ThreadUtils.assertOnUiThread();
        return mStage;
    }

    /**
     * Adds a observer that is alerted as migration progresses.
     *
     * @param observer Observer to add.
     */
    public final void addObserver(final DocumentModeAssassinObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.addObserver(observer);
    }

    /**
     * Removes an Observer.
     *
     * @param observer Observer to remove.
     */
    public final void removeObserver(final DocumentModeAssassinObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.removeObserver(observer);
    }

    private DocumentModeAssassin() {
        mStage = isMigrationNecessary() ? STAGE_UNINITIALIZED : STAGE_DONE;
        mIsPipelineActive = true;
    }

    private DocumentModeAssassin(int stage, boolean isPipelineActive) {
        mStage = stage;
        mIsPipelineActive = isPipelineActive;
    }

    /** DocumentModeAssassin that can have its methods be overridden for testing. */
    static class DocumentModeAssassinForTesting extends DocumentModeAssassin {
        /**
         * Creates a DocumentModeAssassin that starts at the given stage.
         *
         * @param stage             Stage to start at.  See the STAGE_* values above.
         * @param isPipelineActive  Whether the pipeline should continue after a stage finishes.
         */
        @VisibleForTesting
        DocumentModeAssassinForTesting(int stage, boolean isPipelineActive) {
            super(stage, isPipelineActive);
        }
    }

    /** @return Whether or not a migration to tabbed mode from document mode is necessary. */
    public boolean isMigrationNecessary() {
        return FeatureUtilities.isDocumentMode(ContextUtils.getApplicationContext());
    }

    /** @return Context to use when grabbing SharedPreferences, Files, and other resources. */
    protected Context getContext() {
        return ContextUtils.getApplicationContext();
    }

    /** @return Interfaces with the Android ActivityManager. */
    protected ActivityDelegate createActivityDelegate() {
        return new ActivityDelegateImpl(DocumentActivity.class, IncognitoDocumentActivity.class);
    }

    /** @return The {@link DocumentTabModelImpl} for regular tabs. */
    protected DocumentTabModel getNormalDocumentTabModel() {
        return getDocumentTabModelSelector().getModel(false);
    }

    /** @return Where document mode data is stored for the normal {@link DocumentTabModel}. */
    protected File getDocumentDataDirectory() {
        return new StorageDelegate().getStateDirectory();
    }

    /** @return Where tabbed mode data is stored. */
    protected File getTabbedDataDirectory() {
        return TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory();
    }

    /** @return True if the user is not in document mode. */
    public static boolean isOptedOutOfDocumentMode() {
        // The OPT_OUT_STATE preference was introduced sometime after document mode was rolled out.
        // It may not be set for all users, even if they are in document mode. In order to correctly
        // detect whether the user is in document mode, if OPT_OUT_STATE is not state we must check
        // whether MIGRATION_ON_UPGRADE_ATTEMPTED is set.
        int optOutState = ContextUtils.getAppSharedPreferences().getInt(OPT_OUT_STATE,
                OPT_OUT_STATE_UNSET);
        if (optOutState == OPT_OUT_STATE_UNSET) {
            boolean hasMigratedToDocumentMode = ContextUtils.getAppSharedPreferences().getBoolean(
                    MIGRATION_ON_UPGRADE_ATTEMPTED, false);
            if (!hasMigratedToDocumentMode) {
                optOutState = OPTED_OUT_OF_DOCUMENT_MODE;
            } else {
                // Check if a migration has already happened by looking for tab_state0 file.
                // See crbug.com/646146.
                boolean newMetadataFileExists = false;
                StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
                try {
                    File newMetadataFile = new File(
                            TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory(),
                            TabbedModeTabPersistencePolicy.getStateFileName(TAB_MODEL_INDEX));
                    newMetadataFileExists = newMetadataFile.exists();
                } finally {
                    StrictMode.setThreadPolicy(oldPolicy);
                }

                if (newMetadataFileExists) {
                    optOutState = OPTED_OUT_OF_DOCUMENT_MODE;
                } else {
                    optOutState = OPT_IN_TO_DOCUMENT_MODE;
                }
            }
            setOptedOutState(optOutState);
        }
        return optOutState == OPTED_OUT_OF_DOCUMENT_MODE;
    }

    /**
     * Sets the opt out preference.
     * @param state One of OPTED_OUT_OF_DOCUMENT_MODE or OPT_IN_TO_DOCUMENT_MODE.
     */
    public static void setOptedOutState(int state) {
        SharedPreferences.Editor sharedPreferencesEditor =
                ContextUtils.getAppSharedPreferences().edit();
        sharedPreferencesEditor.putInt(OPT_OUT_STATE, state);
        sharedPreferencesEditor.apply();
    }
}
