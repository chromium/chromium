// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.SparseBooleanArray;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.RecentlyClosedEntriesManagerTrackerFactory;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceState.MultiInstanceStateObserver;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

@NullMarked
class MultiInstanceManagerApi31 extends MultiInstanceManagerImpl
        implements ActivityStateListener, InstanceSwitcherActionsDelegate {
    private static final String TAG = "MIMApi31";
    private static final String TAG_MULTI_INSTANCE = "MultiInstance";
    /* package */ static final long SIX_MONTHS_MS = TimeUnit.DAYS.toMillis(6 * 30);
    private static final String EMPTY_DATA = "";
    private static @Nullable MultiInstanceState sState;
    private static final Object sAllocIdLock = new Object();

    @VisibleForTesting protected final int mMaxInstances;

    // Use a static sequenced task runner shared across all instances to ensure metrics tasks
    // execute serially, preventing concurrent read-modify-write races on the global daily max
    // counters in SharedPreferences even when multiple windows trigger state changes.
    private static final TaskRunner sMetricsTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT);

    private final MonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    // Instance ID for the activity associated with this manager.
    private int mInstanceId = INVALID_WINDOW_ID;

    private @Nullable Tab mActiveTab;
    private final TabObserver mActiveTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onTitleUpdated(Tab tab) {
                    if (!tab.isIncognito()) {
                        ChromeMultiInstancePersistentStore.writeActiveTabTitle(
                                mInstanceId, tab.getTitle());
                    }
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    if (!tab.isIncognito()) {
                        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                                mInstanceId, tab.getOriginalUrl().getSpec());
                    }
                }
            };

    private final Supplier<DesktopWindowStateManager> mDesktopWindowStateManagerSupplier;
    private final MultiInstanceStateObserver mOnMultiInstanceStateChanged;

    private boolean mIsCreationLimitMessageEnqueued;

    MultiInstanceManagerApi31(
            Activity activity,
            MonotonicObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            Supplier<DesktopWindowStateManager> desktopWindowStateManagerSupplier) {
        super(
                activity,
                tabModelOrchestratorSupplier,
                multiWindowModeStateDispatcher,
                activityLifecycleDispatcher,
                menuOrKeyboardActionController);
        mMaxInstances = MultiWindowUtils.getMaxInstances();
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mDesktopWindowStateManagerSupplier = desktopWindowStateManagerSupplier;
        mOnMultiInstanceStateChanged = this::onMultiInstanceStateChanged;

        // Check if instance limit has changed and update SharedPrefs.
        int maxInstances = getMaxInstances();
        int prevInstanceLimit =
                ChromeMultiInstancePersistentStore.readMaxInstanceLimit(maxInstances);
        if (maxInstances > prevInstanceLimit) {
            // Reset SharedPrefs for instance limit downgrade if limit has increased.
            ChromeMultiInstancePersistentStore.writeInstanceLimitDowngradeTriggered(false);
        }
        ChromeMultiInstancePersistentStore.writeMaxInstanceLimit(maxInstances);
    }

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.manage_all_windows_menu_id) {
            showInstanceSwitcherDialog();

            if (AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManagerSupplier.get())) {
                RecordUserAction.record("MobileMenuWindowManager.InDesktopWindow");
            } else {
                RecordUserAction.record("MobileMenuWindowManager");
            }

            AppHeaderUtils.recordDesktopWindowModeStateEnumHistogram(
                    mDesktopWindowStateManagerSupplier.get(),
                    "Android.MultiInstance.WindowManager.DesktopWindowModeState");

            Tracker tracker = TrackerFactory.getTrackerForProfile(getProfile());
            assert tracker.isInitialized();
            tracker.notifyEvent(EventConstants.INSTANCE_SWITCHER_IPH_USED);
            return true;
        } else if (id == R.id.new_incognito_window_menu_id) {
            int appSource =
                    fromMenu ? NewWindowAppSource.MENU : NewWindowAppSource.KEYBOARD_SHORTCUT;
            TabModelOrchestrator tabModelOrchestrator = mTabModelOrchestratorSupplier.get();
            if (tabModelOrchestrator == null) return true;
            TabModelSelector tabModelSelector = tabModelOrchestrator.getTabModelSelector();
            if (tabModelSelector == null) return true;
            Profile profile = tabModelSelector.getCurrentModel().getProfile();
            if (profile != null && IncognitoUtils.isIncognitoModeEnabled(profile)) {
                mMultiInstanceOrchestrator.createNewWindow(
                        mActivity,
                        /* isIncognito= */ true,
                        /* additionalIntentExtras= */ null,
                        /* startActivityOptions= */ null,
                        appSource);
            }
            return true;
        }
        return super.handleMenuOrKeyboardAction(id, fromMenu);
    }

    private void showInstanceSwitcherDialog() {
        List<InstanceInfo> info = getInstanceInfo(PersistedInstanceType.ANY);
        boolean isIncognitoWindow =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        && mActivity instanceof ChromeTabbedActivity
                        && ((ChromeTabbedActivity) mActivity).isIncognitoWindow();
        InstanceSwitcherCoordinator.showDialog(
                mActivity,
                assertNonNull(mModalDialogManagerSupplier.get()),
                new LargeIconBridge(getProfile()),
                this,
                MultiWindowUtils.getMaxInstances(),
                info,
                isIncognitoWindow);
    }

    // InstanceSwitcherActionsDelegate implementation.

    @Override
    public void openInstance(int instanceId) {
        RecordUserAction.record("Android.WindowManager.SelectWindow");
        openWindow(instanceId, NewWindowAppSource.WINDOW_MANAGER);
    }

    @Override
    public void closeInstances(List<Integer> instanceIds) {
        RecordUserAction.record("MobileMenuWindowManagerCloseInstance");
        closeWindows(instanceIds, CloseWindowAppSource.WINDOW_MANAGER);
    }

    @Override
    public void renameInstance(int instanceId, String newName) {
        ChromeMultiInstancePersistentStore.writeCustomTitle(instanceId, newName);
    }

    @Override
    public void openNewWindow(boolean isIncognito) {
        RecordUserAction.record("Android.WindowManager.NewWindow");
        mMultiInstanceOrchestrator.createNewWindow(
                mActivity,
                isIncognito,
                /* additionalIntentExtras= */ null,
                /* startActivityOptions= */ null,
                NewWindowAppSource.WINDOW_MANAGER);
    }

    /* package */ void showTargetSelectorDialog(
            Callback<InstanceInfo> moveCallback,
            @PersistedInstanceType int instanceType,
            @StringRes int titleId) {
        TargetSelectorCoordinator.showDialog(
                mActivity,
                assertNonNull(mModalDialogManagerSupplier.get()),
                new LargeIconBridge(getProfile()),
                moveCallback,
                getInstanceInfo(instanceType),
                titleId);
    }

    @Override
    public List<InstanceInfo> getInstanceInfo(@PersistedInstanceType int persistedInstanceType) {
        return getInstanceInfo(persistedInstanceType, /* includeDeleted= */ false);
    }

    @Override
    public List<InstanceInfo> getRecentlyClosedInstances() {
        var instanceType = PersistedInstanceType.INACTIVE;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            instanceType |= PersistedInstanceType.REGULAR;
        }
        return getInstanceInfo(instanceType, /* includeDeleted= */ true);
    }

    private List<InstanceInfo> getInstanceInfo(
            @PersistedInstanceType int persistedInstanceType, boolean includeDeleted) {
        removeInvalidInstanceData();
        List<InstanceInfo> result = new ArrayList<>();
        SparseBooleanArray visibleTasks = MultiWindowUtils.getVisibleTasks();
        for (int i : MultiWindowUtils.getPersistedInstanceIds(persistedInstanceType)) {
            if (!includeDeleted && ChromeMultiInstancePersistentStore.readMarkedForDeletion(i)) {
                continue;
            }
            @InstanceInfo.Type int type = InstanceInfo.Type.OTHER;
            Activity a = MultiWindowUtils.getActivityById(i);
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(i);
            if (a != null && !a.isFinishing()) {
                // The task for the activity must match the persisted task.
                int activityTaskId = a.getTaskId();
                String error =
                        "Invalid instance-task mapping for activity="
                                + a
                                + " with id="
                                + i
                                + ". Expected (stored) taskId="
                                + persistedTaskId
                                + ", activity's taskId="
                                + activityTaskId;
                assert persistedTaskId == activityTaskId : error;
                if (a == mActivity) {
                    type = InstanceInfo.Type.CURRENT;
                } else if (isRunningInAdjacentWindow(visibleTasks, a)) {
                    type = InstanceInfo.Type.ADJACENT;
                }
            }

            long lastAccessedTime = ChromeMultiInstancePersistentStore.readLastAccessedTime(i);
            result.add(
                    new InstanceInfo(
                            i,
                            persistedTaskId,
                            type,
                            assumeNonNull(ChromeMultiInstancePersistentStore.readActiveTabUrl(i)),
                            assumeNonNull(ChromeMultiInstancePersistentStore.readActiveTabTitle(i)),
                            ChromeMultiInstancePersistentStore.readCustomTitle(i),
                            ChromeMultiInstancePersistentStore.readNormalTabCount(i),
                            ChromeMultiInstancePersistentStore.readIncognitoTabCount(i),
                            ChromeMultiInstancePersistentStore.readIncognitoSelected(i),
                            lastAccessedTime,
                            ChromeMultiInstancePersistentStore.readClosureTime(i)));
        }
        return result;
    }

    private boolean isOlderThanSixMonths(long timestampMillis) {
        return (TimeUtils.currentTimeMillis() - timestampMillis) > SIX_MONTHS_MS;
    }

    @Override
    public int getCurrentInstanceId() {
        return mInstanceId;
    }

    @VisibleForTesting
    protected boolean isRunningInAdjacentWindow(
            SparseBooleanArray visibleTasks, Activity activity) {
        assert activity != mActivity;
        return visibleTasks.get(activity.getTaskId());
    }

    @Override
    public AllocatedIdInfo allocInstanceId(
            int windowId, int taskId, boolean preferNew, boolean isIncognitoIntent) {
        synchronized (sAllocIdLock) {
            return allocInstanceIdInternal(windowId, taskId, preferNew, isIncognitoIntent);
        }
    }

    private AllocatedIdInfo allocInstanceIdInternal(
            int preferredInstanceId, int taskId, boolean preferNew, boolean isIncognitoIntent) {
        removeInvalidInstanceData();
        // Finish excess running activities / tasks after an instance limit downgrade.
        finishExcessRunningActivities();

        int instanceIdForTask = getInstanceByTask(taskId);
        @SupportedProfileType int profileType;

        // Explicitly specified window ID should be preferred. This comes from user selecting
        // a certain instance on UI when no task is present for it.
        // When out of range, ignore the ID and apply the normal allocation logic below.
        if (preferredInstanceId >= 0 && instanceIdForTask == INVALID_WINDOW_ID) {
            // If we are at instance limit, immediately block allocation of a valid id for the
            // current activity so that it subsequently finishes. This is useful when multiple
            // windows race to be restored near the limit (e.g. as a result of keyboard presses in
            // quick succession).
            if (!MultiWindowUtils.isWithinInstanceLimit()) {
                profileType = getProfileType(instanceIdForTask, isIncognitoIntent);
                return new AllocatedIdInfo(
                        instanceIdForTask, InstanceAllocationType.INVALID_INSTANCE, profileType);
            }

            Log.i(
                    TAG_MULTI_INSTANCE,
                    "Existing Instance - selected Id allocated: " + preferredInstanceId);
            profileType = getProfileType(preferredInstanceId, isIncognitoIntent);
            return new AllocatedIdInfo(
                    preferredInstanceId,
                    InstanceAllocationType.EXISTING_INSTANCE_UNMAPPED_TASK,
                    profileType);
        }

        // First, see if we have instance-task ID mapping. If we do, use the instance id. This
        // takes care of a task that had its activity destroyed and comes back to create a
        // new one. We pair them again.
        if (instanceIdForTask != INVALID_WINDOW_ID) {
            Log.i(
                    TAG_MULTI_INSTANCE,
                    "Existing Instance - mapped Id allocated: " + instanceIdForTask);
            profileType = getProfileType(instanceIdForTask, isIncognitoIntent);
            return new AllocatedIdInfo(
                    instanceIdForTask,
                    InstanceAllocationType.EXISTING_INSTANCE_MAPPED_TASK,
                    profileType);
        }

        // If asked to always create a fresh new instance, not from persistent state, do it here.
        if (preferNew) {
            // It is possible in a downgraded instance limit scenario that some or all ids of
            // persisted instances are outside the range bounded by the current |mMaxInstances|. In
            // this case, we want to avoid allocating an available id in the new range if we are at
            // or over instance limit, so that we avoid allowing successful creation of the current
            // activity in this scenario.
            if (MultiWindowUtils.isWithinInstanceLimit()) {
                for (int i = 0; i < TabWindowManager.MAX_SELECTORS_1000; ++i) {
                    if (!ChromeMultiInstancePersistentStore.hasInstance(i)) {
                        logNewInstanceId(i);
                        profileType = getProfileType(i, isIncognitoIntent);
                        return new AllocatedIdInfo(
                                i,
                                InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK,
                                profileType);
                    }
                }
            }
            profileType = getProfileType(INVALID_WINDOW_ID, isIncognitoIntent);
            return new AllocatedIdInfo(
                    INVALID_WINDOW_ID,
                    InstanceAllocationType.PREFER_NEW_INVALID_INSTANCE,
                    profileType);
        }

        // Search for an unassigned ID. The index is available for the assignment if:
        // a) there is no associated task and the instance is not marked for deletion, or
        // b) the corresponding persistent state does not exist.
        // Prefer a over b. Pick the MRU instance if there is more than one. Type b returns 0
        // for |readLastAccessedTime|, so can be regarded as the least favored.
        int id = INVALID_WINDOW_ID;
        boolean newInstanceIdAllocated = false;
        @InstanceAllocationType int allocationType = InstanceAllocationType.INVALID_INSTANCE;
        boolean isRelaunch =
                IntentUtils.safeGetBooleanExtra(
                        mActivity.getIntent(), IntentHandler.EXTRA_FROM_RELAUNCH, false);
        for (int i = 0; i < getMaxInstances(); ++i) {
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(i);
            if (persistedTaskId != INVALID_TASK_ID) {
                continue;
            }
            if (ChromeMultiInstancePersistentStore.readMarkedForDeletion(i)) {
                continue;
            }

            boolean instanceExists = ChromeMultiInstancePersistentStore.hasInstance(i);
            if (instanceExists
                    && !isRelaunch
                    && (DeviceInfo.isDesktop()
                            && ChromeFeatureList.sOnStartupWindowPolicy.isEnabled())) {
                // This supports updated default id allocation / startup behavior where a newly
                // created activity will refrain from using existing instance state and will be
                // created as a brand-new window instead.
                continue;
            }

            if (id == INVALID_WINDOW_ID
                    || ChromeMultiInstancePersistentStore.readLastAccessedTime(i)
                            > ChromeMultiInstancePersistentStore.readLastAccessedTime(id)) {
                // Last accessed time equals to 0 means the corresponding persistent state does not
                // exist. The profile type check should only be enforced when restoring from
                // persistent state.
                // TODO(crbug.com/456289090): Handle the scenario where we are at instance limit
                // (with all non-REGULAR windows) with no live activities and a new REGULAR window
                // is attempted to be created from the launcher.
                // TODO(crbug.com/458129266): Rely on profile exists check instead of feature flag 6
                // months post launch.
                if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                        && ChromeMultiInstancePersistentStore.readLastAccessedTime(i) != 0
                        && ChromeMultiInstancePersistentStore.readProfileType(i)
                                != (isIncognitoIntent
                                        ? SupportedProfileType.OFF_THE_RECORD
                                        : SupportedProfileType.REGULAR)) {
                    continue;
                }
                id = i;
                newInstanceIdAllocated = !instanceExists;
                allocationType =
                        newInstanceIdAllocated
                                ? InstanceAllocationType.NEW_INSTANCE_NEW_TASK
                                : InstanceAllocationType.EXISTING_INSTANCE_NEW_TASK;
            }
        }

        if (newInstanceIdAllocated) {
            logNewInstanceId(id);
        } else if (id != INVALID_WINDOW_ID) {
            Log.i(
                    TAG_MULTI_INSTANCE,
                    "Existing Instance - persisted and unmapped Id allocated: " + id);
        }
        profileType = getProfileType(id, isIncognitoIntent);
        return new AllocatedIdInfo(id, allocationType, profileType);
    }

    /**
     * Determines the profile type for a newly created window. See {@link #allocInstanceId(int, int,
     * boolean, boolean)} for usage.
     *
     * @param windowId The id allocated to the newly created window.
     * @param isIncognito Whether the window is an incognito-only window.
     */
    private @SupportedProfileType int getProfileType(int windowId, boolean isIncognito) {
        @SupportedProfileType int profileType;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            profileType =
                    isIncognito
                            ? SupportedProfileType.OFF_THE_RECORD
                            : SupportedProfileType.REGULAR;

            int persistedProfileType = ChromeMultiInstancePersistentStore.readProfileType(windowId);
            if (persistedProfileType != SupportedProfileType.UNSET) {
                // The profile type based on the new window intent and the value from the
                // persistent store should not conflict. The intent should only
                // specify SupportedProfileType for the new window, which will not have value in
                // persistent store.
                profileType = persistedProfileType;
            }
        } else {
            profileType = SupportedProfileType.MIXED;
        }
        return profileType;
    }

    // This method will finish the least recently used excess running activities / tasks exactly
    // once after an instance limit downgrade.
    private void finishExcessRunningActivities() {
        // Return early if an instance limit downgrade has been handled previously. This is to avoid
        // a case where we end up replacing an active instance with a newly created activity (by
        // finishing the task for the former) when max instances are open.
        if (ChromeMultiInstancePersistentStore.readInstanceLimitDowngradeTriggered()) {
            return;
        }

        Set<Integer> activeInstanceIds =
                MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ACTIVE);
        // This method is called before instanceId allocation for the currently starting activity.
        // getPersistedInstanceIds() does not account for this activity since it does not have an
        // associated persisted task state yet. Increment |numTasksToFinish| by 1 to account for
        // this activity in the total active instance count.
        int numTasksToFinish = activeInstanceIds.size() - MultiWindowUtils.getMaxInstances() + 1;

        if (numTasksToFinish <= 0) return;
        ChromeMultiInstancePersistentStore.writeInstanceLimitDowngradeTriggered(true);

        // Get the instance ids of up to |numTasksToFinish| least recently used instances.
        TreeMap<Long, Integer> lruInstanceIds = new TreeMap<>();
        for (int i : activeInstanceIds) {
            if (ChromeMultiInstancePersistentStore.readTaskId(i) == INVALID_TASK_ID) continue;
            long lastAccessedTime = ChromeMultiInstancePersistentStore.readLastAccessedTime(i);
            lruInstanceIds.put(lastAccessedTime, i);
            if (lruInstanceIds.size() > numTasksToFinish) {
                lruInstanceIds.remove(lruInstanceIds.lastKey());
            }
        }

        // Determine the active tasks that need to be finished.
        Map<Integer, Integer> tasksToDelete = new HashMap<>();
        for (Integer i : lruInstanceIds.values()) {
            tasksToDelete.put(ChromeMultiInstancePersistentStore.readTaskId(i), i);
        }

        // Finish AppTasks that are excess of what is required to stay within the instance limit.
        List<AppTask> appTasks =
                ((ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE))
                        .getAppTasks();
        for (AppTask appTask : appTasks) {
            var taskInfo = AndroidTaskUtils.getTaskInfoFromTask(appTask);
            if (taskInfo == null) continue;
            if (tasksToDelete.containsKey(taskInfo.taskId)) {
                appTask.finishAndRemoveTask();
                int instanceId = assertNonNull(tasksToDelete.get(taskInfo.taskId));
                ChromeMultiInstancePersistentStore.removeTaskId(instanceId);
            }
        }
    }

    private void logNewInstanceId(int i) {
        StringBuilder taskData = new StringBuilder();
        ActivityManager activityManager =
                (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        for (AppTask task : activityManager.getAppTasks()) {
            String baseActivity = MultiWindowUtils.getActivityNameFromTask(task);
            ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            taskData.append(
                    "Task with id: "
                            + (info != null ? info.taskId : "NOT_SET")
                            + " has base activity: "
                            + baseActivity
                            + ".\n");
        }
        Log.i(
                TAG_MULTI_INSTANCE,
                "New Instance - unused Id allocated: "
                        + i
                        + ". Task data during instance allocation: "
                        + taskData);
    }

    @Override
    public void initialize(int instanceId, int taskId, @SupportedProfileType int profileType) {
        super.initialize(instanceId, taskId, profileType);
        mInstanceId = instanceId;

        // Ensure we have instance info entry for the current one before writing other fields.
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(mInstanceId);

        ChromeMultiInstancePersistentStore.writeTaskId(instanceId, taskId);
        ChromeMultiInstancePersistentStore.writeProfileType(instanceId, profileType);
        ChromeMultiInstancePersistentStore.writeMarkedForDeletion(
                instanceId, /* markedForDeletion= */ false);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(instanceId, true);
        installTabModelObserver();
        recordInstanceCountHistogram();
        recordActivityCountHistogram();
        ActivityManager activityManager =
                (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        String launchActivityName = ChromeTabbedActivity.MAIN_LAUNCHER_ACTIVITY_NAME;
        if (activityManager != null) {
            sState =
                    MultiInstanceState.maybeCreate(
                            activityManager::getAppTasks,
                            (activityName) ->
                                    TextUtils.equals(
                                                    activityName,
                                                    ChromeTabbedActivity.class.getName())
                                            || TextUtils.equals(activityName, launchActivityName));
            sState.addObserver(mOnMultiInstanceStateChanged);
        }
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
    }

    @Override
    public void onTabStateInitialized() {
        TabModelSelector selector =
                assumeNonNull(mTabModelOrchestratorSupplier.get()).getTabModelSelector();
        assert selector != null;
        writeTabCount(mInstanceId, selector);
    }

    @VisibleForTesting
    protected void installTabModelObserver() {
        TabModelSelector selector =
                assumeNonNull(mTabModelOrchestratorSupplier.get()).getTabModelSelector();
        assert selector != null;
        mTabModelObserver =
                new TabModelSelectorTabModelObserver(selector) {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        // We will check if |mActiveTab| is the same as the selected |tab| to avoid
                        // a superfluous update to an instance's stored active tab info that
                        // remains unchanged.
                        // The check on |lastId| is required to continue updating this info for an
                        // instance even when |mActiveTab| is the same as the selected |tab|, in
                        // the following scenario: If |mActiveTab| is the last tab in instance 1,
                        // and is moved to instance
                        // 2, instance 1 stores "empty" active tab information since it now
                        // contains no tabs.
                        // When |mActiveTab| is moved back to instance 1, |mActiveTab| is now the
                        // same as the selected |tab| in instance 1, however instance 1's active
                        // tab information will not be updated, unless we establish that this
                        // instance is currently holding "empty" info, reflected by the fact that
                        // it has an invalid last selected tab ID, so it's active tab info can
                        // then be updated.
                        if (mActiveTab == tab && lastId != Tab.INVALID_TAB_ID) return;
                        if (mActiveTab != null) mActiveTab.removeObserver(mActiveTabObserver);
                        mActiveTab = tab;
                        if (mActiveTab != null) {
                            mActiveTab.addObserver(mActiveTabObserver);
                            ChromeMultiInstancePersistentStore.writeIncognitoSelected(
                                    mInstanceId, mActiveTab.isIncognito());
                            // When an incognito tab is focused, keep the normal active tab info.
                            Tab urlTab =
                                    mActiveTab.isIncognito()
                                            ? TabModelUtils.getCurrentTab(selector.getModel(false))
                                            : mActiveTab;
                            if (urlTab != null) {
                                ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                                        mInstanceId, urlTab.getOriginalUrl().getSpec());
                                ChromeMultiInstancePersistentStore.writeActiveTabTitle(
                                        mInstanceId, urlTab.getTitle());
                            } else {
                                ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                                        mInstanceId, EMPTY_DATA);
                                ChromeMultiInstancePersistentStore.writeActiveTabTitle(
                                        mInstanceId, EMPTY_DATA);
                            }
                        }
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        writeTabCount(mInstanceId, selector);
                    }

                    @Override
                    public void onFinishingTabClosure(
                            Tab tab, @TabClosingSource int closingSource) {
                        // onFinishingTabClosure is called for both normal/incognito tabs, whereas
                        // tabClosureCommitted is called for normal tabs only.
                        writeTabCount(mInstanceId, selector);
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        // Updates the tab count of the src activity a reparented tab gets detached
                        // from.
                        writeTabCount(mInstanceId, selector);
                    }
                };
    }

    private void removeInvalidInstanceData() {
        // Update persisted task state based on current AppTasks.
        Set<Integer> appTaskIds = MultiWindowUtils.getAllAppTaskIds(mActivity);

        Map<Integer, Integer> taskMap = ChromeMultiInstancePersistentStore.readTaskMap();
        List<String> tasksRemoved = new ArrayList<>();
        for (Map.Entry<Integer, Integer> entry : taskMap.entrySet()) {
            if (!appTaskIds.contains(entry.getValue())) {
                tasksRemoved.add("instanceId: " + entry.getKey() + " taskId: " + entry.getValue());
                ChromeMultiInstancePersistentStore.removeTaskId(entry.getKey());
            }
        }

        List<Integer> instancesRemoved = new ArrayList<>();
        List<Integer> inactiveInstances = new ArrayList<>();
        List<Integer> expiredInstances = new ArrayList<>();
        for (int i : MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ANY)) {
            // Remove persistent data for unrecoverable instances.
            if (!MultiWindowUtils.isRestorableInstance(appTaskIds, i)) {
                instancesRemoved.add(i);
                // An instance with no live task is deleted if it has no tabs.
                removeInstanceInfo(i, CloseWindowAppSource.NO_TABS_IN_WINDOW);
            } else {
                long lastAccessedTime = ChromeMultiInstancePersistentStore.readLastAccessedTime(i);
                if (isOlderThanSixMonths(lastAccessedTime)
                        && MultiWindowUtils.getActivityById(i) != mActivity) {
                    expiredInstances.add(i);
                    continue;
                }

                if (ChromeMultiInstancePersistentStore.readTaskId(i) == INVALID_TASK_ID) {
                    inactiveInstances.add(i);
                }
            }
        }

        // This method could be invoked during early startup before mTabModelOrchestratorSupplier is
        // initialized. In that case, skip cleanup of expired instances and / or instances exceeding
        // the inactive instance limit, and defer to a subsequent call to handle it.
        if (!expiredInstances.isEmpty() && mTabModelOrchestratorSupplier.get() != null) {
            closeWindows(expiredInstances, CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION);
        }

        int numInactiveInstances = inactiveInstances.size();
        int inactiveInstanceLimit =
                RecentlyClosedEntriesManager.MAX_RECENTLY_CLOSED_TABS_AND_WINDOWS;
        if (numInactiveInstances > inactiveInstanceLimit
                && mTabModelOrchestratorSupplier.get() != null) {
            // Sort list by last closure time or last accessed time to ensure only the oldest
            // inactive instances are closed.
            inactiveInstances.sort(
                    (id1, id2) -> {
                        long time1 = ChromeMultiInstancePersistentStore.readClosureTime(id1);
                        if (time1 <= 0) {
                            time1 = ChromeMultiInstancePersistentStore.readLastAccessedTime(id1);
                        }
                        long time2 = ChromeMultiInstancePersistentStore.readClosureTime(id2);
                        if (time2 <= 0) {
                            time2 = ChromeMultiInstancePersistentStore.readLastAccessedTime(id2);
                        }
                        return Long.compare(time2, time1);
                    });
            closeWindows(
                    inactiveInstances.subList(inactiveInstanceLimit, numInactiveInstances),
                    CloseWindowAppSource.RECENTLY_CLOSED_LIMIT_EXCEEDED);
        }

        if (!tasksRemoved.isEmpty()
                || !instancesRemoved.isEmpty()
                || !inactiveInstances.isEmpty()
                || !expiredInstances.isEmpty()) {
            Log.i(
                    TAG_MULTI_INSTANCE,
                    "Removed invalid instance data. Removed tasks-instance mappings: "
                            + tasksRemoved
                            + " and shared prefs for instances: "
                            + instancesRemoved
                            + " and inactive instances in excess of the closed instance limit: "
                            + inactiveInstances
                            + " and expired instances: "
                            + expiredInstances);
        }
    }

    private int getInstanceByTask(int taskId) {
        for (int i : MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ANY)) {
            if (taskId == ChromeMultiInstancePersistentStore.readTaskId(i)) return i;
        }
        return INVALID_WINDOW_ID;
    }

    @Override
    public boolean isTabModelMergingEnabled() {
        return false;
    }

    private void recordActivityCountHistogram() {
        RecordHistogram.recordExactLinearHistogram(
                "Android.MultiInstance.NumActivities",
                MultiWindowUtils.getRunningTabbedActivityCount(),
                TabWindowManager.MAX_SELECTORS_1000 + 1);
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    "Android.MultiInstance.NumActivities.Incognito",
                    MultiWindowUtils.getInstanceCount(
                            PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD),
                    TabWindowManager.MAX_SELECTORS_1000 + 1);
        }
    }

    private void recordInstanceCountHistogram() {
        RecordHistogram.recordExactLinearHistogram(
                "Android.MultiInstance.NumInstances",
                MultiWindowUtils.getInstanceCount(PersistedInstanceType.ANY),
                TabWindowManager.MAX_SELECTORS_1000 + 1);

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    "Android.MultiInstance.NumInstances.Incognito",
                    MultiWindowUtils.getInstanceCount(PersistedInstanceType.OFF_THE_RECORD),
                    TabWindowManager.MAX_SELECTORS_1000 + 1);
        }
    }

    private static void writeTabCount(int index, TabModelSelector selector) {
        if (!selector.isTabStateInitialized()) return;
        int tabCount = selector.getModel(false).getCount();
        int incognitoTabCount = selector.getModel(true).getCount();
        ChromeMultiInstancePersistentStore.writeTabCount(index, tabCount, incognitoTabCount);
        if (tabCount == 0) {
            ChromeMultiInstancePersistentStore.writeActiveTabUrl(index, EMPTY_DATA);
            ChromeMultiInstancePersistentStore.writeActiveTabTitle(index, EMPTY_DATA);
        }
    }

    @Override
    public void openWindow(int instanceId, @NewWindowAppSource int source) {
        Set<Integer> activeTaskIds = MultiWindowUtils.getAllAppTaskIds(mActivity);
        int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(instanceId);
        if (activeTaskIds.contains(persistedTaskId)) {
            // Bring the task to foreground if the activity is alive, this completes the opening
            // of the instance. Otherwise, create a new activity for the instance and kill the
            // existing task.
            Activity activity = MultiWindowUtils.getActivityById(instanceId);
            if (activity != null) {
                ApiCompatibilityUtils.moveTaskToFront(mActivity, persistedTaskId, 0);
                return;
            } else {
                var appTask = AndroidTaskUtils.getAppTaskFromId(mActivity, persistedTaskId);
                if (appTask != null) {
                    appTask.finishAndRemoveTask();
                }
            }
        }

        boolean isTargetIncognito =
                ChromeMultiInstancePersistentStore.readProfileType(instanceId)
                        == SupportedProfileType.OFF_THE_RECORD;
        boolean openAdjacently =
                MultiWindowUtils.shouldOpenInAdjacentWindow(mActivity, isTargetIncognito);
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity, instanceId, /* preferNew= */ false, openAdjacently, source);
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            intent.putExtra(
                    IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW,
                    ChromeMultiInstancePersistentStore.readProfileType(instanceId)
                            == SupportedProfileType.OFF_THE_RECORD);
        }
        ChromeMultiInstancePersistentStore.writeMarkedForDeletion(
                instanceId, /* markedForDeletion= */ false);
        mActivity.startActivity(intent);

        // If a new activity was started, it implies that an inactive instance was restored.
        RecentlyClosedEntriesManagerTrackerFactory.getInstance().onInstanceRestored(instanceId);

        RecordHistogram.recordEnumeratedHistogram(
                "Android.MultiWindowMode.InactiveInstanceRestore.AppSource2",
                source,
                NewWindowAppSource.NUM_ENTRIES);
    }

    @Override
    public void closeWindows(List<Integer> instanceIds, @CloseWindowAppSource int source) {
        boolean shouldCloseCurrentInstance = false;
        for (int instanceId : instanceIds) {
            if (instanceId == mInstanceId) {
                // Close the current instance in the end. This ensures that all other instances in
                // the list are first correctly closed by the current instance before its activity
                // is prematurely destroyed.
                shouldCloseCurrentInstance = true;
                continue;
            }
            closeWindow(instanceId, source);
        }
        if (shouldCloseCurrentInstance) {
            closeWindow(mInstanceId, source);
        }
        notifyInstancesClosed(instanceIds, isPermanentClosureSource(source));
    }

    private void closeWindow(int instanceId, @CloseWindowAppSource int source) {
        boolean shouldPermanentlyDelete = shouldPermanentlyDeleteWindow(instanceId, source);
        if (shouldPermanentlyDelete) {
            removeInstanceInfo(instanceId, source);
            TabModelSelector selector =
                    TabWindowManagerSingleton.getInstance().getTabModelSelectorById(instanceId);
            if (selector != null && source != CloseWindowAppSource.NO_TABS_IN_WINDOW) {
                // Commit all already pending tab closures to ensure that any in-flight closures
                // complete and we don't get back-from-the-dead tabs. Do not initiate this task if
                // there are no tabs in the window to close.
                selector.commitAllTabClosures();

                // Close all tabs as the window is closing. Avoid saving closure to the
                // TabRestoreService as this closure is intended to be permanent.
                TabClosureParams params =
                        TabClosureParams.closeAllTabs()
                                .uponExit(true)
                                .hideTabGroups(true)
                                .saveToTabRestoreService(false)
                                .build();
                selector.getModel(true).getTabRemover().closeTabs(params, /* allowDialog= */ false);
                selector.getModel(false)
                        .getTabRemover()
                        .closeTabs(params, /* allowDialog= */ false);
            }
            assumeNonNull(mTabModelOrchestratorSupplier.get()).cleanupInstance(instanceId);
        } else {
            ChromeMultiInstancePersistentStore.writeMarkedForDeletion(
                    instanceId, /* markedForDeletion= */ true);
            ChromeMultiInstancePersistentStore.writeIsRecoverable(instanceId, false);
            ChromeMultiInstancePersistentStore.writeClosureTime(instanceId);
            ChromeMultiInstancePersistentStore.removeTaskId(instanceId);
        }
        Activity activity = MultiWindowUtils.getActivityById(instanceId);
        if (activity != null) {
            activity.finishAndRemoveTask();
        }

        if (shouldPermanentlyDelete && mInstanceId != instanceId) {
            // Initiate synced tab groups cleanup only if the closed instance is not the
            // current one. If after closure of the current, second to last instance, a
            // single instance remains, this cleanup will be initiated on activity
            // startup of that instance.
            cleanupSyncedTabGroupsIfLastInstance();
        }
    }

    /**
     * Notifies the Recent Tabs UI of instance closures. This method is expected to be called upon
     * initial reception of a user, system or app initiated signal to close instances.
     *
     * @param instanceIds The list of ids of the instances that were closed.
     * @param isPermanentDeletion Whether the instances are permanently deleted.
     */
    private void notifyInstancesClosed(List<Integer> instanceIds, boolean isPermanentDeletion) {
        // Note that instance state (for e.g. taskId) may not be updated if a live activity for the
        // closed instance was finished, because activity destruction is asynchronous.
        // We will create an InstanceInfo synchronously with adequate information about the closed
        // instance, without relying on completion of an asynchronous activity destruction that may
        // be initiated during this time.
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        for (int instanceId : instanceIds) {
            // Do not update the Recent Tabs page if the closed window has no regular tabs.
            if (!hasRestorableRegularTabs(instanceId)) {
                continue;
            }
            InstanceInfo instanceInfo =
                    new InstanceInfo(
                            instanceId,
                            /* taskId= */ INVALID_TASK_ID,
                            InstanceInfo.Type.OTHER,
                            assumeNonNull(
                                    ChromeMultiInstancePersistentStore.readActiveTabUrl(
                                            instanceId)),
                            assumeNonNull(
                                    ChromeMultiInstancePersistentStore.readActiveTabTitle(
                                            instanceId)),
                            ChromeMultiInstancePersistentStore.readCustomTitle(instanceId),
                            ChromeMultiInstancePersistentStore.readNormalTabCount(instanceId),
                            ChromeMultiInstancePersistentStore.readIncognitoTabCount(instanceId),
                            ChromeMultiInstancePersistentStore.readIncognitoSelected(instanceId),
                            ChromeMultiInstancePersistentStore.readLastAccessedTime(instanceId),
                            ChromeMultiInstancePersistentStore.readClosureTime(instanceId));
            instanceInfoList.add(instanceInfo);
        }

        if (!instanceInfoList.isEmpty()) {
            RecentlyClosedEntriesManagerTrackerFactory.getInstance()
                    .onInstancesClosed(instanceInfoList, isPermanentDeletion);
        }
    }

    /**
     * Returns whether a window should be permanently deleted. If the closure is initiated by the
     * user, it usually means that the instance closure is a "soft closure" and should be preserved
     * for later restoration via surfaces (like Recent Tabs) or keyboard shortcuts.
     *
     * <p>A soft closure means the window's {@link InstanceInfo} and {@link TabModel} data are
     * persisted, even though the {@link Activity} and Android task will be removed via {@link
     * Activity#finishAndRemoveTask()}.
     *
     * @param source The window closure source, from {@link CloseWindowAppSource}.
     */
    private static boolean isPermanentClosureSource(@CloseWindowAppSource int source) {
        return source != CloseWindowAppSource.WINDOW_MANAGER;
    }

    private static boolean hasRestorableRegularTabs(int instanceId) {
        int normalTabCount = ChromeMultiInstancePersistentStore.readNormalTabCount(instanceId);

        if (normalTabCount > 1) return true;
        if (normalTabCount == 0) return false;

        String activeUrl = ChromeMultiInstancePersistentStore.readActiveTabUrl(instanceId);
        return !UrlUtilities.isNtpUrl(UrlFormatter.fixupUrl(activeUrl));
    }

    private static boolean shouldPermanentlyDeleteWindow(
            int instanceId, @CloseWindowAppSource int source) {
        return isPermanentClosureSource(source) || !hasRestorableRegularTabs(instanceId);
    }

    private Profile getProfile() {
        TabModelSelector tabModelSelector =
                assumeNonNull(mTabModelOrchestratorSupplier.get()).getTabModelSelector();
        assumeNonNull(tabModelSelector);
        var profile = tabModelSelector.getCurrentModel().getProfile();
        assert profile != null;
        return profile;
    }

    @Override
    public void onDestroy() {
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        // This handles a case where an instance is deleted within Chrome but not through
        // Window manager UI, and the task is removed by system. See https://crbug.com/40194788.
        removeInvalidInstanceData();

        // Activity#isFinishing() is true in case of explicit user intent, for eg. task swipe up
        // from Android Recents or app trigger, e.g. programmatically invoking #finish() on the
        // activity. When the activity gets destroyed by the system in the background while keeping
        // its task alive, we don't want such closure to be reflected on Recent Tabs because an
        // instance with a live task is still considered active. Therefore, we will notify Recent
        // Tabs of activity destruction only if the activity is finishing, with the caveat that a
        // subsequent task kill will also not be reflected as an instance closure until the Recent
        // Tabs page is reopened.
        boolean isPermanentDeletion = !hasRestorableRegularTabs(mInstanceId);
        if (!isPermanentDeletion) {
            ChromeMultiInstancePersistentStore.writeClosureTime(mInstanceId);
        }
        if (mActivity.isFinishing()) {
            ChromeMultiInstancePersistentStore.writeIsRecoverable(mInstanceId, false);
            // Notify Recent Tabs page that the instance is closing.
            notifyInstancesClosed(Collections.singletonList(mInstanceId), isPermanentDeletion);
        }

        if (mInstanceId != INVALID_WINDOW_ID) {
            ApplicationStatus.unregisterActivityStateListener(this);
        }
        if (sState != null) {
            List<Activity> activities = ApplicationStatus.getRunningActivities();
            // We're called before the corresponding activity is actually destroyed, so there should
            // be at least one running activity.
            assert !activities.isEmpty();
            if (activities.size() == 1) {
                sState.clear();
            } else {
                sState.removeObserver(mOnMultiInstanceStateChanged);
            }
        }

        super.onDestroy();
    }

    @VisibleForTesting
    /* package */ static void removeInstanceInfo(int index, @CloseWindowAppSource int source) {
        ChromeMultiInstancePersistentStore.deleteInstanceState(index);

        RecordHistogram.recordEnumeratedHistogram(
                CLOSE_WINDOW_APP_SOURCE_HISTOGRAM, source, CloseWindowAppSource.NUM_ENTRIES);
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        super.onTopResumedActivityChanged(isTopResumedActivity);
        if (isTopResumedActivity) {
            ChromeMultiInstancePersistentStore.writeLastAccessedTime(mInstanceId);
        }
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        // We persist last closed time when the activity is stopped as a fallback for when
        // #onDestroy() is not called for a finishing activity.
        ChromeMultiInstancePersistentStore.writeClosureTime(mInstanceId);
        if (mActivity.isFinishing()) {
            ChromeMultiInstancePersistentStore.writeIsRecoverable(mInstanceId, false);
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        if (newState != ActivityState.RESUMED && newState != ActivityState.STOPPED) return;

        int windowingMode =
                MultiWindowMetricsUtils.getWindowingMode(
                        activity,
                        AppHeaderUtils.isAppInDesktopWindow(
                                mDesktopWindowStateManagerSupplier.get()));
        MultiWindowMetricsUtils.recordWindowingMode(
                windowingMode,
                TabWindowManagerSingleton.getInstance().getIdForWindow(activity),
                newState == ActivityState.RESUMED);

        // Defer metrics collection to a background thread to avoid Binder IPC on the main thread,
        // which can cause ANRs. Use a sequenced task runner to ensure serial execution and
        // prevent concurrent read-modify-write races on daily max counters.
        sMetricsTaskRunner.postDelayedTask(
                () -> {
                    recordInstanceCountMetrics();
                },
                0);
    }

    /** Collect instance count metrics on a background thread to avoid ANR from Binder IPC. */
    private static void recordInstanceCountMetrics() {
        // Check the max instance count in a day for every state update if needed.
        long timestamp = ChromeMultiInstancePersistentStore.readMaxCountHistogramStartTime();
        int maxCount = ChromeMultiInstancePersistentStore.readDailyMaxInstanceCount();
        int maxActiveCount = ChromeMultiInstancePersistentStore.readDailyMaxActiveInstanceCount();
        int incognitoMaxCount =
                ChromeMultiInstancePersistentStore.readDailyMaxIncognitoInstanceCount();
        long current = System.currentTimeMillis();

        if (current - timestamp > DateUtils.DAY_IN_MILLIS) {
            if (timestamp != 0) {
                RecordHistogram.recordExactLinearHistogram(
                        "Android.MultiInstance.MaxInstanceCount",
                        maxCount,
                        TabWindowManager.MAX_SELECTORS_1000 + 1);
                RecordHistogram.recordExactLinearHistogram(
                        "Android.MultiInstance.MaxActiveInstanceCount",
                        maxActiveCount,
                        TabWindowManager.MAX_SELECTORS_1000 + 1);
                if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                    RecordHistogram.recordExactLinearHistogram(
                            "Android.MultiInstance.MaxInstanceCountIncognito",
                            incognitoMaxCount,
                            TabWindowManager.MAX_SELECTORS_1000 + 1);
                }
            }
            ChromeMultiInstancePersistentStore.writeMaxCountHistogramStartTime(current);
            // Reset the count to 0 to be ready to obtain the max count for the next 24-hour
            // period.
            maxCount = 0;
            maxActiveCount = 0;
            incognitoMaxCount = 0;
        }
        // Fetch appTaskIds once and reuse via overloads to avoid redundant Binder IPC calls.
        Context context = ContextUtils.getApplicationContext();
        Set<Integer> appTaskIds = MultiWindowUtils.getAllAppTaskIds(context);
        int instanceCount =
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManager.PersistedInstanceType.ANY, appTaskIds);
        int incognitoInstanceCount =
                MultiWindowUtils.getInstanceCount(
                        PersistedInstanceType.OFF_THE_RECORD, appTaskIds);
        if (instanceCount > maxCount) {
            ChromeMultiInstancePersistentStore.writeDailyMaxInstanceCount(instanceCount);
        }
        int activeInstanceCount =
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManager.PersistedInstanceType.ACTIVE, appTaskIds);
        if (activeInstanceCount > maxActiveCount) {
            ChromeMultiInstancePersistentStore.writeDailyMaxActiveInstanceCount(
                    activeInstanceCount);
        }
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                && incognitoInstanceCount > incognitoMaxCount) {
            ChromeMultiInstancePersistentStore.writeDailyMaxIncognitoInstanceCount(
                    incognitoInstanceCount);
        }
    }

    private void onMultiInstanceStateChanged(boolean inMultiInstanceMode) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        long startTime = ChromeMultiInstancePersistentStore.readMultiInstanceStartTime();
        long current = System.currentTimeMillis();

        // This method in invoked for every ChromeActivity instance. Logging metrics for the first
        // ChromeActivity is enough. The pref |MULTI_INSTANCE_START_TIME| is set to non-zero once
        // Android.MultiInstance.Enter is logged, and reset to zero after
        // Android.MultiInstance.Exit to avoid duplicated logging.
        if (startTime == 0 && inMultiInstanceMode) {
            RecordUserAction.record("Android.MultiInstance.Enter");
            ChromeMultiInstancePersistentStore.writeMultiInstanceStartTime(current);
        } else if (startTime != 0 && !inMultiInstanceMode) {
            RecordUserAction.record("Android.MultiInstance.Exit");
            RecordHistogram.recordLongTimesHistogram(
                    "Android.MultiInstance.TotalDuration", current - startTime);
            ChromeMultiInstancePersistentStore.writeMultiInstanceStartTime(0);
        }
    }

    /**
     * Close a Chrome window instance only if it contains no open tabs including incognito ones.
     *
     * @param instanceId Instance id of the Chrome window that needs to be closed.
     * @return {@code true} if the window was closed, {@code false} otherwise.
     */
    @Override
    public boolean closeChromeWindowIfEmpty(int instanceId) {
        if (instanceId != INVALID_WINDOW_ID) {
            TabModelSelector selector =
                    TabWindowManagerSingleton.getInstance().getTabModelSelectorById(instanceId);
            // Determine if the drag source Chrome instance window has any tabs including incognito
            // ones left so as to close if it is empty.
            if (selector != null && selector.getTotalTabCount() == 0) {
                Log.i(TAG, "Closing empty Chrome instance as no tabs exist.");
                closeWindows(
                        Collections.singletonList(instanceId),
                        CloseWindowAppSource.NO_TABS_IN_WINDOW);
                return true;
            }
        }
        return false;
    }

    /**
     * This method makes a call out to sync to audit all of the tab groups if there is only one
     * remaining active Chrome instance. This is a workaround to the fact that closing an instance
     * that does not have an active {@link TabModelSelector} will never notify sync that the tabs it
     * contained were closed and as such sync will continue to think some inactive instance contains
     * the tab groups that aren't available in the current activity. If we get down to a single
     * instance of Chrome we know any data for tab groups not found in the current activity's {@link
     * TabModelSelector} must be closed and we can remove the sync mapping.
     */
    @VisibleForTesting
    /* package */ void cleanupSyncedTabGroupsIfLastInstance() {
        Set<Integer> info = MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ANY);
        if (info.size() != 1) return;

        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance()
                        .getTabModelSelectorById(info.iterator().next());
        if (selector == null) return;

        cleanupSyncedTabGroups(selector);
    }

    @Override
    public void cleanupSyncedTabGroupsIfOnlyInstance(TabModelSelector selector) {
        TabModelUtils.runOnTabStateInitialized(
                selector,
                (TabModelSelector initializedSelector) -> cleanupSyncedTabGroupsIfLastInstance());
    }

    private int getMaxInstances() {
        return Objects.requireNonNullElse(MultiWindowUtils.sMaxInstancesForTesting, mMaxInstances);
    }

    @Override
    public void showInstanceCreationLimitMessage() {
        if (mIsCreationLimitMessageEnqueued) return;

        MessageDispatcher messageDispatcher = getMessageDispatcher();
        if (messageDispatcher == null) {
            return;
        }

        mIsCreationLimitMessageEnqueued = true;
        MultiWindowUtils.showInstanceCreationLimitMessage(
                messageDispatcher,
                mActivity,
                this::showInstanceSwitcherDialog,
                () -> mIsCreationLimitMessageEnqueued = false);
    }

    @VisibleForTesting
    @Nullable MessageDispatcher getMessageDispatcher() {
        if (mActiveTab == null) return null;
        return MessageDispatcherProvider.from(mActiveTab.getWindowAndroid());
    }

    @Override
    public void showNameWindowDialog(@NameWindowDialogSource int source) {
        String customTitle = ChromeMultiInstancePersistentStore.readCustomTitle(mInstanceId);
        String defaultTitle = ChromeMultiInstancePersistentStore.readActiveTabTitle(mInstanceId);
        String currentTitle = TextUtils.isEmpty(customTitle) ? defaultTitle : customTitle;

        UiUtils.showNameWindowDialog(
                mActivity,
                assumeNonNull(currentTitle),
                newTitle -> renameInstance(mInstanceId, newTitle),
                source);
    }
}
