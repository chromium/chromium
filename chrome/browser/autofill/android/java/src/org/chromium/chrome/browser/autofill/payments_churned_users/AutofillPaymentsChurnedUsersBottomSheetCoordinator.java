// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.autofill.AutofillEnableResurrectingPaymentsUsersTreatmentArm;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for assembling and controlling the Payments Churned Users bottom sheet. */
@NullMarked
public class AutofillPaymentsChurnedUsersBottomSheetCoordinator {
    private final AutofillPaymentsChurnedUsersBottomSheetMediator mMediator;
    private final AutofillPaymentsChurnedUsersBottomSheetView mView;
    private final PropertyModel mModel;
    private @Nullable
            PropertyModelChangeProcessor<
                    PropertyModel, AutofillPaymentsChurnedUsersBottomSheetView, PropertyKey>
            mModelChangeProcessor;

    /** Delegate to receive user action callbacks from the bottom sheet. */
    /*package*/ interface Delegate {
        /** Called when the user clicks the accept ("Turn on") button. */
        void onUiAccepted();

        /** Called when the user clicks the cancel ("No thanks") button. */
        void onUiCanceled();

        /** Called when the user passively dismisses the bottom sheet (e.g. swipe down, scrim). */
        void onUiDismissed();

        /** Called when the bottom sheet could not be shown. */
        void onUiNotShown();
    }

    public AutofillPaymentsChurnedUsersBottomSheetCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm,
            Delegate delegate) {
        mView = new AutofillPaymentsChurnedUsersBottomSheetView(context);

        AutofillPaymentsChurnedUsersBottomSheetContent content =
                new AutofillPaymentsChurnedUsersBottomSheetContent(mView.getContentView());

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
                        .build();

        mMediator =
                new AutofillPaymentsChurnedUsersBottomSheetMediator(
                        bottomSheetController, content, mModel, delegate);

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, AutofillPaymentsChurnedUsersBottomSheetViewBinder::bind);
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

    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    public void destroy() {
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }
        mMediator.destroy();
    }

    View getContentViewForTesting() {
        return mView.getContentView();
    }
}
