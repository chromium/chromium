// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabGroupTask;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabsTask;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/** Class that holds logic to reparent tabs and tab groups to windows. */
@NullMarked
/* package */ class TabReparentingDelegate {
    private static @Nullable TabPersistentStore sPersistentStoreForTesting;

    /* package */ void reparentTabsToNewWindow(
            List<Tab> tabs,
            int windowId,
            boolean openAdjacently,
            @Nullable Runnable finalizeCallback,
            @NewWindowAppSource int source) {
        if (tabs.isEmpty()) return;
        Context context = tabs.get(0).getContext();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context,
                        windowId,
                        /* preferNew= */ windowId == INVALID_WINDOW_ID,
                        openAdjacently,
                        source);
        ReparentingTabsTask.from(tabs)
                .begin(context, intent, /* startActivityOptions= */ null, finalizeCallback);
    }

    /* package */ boolean createNewWindowFromWebContents(
            Activity sourceActivity,
            Profile profile,
            WebContents webContents,
            @Nullable Bundle additionalIntentExtras,
            @Nullable Bundle startActivityOptions,
            @NewWindowAppSource int source) {
        TabDelegateFactory delegateFactory = CustomTabDelegateFactory.createEmpty();
        Tab tab =
                TabBuilder.createDetachedSpareTab(
                        sourceActivity, delegateFactory, profile, webContents);

        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        sourceActivity, profile.isIncognitoBranded(), source);
        if (intent == null) {
            tab.destroy();
            return false;
        }
        if (additionalIntentExtras != null) {
            intent.putExtras(additionalIntentExtras);
        }

        return ReparentingTabsTask.from(List.of(tab))
                .begin(sourceActivity, intent, startActivityOptions, /* finalizeCallback= */ null);
    }

    /* package */ void reparentTabsToExistingWindow(
            ChromeTabbedActivity targetActivity,
            List<Tab> tabs,
            int destTabIndex,
            int destGroupTabId,
            boolean bringToFront) {
        if (tabs.isEmpty()) return;
        Context context = tabs.get(0).getContext();
        Intent intent =
                createIntentToReparentToExistingWindow(
                        context, targetActivity, destTabIndex, destGroupTabId);
        ReparentingTabsTask.from(tabs).setupIntent(intent, /* finalizeCallback= */ null);

        targetActivity.onNewIntent(intent);
        if (bringToFront) {
            ApiCompatibilityUtils.moveTaskToFront(targetActivity, targetActivity.getTaskId(), 0);
        }
    }

    /* package */ void reparentTabGroupToNewWindow(
            TabGroupMetadata tabGroupMetadata,
            int windowId,
            boolean openAdjacently,
            @NewWindowAppSource int source) {
        long startTime = SystemClock.elapsedRealtime();
        Activity sourceActivity = MultiWindowUtils.getActivityById(tabGroupMetadata.sourceWindowId);
        Context context =
                sourceActivity == null ? ContextUtils.getApplicationContext() : sourceActivity;
        TabPersistentStore tabPersistentStore = null;
        if (sourceActivity instanceof ChromeTabbedActivity tabbedActivity) {
            // The tab persistent store is required to pause / resume the tab group sync service
            // during tab group reparenting. This is relevant only for a ChromeTabbedActivity.
            tabPersistentStore = getTabPersistentStore(tabbedActivity);
        }
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context,
                        windowId,
                        /* preferNew= */ windowId == INVALID_WINDOW_ID,
                        openAdjacently,
                        source);
        intent.putExtra(IntentHandler.EXTRA_REPARENT_START_TIME, startTime);

        // Pause observers before detaching tabs.
        pauseObserversForGroupReparenting(tabPersistentStore, tabGroupMetadata);

        // Create the task, detaching the grouped tabs from the current activity.
        ReparentingTabGroupTask reparentingTask = ReparentingTabGroupTask.from(tabGroupMetadata);
        reparentingTask.setupIntent(intent, /* finalizeCallback= */ null);

        // Create the new window and reparent once the TabPersistentStore has resumed.
        if (tabPersistentStore != null) {
            tabPersistentStore.resumeSaveTabList(
                    () -> {
                        reparentingTask.begin(context, intent);
                        resumeSyncService(tabGroupMetadata);
                    });
        } else {
            reparentingTask.begin(context, intent);
        }
    }

    /* package */ void reparentTabGroupToExistingWindow(
            ChromeTabbedActivity targetActivity,
            TabGroupMetadata tabGroupMetadata,
            int destTabIndex,
            boolean bringToFront) {
        long startTime = SystemClock.elapsedRealtime();

        Activity sourceActivity = MultiWindowUtils.getActivityById(tabGroupMetadata.sourceWindowId);
        Context context =
                sourceActivity == null ? ContextUtils.getApplicationContext() : sourceActivity;
        TabPersistentStore tabPersistentStore = null;
        if (sourceActivity instanceof ChromeTabbedActivity tabbedActivity) {
            // The tab persistent store is required to pause / resume the tab group sync service
            // during tab group reparenting. This is relevant only for a ChromeTabbedActivity.
            tabPersistentStore = getTabPersistentStore(tabbedActivity);
        }

        // 1. Pause the relevant observers prior to detaching the grouped tabs.
        pauseObserversForGroupReparenting(tabPersistentStore, tabGroupMetadata);

        // 2. Setup the re-parenting intent, detaching the grouped tabs from the current activity.
        Intent intent =
                createIntentToReparentToExistingWindow(
                        context,
                        targetActivity,
                        destTabIndex,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX);
        intent.putExtra(IntentHandler.EXTRA_REPARENT_START_TIME, startTime);
        ReparentingTabGroupTask.from(tabGroupMetadata)
                .setupIntent(intent, /* finalizeCallback= */ null);

        // 3. Resume writes to TabPersistentStore after detaching the grouped Tabs. Don't begin
        // re-attaching the Tabs to the target activity until they have been cleared from this
        // activity's TabPersistentStore.
        if (tabPersistentStore != null) {
            tabPersistentStore.resumeSaveTabList(
                    () -> {
                        targetActivity.onNewIntent(intent);
                        if (bringToFront) {
                            ApiCompatibilityUtils.moveTaskToFront(
                                    targetActivity, targetActivity.getTaskId(), 0);
                        }
                        // Re-enable sync service observation after re-parenting is completed to
                        // resume normal sync behavior.
                        resumeSyncService(tabGroupMetadata);
                    });
        } else {
            targetActivity.onNewIntent(intent);
            if (bringToFront) {
                ApiCompatibilityUtils.moveTaskToFront(
                        targetActivity, targetActivity.getTaskId(), 0);
            }
        }
    }

    private static Intent createIntentToReparentToExistingWindow(
            Context context,
            ChromeTabbedActivity targetActivity,
            int destTabIndex,
            int destGroupTabId) {
        assert targetActivity != null;
        Intent intent = new Intent();
        Context appContext = ContextUtils.getApplicationContext();
        intent.setClassName(appContext, ChromeTabbedActivity.class.getName());
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(
                intent, context, targetActivity.getClass());
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

    private static void pauseObserversForGroupReparenting(
            @Nullable TabPersistentStore tabPersistentStore, TabGroupMetadata tabGroupMetadata) {
        if (tabPersistentStore == null) return;
        // Temporarily disable sync service from observing local changes to prevent unintended
        // updates during tab group re-parenting.
        TabGroupSyncService syncService =
                getTabGroupSyncService(
                        tabGroupMetadata.sourceWindowId, tabGroupMetadata.isIncognito);
        setSyncServiceLocalObservationMode(syncService, /* shouldObserve= */ false);

        // Pause writes to TabPersistentStore while detaching the grouped Tabs.
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
        TabModel tabModel = getTabModelByWindowId(windowId, isIncognito);
        if (tabModel == null) return null;

        @Nullable Profile profile = tabModel.getProfile();
        if (profile == null
                || profile.isOffTheRecord()
                || !TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) return null;

        return TabGroupSyncServiceFactory.getForProfile(profile);
    }

    private static @Nullable TabModel getTabModelByWindowId(int windowId, boolean isIncognito) {
        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance().getTabModelSelectorById(windowId);
        if (selector == null) return null;

        return selector.getModel(isIncognito);
    }

    private static void setSyncServiceLocalObservationMode(
            @Nullable TabGroupSyncService syncService, boolean shouldObserve) {
        if (syncService != null) {
            syncService.setLocalObservationMode(shouldObserve);
        }
    }

    private static @Nullable TabPersistentStore getTabPersistentStore(
            ChromeTabbedActivity activity) {
        if (sPersistentStoreForTesting != null) return sPersistentStoreForTesting;
        TabPersistentStore tabPersistentStore = null;

        var tabModelOrchestrator = activity.getTabModelOrchestratorSupplier().get();
        if (tabModelOrchestrator != null) {
            tabPersistentStore = tabModelOrchestrator.getTabPersistentStore();
        }

        return tabPersistentStore;
    }

    /* package */ static void setPersistentStoreForTesting(TabPersistentStore tabPersistentStore) {
        sPersistentStoreForTesting = tabPersistentStore;
        ResettersForTesting.register(() -> sPersistentStoreForTesting = null);
    }
}
