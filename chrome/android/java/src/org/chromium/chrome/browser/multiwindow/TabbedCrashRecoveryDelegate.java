// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.SparseIntArray;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Delegate to help recover ChromeTabbedActivity windows from a previous session during app launch
 * after a crash.
 */
@NullMarked
public class TabbedCrashRecoveryDelegate {
    private static @Nullable TabbedCrashRecoveryDelegate sInstance;

    private boolean mCrashRecoveryInProgress;
    private Map<Integer, AppTask> mPreRecoveryAppTasks = new HashMap<>();
    private final List<CrashRecoveryWindowInfo> mNonVisibleWindows = new ArrayList<>();
    private final List<CrashRecoveryWindowInfo> mVisibleWindows = new ArrayList<>();

    private TabbedCrashRecoveryDelegate() {}

    /* package */ static TabbedCrashRecoveryDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedCrashRecoveryDelegate();
        }
        return sInstance;
    }

    /* package */ static void resetForTesting() {
        sInstance = null;
    }

    /**
     * Shows a crash recovery prompt if applicable, when the {@link ModalDialogManager} for the host
     * activity is available.
     *
     * @param modalDialogManagerSupplier Supplier for ModalDialogManager.
     * @param hostActivity The host activity where the prompt will be displayed.
     */
    /* package */ void initiateCrashRecovery(
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            ChromeTabbedActivity hostActivity,
            List<CrashRecoveryWindowInfo> crashedWindows) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)) return;
        if (mCrashRecoveryInProgress) return;

        // Reset state before processing a new crash recovery request to avoid using stale state.
        resetState();

        if (crashedWindows.size() == 1) {
            // If there is only one window to recover (assumed to be the current window), do not
            // show the crash recovery prompt.
            return;
        }

        mPreRecoveryAppTasks = getAppTasksById(hostActivity);
        int crashedWindowTaskCount = 0;
        for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
            int windowId = windowInfo.windowId;
            // Exclude host activity from crash recovery task.
            if (hostActivity.getWindowId() == windowInfo.windowId) continue;
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            if (mPreRecoveryAppTasks.containsKey(persistedTaskId)) {
                crashedWindowTaskCount++;
            }

            if (!windowInfo.isVisible) mNonVisibleWindows.add(windowInfo);
            else mVisibleWindows.add(windowInfo);
        }

        if (crashedWindowTaskCount == crashedWindows.size() - 1) {
            // If all crashed windows (other than the current window) have live tasks already, do
            // not show the crash recovery prompt.
            return;
        }

        modalDialogManagerSupplier.addSyncObserverAndCallIfNonNull(
                new Callback<>() {
                    @Override
                    public void onResult(ModalDialogManager modalDialogManager) {
                        // TODO: Show recovery dialog.
                        modalDialogManagerSupplier.removeObserver(this);
                    }
                });
    }

    /**
     * Registers successful recovery of a window after a crash.
     *
     * @param activity The activity that was created when a window was successfully recovered after
     *     a crash.
     */
    /* package */ void registerRecovery(ChromeTabbedActivity activity) {
        // TODO: Implement this method.
    }

    private static Map<Integer, AppTask> getAppTasksById(Context context) {
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<AppTask> appTasks = activityManager.getAppTasks();
        Map<Integer, AppTask> results = new HashMap<>();
        for (AppTask task : appTasks) {
            ActivityManager.RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (info != null) results.put(info.taskId, task);
        }
        return results;
    }

    /* package */ void restoreWindows(ChromeTabbedActivity hostActivity) {
        SparseIntArray initialTabbedActivityIds =
                MultiWindowUtils.getWindowIdsOfRunningTabbedActivities();
        assert initialTabbedActivityIds.size() == 1
                : "Expected exactly one host activity to be present before initiating crash"
                        + " recovery.";

        mCrashRecoveryInProgress = true;

        // TODO: Restore non-visible windows.

        boolean isInMultiWindowMode = hostActivity.isInMultiWindowMode();
        for (CrashRecoveryWindowInfo visibleWindow : mVisibleWindows) {
            int windowId = visibleWindow.windowId;
            Rect bounds = visibleWindow.bounds;
            restoreVisibleWindow(hostActivity, windowId, bounds, isInMultiWindowMode);
        }

        mCrashRecoveryInProgress = false;
    }

    private void restoreVisibleWindow(
            ChromeTabbedActivity hostActivity,
            int windowId,
            @Nullable Rect bounds,
            boolean openAdjacently) {
        ActivityOptions options = null;
        if (bounds != null && !bounds.isEmpty()) {
            options = ActivityOptions.makeBasic();
            options.setLaunchBounds(bounds);
        }
        Bundle bundle = (options != null) ? options.toBundle() : null;

        // Clear crash recovery state for instance.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, /* isRecoverable= */ false);

        // If this window already has a live task, finish it before starting a new task.
        int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
        if (mPreRecoveryAppTasks.containsKey(persistedTaskId)) {
            mPreRecoveryAppTasks.get(persistedTaskId).finishAndRemoveTask();
        }

        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        hostActivity,
                        windowId,
                        /* preferNew= */ false,
                        openAdjacently,
                        NewWindowAppSource.CRASH_RECOVERY);
        hostActivity.startActivity(intent, bundle);
    }

    private void resetState() {
        mCrashRecoveryInProgress = false;
        mPreRecoveryAppTasks.clear();
        mNonVisibleWindows.clear();
        mVisibleWindows.clear();
    }
}
