// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/**
 * Controller for showing a "Preparing to start your task…" snackbar when Chrome is brought to the
 * foreground via a Glic external trigger fallback (e.g. when experimental opt-in is required, or
 * when system notifications and therefore background actuation are disabled).
 *
 * <p>The snackbar is persistent and is dismissed as soon as a new task is created ({@link
 * ActorTaskState#CREATED}), or replaced with a "Something went wrong" snackbar if no task is
 * created within {@link ActorTaskTimeoutParameters#getPreparingToStartTaskTimeoutMs()}.
 */
@NullMarked
public class ActorExternalTriggerSnackbarController
        implements ActorKeyedService.Observer,
                StartStopWithNativeObserver,
                Destroyable,
                SnackbarManager.SnackbarController {
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mTimeoutRunnable = this::onPreparingTimeout;
    private final Callback<Profile> mProfileObserver = this::onProfileAdded;

    private @Nullable ActorKeyedService mActorKeyedService;
    private boolean mIsPendingTaskStart;
    private boolean mIsErrorSnackbarShowing;

    /**
     * Constructs a new {@link ActorExternalTriggerSnackbarController}.
     *
     * @param activity The current {@link Activity}.
     * @param snackbarManager The {@link SnackbarManager} used to display the snackbar.
     * @param profileSupplier Supplier for the current {@link Profile}.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events.
     */
    public ActorExternalTriggerSnackbarController(
            Activity activity,
            SnackbarManager snackbarManager,
            MonotonicObservableSupplier<Profile> profileSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mProfileSupplier = profileSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;

        mActivityLifecycleDispatcher.register(this);
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
    }

    private void onProfileAdded(@Nullable Profile profile) {
        if (profile == null || mActorKeyedService != null) {
            return;
        }
        mActorKeyedService = ActorKeyedServiceFactory.getForProfile(profile.getOriginalProfile());
        if (mActorKeyedService != null) {
            mActorKeyedService.addObserver(this);
            mProfileSupplier.removeObserver(mProfileObserver);
        }
    }

    /**
     * Called when ChromeTabbedActivity receives a trusted Glic external-trigger fallback intent
     * indicating that a new actor task is pending creation.
     */
    public void onPendingActorTaskTrigger() {
        dismissSnackbar();
        mIsPendingTaskStart = true;
        schedulePreparingTimeout();
        maybeShowSnackbar();
    }

    private void schedulePreparingTimeout() {
        mHandler.removeCallbacks(mTimeoutRunnable);
        mHandler.postDelayed(
                mTimeoutRunnable, ActorTaskTimeoutParameters.getPreparingToStartTaskTimeoutMs());
    }

    private void onPreparingTimeout() {
        if (!mIsPendingTaskStart) {
            return;
        }
        dismissSnackbar();
        if (!mSnackbarManager.canShowSnackbar()) {
            return;
        }
        String errorMessage =
                mActivity.getString(R.string.actor_task_list_bubble_row_failed_task_subtitle);
        Snackbar errorSnackbar =
                Snackbar.make(
                        errorMessage,
                        this,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_ACTOR_EXTERNAL_TRIGGER);
        mSnackbarManager.showSnackbar(errorSnackbar);
        mIsErrorSnackbarShowing = true;
    }

    private void maybeShowSnackbar() {
        if (!mIsPendingTaskStart) {
            return;
        }
        if (!mSnackbarManager.canShowSnackbar()) {
            return;
        }
        String message =
                mActivity.getString(R.string.actor_notification_title_preparing_to_start_task);
        // Use TYPE_NOTIFICATION to avoid TYPE_PERSISTENT's default "OK" button.
        // Set duration to 2x the preparing timeout so mTimeoutRunnable fires before the
        // SnackbarManager auto-dismisses the snackbar and calls onDismissNoAction().
        Snackbar snackbar =
                Snackbar.make(
                        message,
                        this,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_ACTOR_EXTERNAL_TRIGGER)
                        .setDuration(
                                ActorTaskTimeoutParameters.getPreparingToStartTaskTimeoutMs() * 2);
        mSnackbarManager.showSnackbar(snackbar);
    }

    private void dismissSnackbar() {
        mHandler.removeCallbacks(mTimeoutRunnable);
        mIsPendingTaskStart = false;
        mIsErrorSnackbarShowing = false;
        mSnackbarManager.dismissSnackbars(this);
    }

    @Override
    public void onStartWithNative() {
        maybeShowSnackbar();
    }

    @Override
    public void onStopWithNative() {}

    @Override
    public void onTaskStateChanged(@ActorTaskId int taskId, @ActorTaskState int newState) {
        if ((mIsPendingTaskStart || mIsErrorSnackbarShowing)
                && newState == ActorTaskState.CREATED) {
            dismissSnackbar();
        }
    }

    @Override
    public void onAction(@Nullable Object actionData) {
        mHandler.removeCallbacks(mTimeoutRunnable);
        mIsPendingTaskStart = false;
        mIsErrorSnackbarShowing = false;
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        mHandler.removeCallbacks(mTimeoutRunnable);
        mIsPendingTaskStart = false;
        mIsErrorSnackbarShowing = false;
    }

    @Override
    public void destroy() {
        dismissSnackbar();
        mProfileSupplier.removeObserver(mProfileObserver);
        if (mActorKeyedService != null) {
            mActorKeyedService.removeObserver(this);
            mActorKeyedService = null;
        }
        mActivityLifecycleDispatcher.unregister(this);
    }

    boolean isPendingTaskStartForTesting() {
        return mIsPendingTaskStart;
    }

    boolean isErrorSnackbarShowingForTesting() {
        return mIsErrorSnackbarShowing;
    }
}
