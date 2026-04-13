// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.ARCHIVED_WINDOW_TAG;

import org.chromium.base.ResettersForTesting;
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
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.function.Supplier;

/** Cleaner that allows a window to clean up persisted state for another window. */
@NullMarked
public class PersistentStoreCleaner {
    /** Dependencies for cleaning unused data for a specific profile. */
    private static class ProfileScopedUnusedDataDeps {
        public final Profile mProfile;
        public final Set<TabContentManager> mTabContentManagers = new HashSet<>();

        public @Nullable Runnable mCleanupRunnable;

        public ProfileScopedUnusedDataDeps(Profile profile) {
            mProfile = profile;
        }
    }

    private static final Map<Profile, ProfileScopedUnusedDataDeps> sProfileScopedUnusedDataDeps =
            new HashMap<>();

    private static Supplier<TabStateStoreCleaner> sTabStateStoreCleaner = TabStateStoreCleaner::new;
    private static Supplier<TabPersistentStoreImplCleaner> sLegacyCleaner =
            TabPersistentStoreImplCleaner::new;

    private final TabModelOrchestrator mTabModelOrchestrator;
    private final boolean mTabStorageEnabled;
    private final SequencedTaskRunner mSequencedTaskRunner;
    private final TabbedModeTabPersistencePolicy mPersistencePolicy;

