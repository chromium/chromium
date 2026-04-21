// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.StringRes;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabsTask;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Implements {@link MultiInstanceOrchestrator} as a singleton. */
@NullMarked
/* package */ final class MultiInstanceOrchestratorImpl implements MultiInstanceOrchestrator {
    private static @Nullable MultiInstanceOrchestrator sInstance;
    private static @Nullable TabReparentingDelegate sTabReparentingDelegateForTesting;

    private final TabReparentingDelegate mTabReparentingDelegate;
    private final Map<Activity, MultiInstanceManager> mActivityMultiInstanceManagerAssignments =
            new HashMap<>();

    /** Returns the singleton instance for {@link MultiInstanceOrchestrator}. */
    public static MultiInstanceOrchestrator getInstance() {
        if (sInstance == null) {
            TabReparentingDelegate tabReparentingDelegate =
                    sTabReparentingDelegateForTesting != null
                            ? sTabReparentingDelegateForTesting
                            : new TabReparentingDelegate();
            sInstance = new MultiInstanceOrchestratorImpl(tabReparentingDelegate);
            ResettersForTesting.register(() -> sInstance = null);
        }
        return sInstance;
    }

    private MultiInstanceOrchestratorImpl(TabReparentingDelegate tabReparentingDelegate) {
        mTabReparentingDelegate = tabReparentingDelegate;
        ApplicationStatus.registerStateListenerForAllActivities(this::onActivityStateChange);
    }

    @Override
    public void onInitialize(Activity activity, MultiInstanceManager multiInstanceManager) {
        assert !mActivityMultiInstanceManagerAssignments.containsKey(activity)
                : "A MultiInstanceManager for this Activity already exists.";
        mActivityMultiInstanceManagerAssignments.put(activity, multiInstanceManager);
    }

    @Override
    public boolean createNewWindow(
            Activity sourceActivity,
            boolean isIncognito,
            @Nullable Bundle additionalIntentExtras,
            @Nullable Bundle startActivityOptions,
            @NewWindowAppSource int source) {
        Intent intent = MultiWindowUtils.createNewWindowIntent(sourceActivity, isIncognito, source);
        if (intent == null) return false;

        if (additionalIntentExtras != null) {
            intent.putExtras(additionalIntentExtras);
        }

        MultiInstanceManager.onMultiInstanceModeStarted();
        try {
            sourceActivity.startActivity(intent, startActivityOptions);
            return true;
        } catch (RuntimeException e) {
            return false;
        }
    }

    @Override
    public boolean createNewWindowFromWebContents(
            Activity sourceActivity,
            Profile profile,
            WebContents webContents,
            @Nullable Bundle additionalIntentExtras,
            @Nullable Bundle startActivityOptions,
            @NewWindowAppSource int source) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return false;

        if (!MultiWindowUtils.isWithinInstanceLimit()) {
            var multiInstanceManager = getMultiInstanceManager(sourceActivity);
            if (multiInstanceManager != null) {
                multiInstanceManager.showInstanceCreationLimitMessage();
            }
            return false;
        }

        return mTabReparentingDelegate.createNewWindowFromWebContents(
                sourceActivity,
                profile,
                webContents,
                additionalIntentExtras,
                startActivityOptions,
                source);
    }

    @Override
    public void moveTabsToNewWindow(
            List<Tab> tabs, @Nullable Runnable finalizeCallback, @NewWindowAppSource int source) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;
        if (tabs.isEmpty()) return;
        Activity sourceActivity = TabUtils.getActivity(tabs.get(0));
        if (sourceActivity == null) return;

        if (!MultiWindowUtils.isWithinInstanceLimit()) {
            var multiInstanceManager = getMultiInstanceManager(sourceActivity);
            if (multiInstanceManager != null) {
                multiInstanceManager.showInstanceCreationLimitMessage();
            }
            return;
        }

        boolean openAdjacently = shouldMoveTabsInAdjacentWindow(sourceActivity, tabs.size());
        mTabReparentingDelegate.reparentTabsToNewWindow(
                tabs, INVALID_WINDOW_ID, openAdjacently, finalizeCallback, source);
    }

    @Override
    public void moveTabsToWindowByIdChecked(
            int destWindowId,
            List<Tab> tabs,
            int destTabIndex,
            int destGroupTabId,
            boolean bringToFront) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;
        if (tabs.isEmpty()) return;
        assert destTabIndex == TabList.INVALID_TAB_INDEX
                        || destGroupTabId == TabList.INVALID_TAB_INDEX
                : "Only one of destTabIndex or destGroupTabId should be specified.";
        assert ChromeMultiInstancePersistentStore.hasInstance(destWindowId)
                : "Invalid destination window id.";

        // Validate tabs that are being moved to a tab group in the destination window.
        if (BuildConfig.ENABLE_ASSERTS && destGroupTabId != TabList.INVALID_TAB_INDEX) {
            for (Tab tab : tabs) {
                assert tab.getTabGroupId() == null : "Tab should not be part of a group.";
            }
        }

        Activity destActivity = MultiWindowUtils.getActivityById(destWindowId);
        // Reparent tabs to the activity associated with the specified instance if it is alive. If
        // the instance does not have a live activity, restore it in a new activity to reparent the
        // tabs into.
        if (destActivity != null) {
            mTabReparentingDelegate.reparentTabsToExistingWindow(
                    (ChromeTabbedActivity) destActivity,
                    tabs,
                    destTabIndex,
                    destGroupTabId,
                    bringToFront);
        } else {
            Activity sourceActivity = TabUtils.getActivity(tabs.get(0));
            boolean openAdjacently =
                    sourceActivity != null
                            && shouldMoveTabsInAdjacentWindow(sourceActivity, tabs.size());
            mTabReparentingDelegate.reparentTabsToNewWindow(
                    tabs,
                    destWindowId,
                    openAdjacently,
                    /* finalizeCallback= */ null,
                    NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY);
        }
    }

    @Override
    public void moveTabsToOtherWindow(List<Tab> tabs, @NewWindowAppSource int source) {
        if (tabs.isEmpty()) return;
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            moveTabsToOtherWindowPreApi31(tabs);
            return;
        }

        @PersistedInstanceType int instanceType = PersistedInstanceType.ACTIVE;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            instanceType |=
                    (tabs.get(0).isIncognitoBranded()
                            ? PersistedInstanceType.OFF_THE_RECORD
                            : PersistedInstanceType.REGULAR);
        }
        int instanceCount = MultiWindowUtils.getInstanceCount(instanceType);

        Activity sourceActivity = TabUtils.getActivity(tabs.get(0));
        MultiInstanceManager multiInstanceManager = getMultiInstanceManager(sourceActivity);
        if (instanceCount <= 1) {
            moveTabsToNewWindow(tabs, /* finalizeCallback= */ null, source);

            // Close the source instance window, if needed.
            if (multiInstanceManager != null) {
                multiInstanceManager.closeChromeWindowIfEmpty(
                        multiInstanceManager.getCurrentInstanceId());
            }
            return;
        }

        if (multiInstanceManager != null) {
            ((MultiInstanceManagerApi31) multiInstanceManager)
                    .showTargetSelectorDialog(
                            (instanceInfo) -> {
                                moveTabsToWindowByIdChecked(
                                        instanceInfo.instanceId,
                                        tabs,
                                        /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                                        /* bringToFront= */ true);
                                // Close the source instance window, if needed.
                                multiInstanceManager.closeChromeWindowIfEmpty(
                                        multiInstanceManager.getCurrentInstanceId());
                            },
                            instanceType,
                            R.string.menu_move_tab_to_other_window);
        }
    }

    @Override
    public void moveTabGroupToNewWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        Activity sourceActivity = MultiWindowUtils.getActivityById(tabGroupMetadata.sourceWindowId);
        if (sourceActivity == null) return;

        if (!MultiWindowUtils.isWithinInstanceLimit()) {
            MultiInstanceManager multiInstanceManager = getMultiInstanceManager(sourceActivity);
            if (multiInstanceManager != null) {
                multiInstanceManager.showInstanceCreationLimitMessage();
            }
        } else {
            boolean openAdjacently =
                    shouldMoveTabsInAdjacentWindow(
                            sourceActivity, tabGroupMetadata.tabIdsToUrls.size());
            mTabReparentingDelegate.reparentTabGroupToNewWindow(
                    tabGroupMetadata, INVALID_WINDOW_ID, openAdjacently, source);
        }
    }

    @Override
    public void moveTabGroupToWindowByIdChecked(
            int destWindowId,
            TabGroupMetadata tabGroupMetadata,
            int destTabIndex,
            boolean bringToFront) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        Activity sourceActivity = MultiWindowUtils.getActivityById(tabGroupMetadata.sourceWindowId);
        Activity destActivity = MultiWindowUtils.getActivityById(destWindowId);
        if (destActivity != null) {
            mTabReparentingDelegate.reparentTabGroupToExistingWindow(
                    (ChromeTabbedActivity) destActivity,
                    tabGroupMetadata,
                    destTabIndex,
                    bringToFront);
        } else {
            boolean openAdjacently =
                    sourceActivity != null
                            && shouldMoveTabsInAdjacentWindow(
                                    sourceActivity, tabGroupMetadata.tabIdsToUrls.size());
            mTabReparentingDelegate.reparentTabGroupToNewWindow(
                    tabGroupMetadata,
                    destWindowId,
                    openAdjacently,
                    NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY);
        }
    }

    @Override
    public void moveTabGroupToOtherWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        // Check the number of instances that the tab group is able to move into.
        @PersistedInstanceType int instanceType = PersistedInstanceType.ACTIVE;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            instanceType |=
                    (tabGroupMetadata.isIncognito
                            ? PersistedInstanceType.OFF_THE_RECORD
                            : PersistedInstanceType.REGULAR);
        }
        int instanceCount = MultiWindowUtils.getInstanceCount(instanceType);

        if (instanceCount <= 1) {
            moveTabGroupToNewWindow(tabGroupMetadata, source);
            return;
        }

        Activity sourceActivity = MultiWindowUtils.getActivityById(tabGroupMetadata.sourceWindowId);
        MultiInstanceManagerApi31 multiInstanceManager =
                (MultiInstanceManagerApi31) getMultiInstanceManager(sourceActivity);
        if (multiInstanceManager != null) {
            multiInstanceManager.showTargetSelectorDialog(
                    (instanceInfo) -> {
                        moveTabGroupToWindowByIdChecked(
                                instanceInfo.instanceId,
                                tabGroupMetadata,
                                TabList.INVALID_TAB_INDEX,
                                /* bringToFront= */ true);

                        // Close the source instance window, if needed.
                        multiInstanceManager.closeChromeWindowIfEmpty(
                                tabGroupMetadata.sourceWindowId);
                    },
                    instanceType,
                    R.string.menu_move_group_to_other_window);
        }
    }

    /**
     * Opens a URL in a new or existing window.
     *
     * <p>On Android S+ where multi-instance management is supported, the window in which the URL
     * will be opened will depend on the following criteria, checked in order of priority:
     *
     * <ul>
     *   <li>If there is no other window of a matching profile type, or if {@code preferNew} is
     *       true, a brand-new window will be attempted to be created. An instance creation limit
     *       warning message will be shown on a compatible source activity if instance limit is
     *       reached, and the URL will not be opened.
     *   <li>The target selector dialog will be presented to the user on a compatible source
     *       activity to pick a target window to open the URL in. If the source activity is not
     *       available or compatible for dialog display, or if there is exactly one window of the
     *       target profile type, or if the target window is incognito, the URL will be opened in
     *       the most recently accessed window of the target profile type.
     * </ul>
     *
     * @param sourceActivity The activity initiating the url launch request.
     * @param loadUrlParams The {@link LoadUrlParams} describing the url to open.
     * @param parentTabId The ID of the parent tab, or {@link Tab#INVALID_TAB_ID}.
     * @param preferNew Whether we should prioritize launching the tab in a new window.
     * @param isIncognito Whether the target window should be an incognito window when supported.
     * @return {@code true} if the url launch request was successful, {@code false} otherwise.
     */
    @Override
    public boolean openUrlInOtherWindow(
            Activity sourceActivity,
            LoadUrlParams loadUrlParams,
            int parentTabId,
            boolean preferNew,
            boolean isIncognito) {
        var targetActivityClass =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(sourceActivity);
        if (targetActivityClass == null) return false;

        Intent intent =
                getBasicUrlLaunchIntent(
                        sourceActivity,
                        loadUrlParams,
                        parentTabId,
                        isIncognito,
                        targetActivityClass);
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            addOpenUrlInNewWindowIntentExtras(
                    sourceActivity, intent, /* isIncognitoWindow= */ false);
            MultiInstanceManager.onMultiInstanceModeStarted();
            sourceActivity.startActivity(intent);
            return true;
        }

        @PersistedInstanceType int targetInstanceType = PersistedInstanceType.ACTIVE;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            targetInstanceType |=
                    (isIncognito
                            ? PersistedInstanceType.OFF_THE_RECORD
                            : PersistedInstanceType.REGULAR);
        }
        int instanceCount = MultiWindowUtils.getInstanceCount(targetInstanceType);
        boolean isTargetIncognitoWindow =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        && ((targetInstanceType & PersistedInstanceType.OFF_THE_RECORD) != 0);

        if (sourceActivity instanceof ChromeTabbedActivity cta) {
            // Exclude the current activity from instance count if it is of the same instance type
            // as the target window, because we will not open the URL in this window even though it
            // is eligible.
            if ((cta.isIncognitoWindow() && isTargetIncognitoWindow)
                    || (!cta.isIncognitoWindow() && !isTargetIncognitoWindow)) {
                instanceCount -= 1;
            }
        }

        var multiInstanceManager =
                (MultiInstanceManagerApi31) getMultiInstanceManager(sourceActivity);
        if (instanceCount == 0 || preferNew) {
            if (!MultiWindowUtils.isWithinInstanceLimit()) {
                if (multiInstanceManager != null) {
                    multiInstanceManager.showInstanceCreationLimitMessage();
                }
                return false;
            }
            openUrlInOtherWindowApi31(
                    sourceActivity,
                    intent,
                    /* windowId= */ INVALID_WINDOW_ID,
                    isTargetIncognitoWindow);
            return true;
        }

        // Open implicitly in the last accessed window in relevant scenarios as described above.
        if (isTargetIncognitoWindow || instanceCount == 1 || multiInstanceManager == null) {
            int currentInstanceId =
                    sourceActivity instanceof ChromeTabbedActivity cta
                            ? cta.getWindowId()
                            : INVALID_WINDOW_ID;
            int lastAccessedWindowId =
                    MultiWindowUtils.getLastAccessedWindowIdExcludingSelf(
                            currentInstanceId, targetInstanceType);
            assert lastAccessedWindowId != INVALID_WINDOW_ID
                    : "Last accessed window id for the target instance type should be valid.";
            openUrlInOtherWindowApi31(
                    sourceActivity, intent, lastAccessedWindowId, isTargetIncognitoWindow);
            return true;
        }

        @StringRes int title = R.string.contextmenu_open_in_other_window;
        multiInstanceManager.showTargetSelectorDialog(
                (instanceInfo) ->
                        openUrlInOtherWindowApi31(
                                sourceActivity,
                                intent,
                                instanceInfo.instanceId,
                                /* isIncognitoWindow= */ false),
                targetInstanceType,
                title);
        return true;
    }

    private void moveTabsToOtherWindowPreApi31(List<Tab> tabs) {
        if (tabs.isEmpty()) return;
        Activity sourceActivity = TabUtils.getActivity(tabs.get(0));
        if (sourceActivity == null) return;

        Class<? extends Activity> targetActivity =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(sourceActivity);
        if (targetActivity == null) return;

        Intent intent = new Intent(sourceActivity, targetActivity);
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, sourceActivity, targetActivity);

        MultiInstanceManager.onMultiInstanceModeStarted();
        ReparentingTabsTask.from(tabs)
                .begin(
                        sourceActivity,
                        intent,
                        /* startActivityOptions= */ null,
                        /* finalizeCallback= */ null);
        RecordUserAction.record("MobileMenuMoveToOtherWindow");
    }

    private static void openUrlInOtherWindowApi31(
            Activity sourceActivity, Intent intent, int windowId, boolean isIncognitoWindow) {
        boolean preferNew = windowId == INVALID_WINDOW_ID;
        // Launch the url in an existing window if a valid window id is specified.
        if (!preferNew) {
            Activity destActivity = MultiWindowUtils.getActivityById(windowId);
            if (destActivity != null) {
                assert destActivity instanceof ChromeTabbedActivity;
                ((ChromeTabbedActivity) destActivity).onNewIntent(intent);
                ApiCompatibilityUtils.moveTaskToFront(sourceActivity, destActivity.getTaskId(), 0);
                return;
            }
            // Kill the task for the specified window id if it is alive with its activity destroyed,
            // so that we can subsequently start a new activity in a new task for this window. This
            // adequately handles the scenario without leaving orphaned tasks.
            Set<Integer> activeTaskIds = MultiWindowUtils.getAllAppTaskIds(sourceActivity);
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            if (activeTaskIds.contains(persistedTaskId)) {
                var appTask = AndroidTaskUtils.getAppTaskFromId(sourceActivity, persistedTaskId);
                if (appTask != null) {
                    appTask.finishAndRemoveTask();
                }
            }
        }

        addOpenUrlInNewWindowIntentExtras(sourceActivity, intent, isIncognitoWindow);
        if (preferNew) intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, true);
        else intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, windowId);
        MultiInstanceManager.onMultiInstanceModeStarted();
        sourceActivity.startActivity(intent);
    }

    private static Intent getBasicUrlLaunchIntent(
            Activity sourceActivity,
            LoadUrlParams loadUrlParams,
            int parentTabId,
            boolean isIncognito,
            Class<? extends Activity> targetActivity) {
        Intent intent =
                IntentHandler.createAsyncNewTabIntent(
                        new AsyncTabCreationParams(loadUrlParams),
                        parentTabId,
                        TabLaunchType.FROM_CHROME_UI,
                        isIncognito);

        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, sourceActivity, targetActivity);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }

    private static void addOpenUrlInNewWindowIntentExtras(
            Activity sourceActivity, Intent intent, boolean isIncognitoWindow) {
        if (!MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity)) {
            intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }
        intent.putExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, NewWindowAppSource.URL_LAUNCH);

        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, isIncognitoWindow);
        }
    }

    /**
     * Helps determine whether FLAG_ACTIVITY_LAUNCH_ADJACENT needs to be set in the intent to create
     * a new window when tabs are moved.
     */
    private static boolean shouldMoveTabsInAdjacentWindow(
            Activity sourceActivity, int moveTabCount) {
        if (sourceActivity.isInMultiWindowMode()) return true;
        if (sourceActivity instanceof ChromeTabbedActivity tabbedActivity) {
            int totalTabCount = tabbedActivity.getTabModelSelector().getTotalTabCount();
            if (totalTabCount == moveTabCount) {
                // It is likely that some features will finish the source window's activity when the
                // last set of tabs from a fullscreen window are moved. To avoid unexpected system
                // UX to launch the new window adjacently while the source window is getting closed,
                // we will generally avoid setting the flag in this scenario.
                return false;
            }
        }
        return MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity);
    }

    private void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.DESTROYED) {
            mActivityMultiInstanceManagerAssignments.remove(activity);
        }
    }

    private @Nullable MultiInstanceManager getMultiInstanceManager(@Nullable Activity activity) {
        if (activity == null) return null;
        // Avoid using the MultiInstanceManager for a finishing activity, even if it exists.
        if (activity.isFinishing()) return null;
        return mActivityMultiInstanceManagerAssignments.getOrDefault(activity, null);
    }

    /* package */ static void setTabReparentingDelegateForTesting(TabReparentingDelegate delegate) {
        sTabReparentingDelegateForTesting = delegate;
        ResettersForTesting.register(() -> sTabReparentingDelegateForTesting = null);
    }
}
