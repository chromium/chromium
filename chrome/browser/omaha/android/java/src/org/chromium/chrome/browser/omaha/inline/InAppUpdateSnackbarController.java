// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omaha.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

/**
 * Controller managing the in-app update snackbars (both discovery and restart prompts), which
 * notify the user that an update is available or ready to install.
 */
@NullMarked
final class InAppUpdateSnackbarController implements SnackbarController {
    private static final String TAG = "InAppUpdateFlow";

    @VisibleForTesting
    static final String HISTOGRAM_DISCOVERY_SNACKBAR_EVENT =
            "Android.InAppUpdate.DiscoverySnackbar.Event";

    @VisibleForTesting
    static final String HISTOGRAM_RESTART_SNACKBAR_EVENT =
            "Android.InAppUpdate.RestartSnackbar.Event";

    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final String mHistogramName;
    private final Runnable mOnDecline;
    private final Runnable mOnAccept;
    private final @Nullable Runnable mOnDismiss;
    private boolean mIsDismissedOnTeardown;

    private InAppUpdateSnackbarController(
            Activity activity,
            SnackbarManager snackbarManager,
            String histogramName,
            Runnable onDecline,
            Runnable onAccept,
            @Nullable Runnable onDismiss) {
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mHistogramName = histogramName;
        mOnDecline = onDecline;
        mOnAccept = onAccept;
        mOnDismiss = onDismiss;
    }

    /**
     * Displays the discovery snackbar if the activity is active and the snackbar manager is ready.
     *
     * @param activity The host activity for string resources and lifecycle checks.
     * @param snackbarManager The manager to display the snackbar on.
     * @param onAccept Callback invoked when the user taps the action button ("Update").
     * @param onDismiss Optional callback invoked when the snackbar is dismissed without action
     *     (e.g. timeout, swipe, or activity stopped/finishing). Excludes explicit teardown via
     *     {@link #dismiss()}.
     * @return The active controller instance, or {@code null} if the snackbar was not shown.
     */
    static @Nullable InAppUpdateSnackbarController showDiscovery(
            Activity activity,
            SnackbarManager snackbarManager,
            Runnable onAccept,
            @Nullable Runnable onDismiss) {
        return show(
                activity,
                snackbarManager,
                R.string.in_app_update_discovery_title,
                R.string.in_app_update_discovery_action,
                Snackbar.UMA_IN_APP_UPDATE_DISCOVERY,
                HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                InAppUpdatePolicy::recordUpdateDeclined,
                onAccept,
                onDismiss);
    }

    /**
     * Displays the restart snackbar if the activity is active and the snackbar manager is ready.
     *
     * @param activity The host activity for string resources and lifecycle checks.
     * @param snackbarManager The manager to display the snackbar on.
     * @param onAccept Callback invoked when the user taps the action button ("Restart").
     * @param onDismiss Optional callback invoked when the snackbar is dismissed without action
     *     (e.g. timeout, swipe, or activity stopped/finishing). Excludes explicit teardown via
     *     {@link #dismiss()}.
     * @return The active controller instance, or {@code null} if the snackbar was not shown.
     */
    static @Nullable InAppUpdateSnackbarController showRestart(
            Activity activity,
            SnackbarManager snackbarManager,
            Runnable onAccept,
            @Nullable Runnable onDismiss) {
        return show(
                activity,
                snackbarManager,
                R.string.in_app_update_restart_title,
                R.string.in_app_update_restart_action,
                Snackbar.UMA_IN_APP_UPDATE_RESTART,
                HISTOGRAM_RESTART_SNACKBAR_EVENT,
                InAppUpdatePolicy::recordRestartDeclined,
                onAccept,
                onDismiss);
    }

