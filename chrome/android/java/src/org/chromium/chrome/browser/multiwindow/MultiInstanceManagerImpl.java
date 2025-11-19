// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;

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

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabsTask;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.RecreateObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils.InstanceAllocationType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.display.DisplayAndroidManager;

import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/**
 * Manages multi-instance mode for an associated activity. After construction, call {@link
 * #isStartedUpCorrectly(int)} to validate that the owning Activity should be allowed to finish
 * starting up.
 */
@NullMarked
public class MultiInstanceManagerImpl extends MultiInstanceManager
        implements PauseResumeWithNativeObserver,
                RecreateObserver,
                ConfigurationChangedObserver,
                NativeInitObserver,
                MultiWindowModeStateDispatcher.MultiWindowModeObserver,
                DestroyObserver,
                MenuOrKeyboardActionController.MenuOrKeyboardActionHandler,
                TopResumedActivityChangedObserver {

    private @Nullable Boolean mMergeTabsOnResume;

    /**
     * Used to observe state changes to a different ChromeTabbedActivity instances to determine when
     * to merge tabs if applicable.
     */
    private @Nullable ActivityStateListener mOtherCTAStateObserver;

    protected final Activity mActivity;
    protected final ObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier;
    protected final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;

    protected @Nullable TabModelSelectorTabModelObserver mTabModelObserver;
    protected static @Nullable Supplier<ChromeTabbedActivity> sActivitySupplierForTesting;

    private int mActivityTaskId;
    private boolean mNativeInitialized;
    private @Nullable DisplayListener mDisplayListener;
    private boolean mShouldMergeOnConfigurationChange;
    private boolean mIsRecreating;
    private int mDisplayId;
    private boolean mDestroyed;

    /* package */ MultiInstanceManagerImpl(
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

    @Override
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
     * Check if the given display is what Chrome can use for showing activity/tab. It should be
     * either the default display, or secondary one such as external, wireless display.
     *
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

    // TopResumedActivityChangedObserver implementation.
    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {}

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
        int appSource = fromMenu ? NewWindowAppSource.MENU : NewWindowAppSource.KEYBOARD_SHORTCUT;
        if (id == R.id.move_to_other_window_menu_id) {
            TabModelOrchestrator tabModelOrchestrator = mTabModelOrchestratorSupplier.get();
            if (tabModelOrchestrator == null) return true;
            TabModelSelector tabModelSelector = tabModelOrchestrator.getTabModelSelector();
            if (tabModelSelector == null) return true;

            Tab currentTab = tabModelSelector.getCurrentTab();
            if (currentTab != null) {
                moveTabsToOtherWindow(Collections.singletonList(currentTab), appSource);
            }
            return true;
        } else if (id == R.id.new_window_menu_id) {
            openNewWindow("MobileMenuNewWindow", /* incognito= */ false, appSource);
            return true;
        } else if (id == R.id.new_incognito_window_menu_id) {
            TabModelOrchestrator tabModelOrchestrator = mTabModelOrchestratorSupplier.get();
            if (tabModelOrchestrator == null) return true;
            TabModelSelector tabModelSelector = tabModelOrchestrator.getTabModelSelector();
            if (tabModelSelector == null) return true;
            Profile profile = tabModelSelector.getCurrentModel().getProfile();
            if (profile != null && IncognitoUtils.isIncognitoModeEnabled(profile)) {
                openNewWindow("MobileMenuNewIncognitoWindow", /* incognito= */ true, appSource);
            }
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
                assumeNonNull(mMultiWindowModeStateDispatcher.getOpenInOtherWindowActivity());

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

    @Override
    @VisibleForTesting
    public void maybeMergeTabs() {
        assert !mDestroyed;
        if (!isTabModelMergingEnabled() || mDestroyed) return;

        killOtherTask();
        RecordUserAction.record("Android.MergeState.Live");
        mTabModelOrchestratorSupplier.get().mergeState();
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

    @Override
    public void moveTabsToOtherWindow(List<Tab> tabs, @NewWindowAppSource int source) {
        if (MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE) == 1) {
            moveTabsToNewWindow(tabs, source);
            return;
        }

        Intent intent = mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent();
        if (intent == null) return;

        onMultiInstanceModeStarted();
        ReparentingTabsTask.from(tabs)
                .begin(
                        mActivity,
                        intent,
                        /* startActivityOptions= */ null,
                        /* finalizeCallback= */ null);
        RecordUserAction.record("MobileMenuMoveToOtherWindow");
    }

    @Override
    public @Nullable Intent createNewWindowIntent(boolean isIncognito) {
        assert !isIncognito : "Opening an incognito window isn't supported";
        assert mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()
                        || mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                        || mMultiWindowModeStateDispatcher.isInMultiDisplayMode()
                : "Current windowing mode doesn't support opening a new window";

        Intent intent = mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent();
        if (intent == null) {
            return null;
        }

        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);

        // Remove LAUNCH_ADJACENT flag if shouldOpenInAdjacentWindow() is false and if the Activity
        // is in a full screen window.
        if (!mActivity.isInMultiWindowMode() && !MultiWindowUtils.shouldOpenInAdjacentWindow()) {
            intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }

        return intent;
    }
    // TODO(crbug.com/455922432): Clean up the umaAction param.
    protected void openNewWindow(
            String umaAction, boolean incognito, @NewWindowAppSource int source) {
        Intent intent = createNewWindowIntent(incognito);
        if (intent == null) {
            return;
        }

        onMultiInstanceModeStarted();
        mActivity.startActivity(intent);
        RecordHistogram.recordEnumeratedHistogram(
                MultiInstanceManager.NEW_WINDOW_APP_SOURCE_HISTOGRAM,
                source,
                NewWindowAppSource.NUM_ENTRIES);
        RecordUserAction.record(umaAction);
    }

    @Override
    public Pair<Integer, Integer> allocInstanceId(
            int windowId, int taskId, boolean preferNew, @SupportedProfileType int profileType) {
        return Pair.create(0, InstanceAllocationType.DEFAULT); // Use a default index 0.
    }

    @Override
    public void setCurrentDisplayIdForTesting(int displayId) {
        var oldValue = mDisplayId;
        mDisplayId = displayId;
        ResettersForTesting.register(() -> mDisplayId = oldValue);
    }

    @Override
    public @Nullable DisplayListener getDisplayListenerForTesting() {
        return mDisplayListener;
    }

    @Override
    public @Nullable TabModelSelectorTabModelObserver getTabModelObserverForTesting() {
        return mTabModelObserver;
    }

    @Override
    public void setTabModelObserverForTesting(TabModelSelectorTabModelObserver tabModelObserver) {
        mTabModelObserver = tabModelObserver;
    }

    @Override
    public int getCurrentInstanceId() {
        return TabWindowManager.INVALID_WINDOW_ID;
    }

    @Override
    public void cleanupSyncedTabGroupsIfOnlyInstance(TabModelSelector selector) {
        // Should only happen in tests.
        if (BuildConfig.IS_FOR_TEST && selector == null) return;

        assert selector != null;

        TabModelUtils.runOnTabStateInitialized(
                selector,
                (TabModelSelector initializedSelector) -> {
                    if (mMultiWindowModeStateDispatcher.isMultiInstanceRunning()) return;
                    cleanupSyncedTabGroups(initializedSelector);
                });
    }

    protected void cleanupSyncedTabGroups(TabModelSelector selector) {
        TabGroupModelFilter filter =
                selector.getTabGroupModelFilterProvider().getTabGroupModelFilter(false);

        assumeNonNull(filter);
        Profile profile = filter.getTabModel().getProfile();
        if (profile == null || !TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) return;

        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        if (tabGroupSyncService != null) {
            TabGroupSyncUtils.unmapLocalIdsNotInTabGroupModelFilter(tabGroupSyncService, filter);
        }
    }

    public static void setAdjacentWindowActivitySupplierForTesting(
            Supplier<ChromeTabbedActivity> supplier) {
        sActivitySupplierForTesting = supplier;
        ResettersForTesting.register(() -> sActivitySupplierForTesting = null);
    }
}
