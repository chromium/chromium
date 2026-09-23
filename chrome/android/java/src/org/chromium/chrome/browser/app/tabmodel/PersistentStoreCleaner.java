// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.ARCHIVED_WINDOW_TAG;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.LeaseReason;
import org.chromium.chrome.browser.app.tabmodel.TabStateStore.TabStateStoreCleaner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl.TabPersistentStoreImplCleaner;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/** Cleaner that allows a window to clean up persisted state for another window. */
@NullMarked
public class PersistentStoreCleaner {
    /** Dependencies for cleaning unused data for a specific profile. */
    private static class UnusedDataDeps implements Destroyable {
        final Set<TabContentManager> mTabContentManagers = new HashSet<>();
        @Nullable Runnable mCleanupRunnable;
        TabWindowManager.@Nullable Observer mTabWindowManagerObserver;
        @Nullable Destroyable mArchivedTabsLease;

        void maybeAcquireArchivedTabsLease(Profile profile) {
            if (mArchivedTabsLease == null
                    && ArchivedTabModelOrchestrator.isInstantiatedForProfile(profile)) {
                mArchivedTabsLease =
                        ArchivedTabModelOrchestrator.acquireLease(
                                profile, LeaseReason.PERSISTENT_STORE_CLEANER);
            }
        }

        @Override
        public void destroy() {
            if (mTabWindowManagerObserver != null) {
                TabWindowManagerSingleton.getInstance().removeObserver(mTabWindowManagerObserver);
                mTabWindowManagerObserver = null;
            }
            if (mArchivedTabsLease != null) {
                mArchivedTabsLease.destroy();
                mArchivedTabsLease = null;
            }
            mTabContentManagers.clear();
            mCleanupRunnable = null;
        }
    }

    private final Profile mProfile;
    private final boolean mTabStorageEnabled;
    private final SequencedTaskRunner mSequencedTaskRunner;
    private final TabbedModeTabPersistencePolicy mPersistencePolicy;
    private final TabStateStoreCleaner mTabStateStoreCleaner;
    private final TabPersistentStoreImplCleaner mLegacyCleaner;

    private @Nullable UnusedDataDeps mUnusedDataDeps;

    /**
     * @param profile The original profile used for dependencies.
     * @param tabStateStoreCleaner The cleaner for {@link TabStateStore}.
     * @param legacyCleaner The cleaner for {@link TabPersistentStoreImpl}.
     */
    public PersistentStoreCleaner(
            Profile profile,
            TabStateStoreCleaner tabStateStoreCleaner,
            TabPersistentStoreImplCleaner legacyCleaner) {
        assert !profile.isOffTheRecord();

        mProfile = profile;
        mTabStorageEnabled = TabStateStorageFlagHelper.isTabStorageEnabled();
        mTabStateStoreCleaner = tabStateStoreCleaner;
        mLegacyCleaner = legacyCleaner;

        @TaskTraits int taskTraits = TaskTraits.USER_BLOCKING_MAY_BLOCK;
        mSequencedTaskRunner = PostTask.createSequencedTaskRunner(taskTraits);

        // Do not need to provide a valid window ID, since it is not used for any operation.
        mPersistencePolicy =
                new TabbedModeTabPersistencePolicy(
                        /* selectorIndex= */ TabWindowManager.INVALID_WINDOW_ID,
                        /* mergeTabsOnStartup= */ false,
                        /* tabMergingEnabled= */ false,
                        ObservableSuppliers.createNonNull(false));
        mPersistencePolicy.performInitialization(mSequencedTaskRunner);
    }

    /**
     * Clears all persisted state for all {@link TabPersistentStore}s.
     *
     * @param orchestrator Used to determine the active store types and provide access to the
     *     profile.
     */
    public void clearState(TabModelOrchestrator orchestrator) {
        if (storeDoesNotExist(orchestrator, StoreType.LEGACY)) {
            mLegacyCleaner.clearState(mPersistencePolicy, mSequencedTaskRunner);
        }

        if (storeDoesNotExist(orchestrator, StoreType.TAB_STATE_STORE)) {
            maybeClearTabStateStore(orchestrator);
        }

        TabStoreMetricsService.clearAllTabStoreCounts();
    }

