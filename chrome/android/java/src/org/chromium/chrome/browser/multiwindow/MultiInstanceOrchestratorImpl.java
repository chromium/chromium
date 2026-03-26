// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.HashMap;
import java.util.HashSet;
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
    public void moveTabsToNewWindow(
            List<Tab> tabs, @Nullable Runnable finalizeCallback, @NewWindowAppSource int source) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;
        if (tabs.isEmpty()) return;
        Activity sourceActivity = TabUtils.getActivity(tabs.get(0));

        if (!MultiWindowUtils.canCreateNewWindow()) {
            var multiInstanceManager = getMultiInstanceManager(sourceActivity);
            if (multiInstanceManager != null) {
                multiInstanceManager.showInstanceCreationLimitMessage();
            }
            return;
        }

        boolean openAdjacently =
                sourceActivity == null
                        || MultiWindowUtils.shouldOpenInAdjacentWindow(sourceActivity);
        mTabReparentingDelegate.reparentTabsToNewWindow(
                tabs, INVALID_WINDOW_ID, openAdjacently, finalizeCallback, source);
    }

    @Override
    public void moveTabsToWindowByIdChecked(
            int destWindowId, List<Tab> tabs, int destTabIndex, int destGroupTabId) {
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
                    (ChromeTabbedActivity) destActivity, tabs, destTabIndex, destGroupTabId);
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
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            // TODO (crbug.com/475571336): Move ChromeAsyncTabLauncher URL launch for pre-Api31 to
            // this method.
            return false;
        }
        int parentTabId = sourceTab.getParentId();
        Activity sourceActivity = TabUtils.getActivity(sourceTab);
        MultiInstanceManager multiInstanceManager = getMultiInstanceManager(sourceActivity);

        boolean incognitoInstance = sourceTab.isIncognitoBranded();
        @PersistedInstanceType int instanceType = PersistedInstanceType.ACTIVE;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            instanceType |=
                    (incognitoInstance
                            ? PersistedInstanceType.OFF_THE_RECORD
                            : PersistedInstanceType.REGULAR);
        }

        // Check the number of instances that the url can be launched in.
        int instanceCount = MultiWindowUtils.getInstanceCount(instanceType);
        if (instanceCount <= 1 || preferNew) {
            if (preferNew && !MultiWindowUtils.canCreateNewWindow()) {
                if (multiInstanceManager != null) {
                    multiInstanceManager.showInstanceCreationLimitMessage();
                }
                return false;
            }

            return launchUrlInOtherWindow(
                    sourceActivity,
                    incognitoInstance,
                    loadUrlParams,
                    parentTabId,
                    /* otherActivity= */ null,
                    preferNew);
        }

        if (multiInstanceManager != null) {
            ((MultiInstanceManagerApi31) multiInstanceManager)
                    .showTargetSelectorDialog(
                            (instanceInfo) -> {
                                ChromeTabbedActivity selectedActivity =
                                        (ChromeTabbedActivity)
                                                MultiWindowUtils.getActivityById(
                                                        instanceInfo.instanceId);
                                launchUrlInOtherWindow(
                                        sourceActivity,
                                        /* isIncognito= */ selectedActivity != null
                                                && selectedActivity.isIncognitoWindow(),
                                        loadUrlParams,
                                        parentTabId,
                                        selectedActivity,
                                        /* preferNew= */ false);
                            },
                            instanceType,
                            R.string.contextmenu_open_in_other_window);
        }
        return true;
    }

    @Override
    public Set<Integer> getUsableWindowIds(@PersistedInstanceType int type) {
        Set<Integer> ids = MultiWindowUtils.getPersistedInstanceIds(type);
        Set<Integer> usableIds = new HashSet<>();
        for (int id : ids) {
            if (!ChromeMultiInstancePersistentStore.readMarkedForDeletion(id)) {
                usableIds.add(id);
            }
        }
        return usableIds;
    }

    private boolean launchUrlInOtherWindow(
            @Nullable Activity sourceActivity,
            boolean isIncognito,
            LoadUrlParams loadUrlParams,
            int parentId,
            @Nullable Activity otherActivity,
            boolean preferNew) {
        if (sourceActivity == null) return false;
        ChromeAsyncTabLauncher chromeAsyncTabLauncher = new ChromeAsyncTabLauncher(isIncognito);
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
