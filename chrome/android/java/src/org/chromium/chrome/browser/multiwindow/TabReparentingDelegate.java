// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabGroupTask;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabsTask;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;

/** Class that holds logic to reparent tabs and tab groups to windows. */
@NullMarked
/* package */ class TabReparentingDelegate {

    private final Activity mActivity;
    private final MonotonicObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier;

    /**
     * @param activity The current activity.
     * @param tabModelOrchestratorSupplier Supplier for the {@link TabModelOrchestrator} used for
     *     tab group reparenting.
     */
    public TabReparentingDelegate(
            Activity activity,
            MonotonicObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier) {
        mActivity = activity;
        mTabModelOrchestratorSupplier = tabModelOrchestratorSupplier;
    }

    /* package */ void reparentTabsToNewWindow(
            List<Tab> tabs,
            int windowId,
            boolean openAdjacently,
            @Nullable Runnable finalizeCallback,
            @NewWindowAppSource int source) {
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        windowId,
                        /* preferNew= */ windowId == INVALID_WINDOW_ID,
                        openAdjacently,
                        source);
        ReparentingTabsTask.from(tabs)
                .begin(mActivity, intent, /* startActivityOptions= */ null, finalizeCallback);
    }

    /* package */ void reparentTabsToExistingWindow(
            ChromeTabbedActivity targetActivity,
            List<Tab> tabs,
            int destTabIndex,
            int destGroupTabId) {
        Intent intent =
                createIntentToReparentToExistingWindow(
                        targetActivity, destTabIndex, destGroupTabId);
        ReparentingTabsTask.from(tabs).setupIntent(intent, /* finalizeCallback= */ null);

        targetActivity.onNewIntent(intent);
        ApiCompatibilityUtils.moveTaskToFront(mActivity, targetActivity.getTaskId(), 0);
    }

    /* package */ void reparentTabGroupToNewWindow(
            TabGroupMetadata tabGroupMetadata,
            int windowId,
            boolean openAdjacently,
            @NewWindowAppSource int source) {
        long startTime = SystemClock.elapsedRealtime();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        windowId,
                        /* preferNew= */ windowId == INVALID_WINDOW_ID,
                        openAdjacently,
                        source);
        intent.putExtra(IntentHandler.EXTRA_REPARENT_START_TIME, startTime);

        // Pause observers before detaching tabs.
        pauseObserversForGroupReparenting(tabGroupMetadata);

        // Create the task, detaching the grouped tabs from the current activity.
        ReparentingTabGroupTask reparentingTask = ReparentingTabGroupTask.from(tabGroupMetadata);
        reparentingTask.setupIntent(intent, /* finalizeCallback= */ null);

        // Create the new window and reparent once the TabPersistentStore has resumed.
        TabPersistentStore tabPersistentStore =
                assumeNonNull(mTabModelOrchestratorSupplier.get()).getTabPersistentStore();
        tabPersistentStore.resumeSaveTabList(
                () -> {
                    reparentingTask.begin(mActivity, intent);
                    resumeSyncService(tabGroupMetadata);
                });
    }

    /* package */ void reparentTabGroupToExistingWindow(
            ChromeTabbedActivity targetActivity,
            TabGroupMetadata tabGroupMetadata,
            int destTabIndex) {
        long startTime = SystemClock.elapsedRealtime();
        // 1. Pause the relevant observers prior to detaching the grouped tabs.
        pauseObserversForGroupReparenting(tabGroupMetadata);

        // 2. Setup the re-parenting intent, detaching the grouped tabs from the current activity.
        Intent intent =
                createIntentToReparentToExistingWindow(
                        targetActivity,
                        destTabIndex,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX);
        intent.putExtra(IntentHandler.EXTRA_REPARENT_START_TIME, startTime);
        ReparentingTabGroupTask.from(tabGroupMetadata)
                .setupIntent(intent, /* finalizeCallback= */ null);

        // 3. Resume writes to TabPersistentStore after detaching the grouped Tabs. Don't begin
        // re-attaching the Tabs to the target activity until they have been cleared from this
        // activity's TabPersistentStore.
        TabPersistentStore tabPersistentStore =
                assumeNonNull(mTabModelOrchestratorSupplier.get()).getTabPersistentStore();
        tabPersistentStore.resumeSaveTabList(
                () -> {
                    targetActivity.onNewIntent(intent);
                    ApiCompatibilityUtils.moveTaskToFront(mActivity, targetActivity.getTaskId(), 0);
                    // Re-enable sync service observation after re-parenting is completed to resume
                    // normal sync behavior.
                    resumeSyncService(tabGroupMetadata);
                });
    }

    private Intent createIntentToReparentToExistingWindow(
            ChromeTabbedActivity targetActivity, int destTabIndex, int destGroupTabId) {
        assert targetActivity != null;
        Intent intent = new Intent();
        Context appContext = ContextUtils.getApplicationContext();
        intent.setClassName(appContext, ChromeTabbedActivity.class.getName());
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(
                intent, mActivity, targetActivity.getClass());
        RecordUserAction.record("MobileMenuMoveToOtherWindow");

        assert !(destGroupTabId != TabList.INVALID_TAB_INDEX
                        && destTabIndex != TabList.INVALID_TAB_INDEX)
                : "Only one of dest tab index or dest group tab id should be specified.";
        if (destTabIndex != TabList.INVALID_TAB_INDEX) {
            intent.putExtra(IntentHandler.EXTRA_TAB_INDEX, destTabIndex);
        }
        if (destGroupTabId != TabList.INVALID_TAB_INDEX) {
            IntentHandler.setDestTabId(intent, destGroupTabId);
        }
        return intent;
    }

    private void pauseObserversForGroupReparenting(TabGroupMetadata tabGroupMetadata) {
        // Temporarily disable sync service from observing local changes to prevent unintended
        // updates during tab group re-parenting.
        TabGroupSyncService syncService =
                getTabGroupSyncService(
                        tabGroupMetadata.sourceWindowId, tabGroupMetadata.isIncognito);
        setSyncServiceLocalObservationMode(syncService, /* shouldObserve= */ false);

        // Pause writes to TabPersistentStore while detaching the grouped Tabs.
        TabPersistentStore tabPersistentStore =
                assumeNonNull(mTabModelOrchestratorSupplier.get()).getTabPersistentStore();
        tabPersistentStore.pauseSaveTabList();
    }

    private static void resumeSyncService(TabGroupMetadata tabGroupMetadata) {
        TabGroupSyncService syncService =
                getTabGroupSyncService(
                        tabGroupMetadata.sourceWindowId, tabGroupMetadata.isIncognito);
        setSyncServiceLocalObservationMode(syncService, /* shouldObserve= */ true);
    }

    private static @Nullable TabGroupSyncService getTabGroupSyncService(
            int windowId, boolean isIncognito) {
        TabGroupModelFilter filter = getTabGroupModelFilterByWindowId(windowId, isIncognito);
        if (filter == null) return null;

        @Nullable Profile profile = filter.getTabModel().getProfile();
        if (profile == null
                || profile.isOffTheRecord()
                || !TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) return null;

        return TabGroupSyncServiceFactory.getForProfile(profile);
    }

    private static @Nullable TabGroupModelFilter getTabGroupModelFilterByWindowId(
            int windowId, boolean isIncognito) {
        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance().getTabModelSelectorById(windowId);
        if (selector == null) return null;

        return selector.getTabGroupModelFilter(isIncognito);
    }

    private static void setSyncServiceLocalObservationMode(
            @Nullable TabGroupSyncService syncService, boolean shouldObserve) {
        if (syncService != null) {
            syncService.setLocalObservationMode(shouldObserve);
        }
    }
}
