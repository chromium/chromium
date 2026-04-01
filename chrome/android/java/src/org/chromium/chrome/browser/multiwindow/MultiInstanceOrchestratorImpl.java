// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.StringRes;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

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
    public @Nullable Intent createNewWindowIntent(
            Activity sourceActivity, boolean isIncognito, @NewWindowAppSource int source) {
        boolean isInMultiWindowMode =
                MultiWindowUtils.getInstance().isInMultiWindowMode(sourceActivity);
        boolean isInMultiDisplayMode =
                MultiWindowUtils.getInstance().isInMultiDisplayMode(sourceActivity);

        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            boolean openAdjacently =
                    (MultiWindowUtils.canEnterMultiWindowMode()
                                    || isInMultiWindowMode
                                    || isInMultiDisplayMode)
                            && MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity);

            Intent intent =
                    MultiWindowUtils.createNewWindowIntent(
                            sourceActivity,
                            MultiInstanceManager.INVALID_WINDOW_ID,
                            /* preferNew= */ true,
                            openAdjacently,
                            source);
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, isIncognito);
            return intent;
        }

        assert !isIncognito : "Opening an incognito window isn't supported";
        assert isInMultiWindowMode || isInMultiDisplayMode
                : "Current windowing mode doesn't support opening a new window";

        Class<? extends Activity> targetActivity =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(sourceActivity);
        if (targetActivity == null) return null;

        Intent intent = new Intent(sourceActivity, targetActivity);
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, sourceActivity, targetActivity);

        intent.putExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, source);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity)) {
            intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }

        return intent;
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

        boolean openAdjacently = MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity);
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
                            && MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity);
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
            boolean openAdjacently = MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity);
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
                            && MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity);
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
     *   <li>If there is no other window of a matching profile type, a new window will be created.
     *   <li>If {@code preferNew} is true, a new window will be attempted to be created. Note that
     *       this will ensure that the URL is opened in a brand-new window vs in a new activity
     *       created for a restored inactive instance. However, an instance creation limit warning
     *       message will be shown on a compatible source activity if instance limit is reached, and
     *       the URL will not be opened.
     *   <li>The target selector dialog will be presented to the user on a compatible source
     *       activity to pick a target window to open the URL in. If the source activity is not
     *       available or compatible for dialog display, the URL will be opened in the most recently
     *       accessed window of the same profile type.
     * </ul>
     *
     * @param sourceTab The tab that is initiating the URL launch.
     * @param loadUrlParams The url to open.
     * @param preferNew Whether we should prioritize launching the tab in a new window.
     */
    @Override
    public boolean openUrlInOtherWindow(
            Tab sourceTab, LoadUrlParams loadUrlParams, boolean preferNew) {
        int parentTabId = sourceTab.getParentId();
        boolean isIncognitoTab = sourceTab.isIncognitoBranded();
        Activity sourceActivity = TabUtils.getActivity(sourceTab);
        if (sourceActivity == null) return false;

        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            Activity otherActivity = MultiWindowUtils.getForegroundWindowActivity(sourceActivity);
            return launchUrlInOtherWindow(
                    sourceActivity,
                    /* isIncognitoWindow= */ false,
                    loadUrlParams,
                    parentTabId,
                    otherActivity,
                    /* preferNew= */ false);
        }

        @PersistedInstanceType int instanceType = PersistedInstanceType.ACTIVE;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            instanceType |=
                    (isIncognitoTab
                            ? PersistedInstanceType.OFF_THE_RECORD
                            : PersistedInstanceType.REGULAR);
        }

        return openUrlInWindowApi31(sourceTab, loadUrlParams, preferNew, instanceType);
    }

    @Override
    public void openUrlInIncognitoWindow(Tab sourceTab, LoadUrlParams loadUrlParams) {
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            // This also means that Android S+ multi-instance support is disabled.
            return;
        }

        @PersistedInstanceType
        int instanceType = PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD;
        openUrlInWindowApi31(sourceTab, loadUrlParams, /* preferNew= */ false, instanceType);
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

    private boolean openUrlInWindowApi31(
            Tab sourceTab,
            LoadUrlParams loadUrlParams,
            boolean preferNew,
            @PersistedInstanceType int targetInstanceType) {
        int parentTabId = sourceTab.getParentId();
        Activity sourceActivity = TabUtils.getActivity(sourceTab);
        if (sourceActivity == null) return false;

        var multiInstanceManager =
                (MultiInstanceManagerApi31) getMultiInstanceManager(sourceActivity);
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

        if (instanceCount == 0 || preferNew) {
            if (!MultiWindowUtils.isWithinInstanceLimit()) {
                if (multiInstanceManager != null) {
                    multiInstanceManager.showInstanceCreationLimitMessage();
                }
                return false;
            }

            return launchUrlInOtherWindow(
                    sourceActivity,
                    sourceTab.isIncognitoBranded() || isTargetIncognitoWindow,
                    loadUrlParams,
                    parentTabId,
                    /* otherActivity= */ null,
                    /* preferNew= */ true);
        }

        if (isTargetIncognitoWindow) {
            // Launch the URL in the last accessed incognito window.
            Activity destActivity =
                    MultiWindowUtils.getForegroundWindowActivityWithProfileType(
                            sourceActivity, /* incognito= */ true);
            if (destActivity != null) {
                launchUrlInOtherWindow(
                        sourceActivity,
                        /* isIncognitoWindow= */ true,
                        loadUrlParams,
                        parentTabId,
                        destActivity,
                        /* preferNew= */ false);
                return true;
            }
            return false;
        }

        if (multiInstanceManager != null) {
            @StringRes int title = R.string.contextmenu_open_in_other_window;
            multiInstanceManager.showTargetSelectorDialog(
                    onWindowSelectedForUrlLaunch(
                            sourceActivity,
                            parentTabId,
                            loadUrlParams,
                            /* isIncognitoWindow= */ false),
                    targetInstanceType,
                    title);
        }
        return true;
    }

    static Callback<InstanceInfo> onWindowSelectedForUrlLaunch(
            Activity sourceActivity,
            int parentTabId,
            LoadUrlParams loadUrlParams,
            boolean isIncognitoWindow) {
        return (instanceInfo) -> {
            Activity selectedActivity = MultiWindowUtils.getActivityById(instanceInfo.instanceId);
            if (selectedActivity != null) {
                launchUrlInOtherWindow(
                        sourceActivity,
                        isIncognitoWindow,
                        loadUrlParams,
                        parentTabId,
                        selectedActivity,
                        /* preferNew= */ false);
            }
            // TODO (crbug.com/495856301): Handle URL launches for active instances with
            // destroyed activities.
        };
    }

    private static boolean launchUrlInOtherWindow(
            Activity sourceActivity,
            boolean isIncognitoWindow,
            LoadUrlParams loadUrlParams,
            int parentId,
            @Nullable Activity otherActivity,
            boolean preferNew) {
        ChromeAsyncTabLauncher chromeAsyncTabLauncher =
                new ChromeAsyncTabLauncher(isIncognitoWindow);
        chromeAsyncTabLauncher.launchTabInOtherWindow(
                loadUrlParams,
                sourceActivity,
                parentId,
                otherActivity,
                NewWindowAppSource.URL_LAUNCH,
                preferNew);
        return true;
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