    private static @Nullable InAppUpdateSnackbarController show(
            Activity activity,
            SnackbarManager snackbarManager,
            @StringRes int titleResId,
            @StringRes int actionResId,
            int umaId,
            String histogramName,
            Runnable onDecline,
            Runnable onAccept,
            @Nullable Runnable onDismiss) {
        ThreadUtils.assertOnUiThread();
        if (activity.isFinishing()
                || activity.isDestroyed()
                || activity.isChangingConfigurations()) {
            return null;
        }
        if (!snackbarManager.canShowSnackbar()) {
            return null;
        }

        Log.i(TAG, "Showing in-app update snackbar for histogram: %s", histogramName);
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, InAppUpdateSnackbarEvent.IMPRESSION, InAppUpdateSnackbarEvent.COUNT);

        InAppUpdateSnackbarController controller =
                new InAppUpdateSnackbarController(
                        activity, snackbarManager, histogramName, onDecline, onAccept, onDismiss);
        Snackbar snackbar =
                Snackbar.make(
                        activity.getString(titleResId),
                        controller,
                        Snackbar.TYPE_NOTIFICATION,
                        umaId);
        snackbar.setAction(activity.getString(actionResId), /* actionData= */ null);
        snackbar.setDefaultLines(false);
        snackbar.setDuration(SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS);
        snackbarManager.showSnackbar(snackbar);
        return controller;
    }

    @Override
    public void onAction(@Nullable Object actionData) {
        if (mIsDismissedOnTeardown) {
            return;
        }
        Log.i(TAG, "In-app update snackbar action tapped for histogram: %s", mHistogramName);
        RecordHistogram.recordEnumeratedHistogram(
                mHistogramName,
                InAppUpdateSnackbarEvent.ACTION_TAPPED,
                InAppUpdateSnackbarEvent.COUNT);
        mOnAccept.run();
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        if (mIsDismissedOnTeardown) {
            return;
        }
        if (isActivityFinishingOrStopped()) {
            Log.i(
                    TAG,
                    "In-app update snackbar dismissed by lifecycle for histogram: %s",
                    mHistogramName);
            RecordHistogram.recordEnumeratedHistogram(
                    mHistogramName,
                    InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE,
                    InAppUpdateSnackbarEvent.COUNT);
            if (mOnDismiss != null) {
                mOnDismiss.run();
            }
            return;
        }

        // TODO(crbug.com/553918188): Because SnackbarController does not receive @DismissalReason,
        // foreground preemptions (e.g. REPLACED_BY_ACTION_SNACKBAR) cannot be distinguished from
        // user timeouts or swiping. Plumbing @DismissalReason into SnackbarController would allow
        // suppressing the decline backoff on preemption.
        Log.i(
                TAG,
                "In-app update snackbar dismissed without action for histogram: %s",
                mHistogramName);
        RecordHistogram.recordEnumeratedHistogram(
                mHistogramName,
                InAppUpdateSnackbarEvent.DISMISSED_NO_ACTION,
                InAppUpdateSnackbarEvent.COUNT);
        mOnDecline.run();
        if (mOnDismiss != null) {
            mOnDismiss.run();
        }
    }

    private boolean isActivityFinishingOrStopped() {
        if (mActivity.isFinishing()
                || mActivity.isDestroyed()
                || mActivity.isChangingConfigurations()) {
            return true;
        }
        if (ApplicationStatus.isInitialized()) {
            int state = ApplicationStatus.getStateForActivity(mActivity);
            return state == ActivityState.STOPPED || state == ActivityState.DESTROYED;
        }
        return false;
    }

    /**
     * Dismisses the snackbar during teardown, suppressing backoff and telemetry recording.
     *
     * <p>Note that {@code onDismiss} is intentionally not invoked when dismissed through this
     * method, so the caller must clear its own controller reference. Calling this method multiple
     * times or after the snackbar has already been dismissed is a safe no-op.
     */
    void dismiss() {
        ThreadUtils.assertOnUiThread();
        if (mIsDismissedOnTeardown) {
            return;
        }
        mIsDismissedOnTeardown = true;
        mSnackbarManager.dismissSnackbars(this);
    }
}
