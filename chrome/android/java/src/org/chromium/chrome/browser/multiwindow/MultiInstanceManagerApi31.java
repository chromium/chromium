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
import android.provider.Browser;
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
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.RecentlyClosedEntriesManagerTrackerFactory;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceState.MultiInstanceStateObserver;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
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
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
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
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
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

    private final MonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    // Instance ID for the activity associated with this manager.
    private int mInstanceId = INVALID_WINDOW_ID;

    private @Nullable Tab mActiveTab;
    private final TabObserver mActiveTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onTitleUpdated(Tab tab) {
                    if (!tab.isIncognito()) {
                        MultiInstancePersistentStore.writeActiveTabTitle(
                                mInstanceId, tab.getTitle());
                    }
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    if (!tab.isIncognito()) {
                        MultiInstancePersistentStore.writeActiveTabUrl(
                                mInstanceId, tab.getOriginalUrl().getSpec());
                    }
                }
            };

    private final Supplier<DesktopWindowStateManager> mDesktopWindowStateManagerSupplier;
    private final MultiInstanceStateObserver mOnMultiInstanceStateChanged;
    private final TabReparentingDelegate mTabReparentingDelegate;

    private static @Nullable Set<Integer> sAppTaskIdsForTesting;

    MultiInstanceManagerApi31(
            Activity activity,
            MonotonicObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            Supplier<DesktopWindowStateManager> desktopWindowStateManagerSupplier,
            TabReparentingDelegate tabReparentingDelegate) {
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

        mTabReparentingDelegate = tabReparentingDelegate;

        // Check if instance limit has changed and update SharedPrefs.
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        int maxInstances = getMaxInstances();
        int prevInstanceLimit =
                prefs.readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, maxInstances);
        if (maxInstances > prevInstanceLimit) {
            // Reset SharedPrefs for instance limit downgrade if limit has increased.
            prefs.writeBoolean(
                    ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED, false);
            prefs.writeBoolean(
                    ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN, false);
        }
        prefs.writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, maxInstances);
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
        MultiInstancePersistentStore.writeCustomTitle(instanceId, newName);
    }

    @Override
    public void openNewWindow(boolean isIncognito) {
        RecordUserAction.record("Android.WindowManager.NewWindow");
        Intent intent = createNewWindowIntent(isIncognito, NewWindowAppSource.WINDOW_MANAGER);
        assert intent != null : "The Intent to open a new window must not be null";

        mActivity.startActivity(intent);
    }

    @Override
    public void moveTabsToNewWindow(
            List<Tab> tabs, @Nullable Runnable finalizeCallback, @NewWindowAppSource int source) {
        if (tabs.isEmpty()) return;
        boolean openAdjacently = MultiWindowUtils.shouldOpenInAdjacentWindow(mActivity);
        if (isInstanceLimitReached()) {
            showInstanceCreationLimitMessage();
        } else {
            mTabReparentingDelegate.reparentTabsToNewWindow(
                    tabs, INVALID_WINDOW_ID, openAdjacently, finalizeCallback, source);
        }
    }

    @Override
    public void moveTabsToWindowByIdChecked(
            int destWindowId, List<Tab> tabs, int destTabIndex, int destGroupTabId) {
        if (tabs.isEmpty()) return;
        assert destTabIndex == TabList.INVALID_TAB_INDEX
                        || destGroupTabId == TabList.INVALID_TAB_INDEX
                : "Only one of destTabIndex or destGroupTabId should be specified.";
        assert MultiInstancePersistentStore.hasInstance(destWindowId)
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
            // If the source Chrome instance still has tabs (including incognito), allow
            // launching the new window adjacently. Otherwise, skip
            // FLAG_ACTIVITY_LAUNCH_ADJACENT to avoid a black screen caused by the source
            // window closing before the new one launches.
            TabModelSelector selector =
                    TabWindowManagerSingleton.getInstance()
                            .getTabModelSelectorById(getCurrentInstanceId());
            boolean openAdjacently = assumeNonNull(selector).getTotalTabCount() > 1;
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
        // Check the number of instances that the tab/s is able to move into.
        int instanceCount =
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE);
        @PersistedInstanceType int instanceType = PersistedInstanceType.ANY;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            // If one tab is incognito then all other tabs are incognito.
            if (tabs.get(0).isIncognitoBranded()) {
                instanceCount = MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ true);
                instanceType = PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD;
            } else {
                instanceCount =
                        MultiWindowUtils.getInstanceCountWithFallback(
                                PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR);
                instanceType = PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR;
            }
        }

        if (instanceCount <= 1) {
            moveTabsToNewWindow(tabs, /* finalizeCallback= */ null, source);

            // Close the source instance window, if needed.
            closeChromeWindowIfEmpty(mInstanceId);
            return;
        }

        showTargetSelectorDialog(
                (instanceInfo) -> {
                    moveTabsToWindowByIdChecked(
                            instanceInfo.instanceId,
                            tabs,
                            /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                            /* destGroupTabId= */ TabList.INVALID_TAB_INDEX);
                    // Close the source instance window, if needed.
                    closeChromeWindowIfEmpty(mInstanceId);
                },
                instanceType,
                R.string.menu_move_tab_to_other_window);
    }

    /**
     * Opens a URL in another window. The window in which the URL will be opened will depend on the
     * following criteria, checked in order of priority:
     *
     * <ul>
     *   <li>If there is exactly one window, a new window will be created.
     *   <li>If {@code preferNew} is true, a new window will be attempted to be created. Note that
     *       this will ensure that the URL is opened in a brand new window vs in a new activity
     *       created for a restored inactive instance. However, an instance creation limit warning
     *       message will be shown if instance limit is reached in this case.
     *   <li>The target selector dialog will be presented to the user to pick a target window to
     *       open the URL in.
     * </ul>
     *
     * @param loadUrlParams The url to open.
     * @param parentTabId The ID of the parent tab.
     * @param preferNew Whether we should prioritize launching the tab in a new window.
     * @param instanceType The {@link PersistedInstanceType} that will be used to determine the type
     *     of window the URL can be opened in.
     */
    @Override
    public void openUrlInOtherWindow(
            LoadUrlParams loadUrlParams,
            int parentTabId,
            boolean preferNew,
            @PersistedInstanceType int instanceType) {
        boolean incognitoInstance = (instanceType & PersistedInstanceType.OFF_THE_RECORD) != 0;
        boolean needsActive = (instanceType & PersistedInstanceType.ACTIVE) != 0;
        // Check the number of instances that the url is able to move into.
        int instanceCount =
                incognitoInstance
                        ? MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ needsActive)
                        : MultiWindowUtils.getInstanceCountWithFallback(instanceType);
        if (instanceCount <= 1 || preferNew) {
            if (preferNew && isInstanceLimitReached()) {
                assumeNonNull(mActiveTab);
                showInstanceCreationLimitMessage();
                return;
            }

            launchUrlInOtherWindow(
                    incognitoInstance,
                    loadUrlParams,
                    parentTabId,
                    /* otherActivity= */ null,
                    preferNew);
            return;
        }

        showTargetSelectorDialog(
                (instanceInfo) -> {
                    ChromeTabbedActivity selectedActivity =
                            (ChromeTabbedActivity)
                                    MultiWindowUtils.getActivityById(instanceInfo.instanceId);
                    launchUrlInOtherWindow(
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

    @VisibleForTesting
    void showTargetSelectorDialog(
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

    private void launchUrlInOtherWindow(
            boolean isIncognito,
            LoadUrlParams loadUrlParams,
            int parentId,
            @Nullable Activity otherActivity,
            boolean preferNew) {
        ChromeAsyncTabLauncher chromeAsyncTabLauncher = new ChromeAsyncTabLauncher(isIncognito);
        chromeAsyncTabLauncher.launchTabInOtherWindow(
                loadUrlParams,
                mActivity,
                parentId,
                otherActivity,
                NewWindowAppSource.URL_LAUNCH,
                preferNew);
    }

    @Override
    public @Nullable Intent createNewWindowIntent(
            boolean isIncognito, @NewWindowAppSource int source) {
        boolean openAdjacently =
                (mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()
                                || mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                                || mMultiWindowModeStateDispatcher.isInMultiDisplayMode())
                        && MultiWindowUtils.shouldOpenInAdjacentWindow(mActivity);
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        /* windowId= */ INVALID_WINDOW_ID,
                        /* preferNew= */ true,
                        openAdjacently,
                        source);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, isIncognito);
        return intent;
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
        for (int i : getPersistedInstanceIds(persistedInstanceType)) {
            if (!includeDeleted && MultiInstancePersistentStore.readMarkedForDeletion(i)) {
                continue;
            }
            @InstanceInfo.Type int type = InstanceInfo.Type.OTHER;
            Activity a = MultiWindowUtils.getActivityById(i);
            int persistedTaskId = MultiInstancePersistentStore.readTaskId(i);
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

            long lastAccessedTime = MultiInstancePersistentStore.readLastAccessedTime(i);
            // It is generally assumed and expected that the last-accessed time for the current
            // activity is already updated to a "current" time when this method is called. However,
            // we will avoid closing the current instance explicitly to avoid an unexpected outcome
            // if this is not the case.
            if (isOlderThanSixMonths(lastAccessedTime) && type != InstanceInfo.Type.CURRENT) {
                closeWindows(
                        Collections.singletonList(i),
                        CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION);
                continue;
            }
            result.add(
                    new InstanceInfo(
                            i,
                            persistedTaskId,
                            type,
                            assumeNonNull(MultiInstancePersistentStore.readActiveTabUrl(i)),
                            assumeNonNull(MultiInstancePersistentStore.readActiveTabTitle(i)),
                            MultiInstancePersistentStore.readCustomTitle(i),
                            MultiInstancePersistentStore.readNormalTabCount(i),
                            MultiInstancePersistentStore.readIncognitoTabCount(i),
                            MultiInstancePersistentStore.readIncognitoSelected(i),
                            lastAccessedTime,
                            MultiInstancePersistentStore.readClosureTime(i)));
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
            // windows race to be restored near the limit (for eg. as a result of keyboard presses
            // in quick succession). This is valid when only active instances contribute to the
            // instance limit, which is the case when Robust Window Management is enabled. Otherwise
            // we cannot return an invalid id, because we want to allocate a valid id for an
            // inactive instance that is being restored, when limit includes both instance types.
            if (isInstanceLimitReached() && UiUtils.isRobustWindowManagementEnabled()) {
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
            if (!isInstanceLimitReached()) {
                for (int i = 0; i < TabWindowManager.MAX_SELECTORS_1000; ++i) {
                    if (!MultiInstancePersistentStore.hasInstance(i)) {
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
        for (int i = 0; i < getMaxInstances(); ++i) {
            int persistedTaskId = MultiInstancePersistentStore.readTaskId(i);
            if (persistedTaskId != INVALID_TASK_ID) {
                continue;
            }
            if (MultiInstancePersistentStore.readMarkedForDeletion(i)) {
                continue;
            }
            if (id == INVALID_WINDOW_ID
                    || MultiInstancePersistentStore.readLastAccessedTime(i)
                            > MultiInstancePersistentStore.readLastAccessedTime(id)) {
                // Last accessed time equals to 0 means the corresponding persistent state does not
                // exist. The profile type check should only be enforced when restoring from
                // persistent state.
                // TODO(crbug.com/456289090): Handle the scenario where we are at instance limit
                // (with all non-REGULAR windows) with no live activities and a new REGULAR window
                // is attempted to be created from the launcher.
                // TODO(crbug.com/458129266): Rely on profile exists check instead of feature flag 6
                // months post launch.
                if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                        && MultiInstancePersistentStore.readLastAccessedTime(i) != 0
                        && MultiInstancePersistentStore.readProfileType(i)
                                != (isIncognitoIntent
                                        ? SupportedProfileType.OFF_THE_RECORD
                                        : SupportedProfileType.REGULAR)) {
                    continue;
                }
                id = i;
                newInstanceIdAllocated = !MultiInstancePersistentStore.hasInstance(i);
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

            int persistedProfileType = MultiInstancePersistentStore.readProfileType(windowId);
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
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        // Return early if an instance limit downgrade has been handled previously. This is to avoid
        // a case where we end up replacing an active instance with a newly created activity (by
        // finishing the task for the former) when max instances are open.
        if (prefs.readBoolean(
                ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED, false)) {
            return;
        }

        Set<Integer> activeInstanceIds = getPersistedInstanceIds(PersistedInstanceType.ACTIVE);
        // This method is called before instanceId allocation for the currently starting activity.
        // getPersistedInstanceIds() does not account for this activity since it does not have an
        // associated persisted task state yet. Increment |numTasksToFinish| by 1 to account for
        // this activity in the total active instance count.
        int numTasksToFinish = activeInstanceIds.size() - MultiWindowUtils.getMaxInstances() + 1;
        if (numTasksToFinish <= 0) return;

        prefs.writeBoolean(
                ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED, true);

        // Get the instance ids of up to |numTasksToFinish| least recently used instances.
        TreeMap<Long, Integer> lruInstanceIds = new TreeMap<>();
        for (int i : activeInstanceIds) {
            if (MultiInstancePersistentStore.readTaskId(i) == INVALID_TASK_ID) continue;
            long lastAccessedTime = MultiInstancePersistentStore.readLastAccessedTime(i);
            lruInstanceIds.put(lastAccessedTime, i);
            if (lruInstanceIds.size() > numTasksToFinish) {
                lruInstanceIds.remove(lruInstanceIds.lastKey());
            }
        }

        // Determine the active tasks that need to be finished.
        Map<Integer, Integer> tasksToDelete = new HashMap<>();
        for (Integer i : lruInstanceIds.values()) {
            tasksToDelete.put(MultiInstancePersistentStore.readTaskId(i), i);
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
                MultiInstancePersistentStore.removeTaskId(instanceId);
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
    public void initialize(
            int instanceId,
            int taskId,
            @SupportedProfileType int profileType,
            UnownedUserDataHost host) {
        super.initialize(instanceId, taskId, profileType, host);
        mInstanceId = instanceId;
        MultiInstancePersistentStore.writeTaskId(instanceId, taskId);
        MultiInstancePersistentStore.writeProfileType(instanceId, profileType);
        MultiInstancePersistentStore.writeMarkedForDeletion(
                instanceId, /* markedForDeletion= */ false);
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
                            MultiInstancePersistentStore.writeIncognitoSelected(
                                    mInstanceId, mActiveTab.isIncognito());
                            // When an incognito tab is focused, keep the normal active tab info.
                            Tab urlTab =
                                    mActiveTab.isIncognito()
                                            ? TabModelUtils.getCurrentTab(selector.getModel(false))
                                            : mActiveTab;
                            if (urlTab != null) {
                                MultiInstancePersistentStore.writeActiveTabUrl(
                                        mInstanceId, urlTab.getOriginalUrl().getSpec());
                                MultiInstancePersistentStore.writeActiveTabTitle(
                                        mInstanceId, urlTab.getTitle());
                            } else {
                                MultiInstancePersistentStore.writeActiveTabUrl(
                                        mInstanceId, EMPTY_DATA);
                                MultiInstancePersistentStore.writeActiveTabTitle(
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

    /**
     * Gets instance ids filtered by one or more specified {@link PersistedInstanceType}s. To get
     * all persisted ids irrespective of type, use {@link PersistedInstanceType.ANY}.
     *
     * @param type A bit-int representing one or more {@link PersistedInstanceType}s.
     * @return A set of instance ids of the specified {@code type}.
     */
    static Set<Integer> getPersistedInstanceIds(int type) {
        Context context = ContextUtils.getApplicationContext();
        Set<Integer> activeTaskIds = getAllAppTaskIds(context);

        Set<Integer> allIds = MultiInstancePersistentStore.readAllInstanceIds();
        if (type == PersistedInstanceType.ANY) return allIds;

        Set<Integer> filteredIds = new HashSet<>();
        boolean includeOtr = (type & PersistedInstanceType.OFF_THE_RECORD) != 0;
        boolean includeRegular = (type & PersistedInstanceType.REGULAR) != 0;
        boolean includeActive = (type & PersistedInstanceType.ACTIVE) != 0;
        boolean includeInactive = (type & PersistedInstanceType.INACTIVE) != 0;
        assert !includeActive || !includeInactive
                : "To filter both ACTIVE and INACTIVE instance types, use"
                        + " PersistedInstanceType.ANY.";
        for (Integer id : allIds) {
            int persistedTaskId = MultiInstancePersistentStore.readTaskId(id);

            // Exclude ids not satisfying requirements.
            int profileType = MultiInstancePersistentStore.readProfileType(id);
            if (includeOtr && profileType != SupportedProfileType.OFF_THE_RECORD) continue;
            if (includeRegular && profileType != SupportedProfileType.REGULAR) continue;
            if (includeActive && !activeTaskIds.contains(persistedTaskId)) continue;
            if (includeInactive && activeTaskIds.contains(persistedTaskId)) continue;

            filteredIds.add(id);
        }
        return filteredIds;
    }

    static Set<Integer> getAllPersistedInstanceIds() {
        return getPersistedInstanceIds(PersistedInstanceType.ANY);
    }

    private void removeInvalidInstanceData() {
        // Update persisted task state based on current AppTasks.
        Set<Integer> appTaskIds = getAllAppTaskIds(mActivity);
        Map<String, Integer> taskMap = MultiInstancePersistentStore.readTaskMap();
        List<String> tasksRemoved = new ArrayList<>();
        for (Map.Entry<String, Integer> entry : taskMap.entrySet()) {
            if (!appTaskIds.contains(entry.getValue())) {
                tasksRemoved.add(entry.getKey() + " - " + entry.getValue());
                ChromeSharedPreferences.getInstance().removeKey(entry.getKey());
            }
        }

        List<Integer> instancesRemoved = new ArrayList<>();
        // Remove persistent data for unrecoverable instances.
        for (int i : getAllPersistedInstanceIds()) {
            if (!MultiWindowUtils.isRestorableInstance(i)) {
                instancesRemoved.add(i);
                // An instance with no live task is deleted if it has no tabs.
                removeInstanceInfo(i, CloseWindowAppSource.NO_TABS_IN_WINDOW);
            }
        }

        if (!tasksRemoved.isEmpty() || !instancesRemoved.isEmpty()) {
            Log.i(
                    TAG_MULTI_INSTANCE,
                    "Removed invalid instance data. Removed tasks-instance mappings: "
                            + tasksRemoved
                            + " and shared prefs for instances: "
                            + instancesRemoved);
        }
    }

    @VisibleForTesting
    protected static List<Activity> getAllRunningActivities() {
        return ApplicationStatus.getRunningActivities();
    }

    private static Set<Integer> getAllAppTaskIds(Context context) {
        if (sAppTaskIdsForTesting != null) {
            return sAppTaskIdsForTesting;
        }

        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<AppTask> appTasks = activityManager.getAppTasks();
        Set<Integer> results = new HashSet<>();
        for (AppTask task : appTasks) {
            ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (info != null) results.add(info.taskId);
        }
        return results;
    }

    static void setAppTaskIdsForTesting(Set<Integer> appTaskIds) {
        sAppTaskIdsForTesting = appTaskIds;
        ResettersForTesting.register(() -> sAppTaskIdsForTesting = null);
    }

    private int getInstanceByTask(int taskId) {
        for (int i : getAllPersistedInstanceIds()) {
            if (taskId == MultiInstancePersistentStore.readTaskId(i)) return i;
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
                getRunningTabbedActivityCount(),
                TabWindowManager.MAX_SELECTORS_1000 + 1);
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    "Android.MultiInstance.NumActivities.Incognito",
                    MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ true),
                    TabWindowManager.MAX_SELECTORS_1000 + 1);
        }
    }

    static int getRunningTabbedActivityCount() {
        int numActivities = 0;
        List<Activity> activities = getAllRunningActivities();
        for (Activity activity : activities) {
            if (activity instanceof ChromeTabbedActivity) numActivities++;
        }
        return numActivities;
    }

    private void recordInstanceCountHistogram() {
        // Ensure we have instance info entry for the current one.
        MultiInstancePersistentStore.writeLastAccessedTime(mInstanceId);

        RecordHistogram.recordExactLinearHistogram(
                "Android.MultiInstance.NumInstances",
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY),
                TabWindowManager.MAX_SELECTORS_1000 + 1);

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    "Android.MultiInstance.NumInstances.Incognito",
                    MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ false),
                    TabWindowManager.MAX_SELECTORS_1000 + 1);
        }
    }

    private static void writeTabCount(int index, TabModelSelector selector) {
        if (!selector.isTabStateInitialized()) return;
        int tabCount = selector.getModel(false).getCount();
        int incognitoTabCount = selector.getModel(true).getCount();
        // TODO (crbug.com/466168444): Explore extracting tab count from TabModelSelector for both
        // active and inactive instances given that we support headless tab models now.
        MultiInstancePersistentStore.writeTabCount(index, tabCount, incognitoTabCount);
        if (tabCount == 0) {
            MultiInstancePersistentStore.writeActiveTabUrl(index, EMPTY_DATA);
            MultiInstancePersistentStore.writeActiveTabTitle(index, EMPTY_DATA);
        }
    }

    /**
     * Launch an intent in another window. It is unknown to our caller if the other window currently
     * has a live task associated with it. This method will attempt to discern this and take the
     * appropriate action.
     *
     * @param context The context used to launch the intent.
     * @param intent The intent to launch.
     * @param windowId The id to identify the target window/activity.
     */
    static void launchIntentInUnknown(Context context, Intent intent, @WindowId int windowId) {
        // TODO(https://crbug.com/415375532): Remove the need for this to be a public method, and
        // fold all of this functionality into a shared single public method with
        // #launchIntentInInstance.

        if (MultiWindowUtils.launchIntentInInstance(intent, windowId)) return;

        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, windowId);
        IntentUtils.safeStartActivity(context, intent);
    }

    @Override
    public void openWindow(int instanceId, @NewWindowAppSource int source) {
        Set<Integer> activeTaskIds = getAllAppTaskIds(mActivity);
        int persistedTaskId = MultiInstancePersistentStore.readTaskId(instanceId);
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

        boolean openAdjacently = MultiWindowUtils.shouldOpenInAdjacentWindow(mActivity);
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity, instanceId, /* preferNew= */ false, openAdjacently, source);
        MultiInstancePersistentStore.writeMarkedForDeletion(
                instanceId, /* markedForDeletion= */ false);
        mActivity.startActivity(intent);

        // If a new activity was started, it implies that an inactive instance was restored.
        if (UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) {
            RecentlyClosedEntriesManagerTrackerFactory.getInstance().onInstanceRestored(instanceId);
        }

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
            MultiInstancePersistentStore.writeMarkedForDeletion(
                    instanceId, /* markedForDeletion= */ true);
            MultiInstancePersistentStore.writeClosureTime(instanceId);
            MultiInstancePersistentStore.removeTaskId(instanceId);
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
        if (!UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) return;

        // Note that instance state (for e.g. taskId) may not be updated if a live activity for the
        // closed instance was finished, because activity destruction is asynchronous.
        // We will create an InstanceInfo synchronously with adequate information about the closed
        // instance, without relying on completion of an asynchronous activity destruction that may
        // be initiated during this time.
        List<InstanceInfo> instanceInfoList = new ArrayList();
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
                                    MultiInstancePersistentStore.readActiveTabUrl(instanceId)),
                            assumeNonNull(
                                    MultiInstancePersistentStore.readActiveTabTitle(instanceId)),
                            MultiInstancePersistentStore.readCustomTitle(instanceId),
                            MultiInstancePersistentStore.readNormalTabCount(instanceId),
                            MultiInstancePersistentStore.readIncognitoTabCount(instanceId),
                            MultiInstancePersistentStore.readIncognitoSelected(instanceId),
                            MultiInstancePersistentStore.readLastAccessedTime(instanceId),
                            MultiInstancePersistentStore.readClosureTime(instanceId));
            instanceInfoList.add(instanceInfo);
        }

        if (instanceInfoList.size() > 0) {
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
        if (!UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) return true;

        return source != CloseWindowAppSource.WINDOW_MANAGER;
    }

    private static boolean hasRestorableRegularTabs(int instanceId) {
        int normalTabCount = MultiInstancePersistentStore.readNormalTabCount(instanceId);

        if (normalTabCount > 1) return true;
        if (normalTabCount == 0) return false;

        String activeUrl = MultiInstancePersistentStore.readActiveTabUrl(instanceId);
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
        // Window manager UI, and the task is removed by system. See https://crbug.com/1241719.
        removeInvalidInstanceData();

        // Activity#isFinishing() is true in case of explicit user intent, for eg. task swipe up
        // from Android Recents or app trigger, for eg. programmatically invoking #finish() on the
        // activity. When the activity gets destroyed by the system in the background while keeping
        // its task alive, we don't want such closure to be reflected on Recent Tabs because an
        // instance with a live task is still considered active. Therefore, we will notify Recent
        // Tabs of activity destruction only if the activity is finishing, with the caveat that a
        // subsequent task kill will also not be reflected as an instance closure until the Recent
        // Tabs page is reopened.
        if (UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) {
            boolean isPermanentDeletion = !hasRestorableRegularTabs(mInstanceId);

            if (!isPermanentDeletion) {
                MultiInstancePersistentStore.writeClosureTime(mInstanceId);
            }

            if (mActivity.isFinishing()) {
                // Notify Recent Tabs page that the instance is closing.
                notifyInstancesClosed(Collections.singletonList(mInstanceId), isPermanentDeletion);
            }
        }

        if (mInstanceId != INVALID_WINDOW_ID) {
            ApplicationStatus.unregisterActivityStateListener(this);
        }
        if (sState != null) {
            List<Activity> activities = getAllRunningActivities();
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
    static void removeInstanceInfo(int index, @CloseWindowAppSource int source) {
        MultiInstancePersistentStore.deleteInstanceState(index);

        RecordHistogram.recordEnumeratedHistogram(
                CLOSE_WINDOW_APP_SOURCE_HISTOGRAM, source, CloseWindowAppSource.NUM_ENTRIES);
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        super.onTopResumedActivityChanged(isTopResumedActivity);
        if (isTopResumedActivity) {
            MultiInstancePersistentStore.writeLastAccessedTime(mInstanceId);
        }
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        // We persist last closed time when the activity is stopped as a fallback for when
        // #onDestroy() is not called for a finishing activity.
        if (UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) {
            MultiInstancePersistentStore.writeClosureTime(mInstanceId);
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

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        // Check the max instance count in a day for every state update if needed.
        long timestamp = prefs.readLong(ChromePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, 0);
        int maxCount = prefs.readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, 0);
        int maxActiveCount =
                prefs.readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT, 0);
        int incognitoMaxCount =
                prefs.readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO, 0);
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
            prefs.writeLong(ChromePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, current);
            // Reset the count to 0 to be ready to obtain the max count for the next 24-hour period.
            maxCount = 0;
            maxActiveCount = 0;
            incognitoMaxCount = 0;
        }
        int instanceCount =
                MultiWindowUtils.getInstanceCountWithFallback(
                        MultiInstanceManager.PersistedInstanceType.ANY);
        int incognitoInstanceCount =
                MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ false);
        if (instanceCount > maxCount) {
            prefs.writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, instanceCount);
        }
        int activeInstanceCount =
                MultiWindowUtils.getInstanceCountWithFallback(
                        MultiInstanceManager.PersistedInstanceType.ACTIVE);
        if (activeInstanceCount > maxActiveCount) {
            prefs.writeInt(
                    ChromePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT,
                    activeInstanceCount);
        }
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                && incognitoInstanceCount > incognitoMaxCount) {
            prefs.writeInt(
                    ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO,
                    incognitoInstanceCount);
        }
    }

    private void onMultiInstanceStateChanged(boolean inMultiInstanceMode) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        long startTime = prefs.readLong(ChromePreferenceKeys.MULTI_INSTANCE_START_TIME);
        long current = System.currentTimeMillis();

        // This method in invoked for every ChromeActivity instance. Logging metrics for the first
        // ChromeActivity is enough. The pref |MULTI_INSTANCE_START_TIME| is set to non-zero once
        // Android.MultiInstance.Enter is logged, and reset to zero after
        // Android.MultiInstance.Exit to avoid duplicated logging.
        if (startTime == 0 && inMultiInstanceMode) {
            RecordUserAction.record("Android.MultiInstance.Enter");
            prefs.writeLong(ChromePreferenceKeys.MULTI_INSTANCE_START_TIME, current);
        } else if (startTime != 0 && !inMultiInstanceMode) {
            RecordUserAction.record("Android.MultiInstance.Exit");
            RecordHistogram.recordLongTimesHistogram(
                    "Android.MultiInstance.TotalDuration", current - startTime);
            prefs.writeLong(ChromePreferenceKeys.MULTI_INSTANCE_START_TIME, 0);
        }
    }

    @Override
    public void moveTabGroupToNewWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source) {
        boolean openAdjacently = MultiWindowUtils.shouldOpenInAdjacentWindow(mActivity);
        if (isInstanceLimitReached()) {
            showInstanceCreationLimitMessage();
        } else {
            mTabReparentingDelegate.reparentTabGroupToNewWindow(
                    tabGroupMetadata, INVALID_WINDOW_ID, openAdjacently, source);
        }
    }

    @Override
    public void moveTabGroupToWindowByIdChecked(
            int destWindowId, TabGroupMetadata tabGroupMetadata, int destTabIndex) {
        Activity destActivity = MultiWindowUtils.getActivityById(destWindowId);
        if (destActivity != null) {
            mTabReparentingDelegate.reparentTabGroupToExistingWindow(
                    (ChromeTabbedActivity) destActivity, tabGroupMetadata, destTabIndex);
        } else {
            mTabReparentingDelegate.reparentTabGroupToNewWindow(
                    tabGroupMetadata,
                    destWindowId,
                    /* openAdjacently= */ true,
                    NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY);
        }
    }

    @Override
    public void moveTabGroupToOtherWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source) {
        // Check the number of instances that the tab group is able to move into.
        int instanceCount =
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE);
        @PersistedInstanceType int instanceType = PersistedInstanceType.ANY;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            if (tabGroupMetadata.isIncognito) {
                instanceCount = MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ true);
                instanceType = PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD;
            } else {
                instanceCount =
                        MultiWindowUtils.getInstanceCountWithFallback(
                                PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR);
                instanceType = PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR;
            }
        }

        if (instanceCount <= 1) {
            moveTabGroupToNewWindow(tabGroupMetadata, source);
            return;
        }

        showTargetSelectorDialog(
                (instanceInfo) -> {
                    moveTabGroupToWindowByIdChecked(
                            instanceInfo.instanceId, tabGroupMetadata, TabList.INVALID_TAB_INDEX);

                    // Close the source instance window, if needed.
                    closeChromeWindowIfEmpty(mInstanceId);
                },
                instanceType,
                R.string.menu_move_group_to_other_window);
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
    void cleanupSyncedTabGroupsIfLastInstance() {
        Set<Integer> info = getPersistedInstanceIds(PersistedInstanceType.ANY);
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

    private boolean isInstanceLimitReached() {
        int instanceCount =
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE);
        // TODO (crbug.com/460800897): Update conditional logic for opening URLs in other windows.
        return instanceCount >= getMaxInstances();
    }

    private int getMaxInstances() {
        return Objects.requireNonNullElse(MultiWindowUtils.sMaxInstancesForTesting, mMaxInstances);
    }

    @Override
    public boolean showInstanceRestorationMessage() {
        return MultiWindowUtils.maybeShowInstanceRestorationMessage(
                getMessageDispatcher(), mActivity, this::showInstanceSwitcherDialog);
    }

    @Override
    public void showInstanceCreationLimitMessage() {
        MultiWindowUtils.showInstanceCreationLimitMessage(
                getMessageDispatcher(), mActivity, this::showInstanceSwitcherDialog);
    }

    @VisibleForTesting
    @Nullable MessageDispatcher getMessageDispatcher() {
        if (mActiveTab == null) return null;
        return MessageDispatcherProvider.from(mActiveTab.getWindowAndroid());
    }

    @Override
    public void showNameWindowDialog(@NameWindowDialogSource int source) {
        String customTitle = MultiInstancePersistentStore.readCustomTitle(mInstanceId);
        String defaultTitle = MultiInstancePersistentStore.readActiveTabTitle(mInstanceId);
        String currentTitle = TextUtils.isEmpty(customTitle) ? defaultTitle : customTitle;

        UiUtils.showNameWindowDialog(
                mActivity,
                assumeNonNull(currentTitle),
                newTitle -> renameInstance(mInstanceId, newTitle),
                source);
    }
}