    /**
     * Cleans up a specific window's tab state for {@link TabPersistentStore}s that are not
     * available in the calling window.
     *
     * @param windowIdToClean The ID of the window to be cleaned.
     * @param orchestrator Used to determine the active store types and provide access to the
     *     profile.
     */
    public void cleanWindowForUnavailableStores(
            int windowIdToClean, TabModelOrchestrator orchestrator) {
        if (storeDoesNotExist(orchestrator, StoreType.LEGACY)) {
            cleanPersistentStoreImpl(windowIdToClean);
        }

        if (storeDoesNotExist(orchestrator, StoreType.TAB_STATE_STORE)) {
            maybeCleanTabStateStore(windowIdToClean, orchestrator);
        }

        TabStoreMetricsService.clearTabStoreCountsForProfileAndWindow(
                mProfile, String.valueOf(windowIdToClean));
    }

    private void cleanPersistentStoreImpl(int windowIdToClean) {
        // We will not have merged if an instance is not present, so this set will remain empty.
        Set<String> mergedFileNames = new HashSet<>();

        mLegacyCleaner.cleanupStateFile(
                windowIdToClean, mPersistencePolicy, mSequencedTaskRunner, mergedFileNames);
    }

    private void maybeCleanTabStateStore(int windowIdToClean, TabModelOrchestrator orchestrator) {
        if (!mTabStorageEnabled) return;

        TabModelSelectorBase selector = orchestrator.getTabModelSelector();
        assert selector != null;

        mTabStateStoreCleaner.cleanupStateFile(windowIdToClean, mProfile);
    }

    private boolean storeDoesNotExist(TabModelOrchestrator orchestrator, @StoreType int type) {
        @StoreType Integer authoritativeStoreType = orchestrator.getAuthoritativeStoreType();
        @StoreType Integer shadowStoreType = orchestrator.getShadowStoreType();
        return !Objects.equals(authoritativeStoreType, type)
                && !Objects.equals(shadowStoreType, type);
    }

    private void maybeClearTabStateStore(TabModelOrchestrator orchestrator) {
        if (!mTabStorageEnabled) return;

        TabModelSelectorBase selector = orchestrator.getTabModelSelector();
        assert selector != null;

        mTabStateStoreCleaner.clearState(mProfile);
    }

    /**
     * Schedules the cleaning of unused windows and tab data.
     *
     * @param manager Manages tab thumbnails.
     */
    public void scheduleCleanUnusedData(TabContentManager manager) {
        assertOnUiThread();
        if (!ChromeFeatureList.sScheduleWindowCleaning.isEnabled()) return;
        UnusedDataDeps unusedDataDeps = mUnusedDataDeps;
        if (unusedDataDeps == null) {
            unusedDataDeps = new UnusedDataDeps();
            mUnusedDataDeps = unusedDataDeps;
        }
        unusedDataDeps.mTabContentManagers.add(manager);

        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        if (tabWindowManager.isAllTabStateInitialized()) {
            maybeCleanUnusedWindows();
        } else if (unusedDataDeps.mTabWindowManagerObserver == null) {
            unusedDataDeps.mTabWindowManagerObserver =
                    new TabWindowManager.Observer() {
                        @Override
                        public void onAllTabModelStateInitialized() {
                            maybeCleanUnusedWindows();
                        }
                    };
            tabWindowManager.addObserver(unusedDataDeps.mTabWindowManagerObserver);
        }
    }

    /** Schedules the cleaning of unused windows if a cleanup task is not already scheduled. */
    private void maybeCleanUnusedWindows() {
        assertOnUiThread();

        UnusedDataDeps unusedDataDeps = mUnusedDataDeps;
        if (unusedDataDeps == null || unusedDataDeps.mCleanupRunnable != null) return;

        unusedDataDeps.maybeAcquireArchivedTabsLease(mProfile);

        Runnable cleanupRunnable = () -> cleanUnusedWindows(unusedDataDeps);
        unusedDataDeps.mCleanupRunnable = cleanupRunnable;

        PostTask.postTask(TaskTraits.UI_DEFAULT, cleanupRunnable);
    }

