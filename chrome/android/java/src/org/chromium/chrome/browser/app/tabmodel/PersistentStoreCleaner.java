// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.TabStateStore.TabStateStoreCleaner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl.TabPersistentStoreImplCleaner;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.HashSet;
import java.util.Objects;
import java.util.Set;
import java.util.function.Supplier;

/** Cleaner that allows a window to clean up persisted state for another window. */
@NullMarked
public class PersistentStoreCleaner {
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
}
