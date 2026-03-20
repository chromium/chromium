// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;

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
    public void moveTabsToNewWindow(
            List<Tab> tabs, @Nullable Runnable finalizeCallback, @NewWindowAppSource int source) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;
        if (tabs.isEmpty()) return;
        Context tabContext = tabs.get(0).getContext();

        if (!MultiWindowUtils.canCreateNewWindow()) {
            var multiInstanceManager = getMultiInstanceManager(tabContext);
            if (multiInstanceManager != null) {
                multiInstanceManager.showInstanceCreationLimitMessage();
            }
            return;
        }

        boolean openAdjacently =
                !(tabContext instanceof Activity)
                        || MultiWindowUtils.shouldOpenInAdjacentWindow((Activity) tabContext);
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
            Context tabContext = tabs.get(0).getContext();
            boolean openAdjacently = false;
            if (tabContext instanceof Activity activity) {
                openAdjacently = MultiWindowUtils.shouldOpenInAdjacentWindow(activity);
            }
            mTabReparentingDelegate.reparentTabsToNewWindow(
                    tabs,
                    destWindowId,
                    openAdjacently,
                    /* finalizeCallback= */ null,
                    NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY);
        }
    }

    private void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.DESTROYED) {
            mActivityMultiInstanceManagerAssignments.remove(activity);
        }
    }

    private @Nullable MultiInstanceManager getMultiInstanceManager(Context context) {
        if (!(context instanceof Activity activity)) return null;
        // Avoid using the MultiInstanceManager for a finishing activity, even if it exists.
        if (activity.isFinishing()) return null;
        return mActivityMultiInstanceManagerAssignments.getOrDefault(activity, null);
    }

    /* package */ static void setTabReparentingDelegateForTesting(TabReparentingDelegate delegate) {
        sTabReparentingDelegateForTesting = delegate;
        ResettersForTesting.register(() -> sTabReparentingDelegateForTesting = null);
    }
}
