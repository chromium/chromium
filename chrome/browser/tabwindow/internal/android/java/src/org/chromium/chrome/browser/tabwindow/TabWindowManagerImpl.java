// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.util.Pair;
import android.util.SparseArray;

import androidx.annotation.IntDef;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.TimeUtils;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncUtils;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupVisualDataStore;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Manages multiple {@link TabModelSelector} instances, each owned by different {@link Activity}s.
 *
 * <p>Also manages tabs being reparented in AsyncTabParamsManager.
 */
@NullMarked
public class TabWindowManagerImpl implements TabWindowManager {

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

    private final Map<@WindowId Integer, TabModelSelector> mWindowIdToSelectors = new HashMap<>();
    private final Map<TabModelSelector, @WindowId Integer> mSelectorsToWindowId = new HashMap<>();
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    // Selectors exclusively exist in one of the two following maps.
    private final Map<TabModelSelector, Destroyable> mHeadlessAssignments = new HashMap<>();
    private final Map<Activity, TabModelSelector> mActivityAssignments = new HashMap<>();

    private final ActivityStateListener mActivityStateListener = this::onActivityStateChange;
    private final TabModelSelectorFactory mSelectorFactory;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final int mMaxSelectors;

    private @Nullable TabModelSelector mArchivedTabModelSelector;
    private boolean mKeepAllTabModelsLoaded;
    private boolean mTabStateInitialized;

