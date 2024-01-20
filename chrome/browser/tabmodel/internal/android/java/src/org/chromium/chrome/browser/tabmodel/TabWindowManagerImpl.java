// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.Pair;
import android.util.SparseArray;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Manages multiple {@link TabModelSelector} instances, each owned by different {@link Activity}s.
 *
 * Also manages tabs being reparented in AsyncTabParamsManager.
 */
public class TabWindowManagerImpl implements ActivityStateListener, TabWindowManager {

    public static final String TAG_MULTI_INSTANCE = "MultiInstance";
    private TabModelSelectorFactory mSelectorFactory;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final int mMaxSelectors;

    private List<TabModelSelector> mSelectors = new ArrayList<>();

    private Map<Activity, TabModelSelector> mAssignments = new HashMap<>();

    TabWindowManagerImpl(
            TabModelSelectorFactory selectorFactory,
            AsyncTabParamsManager asyncTabParamsManager,
            int maxSelectors) {
        mSelectorFactory = selectorFactory;
        mAsyncTabParamsManager = asyncTabParamsManager;
        ApplicationStatus.registerStateListenerForAllActivities(this);
        mMaxSelectors = maxSelectors;
        for (int i = 0; i < mMaxSelectors; i++) mSelectors.add(null);
    }

    @Override
    public int getMaxSimultaneousSelectors() {
        return mMaxSelectors;
    }

    @Override
    public Pair<Integer, TabModelSelector> requestSelector(
            Activity activity,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            int index) {
        if (index < 0 || index >= mSelectors.size()) return null;

        // Return the already existing selector if found.
        if (mAssignments.get(activity) != null) {
            TabModelSelector assignedSelector = mAssignments.get(activity);
            for (int i = 0; i < mSelectors.size(); i++) {
                if (mSelectors.get(i) == assignedSelector) {
                    Pair res = Pair.create(i, assignedSelector);
                    Log.i(
                            TAG_MULTI_INSTANCE,
                            "Returning existing selector with index: "
                                    + res
                                    + ". Requested index: "
                                    + index);
                    assertIndicesMatch(index, i, "Activity already mapped; ", activity);
                    return res;
                }
            }
            // The following log statement is used in tools/android/build_speed/benchmark.py. Please
            // update the string there if this string is updated.
            throw new IllegalStateException(
                    "TabModelSelector is assigned to an Activity but has no index.");
        }

        int originalIndex = index;
        if (mSelectors.get(index) != null) {
            for (int i = 0; i < mSelectors.size(); i++) {
                if (mSelectors.get(i) == null) {
                    index = i;
                    break;
                }
            }
        }

        // Too many activities going at once.
        if (mSelectors.get(index) != null) return null;

        TabModelSelector selector =
                mSelectorFactory.buildSelector(
                        activity,
                        profileProviderSupplier,
                        tabCreatorManager,
                        nextTabPolicySupplier);
        mSelectors.set(index, selector);
        mAssignments.put(activity, selector);

        Pair res = Pair.create(index, selector);
        Log.i(TAG_MULTI_INSTANCE, "Returning new selector for " + activity + " with index: " + res);
        assertIndicesMatch(originalIndex, index, "Index in use; ", activity);
        return res;
    }

