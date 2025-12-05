// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.isRestorableInstance;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabGroupTask;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabsTask;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceState.MultiInstanceStateObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils.InstanceAllocationType;
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
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

@NullMarked
class MultiInstanceManagerApi31 extends MultiInstanceManagerImpl implements ActivityStateListener {
    private static final String TAG = "MIMApi31";
    private static final String TAG_MULTI_INSTANCE = "MultiInstance";
    /* package */ static final long SIX_MONTHS_MS = TimeUnit.DAYS.toMillis(6 * 30);
    private static final String EMPTY_DATA = "";
    private static @Nullable MultiInstanceState sState;

    @VisibleForTesting protected final int mMaxInstances;

    private final ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    // Instance ID for the activity associated with this manager.
    private int mInstanceId = INVALID_WINDOW_ID;

    private @Nullable Tab mActiveTab;
    private final TabObserver mActiveTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onTitleUpdated(Tab tab) {
                    if (!tab.isIncognito()) writeTitle(mInstanceId, tab);
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    if (!tab.isIncognito()) writeUrl(mInstanceId, tab);
                }
            };

    private final Supplier<DesktopWindowStateManager> mDesktopWindowStateManagerSupplier;
    private final MultiInstanceStateObserver mOnMultiInstanceStateChanged;
    private final Callback<Pair<Integer, String>> mRenameCallback;

    private static @Nullable Set<Integer> sAppTaskIdsForTesting;

    MultiInstanceManagerApi31(
            Activity activity,
            ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
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
        mRenameCallback =
                (pair) -> {
                    writeCustomTitle(pair.first, pair.second);
                };

        // Check if instance limit has changed and update SharedPrefs.
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        int prevInstanceLimit =
                prefs.readInt(
                        ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, mMaxInstances);
        if (mMaxInstances > prevInstanceLimit) {
            // Reset SharedPrefs for instance limit downgrade if limit has increased.
            prefs.writeBoolean(
                    ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED, false);
            prefs.writeBoolean(
                    ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN, false);
        }
        prefs.writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, mMaxInstances);
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
        List<InstanceInfo> info = getInstanceInfo();
        boolean isIncognitoWindow =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        && mActivity instanceof ChromeTabbedActivity
                        && ((ChromeTabbedActivity) mActivity).isIncognitoWindow();
        InstanceSwitcherCoordinator.showDialog(
                mActivity,
                mModalDialogManagerSupplier.get(),
                new LargeIconBridge(getProfile()),
                (item) ->
                        openInstance(
                                item.instanceId, item.taskId, NewWindowAppSource.WINDOW_MANAGER),
                (item) -> {
                    RecordUserAction.record("MobileMenuWindowManagerCloseInstance");
                    closeWindow(item.instanceId, CloseWindowAppSource.WINDOW_MANAGER);
                    if (getCurrentInstanceId() != item.instanceId) {
                        // Initiate synced tab groups cleanup only if the closed instance is not the
                        // current one. If after closure of the current, second to last instance, a
                        // single instance remains, this cleanup will be initiated on activity
                        // startup of that instance.
                        cleanupSyncedTabGroupsIfLastInstance();
                    }
                },
                mRenameCallback,
                () ->
                        openNewWindow(
                                "Android.WindowManager.NewWindow",
                                isIncognitoWindow,
                                NewWindowAppSource.WINDOW_MANAGER),
                MultiWindowUtils.getMaxInstances(),
                info,
                isIncognitoWindow);
    }

    Callback<Pair<Integer, String>> getRenameCallbackForTesting() {
        return mRenameCallback;
    }

    @Override
    public void moveTabsToOtherWindow(List<Tab> tabs, @NewWindowAppSource int source) {
        // TODO(crbug.com/465141949): Add unit tests.
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
            moveTabsToNewWindow(tabs, source);

            // Close the source instance window, if needed.
            closeChromeWindowIfEmpty(mInstanceId);
            return;
        }

        TargetSelectorCoordinator.showDialog(
                mActivity,
                mModalDialogManagerSupplier.get(),
                new LargeIconBridge(getProfile()),
                (instanceInfo) -> {
                    moveTabsToWindow(instanceInfo, tabs, TabList.INVALID_TAB_INDEX, source);
                    // Close the source instance window, if needed.
                    closeChromeWindowIfEmpty(mInstanceId);
                },
                getInstanceInfo(instanceType),
                UiUtils.isInstanceSwitcherV2Enabled()
                        ? R.string.menu_move_tab_to_other_window
                        : R.string.menu_move_to_other_window);
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
    // TODO (crbug.com/460800897): Update conditional logic for opening URLs in other windows.
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
            if (preferNew
                    && mMaxInstances
                            <= MultiWindowUtils.getInstanceCountWithFallback(
                                    PersistedInstanceType.ACTIVE)) {
                assumeNonNull(mActiveTab);
                showInstanceCreationLimitMessage(
                        MessageDispatcherProvider.from(mActiveTab.getWindowAndroid()));
                return;
            }

            ChromeAsyncTabLauncher chromeAsyncTabLauncher =
                    new ChromeAsyncTabLauncher(incognitoInstance);
            // TODO(crbug.com/458761856): Determine the correct NewWindowAppSource instead of assume
            // its always NewWindowAppSource.OTHER.
            chromeAsyncTabLauncher.launchTabInOtherWindow(
                    loadUrlParams,
                    mActivity,
                    parentTabId,
                    /* otherActivity= */ null,
                    NewWindowAppSource.OTHER,
                    preferNew);
            return;
        }

        TargetSelectorCoordinator.showDialog(
                mActivity,
                mModalDialogManagerSupplier.get(),
                new LargeIconBridge(getProfile()),
                (instanceInfo) -> {
                    ChromeTabbedActivity selectedActivity =
                            (ChromeTabbedActivity) getActivityById(instanceInfo.instanceId);
                    ChromeAsyncTabLauncher chromeAsyncTabLauncher =
                            new ChromeAsyncTabLauncher(
                                    selectedActivity != null
                                            && selectedActivity.isIncognitoWindow());
                    chromeAsyncTabLauncher.launchTabInOtherWindow(
                            loadUrlParams,
                            mActivity,
                            parentTabId,
                            selectedActivity,
                            NewWindowAppSource.OTHER,
                            preferNew);
                },
                getInstanceInfo(instanceType),
                R.string.contextmenu_open_in_other_window);
    }

    @Override
    public void moveTabGroupToOtherWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source) {
        // TODO(crbug.com/465141949): Add unit tests.
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

        TargetSelectorCoordinator.showDialog(
                mActivity,
                mModalDialogManagerSupplier.get(),
                new LargeIconBridge(getProfile()),
                (instanceInfo) -> {
                    moveTabGroupToWindow(
                            instanceInfo, tabGroupMetadata, TabList.INVALID_TAB_INDEX, source);

                    // Close the source instance window, if needed.
                    closeChromeWindowIfEmpty(mInstanceId);
                },
                getInstanceInfo(instanceType),
                UiUtils.isInstanceSwitcherV2Enabled()
                        ? R.string.menu_move_group_to_other_window
                        : R.string.menu_move_to_other_window);
    }

    @Override
    public void moveTabsToWindow(
            InstanceInfo info, List<Tab> tabs, int tabAtIndex, @NewWindowAppSource int source) {
        Activity targetActivity = getActivityById(info.instanceId);
        if (targetActivity != null) {
            reparentTabsToRunningActivity((ChromeTabbedActivity) targetActivity, tabs, tabAtIndex);
        } else {
            TabModelSelector selector =
                    TabWindowManagerSingleton.getInstance()
                            .getTabModelSelectorById(getCurrentInstanceId());
            // If the source Chrome instance still has tabs (including incognito), allow
            // launching the new window adjacently. Otherwise, skip
            // FLAG_ACTIVITY_LAUNCH_ADJACENT to avoid a black screen caused by the source
            // window closing before the new one launches.
            boolean openAdjacently = assumeNonNull(selector).getTotalTabCount() > 1;
            moveAndReparentTabsToNewWindow(
                    tabs,
                    info.instanceId,
                    /* preferNew= */ false,
                    openAdjacently,
                    /* addTrustedIntentExtras= */ true,
                    source);
        }
    }

    @Override
    public void moveTabsToWindowAndMergeToDest(InstanceInfo info, List<Tab> tabs, int destTabId) {
        Activity targetActivity = getActivityById(info.instanceId);
        if (BuildConfig.ENABLE_ASSERTS) {
            for (Tab tab : tabs) {
                assert tab.getTabGroupId() == null : "Tab should not be part of a group.";
            }
        }
        if (targetActivity != null) {
            reparentTabsToRunningActivityAndMergeToDest(
                    (ChromeTabbedActivity) targetActivity, tabs, destTabId);
        } else {
            assert false : "Target activity is null";
        }
    }

    @Override
    public void moveTabGroupToWindow(
            InstanceInfo info,
            TabGroupMetadata tabGroupMetadata,
            int startIndex,
            @NewWindowAppSource int source) {
        Activity targetActivity = getActivityById(info.instanceId);
        if (targetActivity != null) {
            reparentTabGroupToRunningActivity(
                    (ChromeTabbedActivity) targetActivity, tabGroupMetadata, startIndex);
        } else {
            moveAndReparentTabGroupToNewWindow(
                    tabGroupMetadata,
                    info.instanceId,
                    /* preferNew= */ false,
                    /* openAdjacently= */ true,
                    /* addTrustedIntentExtras= */ true,
                    source);
        }
    }

    @VisibleForTesting
    void moveAndReparentTabsToNewWindow(
            List<Tab> tabs,
            int instanceId,
            boolean preferNew,
            boolean openAdjacently,
            boolean addTrustedIntentExtras,
            @NewWindowAppSource int source) {
        onMultiInstanceModeStarted();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        instanceId,
                        preferNew,
                        openAdjacently,
                        addTrustedIntentExtras,
                        source);
        beginReparentingTabs(
                tabs, intent, /* startActivityOptions= */ null, /* finalizeCallback= */ null);
    }

    @VisibleForTesting
    void moveAndReparentTabGroupToNewWindow(
            TabGroupMetadata tabGroupMetadata,
            int instanceId,
            boolean preferNew,
            boolean openAdjacently,
            boolean addTrustedIntentExtras,
            @NewWindowAppSource int source) {
        onMultiInstanceModeStarted();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        instanceId,
                        preferNew,
                        openAdjacently,
                        addTrustedIntentExtras,
                        source);
        beginReparentingTabGroup(tabGroupMetadata, intent);
    }

    @VisibleForTesting
    void reparentTabsToRunningActivity(
            ChromeTabbedActivity targetActivity, List<Tab> tabs, int tabAtIndex) {
        Intent intent = createIntentForGeneralReparenting(targetActivity, tabAtIndex);
        setupIntentForTabsReparenting(tabs, intent, null);

        targetActivity.onNewIntent(intent);
        bringTaskForeground(targetActivity.getTaskId());
    }

    @VisibleForTesting
    void reparentTabsToRunningActivityAndMergeToDest(
            ChromeTabbedActivity targetActivity, List<Tab> tabs, int destTabId) {
        Intent intent =
                createIntentForGeneralReparenting(targetActivity, TabList.INVALID_TAB_INDEX);
        setupIntentForTabsReparenting(tabs, intent, null);
        IntentHandler.setDestTabId(intent, destTabId);

        targetActivity.onNewIntent(intent);
        bringTaskForeground(targetActivity.getTaskId());
    }

    @VisibleForTesting
    void reparentTabGroupToRunningActivity(
            ChromeTabbedActivity targetActivity,
            TabGroupMetadata tabGroupMetadata,
            int tabAtIndex) {
        long startTime = SystemClock.elapsedRealtime();
        // 1. Pause the relevant observers prior to detaching the grouped tabs.
        pauseObserversForGroupReparenting(tabGroupMetadata);

        // 2. Setup the re-parenting intent, detaching the grouped tabs from the current activity.
        Intent intent = createIntentForGeneralReparenting(targetActivity, tabAtIndex);
        intent.putExtra(IntentHandler.EXTRA_REPARENT_START_TIME, startTime);
        setupIntentForGroupReparenting(tabGroupMetadata, intent, null);

        // 3. Resume writes to TabPersistentStore after detaching the grouped Tabs. Don't begin
        // re-attaching the Tabs to the target activity until they have been cleared from this
        // activity's TabPersistentStore.
        TabPersistentStore tabPersistentStore =
                mTabModelOrchestratorSupplier.get().getTabPersistentStore();
        tabPersistentStore.resumeSaveTabList(
                () -> {
                    targetActivity.onNewIntent(intent);
                    bringTaskForeground(targetActivity.getTaskId());
                    // Re-enable sync service observation after re-parenting is completed to resume
                    // normal sync behavior.
                    resumeSyncService(tabGroupMetadata);
                });
    }

    private Intent createIntentForGeneralReparenting(
            ChromeTabbedActivity targetActivity, int tabAtIndex) {
        assert targetActivity != null;
        Intent intent = new Intent();
        Context appContext = ContextUtils.getApplicationContext();
        intent.setClassName(appContext, ChromeTabbedActivity.class.getName());
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(
                intent, mActivity, targetActivity.getClass());
        onMultiInstanceModeStarted();
        RecordUserAction.record("MobileMenuMoveToOtherWindow");

        if (tabAtIndex != TabList.INVALID_TAB_INDEX) {
            intent.putExtra(IntentHandler.EXTRA_TAB_INDEX, tabAtIndex);
        }
        return intent;
    }

    private void pauseObserversForGroupReparenting(TabGroupMetadata tabGroupMetadata) {
        // Temporarily disable sync service from observing local changes to prevent unintended
        // updates during tab group re-parenting.
        TabGroupSyncService syncService =
                getTabGroupSyncService(
                        tabGroupMetadata.sourceWindowId, tabGroupMetadata.isIncognito);
        setSyncServiceLocalObservationMode(syncService, /* shouldObserve= */ false);

        // Pause writes to TabPersistentStore while detaching the grouped Tabs.
        TabPersistentStore tabPersistentStore =
                mTabModelOrchestratorSupplier.get().getTabPersistentStore();
        tabPersistentStore.pauseSaveTabList();
    }

    private void resumeSyncService(TabGroupMetadata tabGroupMetadata) {
        TabGroupSyncService syncService =
                getTabGroupSyncService(
                        tabGroupMetadata.sourceWindowId, tabGroupMetadata.isIncognito);
        setSyncServiceLocalObservationMode(syncService, /* shouldObserve= */ true);
    }

    @Override
    public @Nullable Intent createNewWindowIntent(boolean isIncognito) {
        Intent intent = new Intent(mActivity, ChromeTabbedActivity.class);

        MultiWindowUtils.setOpenInOtherWindowIntentExtras(
                intent, mActivity, ChromeTabbedActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, true);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, isIncognito);
        IntentUtils.addTrustedIntentExtras(intent);
        if (mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiDisplayMode()) {
            intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }

        // Remove LAUNCH_ADJACENT flag if shouldOpenInAdjacentWindow() is false and if the Activity
        // is in a full screen window.
        if (!mActivity.isInMultiWindowMode() && !MultiWindowUtils.shouldOpenInAdjacentWindow()) {
            intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }

        return intent;
    }

    @Override
    protected void openNewWindow(
            String umaAction, boolean incognito, @NewWindowAppSource int source) {
        Intent intent = createNewWindowIntent(incognito);
        assert intent != null : "The Intent to open a new window must not be null";

        onMultiInstanceModeStarted();
        mActivity.startActivity(intent);
        Log.i(TAG_MULTI_INSTANCE, "Opening new window from action: " + umaAction);
        RecordHistogram.recordEnumeratedHistogram(
                MultiInstanceManager.NEW_WINDOW_APP_SOURCE_HISTOGRAM,
                source,
                NewWindowAppSource.NUM_ENTRIES);
        RecordUserAction.record(umaAction);
    }

    @Override
    public List<InstanceInfo> getInstanceInfo(@PersistedInstanceType int persistedInstanceType) {
        removeInvalidInstanceData(/* cleanupApplicationStatus= */ false);
        List<InstanceInfo> result = new ArrayList<>();
        SparseBooleanArray visibleTasks = MultiWindowUtils.getVisibleTasks();
        int currentItemPos = -1;
        for (int i : getPersistedInstanceIds(persistedInstanceType)) {
            @InstanceInfo.Type int type = InstanceInfo.Type.OTHER;
            Activity a = getActivityById(i);
            if (a != null) {
                // The task for the activity must match the one found in our mapping.
                int taskIdFromTaskMap = getTaskFromMap(i);
                int activityTaskId = a.getTaskId();
                String error =
                        "Invalid instance-task mapping for activity="
                                + a
                                + " with id="
                                + i
                                + ". Expected (stored) taskId="
                                + taskIdFromTaskMap
                                + ", activity's taskId="
                                + activityTaskId;
                assert taskIdFromTaskMap == activityTaskId : error;
                if (a == mActivity) {
                    type = InstanceInfo.Type.CURRENT;
                    currentItemPos = result.size();
                } else if (isRunningInAdjacentWindow(visibleTasks, a)) {
                    type = InstanceInfo.Type.ADJACENT;
                }
            }
            int taskId = getTaskFromMap(i);
            // It is generally assumed and expected that the last-accessed time for the current
            // activity is already updated to a "current" time when this method is called. However,
            // we will avoid closing the current instance explicitly to avoid an unexpected outcome
            // if this is not the case.
            if (ChromeFeatureList.sDisableInstanceLimit.isEnabled()
                    && isOlderThanSixMonths(readLastAccessedTime(i))
                    && type != InstanceInfo.Type.CURRENT) {
                closeWindow(i, CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION);
                continue;
            }
            result.add(
                    new InstanceInfo(
                            i,
                            taskId,
                            type,
                            assumeNonNull(readUrl(i)),
                            assumeNonNull(readTitle(i)),
                            readCustomTitle(i),
                            readTabCount(i),
                            readIncognitoTabCount(i),
                            readIncognitoSelected(i),
                            readLastAccessedTime(i),
                            readClosedByUser(i)));
        }
        // Move the current instance always to the top of the list for favorable display on the UI.
        // It is possible that |currentItemPos| is invalid if this method is invoked early during
        // app startup or when the current activity is being / already destroyed.
        if (currentItemPos > 0 && currentItemPos < result.size()) {
            result.add(0, result.remove(currentItemPos));
        }
        return result;
    }

    private boolean isOlderThanSixMonths(long timestampMillis) {
        return (TimeUtils.currentTimeMillis() - timestampMillis) > SIX_MONTHS_MS;
    }

    @Override
    public int getCurrentInstanceId() {
        List<InstanceInfo> allInstances = getInstanceInfo();
        if (allInstances == null || allInstances.isEmpty()) return INVALID_WINDOW_ID;
        // Current instance is at top of list.
        return allInstances.get(0).instanceId;
    }

    @VisibleForTesting
    protected boolean isRunningInAdjacentWindow(
            SparseBooleanArray visibleTasks, Activity activity) {
        assert activity != mActivity;
        return visibleTasks.get(activity.getTaskId());
    }

    @Override
    public Pair<Integer, Integer> allocInstanceId(
            int windowId, int taskId, boolean preferNew, @SupportedProfileType int profileType) {
        removeInvalidInstanceData(/* cleanupApplicationStatus= */ true);
        // Finish excess running activities / tasks after an instance limit downgrade.
        finishExcessRunningActivities();

        int instanceId = getInstanceByTask(taskId);

        // Explicitly specified window ID should be preferred. This comes from user selecting
        // a certain instance on UI when no task is present for it.
        // When out of range, ignore the ID and apply the normal allocation logic below.
        if (windowId >= 0 && instanceId == INVALID_WINDOW_ID) {
            Log.i(TAG_MULTI_INSTANCE, "Existing Instance - selected Id allocated: " + windowId);
            return Pair.create(windowId, InstanceAllocationType.EXISTING_INSTANCE_UNMAPPED_TASK);
        }

        // First, see if we have instance-task ID mapping. If we do, use the instance id. This
        // takes care of a task that had its activity destroyed and comes back to create a
        // new one. We pair them again.
        if (instanceId != INVALID_WINDOW_ID) {
            Log.i(TAG_MULTI_INSTANCE, "Existing Instance - mapped Id allocated: " + instanceId);
            return Pair.create(instanceId, InstanceAllocationType.EXISTING_INSTANCE_MAPPED_TASK);
        }

        // If asked to always create a fresh new instance, not from persistent state, do it here.
        if (preferNew) {
            // It is possible in a downgraded instance limit scenario that some or all ids of
            // persisted instances are outside the range bounded by the current |mMaxInstances|. In
            // this case, we want to avoid allocating an available id in the new range if we are at
            // or over instance limit, so that we avoid allowing successful creation of the current
            // activity in this scenario.
            if (MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE)
                    < mMaxInstances) {
                for (int i = 0; i < TabWindowManager.MAX_SELECTORS; ++i) {
                    if (!instanceEntryExists(i)) {
                        logNewInstanceId(i);
                        return Pair.create(i, InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK);
                    }
                }
            }
            return Pair.create(
                    INVALID_WINDOW_ID, InstanceAllocationType.PREFER_NEW_INVALID_INSTANCE);
        }

        // Search for an unassigned ID. The index is available for the assignment if:
        // a) there is no associated task, or
        // b) the corresponding persistent state does not exist.
        // Prefer a over b. Pick the MRU instance if there is more than one. Type b returns 0
        // for |readLastAccessedTime|, so can be regarded as the least favored.
        int id = INVALID_WINDOW_ID;
        boolean newInstanceIdAllocated = false;
        @InstanceAllocationType int allocationType = InstanceAllocationType.INVALID_INSTANCE;
        for (int i = 0; i < mMaxInstances; ++i) {
            int taskIdFromMap = getTaskFromMap(i);
            if (taskIdFromMap != INVALID_TASK_ID) {
                continue;
            }
            if (id == INVALID_WINDOW_ID || readLastAccessedTime(i) > readLastAccessedTime(id)) {
                // Last accessed time equals to 0 means the corresponding persistent state does not
                // exist. The profile type check should only be enforced when restoring from
                // persistent state.
                // TODO(crbug.com/456289090): Handle the scenario where we are at instance limit
                // (with all non-REGULAR windows) with no live activities and a new REGULAR window
                // is attempted to be created from the launcher.
                // TODO(crbug.com/458129266): Rely on profile exists check instead of feature flag 6
                // months post launch.
                if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                        && readLastAccessedTime(i) != 0
                        && readProfileType(i) != profileType) {
                    continue;
                }
                id = i;
                newInstanceIdAllocated = !instanceEntryExists(i);
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

        return Pair.create(id, allocationType);
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
        // associated task mapping yet. Increment |numTasksToFinish| by 1 to account for this
        // activity in the total active instance count.
        int numTasksToFinish = activeInstanceIds.size() - MultiWindowUtils.getMaxInstances() + 1;
        if (numTasksToFinish <= 0) return;

        prefs.writeBoolean(
                ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED, true);

        // Get the instance ids of up to |numTasksToFinish| least recently used instances.
        TreeMap<Long, Integer> lruInstanceIds = new TreeMap<>();
        for (int i : activeInstanceIds) {
            if (getTaskFromMap(i) == INVALID_TASK_ID) continue;
            long lastAccessedTime = readLastAccessedTime(i);
            lruInstanceIds.put(lastAccessedTime, i);
            if (lruInstanceIds.size() > numTasksToFinish) {
                lruInstanceIds.remove(lruInstanceIds.lastKey());
            }
        }

        // Determine the active tasks that need to be finished.
        Map<Integer, Integer> tasksToDelete = new HashMap<>();
        for (Integer i : lruInstanceIds.values()) {
            tasksToDelete.put(getTaskFromMap(i), i);
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
                ChromeSharedPreferences.getInstance().removeKey(taskMapKey(instanceId));
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
                            + (info != null ? info.id : "NOT_SET")
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
    public void initialize(int instanceId, int taskId) {
        mInstanceId = instanceId;
        updateTaskMap(instanceId, taskId);
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
        TabModelSelector selector = mTabModelOrchestratorSupplier.get().getTabModelSelector();
        assert selector != null;
        writeTabCount(mInstanceId, selector);
    }

    @VisibleForTesting
    protected void installTabModelObserver() {
        TabModelSelector selector = mTabModelOrchestratorSupplier.get().getTabModelSelector();
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
                            writeIncognitoSelected(mInstanceId, mActiveTab);
                            // When an incognito tab is focused, keep the normal active tab info.
                            Tab urlTab =
                                    mActiveTab.isIncognito()
                                            ? TabModelUtils.getCurrentTab(selector.getModel(false))
                                            : mActiveTab;
                            if (urlTab != null) {
                                writeUrl(mInstanceId, urlTab);
                                writeTitle(mInstanceId, urlTab);
                            } else {
                                writeUrl(mInstanceId, EMPTY_DATA);
                                writeTitle(mInstanceId, EMPTY_DATA);
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

    static int getTaskFromMap(int index) {
        return ChromeSharedPreferences.getInstance().readInt(taskMapKey(index), INVALID_TASK_ID);
    }

    private static String taskMapKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(String.valueOf(index));
    }

    @VisibleForTesting
    static void updateTaskMap(int instanceId, int taskId) {
        ChromeSharedPreferences.getInstance().writeInt(taskMapKey(instanceId), taskId);
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

        Set<Integer> ids = new HashSet<>();
        Map<String, Long> lastAccessedTimeMap =
                ChromeSharedPreferences.getInstance()
                        .readLongsWithPrefix(
                                ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME);
        Pattern pattern = Pattern.compile("(\\d+)$");
        boolean includeAny = type == PersistedInstanceType.ANY;
        boolean includeOtr = (type & PersistedInstanceType.OFF_THE_RECORD) != 0;
        boolean includeRegular = (type & PersistedInstanceType.REGULAR) != 0;
        boolean includeActive = (type & PersistedInstanceType.ACTIVE) != 0;
        boolean includeInactive = (type & PersistedInstanceType.INACTIVE) != 0;
        assert !includeActive || !includeInactive
                : "To filter both ACTIVE and INACTIVE instance types, use"
                        + " PersistedInstanceType.ANY.";
        for (String prefKey : lastAccessedTimeMap.keySet()) {
            Matcher matcher = pattern.matcher(prefKey);
            boolean matchFound = matcher.find();
            assert matchFound : "Key should be suffixed with the instance id.";
            int id = Integer.parseInt(matcher.group(1));

            if (includeAny) {
                ids.add(id);
                continue;
            }

            int taskIdFromMap = getTaskFromMap(id);

            // Exclude ids not satisfying requirements.
            int profileType = readProfileType(id);
            if (includeOtr && profileType != SupportedProfileType.OFF_THE_RECORD) continue;
            if (includeRegular && profileType != SupportedProfileType.REGULAR) continue;
            if (includeActive && !activeTaskIds.contains(taskIdFromMap)) continue;
            if (includeInactive && activeTaskIds.contains(taskIdFromMap)) continue;

            ids.add(id);
        }
        return ids;
    }

    static Set<Integer> getAllPersistedInstanceIds() {
        return getPersistedInstanceIds(PersistedInstanceType.ANY);
    }

    private void removeInvalidInstanceData(boolean cleanupApplicationStatus) {
        // Remove tasks that do not exist any more from the task map.
        Set<Integer> appTaskIds = getAllAppTaskIds(mActivity);
        Map<String, Integer> taskMap =
                ChromeSharedPreferences.getInstance()
                        .readIntsWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        List<String> tasksRemoved = new ArrayList<>();
        for (Map.Entry<String, Integer> entry : taskMap.entrySet()) {
            if (!appTaskIds.contains(entry.getValue())) {
                tasksRemoved.add(entry.getKey() + " - " + entry.getValue());
                ChromeSharedPreferences.getInstance().removeKey(entry.getKey());
                if (ChromeFeatureList.sMultiInstanceApplicationStatusCleanup.isEnabled()
                        && cleanupApplicationStatus) {
                    boolean foundTasks = ApplicationStatus.cleanupInvalidTask(entry.getValue());
                    if (foundTasks) {
                        if (BuildConfig.ENABLE_ASSERTS) {
                            String logMessage =
                                    "This is not a crash. Found tracked ApplicationStatus for Task "
                                            + " that no longer exists in #getAppTasks().";
                            ChromePureJavaExceptionReporter.reportJavaException(
                                    new Throwable(logMessage));
                        }
                    }
                }
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

    @VisibleForTesting
    static @Nullable Activity getActivityById(int id) {
        if (sActivitySupplierForTesting != null) {
            return sActivitySupplierForTesting.get();
        }
        TabWindowManager windowManager = TabWindowManagerSingleton.getInstance();
        for (Activity activity : getAllRunningActivities()) {
            if (id == windowManager.getIdForWindow(activity)) return activity;
        }
        return null;
    }

    private int getInstanceByTask(int taskId) {
        for (int i : getAllPersistedInstanceIds()) {
            if (taskId == getTaskFromMap(i)) return i;
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
                TabWindowManager.MAX_SELECTORS + 1);
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    "Android.MultiInstance.NumActivities.Incognito",
                    MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ true),
                    TabWindowManager.MAX_SELECTORS + 1);
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
        writeLastAccessedTime(mInstanceId);

        RecordHistogram.recordExactLinearHistogram(
                "Android.MultiInstance.NumInstances",
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY),
                TabWindowManager.MAX_SELECTORS + 1);

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RecordHistogram.recordExactLinearHistogram(
                    "Android.MultiInstance.NumInstances.Incognito",
                    MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ false),
                    TabWindowManager.MAX_SELECTORS + 1);
        }
    }

    @VisibleForTesting
    static String incognitoSelectedKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_IS_INCOGNITO_SELECTED.createKey(
                String.valueOf(index));
    }

    @VisibleForTesting
    static void writeIncognitoSelected(int index, Tab tab) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(incognitoSelectedKey(index), tab.isIncognito());
    }

    @VisibleForTesting
    static boolean readIncognitoSelected(int index) {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(incognitoSelectedKey(index), false);
    }

    @VisibleForTesting
    static String urlKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_URL.createKey(String.valueOf(index));
    }

    @VisibleForTesting
    static @Nullable String readUrl(int index) {
        return ChromeSharedPreferences.getInstance().readString(urlKey(index), null);
    }

    static void writeUrl(int index, String url) {
        ChromeSharedPreferences.getInstance().writeString(urlKey(index), url);
    }

    private static void writeUrl(int index, Tab tab) {
        assert !tab.isIncognito();
        writeUrl(index, tab.getOriginalUrl().getSpec());
    }

    @VisibleForTesting
    static String titleKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TITLE.createKey(String.valueOf(index));
    }

    @VisibleForTesting
    static String customTitleKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_CUSTOM_TITLE.createKey(String.valueOf(index));
    }

    @VisibleForTesting
    static @Nullable String readTitle(int index) {
        return ChromeSharedPreferences.getInstance().readString(titleKey(index), null);
    }

    @VisibleForTesting
    static @Nullable String readCustomTitle(int index) {
        return ChromeSharedPreferences.getInstance().readString(customTitleKey(index), null);
    }

    private static void writeTitle(int index, Tab tab) {
        assert !tab.isIncognito();
        writeTitle(index, tab.getTitle());
    }

    @VisibleForTesting
    static void writeTitle(int index, String title) {
        ChromeSharedPreferences.getInstance().writeString(titleKey(index), title);
    }

    @VisibleForTesting
    static void writeCustomTitle(int index, String customTitle) {
        ChromeSharedPreferences.getInstance().writeString(customTitleKey(index), customTitle);
    }

    @VisibleForTesting
    static String tabCountKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(String.valueOf(index));
    }

    @VisibleForTesting
    static String tabCountForRelaunchKey(int index) {
        return MultiWindowUtils.getTabCountForRelaunchKey(index);
    }

    static int readTabCount(int index) {
        return ChromeSharedPreferences.getInstance().readInt(tabCountKey(index));
    }

    @VisibleForTesting
    static String incognitoTabCountKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT.createKey(
                String.valueOf(index));
    }

    @VisibleForTesting
    static int readIncognitoTabCount(int index) {
        return ChromeSharedPreferences.getInstance().readInt(incognitoTabCountKey(index));
    }

    @VisibleForTesting
    static void writeTabCount(int index, TabModelSelector selector) {
        if (!selector.isTabStateInitialized()) return;
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        int tabCount = selector.getModel(false).getCount();
        prefs.writeInt(tabCountKey(index), tabCount);
        prefs.writeInt(incognitoTabCountKey(index), selector.getModel(true).getCount());
        if (tabCount == 0) {
            writeUrl(index, EMPTY_DATA);
            writeTitle(index, EMPTY_DATA);
        }
    }

    static boolean instanceEntryExists(int index) {
        return readLastAccessedTime(index) != 0;
    }

    @VisibleForTesting
    static String lastAccessedTimeKey(int index) {
        return MultiWindowUtils.lastAccessedTimeKey(index);
    }

    @VisibleForTesting
    static long readLastAccessedTime(int index) {
        return MultiWindowUtils.readLastAccessedTime(index);
    }

    @VisibleForTesting
    static void writeLastAccessedTime(int index) {
        MultiWindowUtils.writeLastAccessedTime(index);
    }

    @VisibleForTesting
    static String profileTypeKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_PROFILE_TYPE.createKey(String.valueOf(index));
    }

    @VisibleForTesting
    static @SupportedProfileType int readProfileType(int index) {
        return ChromeSharedPreferences.getInstance().readInt(profileTypeKey(index));
    }

    @VisibleForTesting
    static String closedByUserKey(int index) {
        return ChromePreferenceKeys.MULTI_INSTANCE_CLOSED_BY_USER.createKey(String.valueOf(index));
    }

    @VisibleForTesting
    static boolean readClosedByUser(int index) {
        return ChromeSharedPreferences.getInstance().readBoolean(closedByUserKey(index), false);
    }

    @VisibleForTesting
    static void writeClosedByUser(int index, boolean closedByUser) {
        ChromeSharedPreferences.getInstance().writeBoolean(closedByUserKey(index), closedByUser);
    }

    /**
     * @return The window IDs of the currently running ChromeTabbedActivity's. It is possible to
     *     have more number of saved instances than the number of currently running activities (for
     *     example, when an activity is killed from the Android app menu, its instance state still
     *     persists for use by Chrome).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    static SparseIntArray getWindowIdsOfRunningTabbedActivities() {
        List<Activity> activities = ApplicationStatus.getRunningActivities();
        var windowIdsOfRunningTabbedActivities = new SparseIntArray();
        for (Activity activity : activities) {
            if (!(activity instanceof ChromeTabbedActivity)) continue;
            int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(activity);
            windowIdsOfRunningTabbedActivities.put(windowId, windowId);
        }
        return windowIdsOfRunningTabbedActivities;
    }

    /**
     * Open or launch a given instance.
     *
     * @param instanceId ID of the instance to open.
     * @param taskId ID of the task the instance resides in.
     * @param source The source of the opening of the instance.
     */
    @VisibleForTesting
    void openInstance(int instanceId, int taskId, @NewWindowAppSource int source) {
        RecordUserAction.record("Android.WindowManager.SelectWindow");
        if (taskId != INVALID_TASK_ID) {
            // Bring the task to foreground if the activity is alive, this completes the opening
            // of the instance. Otherwise, create a new activity for the instance and kill the
            // existing task.
            // TODO: Consider killing the instance and start it again to be able to position it
            //       in the intended window.
            if (getActivityById(instanceId) != null) {
                bringTaskForeground(taskId);
                return;
            } else {
                var appTask = AndroidTaskUtils.getAppTaskFromId(mActivity, taskId);
                if (appTask != null) {
                    appTask.finishAndRemoveTask();
                }
            }
        }
        onMultiInstanceModeStarted();
        // TODO: Pass this flag from UI to control the window to open.
        // If not in multi-window mode, this will be determined by shouldOpenInAdjacentWindow().
        boolean openAdjacently =
                !mActivity.isInMultiWindowMode() && MultiWindowUtils.shouldOpenInAdjacentWindow();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        instanceId,
                        /* preferNew= */ false,
                        openAdjacently,
                        /* addTrustedIntentExtras= */ true,
                        source);
        mActivity.startActivity(intent);
    }

    /**
     * Launch the given intent in an existing ChromeTabbedActivity instance.
     *
     * @param intent The intent to launch.
     * @param windowId ID of the window to launch the intent in.
     * @return Whether the intent was launched successfully.
     */
    static boolean launchIntentInExistingActivity(Intent intent, @WindowId int windowId) {
        Activity activity = getActivityById(windowId);
        if (!(activity instanceof ChromeTabbedActivity)) return false;
        int taskId = activity.getTaskId();
        if (taskId == INVALID_TASK_ID) return false;

        // Launch the intent in the existing activity and bring the task to foreground if it is
        // alive.
        ((ChromeTabbedActivity) activity).onNewIntent(intent);
        var activityManager = (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
        if (activityManager != null) {
            activityManager.moveTaskToFront(taskId, 0);
        }
        return true;
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
        // #launchIntentInExistingActivity.

        if (launchIntentInExistingActivity(intent, windowId)) return;

        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, windowId);
        IntentUtils.safeStartActivity(context, intent);
    }

    @Override
    public void closeWindow(int instanceId, @CloseWindowAppSource int source) {
        if (!isUserInitiatedClosure(source)) {
            removeInstanceInfo(instanceId, source);
            TabModelSelector selector =
                    TabWindowManagerSingleton.getInstance().getTabModelSelectorById(instanceId);
            if (selector != null) {
                // Commit all already pending tab closures to ensure that any in-flight closures
                // complete and we don't get back-from-the-dead tabs.
                selector.commitAllTabClosures();

                // Close all tabs as the window is closing. This ensures the tabs are added to the
                // recent tabs page.
                //
                // TODO(crbug.com/40826734): This only works for windows with live activities. It is
                // non-trivial to add recent tab entries without an active {@link Tab} instance.
                TabClosureParams params =
                        TabClosureParams.closeAllTabs().uponExit(true).hideTabGroups(true).build();
                selector.getModel(true).getTabRemover().closeTabs(params, /* allowDialog= */ false);
                selector.getModel(false)
                        .getTabRemover()
                        .closeTabs(params, /* allowDialog= */ false);
            }
            mTabModelOrchestratorSupplier.get().cleanupInstance(instanceId);
        } else {
            writeClosedByUser(instanceId, /* closedByUser= */ true);
        }
        Activity activity = getActivityById(instanceId);
        if (activity != null) activity.finishAndRemoveTask();
    }

    /**
     * Returns whether a window closure is initiated by the user, it usually means that the instance
     * closure is a "soft closure" and should be preserved for later restoration via surfaces (like
     * Recent Tabs) or keyboard shortcuts.
     *
     * <p>A soft closure means the window's {@link InstanceInfo} and {@link TabModel} data are
     * persisted, even though the {@link Activity} and Android task will be removed via {@link
     * Activity#finishAndRemoveTask()}.
     *
     * @param source The window closure source, from {@link CloseWindowAppSource}.
     */
    private boolean isUserInitiatedClosure(@CloseWindowAppSource int source) {
        return source == CloseWindowAppSource.WINDOW_MANAGER
                && UiUtils.isRecentlyClosedTabsAndWindowsEnabled();
    }

    @VisibleForTesting
    void bringTaskForeground(int taskId) {
        ActivityManager am = (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        am.moveTaskToFront(taskId, 0);
    }

    @VisibleForTesting
    void setupIntentForTabsReparenting(
            List<Tab> tabs, Intent intent, @Nullable Runnable finalizeCallback) {
        ReparentingTabsTask.from(tabs).setupIntent(intent, finalizeCallback);
    }

    @VisibleForTesting
    void setupIntentForGroupReparenting(
            TabGroupMetadata tabGroupMetadata, Intent intent, @Nullable Runnable finalizeCallback) {
        ReparentingTabGroupTask.from(tabGroupMetadata).setupIntent(intent, finalizeCallback);
    }

    @VisibleForTesting
    void beginReparentingTabs(
            List<Tab> tabs,
            Intent intent,
            @Nullable Bundle startActivityOptions,
            @Nullable Runnable finalizeCallback) {
        ReparentingTabsTask.from(tabs)
                .begin(mActivity, intent, startActivityOptions, finalizeCallback);
    }

    @VisibleForTesting
    void beginReparentingTabGroup(TabGroupMetadata tabGroupMetadata, Intent intent) {
        long startTime = SystemClock.elapsedRealtime();
        intent.putExtra(IntentHandler.EXTRA_REPARENT_START_TIME, startTime);

        // Pause observers before detaching tabs.
        pauseObserversForGroupReparenting(tabGroupMetadata);

        // Create the task, detaching the grouped tabs from the current activity.
        ReparentingTabGroupTask reparentingTask = ReparentingTabGroupTask.from(tabGroupMetadata);
        reparentingTask.setupIntent(intent, CallbackUtils.emptyRunnable());

        // Create the new window and reparent once the TabPersistentStore has resumed.
        TabPersistentStore tabPersistentStore =
                mTabModelOrchestratorSupplier.get().getTabPersistentStore();
        tabPersistentStore.resumeSaveTabList(
                () -> {
                    reparentingTask.begin(mActivity, intent);
                    resumeSyncService(tabGroupMetadata);
                });
    }

    // TODO (crbug.com/458784614): Move this to MultiWindowUtils
    static int getInstanceCountForManageWindowsMenu() {
        if (!UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) {
            return MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY);
        }

        Map<String, Long> lastAccessedTimeMap =
                ChromeSharedPreferences.getInstance()
                        .readLongsWithPrefix(
                                ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME);
        Pattern pattern = Pattern.compile("(\\d+)$");

        int count = 0;

        for (String prefKey : lastAccessedTimeMap.keySet()) {
            Matcher matcher = pattern.matcher(prefKey);
            boolean matchFound = matcher.find();
            assert matchFound : "Key should be suffixed with the instance id.";
            int id = Integer.parseInt(matcher.group(1));

            // Exclude instances closed by the user.
            if (readClosedByUser(id)) continue;
            if (!isRestorableInstance(id)) continue;
            count++;
        }
        return count;
    }

    private Profile getProfile() {
        TabModelSelector tabModelSelector =
                mTabModelOrchestratorSupplier.get().getTabModelSelector();
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
        removeInvalidInstanceData(/* cleanupApplicationStatus= */ false);
        if (mInstanceId != INVALID_WINDOW_ID) {
            ApplicationStatus.unregisterActivityStateListener(this);
        }
        if (sState != null) {
            if (getAllRunningActivities().isEmpty()) {
                sState.clear();
            } else {
                sState.removeObserver(mOnMultiInstanceStateChanged);
            }
        }

        super.onDestroy();
    }

    @VisibleForTesting
    static void removeInstanceInfo(int index, @CloseWindowAppSource int source) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.removeKey(urlKey(index));
        prefs.removeKey(titleKey(index));
        prefs.removeKey(tabCountKey(index));
        prefs.removeKey(tabCountForRelaunchKey(index));
        prefs.removeKey(incognitoTabCountKey(index));
        prefs.removeKey(incognitoSelectedKey(index));
        prefs.removeKey(lastAccessedTimeKey(index));
        prefs.removeKey(profileTypeKey(index));
        prefs.removeKey(customTitleKey(index));
        prefs.removeKey(closedByUserKey(index));

        RecordHistogram.recordEnumeratedHistogram(
                CLOSE_WINDOW_APP_SOURCE_HISTOGRAM, source, CloseWindowAppSource.NUM_ENTRIES);
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        super.onTopResumedActivityChanged(isTopResumedActivity);
        writeLastAccessedTime(mInstanceId);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return;

        if (newState != ActivityState.RESUMED && newState != ActivityState.STOPPED) return;

        int windowingMode =
                AppHeaderUtils.getWindowingMode(
                        mActivity,
                        AppHeaderUtils.isAppInDesktopWindow(
                                mDesktopWindowStateManagerSupplier.get()));
        AppHeaderUtils.recordWindowingMode(windowingMode, newState == ActivityState.RESUMED);

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        // Check the max instance count in a day for every state update if needed.
        long timestamp = prefs.readLong(ChromePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, 0);
        int maxCount = prefs.readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, 0);
        int incognitoMaxCount =
                prefs.readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO, 0);
        long current = System.currentTimeMillis();

        if (current - timestamp > DateUtils.DAY_IN_MILLIS) {
            if (timestamp != 0) {
                RecordHistogram.recordExactLinearHistogram(
                        "Android.MultiInstance.MaxInstanceCount",
                        maxCount,
                        TabWindowManager.MAX_SELECTORS + 1);
                if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                    RecordHistogram.recordExactLinearHistogram(
                            "Android.MultiInstance.MaxInstanceCountIncognito",
                            incognitoMaxCount,
                            TabWindowManager.MAX_SELECTORS + 1);
                }
            }
            prefs.writeLong(ChromePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, current);
            // Reset the count to 0 to be ready to obtain the max count for the next 24-hour period.
            maxCount = 0;
            incognitoMaxCount = 0;
        }
        // TODO(crbug.com/454366549): Add metric to record the max active instance count.
        int instanceCount =
                MultiWindowUtils.getInstanceCountWithFallback(
                        MultiInstanceManager.PersistedInstanceType.ANY);
        int incognitoInstanceCount =
                MultiWindowUtils.getIncognitoInstanceCount(/* activeOnly= */ false);
        if (instanceCount > maxCount) {
            prefs.writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, instanceCount);
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
    public void moveTabsToNewWindow(List<Tab> tabs, @NewWindowAppSource int source) {
        boolean openAdjacently =
                !mActivity.isInMultiWindowMode() && MultiWindowUtils.shouldOpenInAdjacentWindow();
        moveToNewWindowIfPossible(
                () ->
                        moveAndReparentTabsToNewWindow(
                                tabs,
                                INVALID_WINDOW_ID,
                                /* preferNew= */ true,
                                openAdjacently,
                                /* addTrustedIntentExtras= */ true,
                                source),
                source);
    }

    @Override
    public void moveTabGroupToNewWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source) {
        boolean openAdjacently =
                !mActivity.isInMultiWindowMode() && MultiWindowUtils.shouldOpenInAdjacentWindow();
        moveToNewWindowIfPossible(
                () ->
                        moveAndReparentTabGroupToNewWindow(
                                tabGroupMetadata,
                                INVALID_WINDOW_ID,
                                /* preferNew= */ true,
                                openAdjacently,
                                /* addTrustedIntentExtras= */ true,
                                source),
                source);
    }

    /**
     * Runs the given runnable to create a new window and reparent if the max instances has not been
     * reached. Otherwise, shows a toast indicating this action failed.
     *
     * @param moveToNewWindow Runnable to create a new window and reparent the given tab or group.
     * @param source The app source of the new window used for metrics.
     */
    private void moveToNewWindowIfPossible(
            Runnable moveToNewWindow, @NewWindowAppSource int source) {
        // Check if the new Chrome instance can be opened.
        int instanceCount =
                MultiWindowUtils.getInstanceCountWithFallback(
                        MultiInstanceManager.PersistedInstanceType.ACTIVE);
        if (instanceCount < mMaxInstances) {
            moveToNewWindow.run();
        } else {
            // Just try to launch a Chrome window to inform user that maximum number of instances
            // limit is exceeded. This will pop up a toast message and the tab will not be removed
            // from the exiting window.
            // TODO(crbug.com/451683614): Add UMA user action for this case.
            openNewWindow("Android.WindowManager.NewWindow", /* incognito= */ false, source);
        }
    }

    @Override
    public void moveTabsToWindow(@Nullable Activity activity, List<Tab> tabs, int atIndex) {
        // Get the current instance and move tab there.
        InstanceInfo info = getInstanceInfoFor(activity);
        if (info != null) {
            moveTabsToWindow(info, tabs, atIndex, NewWindowAppSource.OTHER);
        } else {
            Log.w(TAG, "DnD: InstanceInfo of Chrome Window not found.");
        }
    }

    @Override
    public void moveTabGroupToWindow(
            @Nullable Activity activity, TabGroupMetadata tabGroupMetadata, int atIndex) {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID);

        // Get the current instance and move tab there.
        InstanceInfo info = getInstanceInfoFor(activity);
        if (info != null) {
            moveTabGroupToWindow(info, tabGroupMetadata, atIndex, NewWindowAppSource.OTHER);
        } else {
            Log.w(TAG, "DnD: InstanceInfo of Chrome Window not found.");
        }
    }

    @VisibleForTesting
    @Nullable InstanceInfo getInstanceInfoFor(@Nullable Activity activity) {
        if (activity == null) return null;

        // Loop thru all instances to determine if the destination activity is present.
        int destinationWindowTaskId = INVALID_TASK_ID;
        for (int i : getAllPersistedInstanceIds()) {
            Activity activityById = getActivityById(i);
            if (activityById != null) {
                // The task for the activity must match the one found in our mapping.
                assert getTaskFromMap(i) == activityById.getTaskId();
                if (activityById == activity) {
                    destinationWindowTaskId = activityById.getTaskId();
                    break;
                }
            }
        }
        if (destinationWindowTaskId == INVALID_TASK_ID) return null;

        List<InstanceInfo> allInstances = getInstanceInfo();
        for (InstanceInfo instanceInfo : allInstances) {
            if (instanceInfo.taskId == destinationWindowTaskId) {
                return instanceInfo;
            }
        }
        return null;
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
                closeWindow(instanceId, CloseWindowAppSource.NO_TABS_IN_WINDOW);
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
        List<InstanceInfo> info = getInstanceInfo();
        if (info.size() != 1) return;

        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance()
                        .getTabModelSelectorById(info.get(0).instanceId);
        if (selector == null) return;

        cleanupSyncedTabGroups(selector);
    }

    @Override
    public void cleanupSyncedTabGroupsIfOnlyInstance(TabModelSelector selector) {
        TabModelUtils.runOnTabStateInitialized(
                selector,
                (TabModelSelector initializedSelector) -> cleanupSyncedTabGroupsIfLastInstance());
    }

    private @Nullable TabGroupModelFilter getTabGroupModelFilterByWindowId(
            int windowId, boolean isIncognito) {
        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance().getTabModelSelectorById(windowId);
        if (selector == null) return null;

        return selector.getTabGroupModelFilterProvider().getTabGroupModelFilter(isIncognito);
    }

    private @Nullable TabGroupSyncService getTabGroupSyncService(
            int windowId, boolean isIncognito) {
        TabGroupModelFilter filter = getTabGroupModelFilterByWindowId(windowId, isIncognito);
        if (filter == null) return null;

        @Nullable Profile profile = filter.getTabModel().getProfile();
        if (profile == null
                || profile.isOffTheRecord()
                || !TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) return null;

        return TabGroupSyncServiceFactory.getForProfile(profile);
    }

    private void setSyncServiceLocalObservationMode(
            @Nullable TabGroupSyncService syncService, boolean shouldObserve) {
        if (syncService != null) {
            syncService.setLocalObservationMode(shouldObserve);
        }
    }

    @Override
    public boolean showInstanceRestorationMessage(@Nullable MessageDispatcher messageDispatcher) {
        return MultiWindowUtils.maybeShowInstanceRestorationMessage(
                messageDispatcher, mActivity, this::showInstanceSwitcherDialog);
    }

    @Override
    public void showInstanceCreationLimitMessage(@Nullable MessageDispatcher messageDispatcher) {
        MultiWindowUtils.showInstanceCreationLimitMessage(
                messageDispatcher, mActivity, this::showInstanceSwitcherDialog);
    }

    @Override
    public void showNameWindowDialog(@NameWindowDialogSource int source) {
        String customTitle = readCustomTitle(mInstanceId);
        String currentTitle = TextUtils.isEmpty(customTitle) ? readTitle(mInstanceId) : customTitle;

        UiUtils.showNameWindowDialog(
                mActivity,
                assumeNonNull(currentTitle),
                newTitle -> writeCustomTitle(mInstanceId, newTitle),
                source);
    }
}