    TabWindowManagerImpl(
            TabModelSelectorFactory selectorFactory,
            AsyncTabParamsManager asyncTabParamsManager,
            int maxSelectors) {
        mSelectorFactory = selectorFactory;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mMaxSelectors = maxSelectors;
        ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
        if (mTabStateInitialized) observer.onTabStateInitialized();
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public @Nullable Pair<@WindowId Integer, TabModelSelector> requestSelector(
            Activity activity,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            MultiInstanceManager multiInstanceManager,
            MismatchedIndicesHandler mismatchedIndicesHandler,
            @WindowId int windowId) {
        if (windowId == INVALID_WINDOW_ID) return null;

        // Return the already existing selector if found.
        if (mActivityAssignments.get(activity) != null) {
            TabModelSelector assignedSelector = mActivityAssignments.get(activity);
            for (Integer i : mSelectorsToWindowId.values()) {
                if (mWindowIdToSelectors.get(i) == assignedSelector) {
                    @WindowId
                    int existingWindowId =
                            assertIndicesMatch(
                                    windowId,
                                    i,
                                    "Activity already mapped; ",
                                    activity,
                                    mismatchedIndicesHandler);
                    var res = Pair.create(existingWindowId, assignedSelector);
                    Log.i(
                            TAG_MULTI_INSTANCE,
                            "Returning existing selector with window id: "
                                    + res
                                    + ". Requested window id: "
                                    + windowId);
                    return res;
                }
            }
            // The following log statement is used in tools/android/build_speed/benchmark.py. Please
            // update the string there if this string is updated.
            throw new IllegalStateException(
                    "TabModelSelector is assigned to an Activity but has no mapped window id.");
        }

        @WindowId int originalWindowId = windowId;
        if (mWindowIdToSelectors.get(windowId) != null) {
            if (shutdownIfHeadless(windowId)) {
                // Can safely use requested window id now.
            } else {
                // Find the next valid/empty window id.
                for (int i = 0; i < mMaxSelectors; i++) {
                    if (mWindowIdToSelectors.get(i) == null) {
                        windowId = i;
                        break;
                    }
                }
            }
        }

        // Too many activities going at once.
        if (mWindowIdToSelectors.get(windowId) != null) return null;

        @WindowId
        int assignedWindowId =
                assertIndicesMatch(
                        originalWindowId,
                        windowId,
                        "Window id in use; ",
                        activity,
                        mismatchedIndicesHandler);
        TabModelSelector selector =
                mSelectorFactory.buildTabbedSelector(
                        activity,
                        modalDialogManager,
                        profileProviderSupplier,
                        tabCreatorManager,
                        nextTabPolicySupplier,
                        multiInstanceManager);
        mWindowIdToSelectors.put(assignedWindowId, selector);
        mSelectorsToWindowId.put(selector, assignedWindowId);
        mActivityAssignments.put(activity, selector);

        Pair res = Pair.create(assignedWindowId, selector);
        Log.i(
                TAG_MULTI_INSTANCE,
                "Returning new selector for " + activity + " with window id: " + res);
        for (Observer obs : mObservers) obs.onTabModelSelectorAdded(selector);
        return res;
    }

    @Override
    public @Nullable TabModelSelector requestSelectorWithoutActivity(
            @WindowId int windowId, Profile profile) {
        if (windowId == INVALID_WINDOW_ID) return null;

        if (mWindowIdToSelectors.containsKey(windowId)) {
            return mWindowIdToSelectors.get(windowId);
        }

        Pair<TabModelSelector, Destroyable> pair =
                mSelectorFactory.buildHeadlessSelector(windowId, profile);
        TabModelSelector selector = pair.first;
        mHeadlessAssignments.put(selector, pair.second);
        mSelectorsToWindowId.put(selector, windowId);
        mWindowIdToSelectors.put(windowId, selector);

        for (Observer obs : mObservers) obs.onTabModelSelectorAdded(selector);
        return selector;
    }

    @Override
    public boolean shutdownIfHeadless(@WindowId int windowId) {
        if (!mWindowIdToSelectors.containsKey(windowId)) return false;
        TabModelSelector selector = mWindowIdToSelectors.get(windowId);

        if (!mHeadlessAssignments.containsKey(selector)) return false;

        Destroyable shutdown = mHeadlessAssignments.remove(selector);
        assumeNonNull(shutdown).destroy();
        mWindowIdToSelectors.remove(windowId);
        mSelectorsToWindowId.remove(selector);
        return true;
    }

    /**
     * Check whether the requested and originally assigned window id for the current
     * Activity/TabModelSelector are the same, and potentially reassign the window id to match the
     * requested window id.
     */
    private @WindowId int assertIndicesMatch(
            @WindowId int requestedWindowId,
            @WindowId int originallyAssignedWindowId,
            String type,
            Activity newActivity,
            MismatchedIndicesHandler mismatchedIndicesHandler) {
        @WindowId int assignedWindowId = originallyAssignedWindowId;
        if (requestedWindowId == originallyAssignedWindowId) {
            return assignedWindowId;
        }

        if (mismatchedIndicesHandler.skipIndexReassignment()) {
            return assignedWindowId;
        }

        boolean assertMismatch = !BuildConfig.IS_FOR_TEST && BuildConfig.ENABLE_ASSERTS;
        boolean forceReportMismatch =
                ChromeFeatureList.sTabWindowManagerReportIndicesMismatch.isEnabled();
        if (!forceReportMismatch && !assertMismatch) {
            return assignedWindowId;
        }

        TabModelSelector selectorAtRequestedWindowId = mWindowIdToSelectors.get(requestedWindowId);
        Activity activityAtRequestedWindowId = null;
        for (Activity mappedActivity : mActivityAssignments.keySet()) {
            if (mActivityAssignments.get(mappedActivity).equals(selectorAtRequestedWindowId)) {
                activityAtRequestedWindowId = mappedActivity;
                break;
            }
        }

        String message =
                type
                        + "Requested "
                        + requestedWindowId
                        + " and originally assigned "
                        + originallyAssignedWindowId
                        + " new activity: "
                        + newActivity
                        + " new activity task id: "
                        + newActivity.getTaskId()
                        + " new activity is finishing? "
                        + newActivity.isFinishing()
                        + " activity at requested window id: "
                        + activityAtRequestedWindowId;
        if (activityAtRequestedWindowId == null) {
            recordUmaForAssertIndicesMatch(PreAssignedActivityState.UNKNOWN, false);
        } else {
            int activityAtRequestedWindowIdTaskId = activityAtRequestedWindowId.getTaskId();
            boolean isFinishing = activityAtRequestedWindowId.isFinishing();

            message +=
                    " ApplicationStatus activity state: "
                            + ApplicationStatus.getStateForActivity(activityAtRequestedWindowId)
                            + " activity task Id: "
                            + activityAtRequestedWindowIdTaskId
                            + " activity is finishing? "
                            + isFinishing
                            + " tasks: [";
            ActivityManager activityManager =
                    (ActivityManager)
                            activityAtRequestedWindowId.getSystemService(Context.ACTIVITY_SERVICE);

            boolean isInAppTask = false;
            for (AppTask task : activityManager.getAppTasks()) {
                ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
                if (info == null) {
                    message += "null info;";
                    continue;
                }
                message += info + ";\n";
                if (info.taskId == activityAtRequestedWindowIdTaskId) {
                    isInAppTask = true;
                }
            }

            message += "]";

            boolean isSameTask = activityAtRequestedWindowIdTaskId == newActivity.getTaskId();
            message +=
                    " Activity at requested window id is in app tasks? "
                            + isInAppTask
                            + ", is in same task? "
                            + isSameTask;

            // Try to reassign the originally assigned window id to match the requested window id.
            assignedWindowId =
                    reassignWindowId(
                            mismatchedIndicesHandler,
                            activityAtRequestedWindowId,
                            requestedWindowId,
                            originallyAssignedWindowId,
                            isInAppTask,
                            isSameTask);
            message +=
                    " Reassigned window id for new activity? "
                            + (assignedWindowId != originallyAssignedWindowId);

            @PreAssignedActivityState
            int state = getPreAssignedActivityState(isInAppTask, isSameTask, isFinishing);
            recordUmaForAssertIndicesMatch(state, assignedWindowId != originallyAssignedWindowId);

            // Start actively listen to activity status once conflict at window id is found.
            ApplicationStatus.registerStateListenerForActivity(
                    getActivityStateListenerForPreAssignedActivity(state),
                    activityAtRequestedWindowId);
        }

        assert BuildConfig.IS_FOR_TEST || requestedWindowId == assignedWindowId : message;
        Log.i(TAG_MULTI_INSTANCE, message);
        return assignedWindowId;
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
            @PreAssignedActivityState int state, boolean windowIdReassigned) {
        String histogramSuffix =
                windowIdReassigned
                        ? ASSERT_INDICES_MATCH_HISTOGRAM_SUFFIX_REASSIGNED
                        : ASSERT_INDICES_MATCH_HISTOGRAM_SUFFIX_NOT_REASSIGNED;
        String histogramName = ASSERT_INDICES_MATCH_HISTOGRAM_NAME + histogramSuffix;
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, state, PreAssignedActivityState.NUM_ENTRIES);
    }