    private void assertIndicesMatch(
            int requestedIndex, int returnedIndex, String type, Activity activity) {
        if (requestedIndex == returnedIndex
                || !BuildConfig.ENABLE_ASSERTS
                || BuildConfig.IS_FOR_TEST
                || VERSION.SDK_INT < VERSION_CODES.Q) {
            return;
        }

        TabModelSelector selectorAtRequestedIndex = mSelectors.get(requestedIndex);
        Activity activityAtRequestedIndex = null;
        for (Activity mappedActivity : mAssignments.keySet()) {
            if (mAssignments.get(mappedActivity).equals(selectorAtRequestedIndex)) {
                activityAtRequestedIndex = mappedActivity;
                break;
            }
        }

        String message =
                type
                        + "Requested "
                        + requestedIndex
                        + " and returned "
                        + returnedIndex
                        + " new activity: "
                        + activity
                        + " new activity task id: "
                        + activity.getTaskId()
                        + " activity at requested index: "
                        + activityAtRequestedIndex;
        if (activityAtRequestedIndex != null) {
            // Start actively listen to activity status once conflict at index is found.
            ApplicationStatus.registerStateListenerForActivity(
                    (activityAtIndex, newState) -> {
                        final int localTaskId = ApplicationStatus.getTaskId(activityAtIndex);
                        Log.i(
                                TAG_MULTI_INSTANCE,
                                "ActivityAtRequestedIndex "
                                        + activityAtIndex
                                        + " taskId "
                                        + localTaskId
                                        + " newState "
                                        + newState);
                    },
                    activityAtRequestedIndex);

            message +=
                    " ApplicationStatus activity state: "
                            + ApplicationStatus.getStateForActivity(activityAtRequestedIndex)
                            + " activity task Id: "
                            + activityAtRequestedIndex.getTaskId()
                            + " activity is finishing? "
                            + activityAtRequestedIndex.isFinishing()
                            + " tasks: [";
            ActivityManager activityManager =
                    (ActivityManager)
                            activityAtRequestedIndex.getSystemService(Context.ACTIVITY_SERVICE);
            for (AppTask task : activityManager.getAppTasks()) {
                ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
                message += info + ";\n";
            }

            message += "]";
        }

        assert requestedIndex == returnedIndex : message;
        Log.i(TAG_MULTI_INSTANCE, message);
    }

    @Override
    public int getIndexForWindow(Activity activity) {
        if (activity == null) return TabWindowManager.INVALID_WINDOW_INDEX;
        TabModelSelector selector = mAssignments.get(activity);
        if (selector == null) return TabWindowManager.INVALID_WINDOW_INDEX;
        int index = mSelectors.indexOf(selector);
        return index == -1 ? TabWindowManager.INVALID_WINDOW_INDEX : index;
    }

    @Override
    public int getNumberOfAssignedTabModelSelectors() {
        return mAssignments.size();
    }

    @Override
    public int getIncognitoTabCount() {
        int count = 0;
        for (int i = 0; i < mSelectors.size(); i++) {
            if (mSelectors.get(i) != null) {
                count += mSelectors.get(i).getModel(true).getCount();
            }
        }

        // Count tabs that are moving between activities (e.g. a tab that was recently reparented
        // and hasn't been attached to its new activity yet).
        SparseArray<AsyncTabParams> asyncTabParams = mAsyncTabParamsManager.getAsyncTabParams();
        for (int i = 0; i < asyncTabParams.size(); i++) {
            Tab tab = asyncTabParams.valueAt(i).getTabToReparent();
            if (tab != null && tab.isIncognito()) count++;
        }
        return count;
    }

    @Override
    public TabModel getTabModelForTab(Tab tab) {
        if (tab == null) return null;

        for (int i = 0; i < mSelectors.size(); i++) {
            TabModelSelector selector = mSelectors.get(i);
            if (selector != null) {
                TabModel tabModel = selector.getModelForTabId(tab.getId());
                if (tabModel != null) return tabModel;
            }
        }

        return null;
    }

    @Override
    public Tab getTabById(int tabId) {
        for (int i = 0; i < mSelectors.size(); i++) {
            TabModelSelector selector = mSelectors.get(i);
            if (selector != null) {
                final Tab tab = selector.getTabById(tabId);
                if (tab != null) return tab;
            }
        }

        if (mAsyncTabParamsManager.hasParamsForTabId(tabId)) {
            return mAsyncTabParamsManager.getAsyncTabParams().get(tabId).getTabToReparent();
        }

        return null;
    }

    @Override
    public TabModelSelector getTabModelSelectorById(int index) {
        return mSelectors.get(index);
    }

    // ActivityStateListener
    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.DESTROYED && mAssignments.containsKey(activity)) {
            int index = mSelectors.indexOf(mAssignments.remove(activity));
            if (index >= 0) {
                mSelectors.set(index, null);
            }
            // TODO(dtrainor): Move TabModelSelector#destroy() calls here.
        }
    }
}
