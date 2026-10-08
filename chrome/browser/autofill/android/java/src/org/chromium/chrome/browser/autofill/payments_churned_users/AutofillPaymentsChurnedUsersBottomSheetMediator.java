// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.autofill.AutofillEnableResurrectingPaymentsUsersTreatmentArm;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator handling business logic for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetMediator implements BottomSheetObserver {
    /**
     * Delay in milliseconds before hiding the bottom sheet after the user clicks the accept ("Turn
     * on") button, matching Desktop's {@code kMillisecondsUntilConfirmationBubbleIsShown}.
     */
    // LINT.IfChange(ChurnedUsersLoadingDelayMs)
    static final long LOADING_DELAY_MS = 1000L;

    // LINT.ThenChange(//chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h:ChurnedUsersLoadingDelayMs)

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final PropertyModel mModel;
    private final AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate mDelegate;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private boolean mIsAccepted;
    private boolean mIsCanceled;
    private boolean mShouldShowConfirmation;
    private boolean mIsDestroyed;

    AutofillPaymentsChurnedUsersBottomSheetMediator(
            Context context,
            BottomSheetController bottomSheetController,
            BottomSheetContent bottomSheetContent,
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm,
            AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate delegate) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mDelegate = delegate;
        mModel =
                new PropertyModel.Builder(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.ALL_KEYS)
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.HEADER_ICON,
                                getHeaderIconResId(treatmentArm))
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.TITLE,
                                context.getString(getTitleResId(treatmentArm)))
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.DESCRIPTION,
                                context.getString(getDescriptionResId(treatmentArm)))
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties
                                        .ACCEPT_BUTTON_LABEL,
                                context.getString(
                                        R.string.autofill_churned_users_bubble_accept_button_label))
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties
                                        .CANCEL_BUTTON_LABEL,
                                context.getString(
                                        R.string.autofill_churned_users_bubble_cancel_button_label))
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED,
                                this::onAcceptClicked)
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.ON_CANCEL_CLICKED,
                                this::onCancelClicked)
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties
                                        .SHOW_LOADING_STATE,
                                false)
                        .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    void requestShowContent() {
        if (mIsDestroyed) {
            return;
        }
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            mBottomSheetController.addObserver(this);
        } else {
            mDelegate.onUiNotShown();
            destroy();
        }
    }

    void onAcceptClicked() {
        if (mIsDestroyed || mIsAccepted || mIsCanceled) {
            return;
        }
        mIsAccepted = true;
        mDelegate.onUiAccepted();
        mModel.set(AutofillPaymentsChurnedUsersBottomSheetProperties.SHOW_LOADING_STATE, true);
        mHandler.postDelayed(
                () -> {
                    if (mIsDestroyed) {
                        return;
                    }
                    mShouldShowConfirmation = true;
                    mBottomSheetController.hideContent(
                            mBottomSheetContent,
                            /* animate= */ true,
                            StateChangeReason.INTERACTION_COMPLETE);
                },
                LOADING_DELAY_MS);
    }

    void onCancelClicked() {
        if (mIsDestroyed || mIsAccepted || mIsCanceled) {
            return;
        }
        mIsCanceled = true;
        mDelegate.onUiCanceled();
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        if (mIsDestroyed) {
            return;
        }
        if (!mIsAccepted && !mIsCanceled) {
            mDelegate.onUiDismissed();
        }
        // When hiding after the 1,000ms loading spinner completes, wait for
        // onSheetStateChanged(SheetState.HIDDEN) so ChromeActivitySnackbarHelper pops
        // ParentOverrideSlot.BOTTOM_SHEET before the confirmation snackbar is shown.
        if (!mShouldShowConfirmation) {
            destroy();
        }
    }

    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        if (mIsDestroyed || !mShouldShowConfirmation || newState != SheetState.HIDDEN) {
            return;
        }
        mShouldShowConfirmation = false;
        mDelegate.onShowConfirmation();
        destroy();
    }

    void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mShouldShowConfirmation = false;
        mHandler.removeCallbacksAndMessages(null);
        mBottomSheetController.removeObserver(this);
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ false, StateChangeReason.NONE);
    }

    private static @DrawableRes int getHeaderIconResId(
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm) {
        switch (treatmentArm) {
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.SECURITY:
                return R.drawable.autofill_payments_churned_users_security_illustration;
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE:
                return R.drawable.autofill_payments_churned_users_convenience_illustration;
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.MESSAGE:
            // The MESSAGE arm displays an Android Message banner via AutofillMessageController
            // rather than this bottom sheet, so the bottom sheet should never be created for
            // this arm.
            default:
                assert false : "Unhandled treatment arm: " + treatmentArm;
                return R.drawable.autofill_payments_churned_users_security_illustration;
        }
    }

    private static @StringRes int getTitleResId(
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm) {
        switch (treatmentArm) {
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.SECURITY:
                return R.string.autofill_churned_users_bubble_security_title;
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE:
                return R.string.autofill_churned_users_bubble_convenience_title;
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.MESSAGE:
            // The MESSAGE arm displays an Android Message banner via AutofillMessageController
            // rather than this bottom sheet, so the bottom sheet should never be created for
            // this arm.
            default:
                assert false : "Unhandled treatment arm: " + treatmentArm;
                return R.string.autofill_churned_users_bubble_security_title;
        }
    }

    private static @StringRes int getDescriptionResId(
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm) {
        switch (treatmentArm) {
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.SECURITY:
                return R.string.autofill_churned_users_bubble_security_description;
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE:
                return R.string.autofill_churned_users_bubble_convenience_description;
            case AutofillEnableResurrectingPaymentsUsersTreatmentArm.MESSAGE:
            // The MESSAGE arm displays an Android Message banner via AutofillMessageController
            // rather than this bottom sheet, so the bottom sheet should never be created for
            // this arm.
            default:
                assert false : "Unhandled treatment arm: " + treatmentArm;
                return R.string.autofill_churned_users_bubble_security_description;
        }
    }
}
