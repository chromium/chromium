// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.os.Build;
import android.view.Display;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.RecreateObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.ui.display.DisplayAndroidManager;

import java.util.List;

/**
 * Manages multi-instance mode for an associated activity. After construction, call
 * {@link #isStartedUpCorrectly(int)} to validate that the owning Activity should be allowed to
 * finish starting up.
 */
@TargetApi(Build.VERSION_CODES.N)
public class MultiInstanceManager
        implements PauseResumeWithNativeObserver, RecreateObserver, ConfigurationChangedObserver,
                   NativeInitObserver, MultiWindowModeStateDispatcher.MultiWindowModeObserver,
                   Destroyable, MenuOrKeyboardActionController.MenuOrKeyboardActionHandler {
    /**
     * Should be called when multi-instance mode is started.
     */
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

    private final Activity mActivity;
    private final ObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier;
    private final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;

    private int mActivityTaskId;
    private boolean mNativeInitialized;
    private DisplayManager.DisplayListener mDisplayListener;
    private boolean mShouldMergeOnConfigurationChange;
    private boolean mIsRecreating;
    private int mDisplayId;
    private static List<Integer> sTestDisplayIds;

    /**
     * Create a new {@link MultiInstanceManager}.
     * @param activity The activity.
     * @param tabModelOrchestratorSupplier A supplier for the {@link TabModelOrchestrator} for the
     *         associated activity.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} for the
     *         associated activity.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the
     *         associated activity.
     * @param menuOrKeyboardActionController The {@link MenuOrKeyboardActionController} for the
     *         associated activity.
     */
    public MultiInstanceManager(Activity activity,
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
    public void destroy() {
        mMultiWindowModeStateDispatcher.removeObserver(this);
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(this);
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
        mDisplayListener = new DisplayListener() {
            @Override
            public void onDisplayAdded(int displayId) {
                sActivityTypePendingMergeOnStartup = null;
            }

            @Override
            public void onDisplayRemoved(int displayId) {
                if (displayId == mDisplayId) {
                    // If activity on removed display is in the foreground, do tab merge.
                    // Note that activity on removed display may be recreated because of the
                    // change of the dpi. If it is going to recreate, then CTA will merge on
                    // start up; otherwise, calling maybeMergeTabs() can merge tabs.
                    if (mActivityLifecycleDispatcher.getCurrentActivityState()
                            == ActivityLifecycleDispatcher.ActivityState.RESUMED_WITH_NATIVE) {
                        // wait to merge until onConfigurationChanged so that we can know whether
                        // the activity is going to recreate.
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
                if (displayId == mDisplayId) return;
                List<Integer> ids = sTestDisplayIds != null
                    ? sTestDisplayIds
                    : ApiCompatibilityUtils.getTargetableDisplayIds(mActivity);
                if (ids.size() == 1 && ids.get(0).equals(mDisplayId)) {
                    maybeMergeTabs();
                }
            }
        };
        displayManager.registerDisplayListener(mDisplayListener, null);
    }

    @Override
    public void onResumeWithNative() {
        if (isTabModelMergingEnabled()) {
            boolean inMultiWindowMode = mMultiWindowModeStateDispatcher.isInMultiWindowMode()
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
                    // Wait for the other ChromeTabbedActivity to pause before trying to merge
                    // tabs.
                    mOtherCTAStateObserver = new ApplicationStatus.ActivityStateListener() {
                        @Override
                        public void onActivityStateChange(Activity activity, int newState) {
                            if (newState == ActivityState.PAUSED) {
                                removeOtherCTAStateObserver();
                                maybeMergeTabs();
                            }
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
        if (!isTabModelMergingEnabled()) return;

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

    private void moveTabToOtherWindow(Tab tab) {
        Intent intent = mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent();
        if (intent == null) return;

        onMultiInstanceModeStarted();
        ReparentingTask.from(tab).begin(mActivity, intent,
                mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivityOptions(), null);
    }

    /**
     * @return True if tab model merging for Android N+ is enabled.
     */
    public static boolean isTabModelMergingEnabled() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)) {
            return false;
        }
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.M;
    }

    @VisibleForTesting
    public void setCurrentDisplayIdForTesting(int displayId) {
        mDisplayId = displayId;
    }

    @VisibleForTesting
    public DisplayManager.DisplayListener getDisplayListenerForTesting() {
        return mDisplayListener;
    }

    @VisibleForTesting
    public static void setTestDisplayIds(List<Integer> testDisplayIds) {
        sTestDisplayIds = testDisplayIds;
    }
}