    private ActivityStateListener getActivityStateListenerForPreAssignedActivity(
            @PreAssignedActivityState int state) {
        long mismatchReportTime = TimeUtils.elapsedRealtimeMillis();
        return (activityAtWindowId, newState) -> {
            final int localTaskId = ApplicationStatus.getTaskId(activityAtWindowId);
            Log.i(
                    TAG_MULTI_INSTANCE,
                    "ActivityAtRequestedWindowId "
                            + activityAtWindowId
                            + " taskId "
                            + localTaskId
                            + " newState "
                            + newState
                            + " state during indices mismatch "
                            + state);

            if (newState == ActivityState.DESTROYED) {
                long timeToDestruction = TimeUtils.elapsedRealtimeMillis() - mismatchReportTime;
                RecordHistogram.recordTimesHistogram(
                        "Android.MultiWindowMode.MismatchedIndices.TimeToPreExistingActivityDestruction",
                        timeToDestruction);
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.MultiWindowMode.AssertIndicesMatch.PreExistingActivityDestroyed",
                        state,
                        PreAssignedActivityState.NUM_ENTRIES);
            }
        };
    }

    private @WindowId int reassignWindowId(
            MismatchedIndicesHandler mismatchedIndicesHandler,
            Activity activityAtRequestedWindowId,
            @WindowId int requestedWindowId,
            @WindowId int originallyAssignedWindowIdForNewActivity,
            boolean isActivityAtRequestedWindowIdInAppTasks,
            boolean isActivityAtRequestedWindowIdInSameTask) {
        boolean handled =
                mismatchedIndicesHandler.handleMismatchedIndices(
                        activityAtRequestedWindowId,
                        isActivityAtRequestedWindowIdInAppTasks,
                        isActivityAtRequestedWindowIdInSameTask);
        if (!handled) return originallyAssignedWindowIdForNewActivity;
        @WindowId
        int releasedWindowId = clearSelectorAndWindowIdAssignments(activityAtRequestedWindowId);
        if (releasedWindowId == INVALID_WINDOW_ID) {
            // If the window id mapping is already cleared for |activityAtRequestedWindowId| by this
            // time, simply return the requested window id.
            return requestedWindowId;
        }
        assert releasedWindowId == requestedWindowId
                : "Released activity window id should match the requested window id.";
        return requestedWindowId;
    }

    @Override
    public int getIdForWindow(Activity activity) {
        TabModelSelector selector = mActivityAssignments.get(activity);
        return getWindowIdForSelectorChecked(selector);
    }

    @Override
    public int getNumberOfAssignedTabModelSelectors() {
        return mActivityAssignments.size();
    }

    @Override
    public int getIncognitoTabCount() {
        int count = 0;
        for (TabModelSelector selector : getAllTabModelSelectors()) {
            if (selector != null) {
                count += selector.getModel(true).getCount();
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
    public @Nullable TabModel getTabModelForTab(Tab tab) {
        for (TabModelSelector selector : getAllTabModelSelectors()) {
            if (selector != null) {
                TabModel tabModel = selector.getModelForTabId(tab.getId());
                if (tabModel != null) return tabModel;
            }
        }

        return null;
    }

    @Override
    public @Nullable Tab getTabById(@TabId int tabId) {
        for (TabModelSelector selector : getAllTabModelSelectors()) {
            @Nullable final Tab tab = getTabFromTabModelSelector(selector, tabId);
            if (tab != null) return tab;
        }

        return getTabFromOtherSource(tabId);
    }

    @Override
    public @Nullable Tab getTabById(@TabId int tabId, @WindowId int windowId) {
        @Nullable TabModelSelector selector = getTabModelSelectorById(windowId);
        @Nullable final Tab tab = getTabFromTabModelSelector(selector, tabId);
        if (tab != null) return tab;

        return getTabFromOtherSource(tabId);
    }

    @Override
    public @Nullable TabWindowInfo getTabWindowInfoById(@TabId int tabId) {
        for (Map.Entry<@WindowId Integer, TabModelSelector> entry :
                mWindowIdToSelectors.entrySet()) {
            TabModelSelector selector = entry.getValue();
            for (TabModel tabModel : selector.getModels()) {
                @Nullable final Tab tab = tabModel.getTabById(tabId);
                if (tab != null) {
                    return new TabWindowInfo(entry.getKey(), selector, tabModel, tab);
                }
            }
        }
        return null;
    }

    @Override
    public @Nullable List<Tab> getGroupedTabsByWindow(
            @WindowId int windowId, Token tabGroupId, boolean isIncognito) {
        @Nullable TabModelSelector tabModelSelector = getTabModelSelectorById(windowId);
        if (tabModelSelector == null) return null;

        @Nullable TabGroupModelFilter tabGroupModelFilter =
                tabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(isIncognito);
        if (tabGroupModelFilter == null) return null;

        return tabGroupModelFilter.getTabsInGroup(tabGroupId);
    }

    @Override
    public @Nullable TabModelSelector getTabModelSelectorById(@WindowId int windowId) {
        return mWindowIdToSelectors.get(windowId);
    }

    @Override
    public @WindowId int getWindowIdForSelector(TabModelSelector selector) {
        @WindowId Integer windowId = mSelectorsToWindowId.get(selector);
        if (windowId == null) return INVALID_WINDOW_ID;
        return windowId;
    }

    @Override
    public Collection<TabModelSelector> getAllTabModelSelectors() {
        return mWindowIdToSelectors.values();
    }

    @Override
    public void setArchivedTabModelSelector(@Nullable TabModelSelector archivedTabModelSelector) {
        mArchivedTabModelSelector = archivedTabModelSelector;
    }

    @Override
    public boolean canTabStateBeDeleted(@TabId int tabId) {
        boolean isPossiblyAnArchivedTab = isPossiblyAnArchivedTab();
        RecordHistogram.recordBooleanHistogram(
                "Tabs.TabStateCleanupAbortedByArchive", isPossiblyAnArchivedTab);
        return !isPossiblyAnArchivedTab && getTabById(tabId) == null;
    }

    @Override
    public boolean canTabThumbnailBeDeleted(@TabId int tabId) {
        boolean isPossiblyAnArchivedTab = isPossiblyAnArchivedTab();
        RecordHistogram.recordBooleanHistogram(
                "Tabs.TabThumbnailCleanupAbortedByArchive", isPossiblyAnArchivedTab);
        return !isPossiblyAnArchivedTab && getTabById(tabId) == null;
    }

    @Override
    public void keepAllTabModelsLoaded(
            MultiInstanceManager multiInstanceManager, Profile profile, TabModelSelector selector) {
        if (mKeepAllTabModelsLoaded) return;

        mKeepAllTabModelsLoaded = true;

        List<TabModelSelector> tabModelSelectorList = new ArrayList<>();
        List<InstanceInfo> instanceInfoList = multiInstanceManager.getInstanceInfo();
        if (instanceInfoList.isEmpty()) {
            tabModelSelectorList.add(selector);
        } else {
            for (InstanceInfo instanceInfo : instanceInfoList) {
                @WindowId int windowId = instanceInfo.instanceId;
                if (!mWindowIdToSelectors.containsKey(windowId)) {
                    tabModelSelectorList.add(requestSelectorWithoutActivity(windowId, profile));
                } else {
                    tabModelSelectorList.add(mWindowIdToSelectors.get(windowId));
                }
            }
        }
        TabModelUtils.runOnTabStateInitialized(
                () -> {
                    mTabStateInitialized = true;
                    for (Observer observer : mObservers) {
                        observer.onTabStateInitialized();
                    }

                    TabModel model = tabModelSelectorList.get(0).getModel(/* incognito= */ false);

                    // TODO(https://crbug.com/420738506): Remove this post once the order is
                    // flipped.
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                model.broadcastSessionRestoreComplete();
                                unmapOrphanedTabGroups(profile, tabModelSelectorList);
                                deleteOrphanedTabGroupData(tabModelSelectorList);
                            });
                },
                tabModelSelectorList.toArray(new TabModelSelector[0]));
    }

    private void unmapOrphanedTabGroups(
            Profile profile, List<TabModelSelector> tabModelSelectorList) {
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        if (tabGroupSyncService == null) return;

        List<TabGroupModelFilter> filterList = new ArrayList<>();
        for (TabModelSelector selector : tabModelSelectorList) {
            // This process is async and it's possible something was shut down during
            // the wait for all these tab models to init. In that case, just bail and
            // try again next restart. This clean up is optional.
            if (!mSelectorsToWindowId.containsKey(selector)) {
                return;
            }

            filterList.add(
                    selector.getTabGroupModelFilterProvider()
                            .getTabGroupModelFilter(/* isIncognito= */ false));
        }
        TabGroupSyncUtils.unmapLocalIdsNotInTabGroupModelFilterList(
                tabGroupSyncService, filterList);
    }

    private void deleteOrphanedTabGroupData(List<TabModelSelector> tabModelSelectors) {
        if (!ChromeFeatureList.sTabGroupAndroidVisualDataCleanup.isEnabled()) return;

        Set<String> tabGroupIdTokenStrings = new HashSet<>();
        for (TabModelSelector selector : tabModelSelectors) {
            var filterProvider = selector.getTabGroupModelFilterProvider();
            for (boolean isIncognito : List.of(false, true)) {
                TabGroupModelFilter filter = filterProvider.getTabGroupModelFilter(isIncognito);
                assumeNonNull(filter);
                for (Token tabGroupId : filter.getAllTabGroupIds()) {
                    tabGroupIdTokenStrings.add(tabGroupId.toString());
                }
            }
        }
        TabGroupVisualDataStore.deleteTabGroupDataExcluding(tabGroupIdTokenStrings);
    }

    @Override
    public @WindowId int findWindowIdForTabGroup(Token tabGroupId) {
        for (Map.Entry<TabModelSelector, @WindowId Integer> entry :
                mSelectorsToWindowId.entrySet()) {
            TabModelSelector selector = entry.getKey();
            if (!selector.isTabStateInitialized()) continue;

            TabGroupModelFilter filter =
                    selector.getTabGroupModelFilterProvider()
                            .getTabGroupModelFilter(/* isIncognito= */ false);
            if (filter == null) continue;

            if (TabGroupSyncUtils.isInCurrentWindow(filter, new LocalTabGroupId(tabGroupId))) {
                return entry.getValue();
            }
        }

        return INVALID_WINDOW_ID;
    }

    private void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.DESTROYED) {
            clearSelectorAndWindowIdAssignments(activity);
            // TODO(dtrainor): Move TabModelSelector#destroy() calls here.
        }
    }

    private @WindowId int clearSelectorAndWindowIdAssignments(Activity activity) {
        if (!mActivityAssignments.containsKey(activity)) return INVALID_WINDOW_ID;
        TabModelSelector selector = mActivityAssignments.remove(activity);
        @WindowId int windowId = getWindowIdForSelectorChecked(selector);
        if (windowId >= 0) {
            mWindowIdToSelectors.remove(windowId);
            mSelectorsToWindowId.remove(selector);
            if (mKeepAllTabModelsLoaded) {
                Profile profile = findActiveProfile();
                if (profile != null) {
                    requestSelectorWithoutActivity(windowId, profile);
                }
            }
        }
        return windowId;
    }

    private @Nullable Profile findActiveProfile() {
        for (TabModelSelector selector : mActivityAssignments.values()) {
            Profile profile = selector.getModel(/* incognito= */ false).getProfile();
            if (profile != null && !profile.isOffTheRecord()) {
                return profile;
            }
        }
        return null;
    }

    private boolean isPossiblyAnArchivedTab() {
        return ChromeFeatureList.sAndroidTabDeclutterRescueKillSwitch.isEnabled()
                && (mArchivedTabModelSelector == null
                        || !mArchivedTabModelSelector.isTabStateInitialized());
    }

    private @Nullable Tab getTabFromTabModelSelector(
            @Nullable TabModelSelector selector, @TabId int tabId) {
        if (selector == null) return null;
        return selector.getTabById(tabId);
    }

    private @Nullable Tab getTabFromOtherSource(@TabId int tabId) {
        AsyncTabParams asyncTabParams = mAsyncTabParamsManager.getAsyncTabParams().get(tabId);
        if (asyncTabParams != null) {
            return asyncTabParams.getTabToReparent();
        }

        if (mArchivedTabModelSelector != null) {
            return mArchivedTabModelSelector.getTabById(tabId);
        }

        return null;
    }

    private @WindowId int getWindowIdForSelectorChecked(@Nullable TabModelSelector selector) {
        if (selector == null) return TabWindowManager.INVALID_WINDOW_ID;
        @WindowId Integer windowId = mSelectorsToWindowId.get(selector);
        return windowId == null || windowId == -1 ? TabWindowManager.INVALID_WINDOW_ID : windowId;
    }
}
