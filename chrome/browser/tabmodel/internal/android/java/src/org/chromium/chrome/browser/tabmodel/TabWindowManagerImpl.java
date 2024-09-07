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

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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

    /**
     * Debugging enums representing activity state in {@link #assertIndicesMatch}. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        PreAssignedActivityState.UNKNOWN,
        PreAssignedActivityState.NOT_IN_APP_TASK_IS_FINISHING,
        PreAssignedActivityState.NOT_IN_APP_TASK_NOT_FINISHING,
        PreAssignedActivityState.IN_APP_TASK_SAME_TASK_IS_FINISHING,
        PreAssignedActivityState.IN_APP_TASK_SAME_TASK_NOT_FINISHING,
        PreAssignedActivityState.IN_APP_TASK_DIFFERENT_TASK_IS_FINISHING,
        PreAssignedActivityState.IN_APP_TASK_DIFFERENT_TASK_NOT_FINISHING,
    })
    @interface PreAssignedActivityState {
        int UNKNOWN = 0;
        int NOT_IN_APP_TASK_IS_FINISHING = 1;
        int NOT_IN_APP_TASK_NOT_FINISHING = 2;
        int IN_APP_TASK_SAME_TASK_IS_FINISHING = 3;
        int IN_APP_TASK_SAME_TASK_NOT_FINISHING = 4;
        int IN_APP_TASK_DIFFERENT_TASK_IS_FINISHING = 5;
        int IN_APP_TASK_DIFFERENT_TASK_NOT_FINISHING = 6;
        int NUM_ENTRIES = 7;
    }

    private final List<TabModelSelector> mSelectors = new ArrayList<>();
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Map<Activity, TabModelSelector> mAssignments = new HashMap<>();

    private final TabModelSelectorFactory mSelectorFactory;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final int mMaxSelectors;

    private TabModelSelector mArchivedTabModelSelector;

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
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
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
            @NonNull MismatchedIndicesHandler mismatchedIndicesHandler,
            int index) {
        if (index < 0 || index >= mSelectors.size()) return null;

        // Return the already existing selector if found.
        if (mAssignments.get(activity) != null) {
            TabModelSelector assignedSelector = mAssignments.get(activity);
            for (int i = 0; i < mSelectors.size(); i++) {
                if (mSelectors.get(i) == assignedSelector) {
                    var assignedIndex =
                            assertIndicesMatch(
                                    index,
                                    i,
                                    "Activity already mapped; ",
                                    activity,
                                    mismatchedIndicesHandler);
                    var res = Pair.create(assignedIndex, assignedSelector);
                    Log.i(
                            TAG_MULTI_INSTANCE,
                            "Returning existing selector with index: "
                                    + res
                                    + ". Requested index: "
                                    + index);
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

        var assignedIndex =
                assertIndicesMatch(
                        originalIndex, index, "Index in use; ", activity, mismatchedIndicesHandler);
        TabModelSelector selector =
                mSelectorFactory.buildSelector(
                        activity,
                        profileProviderSupplier,
                        tabCreatorManager,
                        nextTabPolicySupplier);
        mSelectors.set(assignedIndex, selector);
        mAssignments.put(activity, selector);

        Pair res = Pair.create(assignedIndex, selector);
        Log.i(TAG_MULTI_INSTANCE, "Returning new selector for " + activity + " with index: " + res);
        for (Observer obs : mObservers) obs.onTabModelSelectorAdded(selector);
        return res;
    }

    /**
     * Check whether the requested and originally assigned index for the current
     * Activity/TabModelSelector are the same, and potentially reassign the index to match the
     * requested index.
     */
    private int assertIndicesMatch(
            int requestedIndex,
            int originallyAssignedIndex,
            String type,
            Activity newActivity,
            MismatchedIndicesHandler mismatchedIndicesHandler) {
        int assignedIndex = originallyAssignedIndex;
        if (requestedIndex == originallyAssignedIndex
                // Needed for ActivityManager.RecentTaskInfo.taskId
                || VERSION.SDK_INT < VERSION_CODES.Q) {
            return assignedIndex;
        }

        boolean assertMismatch = !BuildConfig.IS_FOR_TEST && BuildConfig.ENABLE_ASSERTS;
        boolean forceReportMismatch =
                ChromeFeatureList.sTabWindowManagerReportIndicesMismatch.isEnabled();
        if (!forceReportMismatch && !assertMismatch) {
            return assignedIndex;
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
                        + " and originally assigned "
                        + originallyAssignedIndex
                        + " new activity: "
                        + newActivity
                        + " new activity task id: "
                        + newActivity.getTaskId()
                        + " new activity is finishing? "
                        + newActivity.isFinishing()
                        + " activity at requested index: "
                        + activityAtRequestedIndex;
        if (activityAtRequestedIndex == null) {
            recordUmaForAssertIndicesMatch(PreAssignedActivityState.UNKNOWN, false);
        } else {
            int activityAtRequestedIndexTaskId = activityAtRequestedIndex.getTaskId();
            boolean isFinishing = activityAtRequestedIndex.isFinishing();

            message +=
                    " ApplicationStatus activity state: "
                            + ApplicationStatus.getStateForActivity(activityAtRequestedIndex)
                            + " activity task Id: "
                            + activityAtRequestedIndexTaskId
                            + " activity is finishing? "
                            + isFinishing
                            + " tasks: [";
            ActivityManager activityManager =
                    (ActivityManager)
                            activityAtRequestedIndex.getSystemService(Context.ACTIVITY_SERVICE);

            boolean isInAppTask = false;
            for (AppTask task : activityManager.getAppTasks()) {
                ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
                message += info + ";\n";
                if (info.taskId == activityAtRequestedIndexTaskId) {
                    isInAppTask = true;
                }
            }

            message += "]";

            boolean isSameTask = activityAtRequestedIndexTaskId == newActivity.getTaskId();
            message +=
                    " Activity at requested index is in app tasks? "
                            + isInAppTask
                            + ", is in same task? "
                            + isSameTask;

            // Try to reassign the originally assigned index to match the requested index.
            assignedIndex =
                    reassignIndex(
                            mismatchedIndicesHandler,
                            activityAtRequestedIndex,
                            requestedIndex,
                            originallyAssignedIndex,
                            isInAppTask,
                            isSameTask);
            message +=
                    " Reassigned index for new activity? "
                            + (assignedIndex != originallyAssignedIndex);

            @PreAssignedActivityState
            int state = getPreAssignedActivityState(isInAppTask, isSameTask, isFinishing);
            recordUmaForAssertIndicesMatch(state, assignedIndex != originallyAssignedIndex);

            // Start actively listen to activity status once conflict at index is found.
            ApplicationStatus.registerStateListenerForActivity(
                    getActivityStateListenerForPreAssignedActivity(state),
                    activityAtRequestedIndex);
        }

        if (!BuildConfig.IS_FOR_TEST) {
            assert requestedIndex == assignedIndex : message;
            assert assignedIndex == originallyAssignedIndex : message;
        }
        Log.i(TAG_MULTI_INSTANCE, message);
        return assignedIndex;
    }

    private @PreAssignedActivityState int getPreAssignedActivityState(
            boolean isInAppTask, boolean isSameTask, boolean isFinishing) {
        if (!isInAppTask) {
            return isFinishing
                    ? PreAssignedActivityState.NOT_IN_APP_TASK_IS_FINISHING
                    : PreAssignedActivityState.NOT_IN_APP_TASK_NOT_FINISHING;
        }
        if (isSameTask) {
            return isFinishing
                    ? PreAssignedActivityState.IN_APP_TASK_SAME_TASK_IS_FINISHING
                    : PreAssignedActivityState.IN_APP_TASK_SAME_TASK_NOT_FINISHING;
        }
        return isFinishing
                ? PreAssignedActivityState.IN_APP_TASK_DIFFERENT_TASK_IS_FINISHING
                : PreAssignedActivityState.IN_APP_TASK_DIFFERENT_TASK_NOT_FINISHING;
    }

    private void recordUmaForAssertIndicesMatch(
            @PreAssignedActivityState int state, boolean indexReassigned) {
        String histogramSuffix =
                indexReassigned
                        ? ASSERT_INDICES_MATCH_HISTOGRAM_SUFFIX_REASSIGNED
                        : ASSERT_INDICES_MATCH_HISTOGRAM_SUFFIX_NOT_REASSIGNED;
        String histogramName = ASSERT_INDICES_MATCH_HISTOGRAM_NAME + histogramSuffix;
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, state, PreAssignedActivityState.NUM_ENTRIES);
    }

    private ActivityStateListener getActivityStateListenerForPreAssignedActivity(
            @PreAssignedActivityState int state) {
        return (activityAtIndex, newState) -> {
            final int localTaskId = ApplicationStatus.getTaskId(activityAtIndex);
            Log.i(
                    TAG_MULTI_INSTANCE,
                    "ActivityAtRequestedIndex "
                            + activityAtIndex
                            + " taskId "
                            + localTaskId
                            + " newState "
                            + newState
                            + " state during indices mismatch "
                            + state);

            if (newState == ActivityState.DESTROYED) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.MultiWindowMode.AssertIndicesMatch.PreExistingActivityDestroyed",
                        state,
                        PreAssignedActivityState.NUM_ENTRIES);
            }
        };
    }

    private int reassignIndex(
            MismatchedIndicesHandler mismatchedIndicesHandler,
            Activity activityAtRequestedIndex,
            int requestedIndex,
            int originallyAssignedIndexForNewActivity,
            boolean isActivityAtRequestedIndexInAppTasks,
            boolean isActivityAtRequestedIndexInSameTask) {
        boolean handled =
                mismatchedIndicesHandler.handleMismatchedIndices(
                        activityAtRequestedIndex,
                        isActivityAtRequestedIndexInAppTasks,
                        isActivityAtRequestedIndexInSameTask);
        if (!handled) return originallyAssignedIndexForNewActivity;
        int releasedIndex = clearSelectorAndIndexAssignments(activityAtRequestedIndex);
        if (releasedIndex == INVALID_WINDOW_INDEX) {
            // If the index mapping is already cleared for |activityAtRequestedIndex| by this time,
            // simply return the requested index.
            return requestedIndex;
        }
        assert releasedIndex == requestedIndex
                : "Released activity index should match the requested index.";
        return requestedIndex;
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

        if (mArchivedTabModelSelector != null) {
            final Tab tab = mArchivedTabModelSelector.getTabById(tabId);
            if (tab != null) return tab;
        }

        return null;
    }

    @Override
    public TabModelSelector getTabModelSelectorById(int index) {
        return mSelectors.get(index);
    }

    @Override
    public void setArchivedTabModelSelector(TabModelSelector archivedTabModelSelector) {
        mArchivedTabModelSelector = archivedTabModelSelector;
    }

    @Override
    public boolean canTabStateBeDeleted(int tabId) {
        boolean isPossiblyAnArchivedTab = isPossiblyAnArchivedTab();
        RecordHistogram.recordBooleanHistogram(
                "Tabs.TabStateCleanupAbortedByArchive", isPossiblyAnArchivedTab);
        return !isPossiblyAnArchivedTab && getTabById(tabId) == null;
    }

    @Override
    public boolean canTabThumbnailBeDeleted(int tabId) {
        boolean isPossiblyAnArchivedTab = isPossiblyAnArchivedTab();
        RecordHistogram.recordBooleanHistogram(
                "Tabs.TabThumbnailCleanupAbortedByArchive", isPossiblyAnArchivedTab);
        return !isPossiblyAnArchivedTab && getTabById(tabId) == null;
    }

    // ActivityStateListener
    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.DESTROYED) {
            clearSelectorAndIndexAssignments(activity);
            // TODO(dtrainor): Move TabModelSelector#destroy() calls here.
        }
    }

    private int clearSelectorAndIndexAssignments(Activity activity) {
        if (!mAssignments.containsKey(activity)) return INVALID_WINDOW_INDEX;
        int index = mSelectors.indexOf(mAssignments.remove(activity));
        if (index >= 0) {
            mSelectors.set(index, null);
        }
        return index;
    }

    private boolean isPossiblyAnArchivedTab() {
        return ChromeFeatureList.sAndroidTabDeclutterRescueKillSwitch.isEnabled()
                && (mArchivedTabModelSelector == null
                        || !mArchivedTabModelSelector.isTabStateInitialized());
    }
}
