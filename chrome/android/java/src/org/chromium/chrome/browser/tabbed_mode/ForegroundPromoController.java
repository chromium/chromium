// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.ChromeInactivityTracker.InactivityObserver;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics.LaunchCause;
import org.chromium.chrome.browser.glic.ui.GlicPromoCoordinator;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tips.TipsOptInCoordinator;
import org.chromium.chrome.browser.tips.TipsUtils;
import org.chromium.chrome.browser.tips.TipsUtils.TipsOptInPromoTriggerOutcome;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.function.IntSupplier;
import java.util.function.Supplier;

/**
 * Controller for managing and showing foreground promos (such as the Glic promo and Tips Opt-In
 * promo) when the activity resumes in a new foreground session.
 */
@NullMarked
public class ForegroundPromoController {
    private final Activity mActivity;
    private final Supplier<@Nullable Profile> mProfileSupplier;
    private final OneshotSupplier<ChromeInactivityTracker> mInactivityTrackerSupplier;
    private final OneshotSupplier<Boolean> mTrackerInitializedOneshotSupplier;
    private final Supplier<@Nullable BottomSheetController> mBottomSheetControllerSupplier;
    private final Supplier<@Nullable SnackbarManager> mSnackbarManagerSupplier;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final IntSupplier mLaunchCauseSupplier;
    private final InactivityObserver mInactivityObserver;
    private final CallbackController mCallbackController;
    private final Runnable mShowPromoCallback;

    private boolean mPromosEvaluatedForCurrentForeground;
    // Captured in onForegrounded() because ChromeInactivityTracker#onResumeWithNative clears the
    // stored timestamp before promo evaluation runs on resume (or on late tracker initialization).
    private long mTimeSinceLastBackgroundedMs =
            ChromeInactivityTracker.UNKNOWN_LAST_BACKGROUNDED_TIME;
    // Number of onForegrounded() calls so far. The first one belongs to the activity's initial
    // start, so promo evaluations before or during it count as a cold start.
    private int mForegroundCount;
    private Callback<Long> mTipsOptInPromoShowCallback = this::maybeShowTipsOptInPromo;
    private @Nullable TipsOptInCoordinator mTipsOptInCoordinator;
    private @Nullable GlicPromoCoordinator mGlicPromoCoordinator;

    /**
     * @param activity The {@link Activity} associated with this controller.
     * @param profileSupplier Supplier for the current {@link Profile}.
     * @param inactivityTrackerSupplier Supplier for the {@link ChromeInactivityTracker}.
     * @param trackerInitializedOneshotSupplier Supplier indicating if the feature engagement
     *     tracker is initialized.
     * @param bottomSheetControllerSupplier Supplier for the {@link BottomSheetController}.
     * @param snackbarManagerSupplier Supplier for the {@link SnackbarManager}.
     * @param activityLifecycleDispatcher Lifecycle dispatcher for checking resumed state.
     * @param launchCauseSupplier Supplier for the current {@link LaunchCause}.
     */
    public ForegroundPromoController(
            Activity activity,
            Supplier<@Nullable Profile> profileSupplier,
            OneshotSupplier<ChromeInactivityTracker> inactivityTrackerSupplier,
            OneshotSupplier<Boolean> trackerInitializedOneshotSupplier,
            Supplier<@Nullable BottomSheetController> bottomSheetControllerSupplier,
            Supplier<@Nullable SnackbarManager> snackbarManagerSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            IntSupplier launchCauseSupplier) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        mInactivityTrackerSupplier = inactivityTrackerSupplier;
        mTrackerInitializedOneshotSupplier = trackerInitializedOneshotSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mLaunchCauseSupplier = launchCauseSupplier;
        mCallbackController = new CallbackController();
        mShowPromoCallback = mCallbackController.makeCancelable(this::maybeShowPromosOnResume);

        mInactivityObserver =
                new InactivityObserver() {
                    @Override
                    public void onForegrounded(long timeSinceLastBackgroundedMs) {
                        // Reset the evaluation flag when the app starts a new foreground session.
                        // Evaluation runs once the activity actually resumes so activities started
                        // without resuming do not show promos unseen.
                        mPromosEvaluatedForCurrentForeground = false;
                        mTimeSinceLastBackgroundedMs = timeSinceLastBackgroundedMs;
                        mForegroundCount++;
                    }
                };

        mInactivityTrackerSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (inactivityTracker) -> {
                            inactivityTracker.addObserver(mInactivityObserver);
                        }));

        mTrackerInitializedOneshotSupplier.onAvailable((initialized) -> mShowPromoCallback.run());
    }

    /**
     * Evaluates foreground promos after {@code ChromeTabbedActivity#onResumeWithNative} completes
     * (after home-surface NTP selection and launch-cause recording).
     *
     * <p>Not a {@code PauseResumeWithNativeObserver} because {@code ChromeActivity} records the
     * launch cause after lifecycle observers are dispatched.
     */
    public void onResumeWithNativeCompleted() {
        mShowPromoCallback.run();
    }

    /** Destroys the controller and any promo it is showing. */
    public void destroy() {
        mCallbackController.destroy();
        if (mTipsOptInCoordinator != null) {
            mTipsOptInCoordinator.destroy();
            mTipsOptInCoordinator = null;
        }
        if (mGlicPromoCoordinator != null) {
            mGlicPromoCoordinator.destroy();
            mGlicPromoCoordinator = null;
        }
        var inactivityTracker = mInactivityTrackerSupplier.get();
        if (inactivityTracker != null) {
            inactivityTracker.removeObserver(mInactivityObserver);
        }
    }

    private void maybeShowPromosOnResume() {
        if (mActivityLifecycleDispatcher.isActivityFinishingOrDestroyed()
                || mActivityLifecycleDispatcher.getCurrentActivityState()
                        != ActivityState.RESUMED_WITH_NATIVE) {
            return;
        }
        if (!Boolean.TRUE.equals(mTrackerInitializedOneshotSupplier.get())) {
            return;
        }
        if (mInactivityTrackerSupplier.get() == null) {
            return;
        }
        if (mPromosEvaluatedForCurrentForeground) {
            return;
        }
        mPromosEvaluatedForCurrentForeground = true;

        if (maybeShowGlicPromo()) {
            TipsUtils.recordTipsOptInPromoTriggerOutcome(
                    TipsOptInPromoTriggerOutcome.SKIPPED_FOR_GLIC_PROMO, isColdStart());
            return;
        }

        // External-intent suppression is scoped to the Tips opt-in sheet (Glic behavior is
        // unchanged), so the Tips sheet does not cover content the user navigated to via a deep
        // link or notification.
        if (isExternalIntentLaunch(mLaunchCauseSupplier.getAsInt())) {
            return;
        }

        mTipsOptInPromoShowCallback.onResult(mTimeSinceLastBackgroundedMs);
    }

    /** Whether promos are being evaluated for the first foreground since the activity started. */
    private boolean isColdStart() {
        return mForegroundCount <= 1;
    }

    private static boolean isExternalIntentLaunch(@LaunchCause int launchCause) {
        return switch (launchCause) {
            case LaunchCause.EXTERNAL_VIEW_INTENT,
                    LaunchCause.NOTIFICATION,
                    LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT,
                    LaunchCause.SHARE_INTENT,
                    LaunchCause.NFC,
                    LaunchCause.OPEN_IN_BROWSER_FROM_MENU,
                    LaunchCause.HOME_SCREEN_WIDGET,
                    LaunchCause.HOME_SCREEN_SHORTCUT,
                    LaunchCause.HANDOFF,
                    LaunchCause.OTHER_CHROME ->
                    true;
            default -> false;
        };
    }

    private void maybeShowTipsOptInPromo(long timeSinceLastBackgroundedMs) {
        var bottomSheetController = mBottomSheetControllerSupplier.get();
        var snackbarManager = mSnackbarManagerSupplier.get();
        if (bottomSheetController == null || snackbarManager == null) {
            return;
        }
        TipsUtils.maybeShowTipsOptInPromo(
                mActivity,
                mProfileSupplier.get(),
                bottomSheetController,
                snackbarManager,
                ChromeSharedPreferences.getInstance(),
                timeSinceLastBackgroundedMs,
                isColdStart(),
                (coordinator) -> mTipsOptInCoordinator = coordinator);
    }

    @VisibleForTesting
    boolean maybeShowGlicPromo() {
        var bottomSheetController = mBottomSheetControllerSupplier.get();
        if (bottomSheetController == null) {
            return false;
        }
        mGlicPromoCoordinator =
                GlicPromoCoordinator.maybeShowPromo(
                        mActivity, mProfileSupplier.get(), bottomSheetController);
        return mGlicPromoCoordinator != null;
    }

    @Nullable GlicPromoCoordinator getGlicPromoCoordinatorForTesting() {
        return mGlicPromoCoordinator;
    }

    void setGlicPromoCoordinatorForTesting(@Nullable GlicPromoCoordinator coordinator) {
        mGlicPromoCoordinator = coordinator;
    }

    void setTipsOptInCoordinatorForTesting(@Nullable TipsOptInCoordinator coordinator) {
        mTipsOptInCoordinator = coordinator;
    }

    void setTipsOptInPromoShowCallbackForTesting(Callback<Long> callback) {
        mTipsOptInPromoShowCallback = callback;
    }
}
