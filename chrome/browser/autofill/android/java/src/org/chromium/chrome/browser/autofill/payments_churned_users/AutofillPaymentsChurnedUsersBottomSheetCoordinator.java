// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
    private @Nullable
            PropertyModelChangeProcessor<
                    PropertyModel, AutofillPaymentsChurnedUsersBottomSheetView, PropertyKey>
            mModelChangeProcessor;

    /** Delegate to receive user action callbacks from the bottom sheet. */
    /*package*/ interface Delegate {
        /** Called when the user clicks the accept ("Turn on") button. */
        void onUiAccepted();

        /**
         * Called after the post-accept loading state completes and the confirmation snackbar should
         * be shown.
         */
        void onShowConfirmation();

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

        mMediator =
                new AutofillPaymentsChurnedUsersBottomSheetMediator(
                        context, bottomSheetController, content, treatmentArm, delegate);

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(),
                        mView,
                        AutofillPaymentsChurnedUsersBottomSheetViewBinder::bind);
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

    PropertyModel getModelForTesting() {
        return mMediator.getModel();
    }
}
