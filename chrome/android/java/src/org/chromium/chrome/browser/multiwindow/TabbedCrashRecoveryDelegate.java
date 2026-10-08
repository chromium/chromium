// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.ApplicationExitInfo;
import android.os.Build;
import android.util.SparseIntArray;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Delegate to help recover ChromeTabbedActivity windows from a previous session during app launch
 * after a crash.
 */
@NullMarked
/* package */ class TabbedCrashRecoveryDelegate extends BaseTabbedStartupDelegate {
    private static final String TAG = "TabbedCrashRecovery";

    private static @Nullable TabbedCrashRecoveryDelegate sInstance;

    private final List<CrashRecoveryWindowInfo> mNonVisibleWindows = new ArrayList<>();
    private final List<CrashRecoveryWindowInfo> mVisibleWindows = new ArrayList<>();

    private boolean mIsCrashRecoveryEligible;
    private @Nullable List<CrashRecoveryWindowInfo> mCrashedWindows;

    private TabbedCrashRecoveryDelegate() {}

    // BaseTabbedStartupDelegate implementation.
    @Override
    protected void onRestorationInitiated() {
        RecordUserAction.record("Android.MultiWindow.CrashRecoveryInitiated");
    }

    @Override
    protected void onAllWindowsRestored(long durationMillis) {
        RecordHistogram.recordTimesHistogram(
                "Android.MultiWindow.CrashRecoveryDuration", durationMillis);
        RecordUserAction.record("Android.MultiWindow.CrashRecoveryCompleted");
        Log.i(TAG, "Successfully completed crash recovery.");
        resetState();
    }

    @Override
    protected void resetState() {
        super.resetState();
        ChromeMultiInstancePersistentStore.writeIsCrashRecoveryPending(false);
        resetStateExceptIdsPendingRestoration();
    }

    /* package */ static TabbedCrashRecoveryDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedCrashRecoveryDelegate();
        }
        return sInstance;
    }

    /* package */ boolean maybeShowCrashRecoveryDialog(
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
                int taskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
                cleanUpWindow(
                        windowId,
                        MultiWindowUtils.hasNoNormalTabs(windowId) ? appTasks.get(taskId) : null);
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
        int nonHostCrashedTaskCount = 0;
        for (CrashRecoveryWindowInfo windowInfo : crashedWindows) {
            int windowId = windowInfo.windowId;
            // Exclude host activity from crash recovery task.
            if (hostActivity.getWindowId() == windowInfo.windowId) continue;

            int taskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            AppTask task = appTasks.get(taskId);

            // Windows with no regular tabs (e.g. empty or windows with incognito-only tabs) should
            // not be usable after a crash. Clean them up (e.g. finish their live tasks and mark
            // them as non-recoverable) so we don't restore them or leave orphaned, unusable tasks
            // in Android Recents.
            if (MultiWindowUtils.hasNoNormalTabs(windowId)) {
                cleanUpWindow(windowId, task);
                continue;
            }

            nonHostCrashedWindowCount++;
            if (task != null) {
                nonHostCrashedTaskCount++;
            }

            if (!windowInfo.isVisible) mNonVisibleWindows.add(windowInfo);
            else mVisibleWindows.add(windowInfo);
        }

        // If there are no recoverable windows pending restoration (e.g. they were all empty or
        // incognito windows that got cleaned up above), skip showing the dialog.
        if (nonHostCrashedWindowCount == 0) {
            resetState();
            return false;
        }

        if (nonHostCrashedTaskCount == nonHostCrashedWindowCount
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
                cleanUpWindow(windowId, /* task= */ null);
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

    /* package */ void maybeDeferCrashRecovery() {
        // If there is no ChromeTabbedActivity to initiate crash recovery when the browser process
        // starts after a crash, track this as a pending task that can be addressed when the next
        // ChromeTabbedActivity is registered with the orchestrator.
        if (didLastSessionCrashWithRecoverableWindows()) {
            ChromeMultiInstancePersistentStore.writeIsCrashRecoveryPending(true);
        }
    }

    /**
     * Initializes crash recovery metadata if the previous session crashed with recoverable windows
     * or if a deferred crash recovery is pending.
     *
     * @return {@code true} if crash recovery metadata was initialized and the session is eligible
     *     for crash recovery; {@code false} otherwise.
     */
    /* package */ boolean initializeCrashRecoveryMetadata() {
        if (!MultiWindowUtils.isSessionRestoreAfterCrashEnabled()) {
            return false;
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
            // Lazy load mCrashedWindows if it was not loaded yet (e.g. if shouldInitializeMetadata
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
        }
        return mIsCrashRecoveryEligible;
    }

    @VisibleForTesting
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
            return false;
        }

        if (mCrashedWindows == null) {
            mCrashedWindows = ChromeMultiInstancePersistentStore.readCrashRecoveryData();
        }
        return !mCrashedWindows.isEmpty();
    }

    @VisibleForTesting
    /* package */ void restoreWindows(
            ChromeTabbedActivity hostActivity, Map<Integer, AppTask> appTasks) {
        SparseIntArray initialTabbedActivityIds =
                MultiWindowUtils.getWindowIdsOfRunningTabbedActivities();
        assert initialTabbedActivityIds.size() == 1
                : "Expected exactly one host activity to be present before initiating crash"
                        + " recovery.";

        Log.i(
                TAG,
                "Initiating restoration of %d non-visible windows and %d visible windows.",
                mNonVisibleWindows.size(),
                mVisibleWindows.size());

        // Restore non-visible windows prior to visible ones to ensure that visible windows end up
        // at the top of the stack. Additionally, the windows restored first may be automatically
        // minimized by Android to honor the system-enforced on-screen visible task limit.
        for (CrashRecoveryWindowInfo nonVisibleWindow : mNonVisibleWindows) {
            int windowId = nonVisibleWindow.windowId;
            int taskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            restoreWindow(
                    hostActivity,
                    windowId,
                    appTasks.get(taskId),
                    NewWindowAppSource.CRASH_RECOVERY);
        }

        for (CrashRecoveryWindowInfo visibleWindow : mVisibleWindows) {
            int windowId = visibleWindow.windowId;
            int taskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            restoreWindow(
                    hostActivity,
                    windowId,
                    appTasks.get(taskId),
                    NewWindowAppSource.CRASH_RECOVERY);
        }

        // Clear eligibility and cached window lists immediately so stale state is not retained or
        // re-triggered while restored window activities launch asynchronously.
        resetStateExceptIdsPendingRestoration();
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
                            for (var windowList : List.of(mNonVisibleWindows, mVisibleWindows)) {
                                for (CrashRecoveryWindowInfo windowInfo : windowList) {
                                    int windowId = windowInfo.windowId;
                                    int taskId =
                                            ChromeMultiInstancePersistentStore.readTaskId(windowId);
                                    cleanUpWindow(windowId, appTasks.get(taskId));
                                }
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

    private void resetStateExceptIdsPendingRestoration() {
        mIsCrashRecoveryEligible = false;
        mCrashedWindows = null;
        mNonVisibleWindows.clear();
        mVisibleWindows.clear();
    }

    /* package */ static void setInstanceForTesting(
            @Nullable TabbedCrashRecoveryDelegate delegate) {
        sInstance = delegate;
        ResettersForTesting.register(() -> sInstance = null);
    }

    /* package */ boolean isCrashRecoveryEligibleForTesting() {
        return mIsCrashRecoveryEligible;
    }

    /* package */ @Nullable List<CrashRecoveryWindowInfo> getCrashedWindowsForTesting() {
        return mCrashedWindows;
    }

    /* package */ List<CrashRecoveryWindowInfo> getNonVisibleWindowsForTesting() {
        return mNonVisibleWindows;
    }

    /* package */ List<CrashRecoveryWindowInfo> getVisibleWindowsForTesting() {
        return mVisibleWindows;
    }
}
