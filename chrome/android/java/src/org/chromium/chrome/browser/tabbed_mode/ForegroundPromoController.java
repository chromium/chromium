// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.ChromeInactivityTracker.InactivityObserver;
import org.chromium.chrome.browser.glic.ui.GlicPromoCoordinator;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tips.TipsOptInCoordinator;
import org.chromium.chrome.browser.tips.TipsUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.function.Supplier;

/**
 * Controller for managing and showing foreground promos (such as the Glic promo and Tips Opt-In
 * promo) when the activity is foregrounded.
 */
@NullMarked
public class ForegroundPromoController {
    private final Activity mActivity;
    private final Supplier<@Nullable Profile> mProfileSupplier;
    private final OneshotSupplier<ChromeInactivityTracker> mInactivityTrackerSupplier;
    private final OneshotSupplier<Boolean> mTrackerInitializedOneshotSupplier;
    private final Supplier<@Nullable BottomSheetController> mBottomSheetControllerSupplier;
    private final Supplier<@Nullable SnackbarManager> mSnackbarManagerSupplier;
    private final InactivityObserver mInactivityObserver;
    private final CallbackController mCallbackController;
    private final Runnable mShowPromoCallback;

    private boolean mPromosEvaluatedForCurrentForeground;
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
     */
    public ForegroundPromoController(
            Activity activity,
            Supplier<@Nullable Profile> profileSupplier,
            OneshotSupplier<ChromeInactivityTracker> inactivityTrackerSupplier,
            OneshotSupplier<Boolean> trackerInitializedOneshotSupplier,
            Supplier<@Nullable BottomSheetController> bottomSheetControllerSupplier,
            Supplier<@Nullable SnackbarManager> snackbarManagerSupplier) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        mInactivityTrackerSupplier = inactivityTrackerSupplier;
        mTrackerInitializedOneshotSupplier = trackerInitializedOneshotSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mCallbackController = new CallbackController();
        mShowPromoCallback = mCallbackController.makeCancelable(this::maybeShowPromosOnForeground);

        mInactivityObserver =
                new InactivityObserver() {
                    @Override
                    public void onForegrounded(long timeSinceLastBackgroundedMs) {
                        // Reset the evaluation flag when the app is foregrounded, indicating a
                        // new foreground session has started and promos should be re-evaluated.
                        mPromosEvaluatedForCurrentForeground = false;
                        mShowPromoCallback.run();
                    }
                };

        mInactivityTrackerSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (inactivityTracker) -> {
                            inactivityTracker.addObserver(mInactivityObserver);
                        }));

        mTrackerInitializedOneshotSupplier.onAvailable((initialized) -> mShowPromoCallback.run());
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

    private void maybeShowPromosOnForeground() {
        if (!Boolean.TRUE.equals(mTrackerInitializedOneshotSupplier.get())) {
            return;
        }
        var inactivityTracker = mInactivityTrackerSupplier.get();
        if (inactivityTracker == null) {
            return;
        }
        if (mPromosEvaluatedForCurrentForeground) {
            return;
        }
        mPromosEvaluatedForCurrentForeground = true;

        if (maybeShowGlicPromo()) {
            return;
        }

        maybeShowTipsOptInPromo(inactivityTracker.getTimeSinceLastBackgroundedMs());
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
}
