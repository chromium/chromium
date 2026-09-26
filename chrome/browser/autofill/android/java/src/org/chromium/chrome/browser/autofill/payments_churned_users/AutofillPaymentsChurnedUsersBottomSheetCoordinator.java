// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;
import android.view.View;

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

    public AutofillPaymentsChurnedUsersBottomSheetCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm) {
        mView = new AutofillPaymentsChurnedUsersBottomSheetView(context);

        AutofillPaymentsChurnedUsersBottomSheetContent content =
                new AutofillPaymentsChurnedUsersBottomSheetContent(mView.getContentView());

        mModel =
                new PropertyModel.Builder(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.ALL_KEYS)
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.TITLE,
                                context.getString(getTitleResId(treatmentArm)))
                        .build();

        mMediator =
                new AutofillPaymentsChurnedUsersBottomSheetMediator(bottomSheetController, content);

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, AutofillPaymentsChurnedUsersBottomSheetViewBinder::bind);
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