    private PersistentStoreCleaner(TabModelOrchestrator tabModelOrchestrator) {
        mTabModelOrchestrator = tabModelOrchestrator;
        mTabStorageEnabled = TabStateStorageFlagHelper.isTabStorageEnabled();

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
    public static void cleanAllWindowsForUnavailableStores(TabModelOrchestrator orchestrator) {
        new PersistentStoreCleaner(orchestrator).clearState();
    }

    /**
     * Cleans up a specific window's tab state for {@link TabPersistentStore}s that are not
     * available in the calling window.
     *
     * @param windowIdToClean The ID of the window to be cleaned.
     * @param orchestrator Used to determine the active store types and provide access to the
     *     profile.
     */
    public static void cleanWindowForUnavailableStores(
            int windowIdToClean, TabModelOrchestrator orchestrator) {
        new PersistentStoreCleaner(orchestrator).cleanWindowForUnavailableStores(windowIdToClean);
    }

    private void cleanWindowForUnavailableStores(int windowIdToClean) {
        if (storeDoesNotExist(StoreType.LEGACY)) {
            cleanPersistentStoreImpl(windowIdToClean);
        }

        if (storeDoesNotExist(StoreType.TAB_STATE_STORE)) {
            maybeCleanTabStateStore(windowIdToClean);
        }
    }

    private void cleanPersistentStoreImpl(int windowIdToClean) {
        // We will not have merged if an instance is not present, so this set will remain empty.
        Set<String> mergedFileNames = new HashSet<>();

        sLegacyCleaner
                .get()
                .cleanupStateFile(
                        windowIdToClean, mPersistencePolicy, mSequencedTaskRunner, mergedFileNames);
    }

    private void maybeCleanTabStateStore(int windowIdToClean) {
        if (!mTabStorageEnabled) return;

        TabModelSelectorBase selector = mTabModelOrchestrator.getTabModelSelector();
        assert selector != null;

        // TODO(crbug.com/479532655): The tab model may have become an {@link EmptyTabModel} at this
        // point. Find a more long-lived source for {@link Profile}s.
        Profile profile = selector.getModel(/* incognito= */ false).getProfile();
        if (profile == null) return;

        sTabStateStoreCleaner.get().cleanupStateFile(windowIdToClean, profile);
    }

    private boolean storeDoesNotExist(@StoreType int type) {
        @StoreType
        Integer authoritativeStoreType = mTabModelOrchestrator.getAuthoritativeStoreType();
        @StoreType Integer shadowStoreType = mTabModelOrchestrator.getShadowStoreType();
        return !Objects.equals(authoritativeStoreType, type)
                && !Objects.equals(shadowStoreType, type);
    }

    private void clearState() {
        if (storeDoesNotExist(StoreType.LEGACY)) {
            sLegacyCleaner.get().clearState(mPersistencePolicy, mSequencedTaskRunner);
        }

        if (storeDoesNotExist(StoreType.TAB_STATE_STORE)) {
            maybeClearTabStateStore();
        }
    }

    private void maybeClearTabStateStore() {
        if (!mTabStorageEnabled) return;

        TabModelSelectorBase selector = mTabModelOrchestrator.getTabModelSelector();
        assert selector != null;

        Profile profile = selector.getModel(/* incognito= */ false).getProfile();
        assert profile != null;

        sTabStateStoreCleaner.get().clearState(profile);
    }

    public static void setTabStateStoreCleanerForTesting(
            Supplier<TabStateStoreCleaner> cleanerFactory) {
        sTabStateStoreCleaner = cleanerFactory;
        ResettersForTesting.register(() -> sTabStateStoreCleaner = TabStateStoreCleaner::new);
    }

    public static void setTabPersistentStoreImplCleanerForTesting(
            Supplier<TabPersistentStoreImplCleaner> cleanerFactory) {
        sLegacyCleaner = cleanerFactory;
        ResettersForTesting.register(() -> sLegacyCleaner = TabPersistentStoreImplCleaner::new);
    }

    /**
     * Schedules the cleaning of unused windows and tab data.
     *
     * @param profile The original profile to which the tabs belong to.
     * @param manager Manages tab thumbnails.
     */
    public static void scheduleCleanUnusedData(Profile profile, TabContentManager manager) {
        if (!ChromeFeatureList.sScheduleWindowCleaning.isEnabled()) return;
        assert !profile.isOffTheRecord();

        ProfileScopedUnusedDataDeps deps = sProfileScopedUnusedDataDeps.get(profile);
        if (deps == null) {
            deps = new ProfileScopedUnusedDataDeps(profile);
            sProfileScopedUnusedDataDeps.put(profile, deps);
        }
        deps.mTabContentManagers.add(manager);

        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        if (tabWindowManager.isAllTabStateInitialized()) {
            maybeCleanUnusedWindows(profile);
        } else {
            tabWindowManager.addObserver(
                    new TabWindowManager.Observer() {
                        @Override
                        public void onAllTabModelStateInitialized() {
                            maybeCleanUnusedWindows(profile);
                            tabWindowManager.removeObserver(this);
                        }
                    });
        }
    }

    /** Schedules the cleaning of unused windows if a cleanup task is not already scheduled. */
    private static void maybeCleanUnusedWindows(Profile profile) {
        assertOnUiThread();

        ProfileScopedUnusedDataDeps deps = sProfileScopedUnusedDataDeps.get(profile);
        if (deps == null || deps.mCleanupRunnable != null) return;

        deps.mCleanupRunnable = () -> cleanUnusedWindows(deps);

        PostTask.postTask(TaskTraits.UI_DEFAULT, deps.mCleanupRunnable);
    }

    private static void cleanUnusedWindows(ProfileScopedUnusedDataDeps deps) {
        deps.mCleanupRunnable = null;
        TabContentManager validManager = null;
        for (TabContentManager manager : deps.mTabContentManagers) {
            if (!manager.isDestroyed()) {
                validManager = manager;
                break;
            }
        }

        if (validManager == null) {
            deps.mTabContentManagers.clear();
            sProfileScopedUnusedDataDeps.remove(deps.mProfile);
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
                        () -> maybeCleanUnusedWindows(deps.mProfile),
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
        deleteAllWindowsExceptFor(deps.mProfile, windowTags);

        sProfileScopedUnusedDataDeps.remove(deps.mProfile);
    }

    private static void deleteAllTabDataExceptFor(TabContentManager manager, List<Integer> tabIds) {
        int[] tabIdsArray = new int[tabIds.size()];
        for (int i = 0; i < tabIds.size(); i++) {
            tabIdsArray[i] = tabIds.get(i);
        }
        manager.removeAllTabThumbnailsExceptForIds(tabIdsArray);
    }

    private static void deleteAllWindowsExceptFor(Profile profile, List<String> windowTags) {
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
            TabStateStorageService service = TabStateStorageServiceFactory.getForProfile(profile);
            if (service != null) service.clearAllWindowsExcept(windowTags);
        }
    }
}
