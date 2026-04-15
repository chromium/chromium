// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.ARCHIVED_WINDOW_TAG;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.TabStateStore.TabStateStoreCleaner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
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
    private static class UnusedDataDeps {
        public final Set<TabContentManager> mTabContentManagers = new HashSet<>();
        public @Nullable Runnable mCleanupRunnable;
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
                        /* tabMergingEnabled= */ false);
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
        if (!ChromeFeatureList.sScheduleWindowCleaning.isEnabled()) return;
        if (mUnusedDataDeps == null) {
            mUnusedDataDeps = new UnusedDataDeps();
        }
        mUnusedDataDeps.mTabContentManagers.add(manager);

        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        if (tabWindowManager.isAllTabStateInitialized()) {
            maybeCleanUnusedWindows();
        } else {
            tabWindowManager.addObserver(
                    new TabWindowManager.Observer() {
                        @Override
                        public void onAllTabModelStateInitialized() {
                            maybeCleanUnusedWindows();
                            tabWindowManager.removeObserver(this);
                        }
                    });
        }
    }

    /** Schedules the cleaning of unused windows if a cleanup task is not already scheduled. */
    private void maybeCleanUnusedWindows() {
        assertOnUiThread();

        if (mUnusedDataDeps == null || mUnusedDataDeps.mCleanupRunnable != null) return;
        mUnusedDataDeps.mCleanupRunnable = () -> cleanUnusedWindows(assumeNonNull(mUnusedDataDeps));

        PostTask.postTask(TaskTraits.UI_DEFAULT, mUnusedDataDeps.mCleanupRunnable);
    }

    private void cleanUnusedWindows(UnusedDataDeps deps) {
        TabContentManager validManager = null;
        for (TabContentManager manager : deps.mTabContentManagers) {
            if (!manager.isDestroyed()) {
                validManager = manager;
                break;
            }
        }

        if (validManager == null) {
            deps.mTabContentManagers.clear();
            mUnusedDataDeps = null;
            return;
        }

        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        List<String> windowTags = new ArrayList<>();
        List<TabModelSelector> selectors = new ArrayList<>();

        TabModelSelector archivedTabModelSelector = tabWindowManager.getArchivedTabModelSelector();
        assert archivedTabModelSelector != null;
        selectors.add(archivedTabModelSelector);
        windowTags.add(ARCHIVED_WINDOW_TAG);

        for (TabModelSelector selector : tabWindowManager.getAllTabModelSelectors()) {
            selectors.add(selector);
            int windowId = tabWindowManager.getWindowIdForSelector(selector);
            windowTags.add(String.valueOf(windowId));
        }

        for (TabModelSelector selector : tabWindowManager.getCustomTabsTabModelSelectors()) {
            selectors.add(selector);
            int taskId = tabWindowManager.getTaskIdForCustomTab(selector);
            windowTags.add(String.valueOf(taskId));
        }

        // Retry once selectors are fully initialized.
        assert !selectors.isEmpty();
        for (TabModelSelector selector : selectors) {
            if (!selector.isTabStateInitialized()) {
                TabModelUtils.runOnTabStateInitialized(
                        () -> {
                            deps.mCleanupRunnable = null;
                            maybeCleanUnusedWindows();
                        },
                        selectors.toArray(new TabModelSelector[0]));
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

        deleteAllTabDataExceptFor(validManager, tabIds);
        deleteAllWindowsExceptFor(windowTags);

        mUnusedDataDeps = null;
    }

    private void deleteAllTabDataExceptFor(TabContentManager manager, List<Integer> tabIds) {
        int[] tabIdsArray = new int[tabIds.size()];
        for (int i = 0; i < tabIds.size(); i++) {
            tabIdsArray[i] = tabIds.get(i);
        }
        manager.removeAllTabThumbnailsExceptForIds(tabIdsArray);
    }

    private void deleteAllWindowsExceptFor(List<String> windowTags) {
        SequencedTaskRunner sequencedTaskRunner =
                PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);

        // Do not need to provide a valid window ID, since it is not used for any operation.
        TabbedModeTabPersistencePolicy policy =
                new TabbedModeTabPersistencePolicy(
                        /* selectorIndex= */ TabWindowManager.INVALID_WINDOW_ID,
                        /* mergeTabsOnStartup= */ false,
                        /* tabMergingEnabled= */ false);
        policy.performInitialization(sequencedTaskRunner);
        policy.clearAllWindowsExceptFor(windowTags);

        if (TabStateStorageFlagHelper.isTabStorageEnabled()) {
            TabStateStorageService service = TabStateStorageServiceFactory.getForProfile(mProfile);
            if (service != null) service.clearAllWindowsExcept(windowTags);
        }
    }

    public boolean hasUnusedDataDepsForTesting() {
        return mUnusedDataDeps != null;
    }
}
