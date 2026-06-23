// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.util.SparseIntArray;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Delegate to help recover ChromeTabbedActivity windows from a previous session during app launch
 * after a crash.
 */
@NullMarked
public class TabbedCrashRecoveryDelegate {
    private static @Nullable TabbedCrashRecoveryDelegate sInstance;

    private long mRecoveryStartTime;
    private boolean mCrashRecoveryInProgress;
    private Map<Integer, AppTask> mPreRecoveryAppTasks = new HashMap<>();
    private final List<CrashRecoveryWindowInfo> mNonVisibleWindows = new ArrayList<>();
    private final List<CrashRecoveryWindowInfo> mVisibleWindows = new ArrayList<>();
    private final Set<Integer> mWindowIdsPendingRecovery = new HashSet<>();

    private TabbedCrashRecoveryDelegate() {}

    public static TabbedCrashRecoveryDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedCrashRecoveryDelegate();
        }
        return sInstance;
    }

    /* package */ static void setInstanceForTesting(TabbedCrashRecoveryDelegate delegate) {
        sInstance = delegate;
        ResettersForTesting.register(() -> sInstance = null);
    }

    /**
     * Registers successful recovery of a window after a crash.
     *
     * @param windowId The id of the window that was successfully recovered after a crash.
     */
    public void registerRecovery(int windowId) {
        boolean updated = mWindowIdsPendingRecovery.remove(windowId);
        if (updated && mWindowIdsPendingRecovery.isEmpty()) {
            // After the last window is recovered, update success metrics.
            long duration = TimeUtils.elapsedRealtimeMillis() - mRecoveryStartTime;
            RecordHistogram.recordTimesHistogram(
                    "Android.MultiWindow.CrashRecoveryDuration", duration);
            RecordUserAction.record("Android.MultiWindow.CrashRecoveryCompleted");
        }
    }

    /**
     * Shows a crash recovery prompt if applicable, when the {@link ModalDialogManager} for the host
     * activity is available.
     *
     * @param modalDialogManagerSupplier Supplier for ModalDialogManager.
     * @param hostActivity The host activity where the prompt will be displayed.
     * @param crashedWindows A list of windows that need to be recovered.
     */
    /* package */ void initiateCrashRecovery(
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            ChromeTabbedActivity hostActivity,
            List<CrashRecoveryWindowInfo> crashedWindows) {
        if (!ChromeFeatureList.sSessionRestoreAfterCrash.isEnabled()) return;

        if (mCrashRecoveryInProgress) return;

        // Reset state before processing a new crash recovery request to avoid using stale state.
        resetState();

        RecordHistogram.recordExactLinearHistogram(
                "Android.MultiWindow.CrashRecoveryWindowCount",
                crashedWindows.size(),
                TabWindowManager.MAX_SELECTORS_1000 + 1);

        if (crashedWindows.size() == 1
                && crashedWindows.get(0).windowId == hostActivity.getWindowId()) {
            // If there is only one window to recover (assumed to be the current window), do not
            // show the crash recovery prompt.
            return;
        }

        mPreRecoveryAppTasks = getAppTasksById(hostActivity);
        int nonHostCrashedWindowCount = 0;
        int crashedWindowTaskCount = 0;
        for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
            int windowId = windowInfo.windowId;
            // Exclude host activity from crash recovery task.
            if (hostActivity.getWindowId() == windowInfo.windowId) continue;
            nonHostCrashedWindowCount++;
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            if (mPreRecoveryAppTasks.containsKey(persistedTaskId)) {
                crashedWindowTaskCount++;
            }

            mWindowIdsPendingRecovery.add(windowId);
            if (!windowInfo.isVisible) mNonVisibleWindows.add(windowInfo);
            else mVisibleWindows.add(windowInfo);
        }

        if (crashedWindowTaskCount == nonHostCrashedWindowCount) {
            // If all crashed windows (other than the current window) have live tasks already, do
            // not show the crash recovery prompt.
            for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
                int windowId = windowInfo.windowId;
                if (windowId == hostActivity.getWindowId()) continue;
                ChromeMultiInstancePersistentStore.writeIsRecoverable(
                        windowId, /* isRecoverable= */ false);
            }
            return;
        }

        modalDialogManagerSupplier.addSyncObserverAndCallIfNonNull(
                new Callback<>() {
                    @Override
                    public void onResult(ModalDialogManager modalDialogManager) {
                        showRecoveryDialog(modalDialogManager, hostActivity, crashedWindows);
                        modalDialogManagerSupplier.removeObserver(this);
                    }
                });
    }

    private void showRecoveryDialog(
            ModalDialogManager modalDialogManager,
            ChromeTabbedActivity hostActivity,
            List<CrashRecoveryWindowInfo> crashedWindows) {
        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                            // When the recovery dialog is dismissed, cleanup recovery state for
                            // non-recovered windows since this data will now be stale.
                            for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
                                int windowId = windowInfo.windowId;
                                if (windowId == hostActivity.getWindowId()) continue;
                                ChromeMultiInstancePersistentStore.writeIsRecoverable(
                                        windowId, /* isRecoverable= */ false);
                                int persistedTaskId =
                                        ChromeMultiInstancePersistentStore.readTaskId(windowId);
                                if (mPreRecoveryAppTasks.containsKey(persistedTaskId)) {
                                    mPreRecoveryAppTasks.get(persistedTaskId).finishAndRemoveTask();
                                }
                            }
                            mCrashRecoveryInProgress = false;
                        }
                    }

                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        switch (buttonType) {
                            case ModalDialogProperties.ButtonType.NEGATIVE:
                                modalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                                break;
                            case ModalDialogProperties.ButtonType.POSITIVE:
                                RecordUserAction.record("Android.MultiWindow.CrashRecoveryOptIn");
                                restoreWindows(hostActivity);
                                modalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                                break;
                        }
                    }
                };

        int pendingWindows = mWindowIdsPendingRecovery.size();
        String positiveButtonText =
                hostActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.crash_recovery_dialog_positive_button_text,
                                pendingWindows,
                                pendingWindows);

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                hostActivity.getString(R.string.crash_recovery_dialog_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                hostActivity.getString(R.string.crash_recovery_dialog_message))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                hostActivity.getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();

        RecordUserAction.record("Android.MultiWindow.CrashRecoveryDialogShown");
        modalDialogManager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
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
        mRecoveryStartTime = TimeUtils.elapsedRealtimeMillis();
        RecordUserAction.record("Android.MultiWindow.CrashRecoveryInitiated");

        boolean isInMultiWindowMode = hostActivity.isInMultiWindowMode();
        for (CrashRecoveryWindowInfo nonVisibleWindow : mNonVisibleWindows) {
            int windowId = nonVisibleWindow.windowId;
            restoreNonVisibleWindow(hostActivity, windowId, isInMultiWindowMode);
        }

        for (CrashRecoveryWindowInfo visibleWindow : mVisibleWindows) {
            int windowId = visibleWindow.windowId;
            restoreVisibleWindow(hostActivity, windowId, isInMultiWindowMode);
        }

        mCrashRecoveryInProgress = false;
    }

    private void restoreNonVisibleWindow(
            ChromeTabbedActivity hostActivity, int windowId, boolean openAdjacently) {
        // Clear crash recovery state for instance.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, /* isRecoverable= */ false);
        int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
        if (mPreRecoveryAppTasks.containsKey(persistedTaskId)) {
            // Skip starting a new task because this instance already has a live task in the
            // background.
            registerRecovery(windowId);
            return;
        }

        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        hostActivity,
                        windowId,
                        /* preferNew= */ false,
                        openAdjacently,
                        NewWindowAppSource.CRASH_RECOVERY);
        hostActivity.startActivity(intent);
    }

    private void restoreVisibleWindow(
            ChromeTabbedActivity hostActivity, int windowId, boolean openAdjacently) {
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
        hostActivity.startActivity(intent);
    }

    @VisibleForTesting
    /* package */ void resetState() {
        mCrashRecoveryInProgress = false;
        mPreRecoveryAppTasks.clear();
        mNonVisibleWindows.clear();
        mVisibleWindows.clear();
        mWindowIdsPendingRecovery.clear();
    }
}
