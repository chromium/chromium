// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.util.Pair;
import android.view.Display;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.RecreateObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils.InstanceAllocationType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collections;
import java.util.List;

/**
 * Manages multi-instance mode for an associated activity. After construction, call {@link
 * #isStartedUpCorrectly(int)} to validate that the owning Activity should be allowed to finish
 * starting up.
 */
public class MultiInstanceManager
        implements PauseResumeWithNativeObserver,
                RecreateObserver,
                ConfigurationChangedObserver,
                NativeInitObserver,
                MultiWindowModeStateDispatcher.MultiWindowModeObserver,
                DestroyObserver,
                MenuOrKeyboardActionController.MenuOrKeyboardActionHandler {
    /** Should be called when multi-instance mode is started. */
    public static void onMultiInstanceModeStarted() {
        // When a second instance is created, the merged instance task id should be cleared.
        setMergedInstanceTaskId(0);
    }

    /** The task id of the activity that tabs were merged into. */
    private static int sMergedInstanceTaskId;

    /** The class of the activity will do merge on start up. */
    private static Class sActivityTypePendingMergeOnStartup;

    private Boolean mMergeTabsOnResume;

    /**
     * Used to observe state changes to a different ChromeTabbedActivity instances to determine
     * when to merge tabs if applicable.
     */
    private ApplicationStatus.ActivityStateListener mOtherCTAStateObserver;

    protected final Activity mActivity;
    protected final ObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier;
    protected final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;

    protected TabModelSelectorTabModelObserver mTabModelObserver;

    private int mActivityTaskId;
    private boolean mNativeInitialized;
    private DisplayManager.DisplayListener mDisplayListener;
    private boolean mShouldMergeOnConfigurationChange;
    private boolean mIsRecreating;
    private int mDisplayId;
    private static List<Integer> sTestDisplayIds;
    private boolean mDestroyed;

    /**
     * Create a new {@link MultiInstanceManager}.
     *
     * @param activity The activity.
     * @param tabModelOrchestratorSupplier A supplier for the {@link TabModelOrchestrator} for the
     *     associated activity.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} for the
     *     associated activity.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the associated
     *     activity.
     * @param modalDialogManagerSupplier A supplier for the {@link ModalDialogManager}.
     * @param menuOrKeyboardActionController The {@link MenuOrKeyboardActionController} for the
     *     associated activity.
     * @param desktopWindowStateProviderSupplier A supplier for the {@link
     *     DesktopWindowStateProvider} instance.
     * @return {@link MultiInstanceManager} object or {@code null} on the platform it is not needed.
     */
    public @Nullable static MultiInstanceManager create(
            Activity activity,
            ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            Supplier<DesktopWindowStateProvider> desktopWindowStateProviderSupplier) {
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            return new MultiInstanceManagerApi31(
                    activity,
                    tabModelOrchestratorSupplier,
                    multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher,
                    modalDialogManagerSupplier,
                    menuOrKeyboardActionController,
                    desktopWindowStateProviderSupplier);
        } else {
            return new MultiInstanceManager(
                    activity,
                    tabModelOrchestratorSupplier,
                    multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher,
                    menuOrKeyboardActionController);
        }
    }

    protected MultiInstanceManager(
            Activity activity,
            ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuOrKeyboardActionController menuOrKeyboardActionController) {
        mActivity = activity;
        mTabModelOrchestratorSupplier = tabModelOrchestratorSupplier;

        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mMultiWindowModeStateDispatcher.addObserver(this);

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);

        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(this);
    }

    @Override
    public void onDestroy() {
        mDestroyed = true;
        mMultiWindowModeStateDispatcher.removeObserver(this);
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(this);
        mActivityLifecycleDispatcher.unregister(this);
        removeOtherCTAStateObserver();

        DisplayManager displayManager =
                (DisplayManager) mActivity.getSystemService(Context.DISPLAY_SERVICE);
        if (displayManager != null && mDisplayListener != null) {
            displayManager.unregisterDisplayListener(mDisplayListener);
        }
    }

    /**
     * Called during activity startup to check whether the activity is recreated because
     * the secondary display is removed.
     *
     * @return True if the activity is recreated after a display is removed. Should consider
     *         merging tabs.
     */
    public static boolean shouldMergeOnStartup(Activity activity) {
        return sActivityTypePendingMergeOnStartup != null
                && sActivityTypePendingMergeOnStartup.equals(activity.getClass());
    }

    /**
     * Called after {@link #shouldMergeOnStartup(Activity)} to indicate merge has started,
     * so there is no merge on following recreate.
     */
    public static void mergedOnStartup() {
        sActivityTypePendingMergeOnStartup = null;
    }

    /**
     * Called during activity startup to check whether this instance of the MultiInstanceManager
     * is associated with an activity task ID that should be started up.
     *
     * @return True if the activity should proceed with startup. False otherwise.
     */
    public boolean isStartedUpCorrectly(int activityTaskId) {
        mActivityTaskId = activityTaskId;

        // If tabs from this instance were merged into a different ChromeTabbedActivity instance
        // and the other instance is still running, then this instance should not be created. This
        // may happen if the process is restarted e.g. on upgrade or from about://flags.
        // See crbug.com/657418
        boolean tabsMergedIntoAnotherInstance =
                sMergedInstanceTaskId != 0 && sMergedInstanceTaskId != mActivityTaskId;

        // Since a static is used to track the merged instance task id, it is possible that
        // sMergedInstanceTaskId is still set even though the associated task is not running.
        boolean mergedInstanceTaskStillRunning = isMergedInstanceTaskRunning();

        if (tabsMergedIntoAnotherInstance && mergedInstanceTaskStillRunning) {
            // Currently only two instances of ChromeTabbedActivity may be running at any given
            // time. If tabs were merged into another instance and this instance is being killed due
            // to incorrect startup, then no other instances should exist. Reset the merged instance
            // task id.
            setMergedInstanceTaskId(0);
            return false;
        } else if (!mergedInstanceTaskStillRunning) {
            setMergedInstanceTaskId(0);
        }

        return true;
    }

    @Override
    public void onFinishNativeInitialization() {
        mNativeInitialized = true;
        DisplayManager displayManager =
                (DisplayManager) mActivity.getSystemService(Context.DISPLAY_SERVICE);
        if (displayManager == null) return;
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(mActivity);
        mDisplayId = display.getDisplayId();
        mDisplayListener =
                new DisplayListener() {
                    @Override
                    public void onDisplayAdded(int displayId) {
                        if (!isNormalDisplay(displayId)) return;
                        sActivityTypePendingMergeOnStartup = null;
                    }

                    @Override
                    public void onDisplayRemoved(int displayId) {
                        if (!isNormalDisplay(displayId)) return;
                        if (displayId == mDisplayId) {
                            // If activity on removed display is in the foreground, do tab merge.
                            // Note that activity on removed display may be recreated because of the
                            // change of the dpi. If it is going to recreate, then CTA will merge on
                            // start up; otherwise, calling maybeMergeTabs() can merge tabs.
                            if (mActivityLifecycleDispatcher.getCurrentActivityState()
                                    == ActivityLifecycleDispatcher.ActivityState
                                            .RESUMED_WITH_NATIVE) {
                                // wait to merge until onConfigurationChanged so that we can know
                                // whether the activity is going to recreate.
                                mShouldMergeOnConfigurationChange = true;
                            }
                        } else {
                            // Otherwise, activity on the remaining display does tab merge.
                            Activity cta = getOtherResumedCTA();
                            if (cta == null) {
                                maybeMergeTabs();
                            }
                        }
                    }

                    @Override
                    public void onDisplayChanged(int displayId) {
                        if (displayId == mDisplayId || !isNormalDisplay(displayId)) return;
                        List<Integer> ids =
                                sTestDisplayIds != null
                                        ? sTestDisplayIds
                                        : ApiCompatibilityUtils.getTargetableDisplayIds(mActivity);
                        if (ids.size() == 1 && ids.get(0).equals(mDisplayId)) {
                            maybeMergeTabs();
                        }
                    }
                };
        displayManager.registerDisplayListener(mDisplayListener, null);
    }

    /**
     * Check if the given display is what Chrome can use for showing activity/tab.
     * It should be either the default display, or secondary one such as external,
     * wireless display.
     * @param id ID of the display.
     * @return {@code true} if the display is a normal one.
     */
    private boolean isNormalDisplay(int id) {
        if (id == Display.DEFAULT_DISPLAY || sTestDisplayIds != null) return true;
        Display display = getDisplayFromId(id);
        return (display != null && (display.getFlags() & Display.FLAG_PRESENTATION) != 0);
    }

    private @Nullable Display getDisplayFromId(int id) {
        DisplayManager displayManager =
                (DisplayManager) mActivity.getSystemService(Context.DISPLAY_SERVICE);
        if (displayManager == null) return null;
        Display[] displays = displayManager.getDisplays();
        for (Display display : displays) {
            if (display.getDisplayId() == id) return display;
        }
        return null;
    }

    @Override
    public void onResumeWithNative() {
        if (isTabModelMergingEnabled()) {
            boolean inMultiWindowMode =
                    mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                            || mMultiWindowModeStateDispatcher.isInMultiDisplayMode();
            // Don't need to merge tabs when mMergeTabsOnResume is null (cold start) since they get
            // merged when TabPersistentStore.loadState(boolean) is called from initializeState().
            if (!inMultiWindowMode && (mMergeTabsOnResume != null && mMergeTabsOnResume)) {
                maybeMergeTabs();
            } else if (!inMultiWindowMode && mMergeTabsOnResume == null) {
                // This happens on cold start to kill any second activity that might exist.
                killOtherTask();
            }
            mMergeTabsOnResume = false;
        }
    }

    @Override
    public void onPauseWithNative() {
        removeOtherCTAStateObserver();
    }

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        if (!isTabModelMergingEnabled() || !mNativeInitialized) {
            return;
        }

        if (!isInMultiWindowMode) {
            // If the activity is currently resumed when multi-window mode is exited, try to merge
            // tabs from the other activity instance.
            if (mActivityLifecycleDispatcher.getCurrentActivityState()
                    == ActivityLifecycleDispatcher.ActivityState.RESUMED_WITH_NATIVE) {
                ChromeTabbedActivity otherResumedCTA = getOtherResumedCTA();
                if (otherResumedCTA == null) {
                    maybeMergeTabs();
                } else {
                    // Remove the other CTA state observer if one already exists to protect
                    // against multiple #onMultiWindowModeChanged calls.
                    // See https://crbug.com/1385987.
                    removeOtherCTAStateObserver();
                    // Wait for the other ChromeTabbedActivity to pause before trying to merge
                    // tabs.
                    mOtherCTAStateObserver =
                            (activity, newState) -> {
                                if (newState == ActivityState.PAUSED) {
                                    removeOtherCTAStateObserver();
                                    maybeMergeTabs();
                                }
                            };
                    ApplicationStatus.registerStateListenerForActivity(
                            mOtherCTAStateObserver, otherResumedCTA);
                }
            } else {
                mMergeTabsOnResume = true;
            }
        }
    }

    @Override
    public void onRecreate() {
        mIsRecreating = true;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // Prepare for merging tabs: do tab merge now if the activity is not going to recreate;
        // otherwise, do it on start up.
        if (mShouldMergeOnConfigurationChange) {
            if (mIsRecreating) {
                sActivityTypePendingMergeOnStartup = mActivity.getClass();
            } else {
                sActivityTypePendingMergeOnStartup = null;
                maybeMergeTabs();
            }
            mShouldMergeOnConfigurationChange = false;
        }
    }

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == org.chromium.chrome.R.id.move_to_other_window_menu_id) {
            TabModelOrchestrator tabModelOrchestrator = mTabModelOrchestratorSupplier.get();
            if (tabModelOrchestrator == null) return true;
            TabModelSelector tabModelSelector = tabModelOrchestrator.getTabModelSelector();
            if (tabModelSelector == null) return true;

            Tab currentTab = tabModelSelector.getCurrentTab();
            if (currentTab != null) moveTabToOtherWindow(currentTab);
            return true;
        } else if (id == org.chromium.chrome.R.id.new_window_menu_id) {
            openNewWindow("MobileMenuNewWindow");
            return true;
        }

        return false;
    }

    private @Nullable ChromeTabbedActivity getOtherResumedCTA() {
        Class<?> otherWindowActivityClass =
                mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivity();
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity.getClass().equals(otherWindowActivityClass)
                    && ApplicationStatus.getStateForActivity(activity) == ActivityState.RESUMED) {
                return (ChromeTabbedActivity) activity;
            }
        }
        return null;
    }

    private void removeOtherCTAStateObserver() {
        if (mOtherCTAStateObserver != null) {
            ApplicationStatus.unregisterActivityStateListener(mOtherCTAStateObserver);
            mOtherCTAStateObserver = null;
        }
    }

    private void killOtherTask() {
        if (!isTabModelMergingEnabled()) return;

        Class<?> otherWindowActivityClass =
                mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivity();

        // 1. Find the other activity's task if it's still running so that it can be removed from
        //    Android recents.
        ActivityManager activityManager =
                (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.AppTask> appTasks = activityManager.getAppTasks();
        ActivityManager.AppTask otherActivityTask = null;
        for (ActivityManager.AppTask task : appTasks) {
            String baseActivity = MultiWindowUtils.getActivityNameFromTask(task);

            if (baseActivity.equals(otherWindowActivityClass.getName())) {
                otherActivityTask = task;
            }
        }

        if (otherActivityTask != null) {
            for (Activity activity : ApplicationStatus.getRunningActivities()) {
                // 2. If the other activity is still running (not destroyed), save its tab list.
                //    Saving the tab list prevents missing tabs or duplicate tabs if tabs have been
                //    reparented.
                // TODO(twellington): saveState() gets called in onStopWithNative() after the merge
                // starts, causing some duplicate work to be done. Avoid the redundancy.
                if (activity.getClass().equals(otherWindowActivityClass)) {
                    ((ChromeTabbedActivity) activity).saveState();
                    break;
                }
            }
            // 3. Kill the other activity's task to remove it from Android recents.
            otherActivityTask.finishAndRemoveTask();
        }
        setMergedInstanceTaskId(mActivityTaskId);
    }

    /**
     * Merges tabs from a second ChromeTabbedActivity instance if necessary and calls
     * finishAndRemoveTask() on the other activity.
     */
    @VisibleForTesting
    public void maybeMergeTabs() {
        assert !mDestroyed;
        if (!isTabModelMergingEnabled() || mDestroyed) return;

        killOtherTask();
        RecordUserAction.record("Android.MergeState.Live");
        mTabModelOrchestratorSupplier.get().mergeState();
    }

    private static void setMergedInstanceTaskId(int mergedInstanceTaskId) {
        sMergedInstanceTaskId = mergedInstanceTaskId;
    }

    @SuppressLint("NewApi")
    private boolean isMergedInstanceTaskRunning() {
        if (!isTabModelMergingEnabled() || sMergedInstanceTaskId == 0) {
            return false;
        }

        ActivityManager manager =
                (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask task : manager.getAppTasks()) {
            ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (info == null) continue;
            if (info.id == sMergedInstanceTaskId) return true;
        }
        return false;
    }

    public void moveTabToNewWindow(Tab tab) {
        // Not implemented
    }

    public void moveTabToWindow(Activity activity, Tab tab, int atIndex) {
        // Not implemented
    }

    protected void moveTabToOtherWindow(Tab tab) {
        Intent intent = mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent();
        if (intent == null) return;

        onMultiInstanceModeStarted();
        ReparentingTask.from(tab)
                .begin(
                        mActivity,
                        intent,
                        mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivityOptions(),
                        null);
        RecordUserAction.record("MobileMenuMoveToOtherWindow");
    }

    protected void openNewWindow(String umaAction) {
        assert mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiDisplayMode();

        Intent intent = mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent();
        if (intent == null) return;
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);

        onMultiInstanceModeStarted();
        mActivity.startActivity(
                intent, mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivityOptions());
        RecordUserAction.record(umaAction);
    }

    /**
     * @return List of {@link InstanceInfo} structs for an activity that can be switched to, or
     *         newly launched.
     */
    public List<InstanceInfo> getInstanceInfo() {
        return Collections.emptyList();
    }

    /**
     * Assigned an ID for the current activity instance.
     *
     * @param windowId Instance ID explicitly given for assignment.
     * @param taskId Task ID of the activity.
     * @param preferNew Boolean indicating a fresh new instance is preferred over the one that will
     *     load previous tab files from disk.
     */
    public Pair<Integer, Integer> allocInstanceId(int windowId, int taskId, boolean preferNew) {
        return Pair.create(0, InstanceAllocationType.DEFAULT); // Use a default index 0.
    }

    /**
     * Initialize the manager with the allocated instance ID.
     * @param instanceId Instance ID of the activity.
     * @param taskId Task ID of the activity.
     */
    public void initialize(int instanceId, int taskId) {}

    /** Perform initialization tasks for the manager after the tab state is initialized. */
    public void onTabStateInitialized() {}

    /**
     * @return True if tab model merging for Android N+ is enabled.
     */
    public boolean isTabModelMergingEnabled() {
        return !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING);
    }

    public void setCurrentDisplayIdForTesting(int displayId) {
        var oldValue = mDisplayId;
        mDisplayId = displayId;
        ResettersForTesting.register(() -> mDisplayId = oldValue);
    }

    public DisplayManager.DisplayListener getDisplayListenerForTesting() {
        return mDisplayListener;
    }

    @VisibleForTesting
    public static void setTestDisplayIds(List<Integer> testDisplayIds) {
        sTestDisplayIds = testDisplayIds;
    }

    public TabModelSelectorTabModelObserver getTabModelObserverForTesting() {
        return mTabModelObserver;
    }

    public void setTabModelObserverForTesting(TabModelSelectorTabModelObserver tabModelObserver) {
        mTabModelObserver = tabModelObserver;
    }

    /**
     * @return InstanceId for current instance.
     */
    public int getCurrentInstanceId() {
        return MultiWindowUtils.INVALID_INSTANCE_ID;
    }

    /**
     * Close a Chrome window instance only if it contains no open tabs including incognito ones.
     *
     * @param instanceId Instance id of the Chrome window that needs to be closed.
     * @return {@code true} if the window was closed, {@code false} otherwise.
     */
    public boolean closeChromeWindowIfEmpty(int instanceId) {
        return false;
    }
}