    private void cleanUnusedWindows(UnusedDataDeps deps) {
        assertOnUiThread();
        TabContentManager validManager = null;
        for (TabContentManager manager : deps.mTabContentManagers) {
            if (!manager.isDestroyed()) {
                validManager = manager;
                break;
            }
        }

        if (validManager == null) {
            cleanUnusedDeps();
            return;
        }

        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        if (tabWindowManager.getAllTabModelSelectors().isEmpty()
                && tabWindowManager.getCustomTabsTabModelSelectors().isEmpty()) {
            cleanUnusedDeps();
            return;
        }

        List<String> windowTags = new ArrayList<>();
        List<TabModelSelector> selectors = new ArrayList<>();

        // Unconditionally preserve archived window state files on disk.
        windowTags.add(ARCHIVED_WINDOW_TAG);
        TabModelSelector archivedTabModelSelector = tabWindowManager.getArchivedTabModelSelector();
        if (archivedTabModelSelector != null) {
            selectors.add(archivedTabModelSelector);
        }

        for (TabModelSelector selector : tabWindowManager.getAllTabModelSelectors()) {
            if (selector == null) continue;

            selectors.add(selector);
            int windowId = tabWindowManager.getWindowIdForSelector(selector);
            windowTags.add(String.valueOf(windowId));
        }

        for (TabModelSelector selector : tabWindowManager.getCustomTabsTabModelSelectors()) {
            if (selector == null) continue;

            selectors.add(selector);
            int taskId = tabWindowManager.getTaskIdForCustomTab(selector);
            windowTags.add(String.valueOf(taskId));
        }

        // If there are no selectors available (e.g. during activity teardown or window detachment),
        // abort cleanup safely to avoid wiping out saved windows and thumbnails.
        if (selectors.isEmpty()) {
            cleanUnusedDeps();
            return;
        }

        for (TabModelSelector selector : selectors) {
            assert selector != null;
            if (!selector.isTabStateInitialized()) {
                // Wait for the selector to initialize tab state. If the selector is destroyed
                // before initializing (e.g. window closed during startup), re-evaluate cleanup:
                // if other selectors remain, continue waiting or proceed; if all selectors are
                // gone, maybeCleanUnusedWindows() will safely abort.
                TabModelSelectorObserver destructionObserver =
                        new TabModelSelectorObserver() {
                            @Override
                            public void onDestroyed() {
                                selector.removeObserver(this);
                                deps.mCleanupRunnable = null;
                                maybeCleanUnusedWindows();
                            }
                        };
                selector.addObserver(destructionObserver);
                TabModelUtils.runOnTabStateInitialized(
                        () -> {
                            selector.removeObserver(destructionObserver);
                            deps.mCleanupRunnable = null;
                            maybeCleanUnusedWindows();
                        },
                        selector);
                return;
            }
        }

        List<@TabId Integer> tabIds = new ArrayList<>();
        for (TabModelSelector selector : selectors) {
            for (TabModel tabModel : selector.getModels()) {
                for (Tab tab : tabModel) {
                    tabIds.add(tab.getId());
                }
            }
        }

        TabArchiveSettings archiveSettings = TabArchiveSettings.getInstance();
        // Check if there are any archived tabs either tracked in persistent settings or in
        // memory. archiveSettings.getArchivedTabCount() reads the persisted count from
        // SharedPreferences, and isInstantiatedForProfile checks memory residency.
        // If hasArchivedTabs is true but archivedTabModelSelector is null (e.g. a brand new lease
        // where models haven't been created yet), thumbnail pruning is safely deferred to prevent
        // deleting thumbnails of archived tabs whose IDs are not yet loaded in memory.
        boolean hasArchivedTabs =
                archiveSettings.getArchivedTabCount() > 0
                        || ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile);
        if (!hasArchivedTabs || archivedTabModelSelector != null) {
            deleteAllTabDataExceptFor(validManager, tabIds);
        }
        deleteAllWindowsExceptFor(windowTags);

        cleanUnusedDeps();
    }

    private void cleanUnusedDeps() {
        if (mUnusedDataDeps != null) {
            mUnusedDataDeps.destroy();
            mUnusedDataDeps = null;
        }
    }

    private void deleteAllTabDataExceptFor(TabContentManager manager, List<Integer> tabIds) {
        int[] tabIdsArray = new int[tabIds.size()];
        for (int i = 0; i < tabIds.size(); i++) {
            tabIdsArray[i] = tabIds.get(i);
        }
        manager.removeAllTabThumbnailsExceptForIds(tabIdsArray);
    }

    private void deleteAllWindowsExceptFor(List<String> windowTags) {
        mPersistencePolicy.clearAllWindowsExceptFor(windowTags);

        if (TabStateStorageFlagHelper.isTabStorageEnabled()) {
            TabStateStorageService service = TabStateStorageServiceFactory.getForProfile(mProfile);
            if (service != null) service.clearAllWindowsExcept(windowTags);
        }
    }

    boolean hasUnusedDataDepsForTesting() {
        return mUnusedDataDeps != null;
    }

    @Nullable Destroyable getArchivedTabsLeaseForTesting() {
        return mUnusedDataDeps != null ? mUnusedDataDeps.mArchivedTabsLease : null;
    }
}
