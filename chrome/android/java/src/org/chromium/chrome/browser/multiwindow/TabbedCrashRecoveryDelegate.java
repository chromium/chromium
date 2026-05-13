// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

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

    private TabbedCrashRecoveryDelegate() {}

    /* package */ static TabbedCrashRecoveryDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedCrashRecoveryDelegate();
        }
        return sInstance;
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

        if (crashedWindows.size() == 1) {
            // If there is only one window to recover (assumed to be the current window), do not
            // show the crash recovery prompt.
            return;
        }

        Map<Integer, AppTask> preRecoveryAppTasks = getAppTasksById(hostActivity);
        int crashedWindowTaskCount = 0;
        for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
            int windowId = windowInfo.windowId;
            // Exclude host activity from crash recovery task.
            if (hostActivity.getWindowId() == windowInfo.windowId) continue;
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            if (preRecoveryAppTasks.containsKey(persistedTaskId)) {
                crashedWindowTaskCount++;
            }
            // TODO: Segregate into windows that were visible / non-visible pre-crash.
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
}
