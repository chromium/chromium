// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.text.TextUtils;

import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TriState;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelMetadata;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Per-app, per-account singleton orchestrator for app-scoped Custom Tabs (Tabbed CCT).
 *
 * <p>Manages two kinds of tabs for a verified client app ({@code packageName}) and account ID:
 *
 * <ul>
 *   <li>Live {@link Tab}s in each {@code CustomTabActivity} (each owning its own {@link
 *       CustomTabsTabModelOrchestrator} for its {@code tab_state_active_<taskId>_<uuid>} file),
 *       aggregated across active Custom Tab activities via {@link #getActivityTabModelSelectors()}.
 *   <li>Closed tabs persisted on disk whose retired metadata files ({@code
 *       tab_state_retired_<taskId>_<uuid>_<timestamp>}) are merged into the canonical per-app
 *       metadata file ({@link CustomTabTabPersistencePolicy#APP_METADATA_FILE_NAME}).
 * </ul>
 *
 * <p>A singleton instance is maintained per client app ({@code packageName}), shared by Custom Tabs
 * opened with the same identity under the same account ID. Two instances for different accounts of
 * the same app cannot coexist: if a Custom Tab launches for account B while an instance for account
 * A is active, the account A instance is torn down by saving/retiring its active metadata, closing
 * its live {@link Tab}s, and finishing its Custom Tab activities before the account B instance
 * takes over.
 */
@NullMarked
public class CustomTabAppTabModelOrchestrator {
    private static final String TAG = "CctAppTabOrch";

    /** Active singleton orchestrator per verified client package name. */
    private static final Map<String, CustomTabAppTabModelOrchestrator> sInstancesByPackage =
            new HashMap<>();

    /**
     * Shared sequenced task runner for retiring and merging per-app metadata files across all
     * {@link CustomTabAppTabModelOrchestrator} instances. Lazily initialized so {@link
     * ResettersForTesting} can reset it between tests without leaking undrained tasks.
     */
    private static @Nullable SequencedTaskRunner sTaskRunner;

    private final String mClientPackageName;
    private final @Nullable String mAccountId;
    private final List<BaseCustomTabActivity> mActivities = new ArrayList<>();
    private final List<TabModelSelector> mActivityTabModelSelectors = new ArrayList<>();
    private final Map<BaseCustomTabActivity, Callback<TabModelSelector>> mSelectorObservers =
            new HashMap<>();

    private boolean mTornDown;

    private static SequencedTaskRunner getOrCreateTaskRunner() {
        if (sTaskRunner == null) {
            sTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);
            ResettersForTesting.register(() -> sTaskRunner = null);
        }
        return sTaskRunner;
    }

    /**
     * Returns the singleton {@link CustomTabAppTabModelOrchestrator} for the given {@code
     * clientPackageName} and {@code accountId}.
     *
     * <p>If an orchestrator already exists for {@code clientPackageName} under a different account,
     * the existing orchestrator is torn down first (closing all live tabs and Custom Tab activities
     * for the previous account) before creating the new instance.
     *
     * @param activity The hosting Custom Tab activity.
     * @param clientPackageName Verified package name of the client app.
     * @param accountId Optional account ID from the launch intent, or null for the default account.
     * @return The {@link CustomTabAppTabModelOrchestrator} instance.
     */
    public static CustomTabAppTabModelOrchestrator getOrCreate(
            BaseCustomTabActivity activity, String clientPackageName, @Nullable String accountId) {
        ThreadUtils.assertOnUiThread();
        CustomTabAppTabModelOrchestrator existing = sInstancesByPackage.get(clientPackageName);
        if (existing != null && !existing.mTornDown) {
            if (!existing.isSameAccount(accountId)) {
                existing.tearDownForAccountSwitch();
            } else {
                existing.registerActivity(activity);
                existing.scheduleMergeRetiredMetadataFiles();
                return existing;
            }
        }

        CustomTabAppTabModelOrchestrator orchestrator =
                new CustomTabAppTabModelOrchestrator(clientPackageName, accountId);
        orchestrator.registerActivity(activity);
        if (sInstancesByPackage.isEmpty()) {
            ResettersForTesting.register(sInstancesByPackage::clear);
        }
        sInstancesByPackage.put(clientPackageName, orchestrator);
        orchestrator.scheduleMergeRetiredMetadataFiles();
        return orchestrator;
    }

    private CustomTabAppTabModelOrchestrator(String clientPackageName, @Nullable String accountId) {
        mClientPackageName = clientPackageName;
        mAccountId = accountId;
    }

    /** Returns the active orchestrator instance for {@code clientPackageName}, or null. */
    public static @Nullable CustomTabAppTabModelOrchestrator getInstanceForPackageForTesting(
            String clientPackageName) {
        return sInstancesByPackage.get(clientPackageName);
    }

    /** Returns the client package name associated with this orchestrator. */
    public String getClientPackageName() {
        return mClientPackageName;
    }

    /**
     * Returns the account ID associated with this orchestrator, or null for the default account.
     */
    public @Nullable String getAccountId() {
        return mAccountId;
    }

    /** Returns the number of live activities currently sharing this orchestrator. */
    public int getActiveActivityCountForTesting() {
        return mActivities.size();
    }

    /**
     * Returns the cached {@link TabModelSelector}s of all live Custom Tab activities currently
     * registered with this orchestrator whose tab models have been initialized.
     */
    public List<TabModelSelector> getActivityTabModelSelectors() {
        ThreadUtils.assertOnUiThread();
        return Collections.unmodifiableList(mActivityTabModelSelectors);
    }

    private boolean isSameAccount(@Nullable String otherAccountId) {
        String normThis = TextUtils.isEmpty(mAccountId) ? null : mAccountId;
        String normOther = TextUtils.isEmpty(otherAccountId) ? null : otherAccountId;
        return Objects.equals(normThis, normOther);
    }

    private void registerActivity(BaseCustomTabActivity activity) {
        if (mActivities.contains(activity)) {
            return;
        }
        mActivities.add(activity);
        Callback<TabModelSelector> observer =
                selector -> {
                    if (!mActivityTabModelSelectors.contains(selector)) {
                        mActivityTabModelSelectors.add(selector);
                    }
                };
        mSelectorObservers.put(activity, observer);
        activity.getTabModelSelectorSupplier().addSyncObserverAndCallIfNonNull(observer);
    }

    private void unregisterActivitySelectorObserver(BaseCustomTabActivity activity) {
        Callback<TabModelSelector> observer = mSelectorObservers.remove(activity);
        if (observer != null) {
            activity.getTabModelSelectorSupplier().removeObserver(observer);
        }
        TabModelSelector selector = activity.getTabModelSelectorSupplier().get();
        if (selector != null) {
            mActivityTabModelSelectors.remove(selector);
        }
    }

    /**
     * Releases a {@link BaseCustomTabActivity}'s reference to this orchestrator when the activity
     * is destroyed. Destroys the orchestrator only when the last {@code CustomTabActivity} opened
     * for the same identity and account ID is closed.
     */
    public void releaseActivity(BaseCustomTabActivity activity) {
        ThreadUtils.assertOnUiThread();
        mActivities.remove(activity);
        unregisterActivitySelectorObserver(activity);
        if (mTornDown) {
            return;
        }
        if (mActivities.isEmpty()) {
            destroy();
        }
    }

    /** Returns whether this orchestrator has been destroyed. */
    public boolean isDestroyedForTesting() {
        return mTornDown;
    }

    /** Destroys this orchestrator and unregisters it from the active singleton map. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        if (mTornDown) {
            return;
        }
        if (sInstancesByPackage.get(mClientPackageName) == this) {
            sInstancesByPackage.remove(mClientPackageName);
        }
        for (BaseCustomTabActivity activity : mActivities) {
            unregisterActivitySelectorObserver(activity);
        }
        mActivities.clear();
        mActivityTabModelSelectors.clear();
        mSelectorObservers.clear();
        mTornDown = true;
    }

    /**
     * Tears down this orchestrator when a Custom Tab is launched for the same app under a different
     * account: saves and retires each Custom Tab activity's active metadata file to disk, closes
     * all live {@link Tab} objects for this account, finishes all Custom Tab activities for this
     * account, and destroys this orchestrator.
     */
    private void tearDownForAccountSwitch() {
        ThreadUtils.assertOnUiThread();
        if (mTornDown) return;

        TabClosureParams closeAllParams =
                TabClosureParams.closeAllTabs().allowUndo(false).uponExit(true).build();
        List<BaseCustomTabActivity> activitiesToClose = new ArrayList<>(mActivities);
        for (BaseCustomTabActivity customTabActivity : activitiesToClose) {
            if (!customTabActivity.isFinishing() && !customTabActivity.isDestroyed()) {
                if (customTabActivity.areTabModelsInitialized()) {
                    TabModelOrchestrator activityOrchestrator =
                            customTabActivity.getTabModelOrchestratorSupplier().get();
                    TabModelSelector selector = customTabActivity.getTabModelSelector();
                    // Only save state if tab state has finished loading/initializing; otherwise
                    // saveState() would overwrite an existing tab_state_active_* file on disk with
                    // 0 tabs before loadState() restores them.
                    if (selector.isTabStateInitialized() && activityOrchestrator != null) {
                        activityOrchestrator.saveState();
                    }
                    customTabActivity
                            .getCustomTabTabPersistencePolicy()
                            .retireMetadataStateFileAsync();
                    if (activityOrchestrator != null) {
                        activityOrchestrator.destroyTabPersistentStore();
                    }
                    for (TabModel model : selector.getModels()) {
                        if (model.getCount() > 0) {
                            model.getTabRemover()
                                    .closeTabs(closeAllParams, /* allowDialog= */ false);
                        }
                    }
                }
                customTabActivity
                        .getCustomTabActivityNavigationController()
                        .finish(FinishReason.OTHER);
            }
        }

        destroy();
    }

    /**
     * Asynchronously checks the app/account state directory for any retired tab state metadata
     * files ({@code tab_state_retired_*}) and merges them into {@link
     * CustomTabTabPersistencePolicy#APP_METADATA_FILE_NAME}.
     */
    private void scheduleMergeRetiredMetadataFiles() {
        getOrCreateTaskRunner().execute(this::mergeRetiredMetadataFilesOnBackgroundThread);
    }

    /**
     * Synchronously merges any retired tab metadata files ({@code
     * tab_state_retired_<taskId>_<uuid>_<timestamp>}) in the app/account state directory into
     * {@link CustomTabTabPersistencePolicy#APP_METADATA_FILE_NAME} and deletes the retired files.
     */
    @WorkerThread
    private void mergeRetiredMetadataFilesOnBackgroundThread() {
        ThreadUtils.assertOnBackgroundThread();
        File stateDir =
                TabStateDirectory.getOrCreateCustomTabAppStateDirectory(
                        mClientPackageName, mAccountId);
        File[] files = stateDir.listFiles();
        if (files == null) return;

        List<File> retiredFiles = new ArrayList<>();
        for (File file : files) {
            String name = file.getName();
            if (TabMetadataFileManager.isMetadataFile(name)
                    && name.startsWith(
                            CustomTabTabPersistencePolicy.RETIRED_METADATA_FILE_PREFIX)) {
                retiredFiles.add(file);
            }
        }
        if (retiredFiles.isEmpty()) return;

        File appMetadataFile =
                new File(stateDir, CustomTabTabPersistencePolicy.APP_METADATA_FILE_NAME);
        LinkedHashMap<Integer, String> mergedRegularTabs = new LinkedHashMap<>();

        if (appMetadataFile.exists()
                && !readTabsFromMetadataFile(appMetadataFile, mergedRegularTabs)) {
            return;
        }

        List<File> filesToDelete = new ArrayList<>();
        for (File retiredFile : retiredFiles) {
            if (readTabsFromMetadataFile(retiredFile, mergedRegularTabs)) {
                filesToDelete.add(retiredFile);
            }
        }
        if (filesToDelete.isEmpty()) return;

        TabModelMetadata normalMetadata =
                new TabModelMetadata(
                        mergedRegularTabs.isEmpty()
                                ? TabModel.INVALID_TAB_INDEX
                                : mergedRegularTabs.size() - 1);
        for (Map.Entry<Integer, String> entry : mergedRegularTabs.entrySet()) {
            normalMetadata.ids.add(entry.getKey());
            normalMetadata.urls.add(entry.getValue());
        }

        TabModelMetadata incognitoMetadata = new TabModelMetadata(TabModel.INVALID_TAB_INDEX);

        if (!TabMetadataFileManager.saveListToFile(
                appMetadataFile, new TabModelSelectorMetadata(normalMetadata, incognitoMetadata))) {
            return;
        }

        for (File retiredFile : filesToDelete) {
            if (!retiredFile.delete()) {
                Log.e(TAG, "Failed to delete merged retired metadata file: %s", retiredFile);
            }
        }
    }

    @WorkerThread
    private static boolean readTabsFromMetadataFile(
            File metadataFile, LinkedHashMap<Integer, String> regularTabs) {
        DataInputStream stream = null;
        try {
            stream =
                    new DataInputStream(new BufferedInputStream(new FileInputStream(metadataFile)));
            TabMetadataFileManager.readSavedMetadataFile(
                    stream,
                    (_, id, url, isIncognito, _, _) -> {
                        if (isIncognito != TriState.TRUE) {
                            regularTabs.remove(id);
                            regularTabs.put(id, url);
                        }
                    },
                    /* tabIds= */ null);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Unable to read metadata file %s", metadataFile.getName(), e);
            return false;
        } finally {
            StreamUtil.closeQuietly(stream);
        }
    }

    /**
     * Returns the shared {@link SequencedTaskRunner} used for retiring and merging metadata files.
     */
    public SequencedTaskRunner getTaskRunner() {
        return getOrCreateTaskRunner();
    }
}
