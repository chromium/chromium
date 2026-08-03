// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.ApplicationExitInfo;
import android.content.Intent;
import android.os.Build;
import android.util.SparseIntArray;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
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
    private static final String TAG = "TabbedCrashRecovery";

    private static @Nullable TabbedCrashRecoveryDelegate sInstance;

    private long mRecoveryStartTime;
    private boolean mIsCrashRecoveryEligible;
    private @Nullable List<CrashRecoveryWindowInfo> mCrashedWindows;
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
            Log.i(TAG, "Successfully completed crash recovery.");
            resetState();
        }
    }

    /**
     * Shows a crash recovery dialog if applicable, when the {@link ModalDialogManager} for the host
     * activity is available.
     *
     * @param modalDialogManagerSupplier Supplier for ModalDialogManager.
     * @param activity The host activity where the prompt will be displayed.
     * @return true if the dialog was shown/triggered; false otherwise.
     */
    public boolean maybeShowCrashRecoveryDialog(
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            Activity activity) {
        if (!mIsCrashRecoveryEligible) return false;
        if (!(activity instanceof ChromeTabbedActivity hostActivity)) return false;

        List<CrashRecoveryWindowInfo> crashedWindows = mCrashedWindows;
        assert crashedWindows != null : "mCrashedWindows should be set.";

        // Reset state before processing a new crash recovery request to avoid using stale state.
        resetState();

        // Avoid showing the dialog on an incognito host window. We will clean up crash recovery
        // data because of the uncertainty surrounding when the next regular window will be opened,
        // which could happen after several incognito windows are opened, at which point recovering
        // these crashed windows may be confusing.
        if (hostActivity.isIncognitoWindow()) {
            Map<Integer, AppTask> appTasks = MultiWindowUtils.getAppTasksById(hostActivity);
            for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
                int windowId = windowInfo.windowId;
                if (windowId == hostActivity.getWindowId()) continue;
                // Since recovery is cancelled, mark all windows as non-recoverable. Additionally,
                // finish tasks for windows without any normal tabs to avoid keeping unusable tasks.
                cleanUpWindow(windowId, appTasks, MultiWindowUtils.hasNoNormalTabs(windowId));
            }
            return false;
        }

        // If the only crashed window is the host activity itself, do not show the dialog.
        if (crashedWindows.size() == 1
                && crashedWindows.get(0).windowId == hostActivity.getWindowId()) {
            return false;
        }

        Map<Integer, AppTask> appTasks = MultiWindowUtils.getAppTasksById(hostActivity);
        int nonHostCrashedWindowCount = 0;
        int crashedWindowTaskCount = 0;
        for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
            int windowId = windowInfo.windowId;
            // Exclude host activity from crash recovery task.
            if (hostActivity.getWindowId() == windowInfo.windowId) continue;

            // Windows with no regular tabs (e.g. empty or windows with incognito-only tabs) should
            // not be usable after a crash. Clean them up (e.g. finish their live tasks and mark
            // them as non-recoverable) so we don't restore them or leave orphaned, unusable tasks
            // in Android Recents.
            if (MultiWindowUtils.hasNoNormalTabs(windowId)) {
                cleanUpWindow(windowId, appTasks, /* shouldFinishTask= */ true);
                continue;
            }

            nonHostCrashedWindowCount++;
            if (MultiWindowUtils.isTaskAlive(windowId, appTasks)) {
                crashedWindowTaskCount++;
            }

            mWindowIdsPendingRecovery.add(windowId);
            if (!windowInfo.isVisible) mNonVisibleWindows.add(windowInfo);
            else mVisibleWindows.add(windowInfo);
        }

        // If there are no recoverable windows pending recovery (e.g. they were all empty or
        // incognito windows that got cleaned up above), skip showing the dialog.
        if (mWindowIdsPendingRecovery.isEmpty()) {
            resetState();
            return false;
        }

        if (crashedWindowTaskCount == nonHostCrashedWindowCount
                && !hostActivity.isInMultiWindowMode()) {
            // If all crashed windows (other than the current window) have live tasks already, and
            // the host is not in multi-window mode, do not show the crash recovery prompt.
            Log.i(
                    TAG,
                    "Skipping crash recovery dialog because all other windows already have live"
                            + " tasks and host is not in multi-window mode.");
            for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
                int windowId = windowInfo.windowId;
                if (windowId == hostActivity.getWindowId()) continue;
                cleanUpWindow(windowId, /* appTasks= */ null, /* shouldFinishTask= */ false);
            }
            resetState();
            return false;
        }

        modalDialogManagerSupplier.addSyncObserverAndCallIfNonNull(
                new Callback<>() {
                    @Override
                    public void onResult(ModalDialogManager modalDialogManager) {
                        showRecoveryDialog(modalDialogManager, hostActivity, appTasks);
                        modalDialogManagerSupplier.removeObserver(this);
                    }
                });
        return true;
    }

    /**
     * Flags a pending crash recovery if the last session ended in a crash/ANR and there are
     * recoverable windows, so that recovery can be handled when the next ChromeTabbedActivity
     * starts.
     */
    /* package */ void maybeDeferCrashRecovery() {
        if (didLastSessionCrashWithRecoverableWindows()) {
            ChromeMultiInstancePersistentStore.writeIsCrashRecoveryPending(true);
        }
    }

    /**
     * Evaluates and caches crash recovery metadata synchronously on ChromeTabbedActivity
     * initialization.
     */
    /* package */ void initializeCrashRecoveryMetadata() {
        if (!ChromeFeatureList.sSessionRestoreAfterCrash.isEnabled()) {
            return;
        }

        // This method runs synchronously inside onCreate() of ChromeTabbedActivity on the UI
        // thread. Because Android's main thread message loop processes onCreate() synchronously to
        // completion before handling any subsequent idle/deferred tasks, this method is guaranteed
        // to execute and read SharedPreferences before BrowserExitReasonTracker clears them during
        // deferred startup.
        boolean isRecoveryPending = ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending();
        boolean didLastSessionCrash = didLastSessionCrashWithRecoverableWindows();
        boolean shouldInitializeMetadata = isRecoveryPending || didLastSessionCrash;

        if (shouldInitializeMetadata) {
            Log.i(
                    TAG,
                    "Crash recovery initiated. Pending recovery: %b, New crash detected: %b",
                    isRecoveryPending,
                    didLastSessionCrash);
            // Lazy load mCrashedWindows if it was not loaded yet (e.g. if isRecoveryNeeded
            // evaluated to true due to a pending recovery flag from a prior session, which
            // short-circuited it during didLastSessionCrashWithRecoverableWindows() evaluation).
            if (mCrashedWindows == null) {
                mCrashedWindows = ChromeMultiInstancePersistentStore.readCrashRecoveryData();
            }

            assert !mCrashedWindows.isEmpty()
                    : "Expected crash-recoverable window list to be non-empty.";

            // Log metric immediately upon caching. Placing it here guarantees that all crash starts
            // (including single-window post-crash launches) are logged accurately, while preventing
            // any metric pollution from normal non-crash launches.
            RecordHistogram.recordExactLinearHistogram(
                    "Android.MultiWindow.CrashRecoveryWindowCount",
                    mCrashedWindows.size(),
                    TabWindowManager.MAX_SELECTORS_1000 + 1);

            // Potentially show the crash recovery dialog if there is at least one crashed window.
            // At this time, we cannot always evaluate whether the host activity is also a crashed
            // window (e.g. on desktop devices, a brand new window is likely to be launched in a new
            // process), so we will defer to until we have this information to decide whether the
            // recovery dialog needs to be shown.
            mIsCrashRecoveryEligible = true;
            Log.i(
                    TAG,
                    "Multi-window crash recovery metadata initialized. Total crashed windows: %d.",
                    mCrashedWindows.size());

            // Reset persisted pending state since metadata has been successfully processed.
            ChromeMultiInstancePersistentStore.writeIsCrashRecoveryPending(false);
        }
    }

    /**
     * Returns whether the last session ended in a crash/ANR and there are recoverable
     * ChromeTabbedActivity windows.
     */
    /* package */ boolean didLastSessionCrashWithRecoverableWindows() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return false;

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        if (!prefs.contains(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON)) {
            return false;
        }
        int reason = prefs.readInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON);
        Log.i(TAG, "Last session exit reason: %d", reason);
        boolean isCrash =
                reason == ApplicationExitInfo.REASON_CRASH
                        || reason == ApplicationExitInfo.REASON_CRASH_NATIVE
                        || reason == ApplicationExitInfo.REASON_ANR;
        if (!isCrash) {
            // Clear crash recovery state for all windows when we detect an exit reason ineligible
            // for crash recovery so that we don't attempt stale crash recovery in the future.
            // Do not clear the state if there is already a pending recovery from a prior session.
            if (!ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending()) {
                for (int windowId : ChromeMultiInstancePersistentStore.readAllInstanceIds()) {
                    cleanUpWindow(windowId, /* appTasks= */ null, /* shouldFinishTask= */ false);
                }
            }
            return false;
        }

        if (mCrashedWindows == null) {
            mCrashedWindows = ChromeMultiInstancePersistentStore.readCrashRecoveryData();
        }
        return !mCrashedWindows.isEmpty();
    }

    private void showRecoveryDialog(
            ModalDialogManager modalDialogManager,
            ChromeTabbedActivity hostActivity,
            Map<Integer, AppTask> appTasks) {
        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                            // When the recovery dialog is dismissed, cleanup recovery state for
                            // non-recovered windows since this data will now be stale.
                            for (int windowId : mWindowIdsPendingRecovery) {
                                cleanUpWindow(windowId, appTasks, /* shouldFinishTask= */ true);
                            }
                            resetState();
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
                                restoreWindows(hostActivity, appTasks);
                                modalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                                break;
                        }
                    }
                };

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                hostActivity.getString(R.string.crash_recovery_dialog_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                hostActivity.getString(R.string.crash_recovery_dialog_message))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                hostActivity.getString(
                                        R.string.crash_recovery_dialog_positive_button_text))
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

    /* package */ void restoreWindows(
            ChromeTabbedActivity hostActivity, Map<Integer, AppTask> appTasks) {
        SparseIntArray initialTabbedActivityIds =
                MultiWindowUtils.getWindowIdsOfRunningTabbedActivities();
        assert initialTabbedActivityIds.size() == 1
                : "Expected exactly one host activity to be present before initiating crash"
                        + " recovery.";

        mRecoveryStartTime = TimeUtils.elapsedRealtimeMillis();
        RecordUserAction.record("Android.MultiWindow.CrashRecoveryInitiated");
        Log.i(
                TAG,
                "Initiating restoration of %d non-visible windows and %d visible windows.",
                mNonVisibleWindows.size(),
                mVisibleWindows.size());

        boolean isInMultiWindowMode = hostActivity.isInMultiWindowMode();

        // Restore non-visible windows prior to visible ones to ensure that visible windows end up
        // at the top of the stack. Additionally, the windows restored first may be automatically
        // minimized by Android to honor the system-enforced on-screen visible task limit.
        for (CrashRecoveryWindowInfo nonVisibleWindow : mNonVisibleWindows) {
            int windowId = nonVisibleWindow.windowId;
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            AppTask task = appTasks.get(persistedTaskId);
            restoreWindow(hostActivity, windowId, isInMultiWindowMode, task);
        }

        for (CrashRecoveryWindowInfo visibleWindow : mVisibleWindows) {
            int windowId = visibleWindow.windowId;
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            AppTask task = appTasks.get(persistedTaskId);
            restoreWindow(hostActivity, windowId, isInMultiWindowMode, task);
        }

        resetStateExceptIdsPendingRecovery();
    }

    private void restoreWindow(
            ChromeTabbedActivity hostActivity,
            int windowId,
            boolean isInMultiWindowMode,
            @Nullable AppTask task) {
        // Clear crash recovery state for instance.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, /* isRecoverable= */ false);

        if (task != null) {
            if (!isInMultiWindowMode) {
                // When we have a subset of tasks that sustained a crash while others got killed,
                // and Chrome starts in a fullscreen window after the crash, we will only target
                // creating new tasks for windows whose tasks got killed and skip recovery for
                // windows with live tasks. This is because recovery in non-multi-window mode will
                // result in at most one visible window at the end of recovery and keep all other
                // tasks in the background anyway.
                registerRecovery(windowId);
                return;
            }
            // If the host is in multi-window mode, bring the restored window to the foreground by
            // finishing an existing live task before starting a new task. Bringing an existing live
            // task to the foreground via the moveTaskToFront() Android API is known to fail when
            // there is no activity in the task (as in the case of a task that sustains a crash).
            task.finishAndRemoveTask();
        }
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        hostActivity,
                        windowId,
                        /* preferNew= */ false,
                        isInMultiWindowMode,
                        NewWindowAppSource.CRASH_RECOVERY);
        hostActivity.startActivity(intent);
    }

    /** Cleans up a crashed window's state, and optionally finishes its associated task. */
    private void cleanUpWindow(
            int windowId, @Nullable Map<Integer, AppTask> appTasks, boolean shouldFinishTask) {
        ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, /* isRecoverable= */ false);
        if (shouldFinishTask && appTasks != null) {
            int persistedTaskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            AppTask task = appTasks.get(persistedTaskId);
            if (task != null) {
                task.finishAndRemoveTask();
                appTasks.remove(persistedTaskId);
            }
        }
    }

    @VisibleForTesting
    /* package */ void resetState() {
        mWindowIdsPendingRecovery.clear();
        resetStateExceptIdsPendingRecovery();
    }

    private void resetStateExceptIdsPendingRecovery() {
        mIsCrashRecoveryEligible = false;
        mCrashedWindows = null;
        mNonVisibleWindows.clear();
        mVisibleWindows.clear();
    }

    boolean isCrashRecoveryEligibleForTesting() {
        return mIsCrashRecoveryEligible;
    }

    @Nullable List<CrashRecoveryWindowInfo> getCrashedWindowsForTesting() {
        return mCrashedWindows;
    }

    List<CrashRecoveryWindowInfo> getNonVisibleWindowsForTesting() {
        return mNonVisibleWindows;
    }

    List<CrashRecoveryWindowInfo> getVisibleWindowsForTesting() {
        return mVisibleWindows;
    }

    Set<Integer> getWindowIdsPendingRecoveryForTesting() {
        return mWindowIdsPendingRecovery;
    }
}
