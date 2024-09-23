// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.UNINITIALIZED;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE_STATE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.HIDDEN;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * Implements the FacilitatedPaymentsPaymentMethodsComponent. It uses a bottom sheet to let the user
 * select a form of payment.
 */
public class FacilitatedPaymentsPaymentMethodsCoordinator
        implements FacilitatedPaymentsPaymentMethodsComponent {
    private final FacilitatedPaymentsPaymentMethodsMediator mMediator =
            new FacilitatedPaymentsPaymentMethodsMediator();
    private PropertyModel mFacilitatedPaymentsPaymentMethodsModel;

    @Override
    public void initialize(
            Context context,
            BottomSheetController bottomSheetController,
            Delegate delegate,
            Profile profile) {
        mFacilitatedPaymentsPaymentMethodsModel = createModel(mMediator);
        mMediator.initialize(context, mFacilitatedPaymentsPaymentMethodsModel, delegate, profile);
        setUpModelChangeProcessors(
                mFacilitatedPaymentsPaymentMethodsModel,
                new FacilitatedPaymentsPaymentMethodsView(context, bottomSheetController));
    }

    @Override
    public boolean isInLandscapeMode() {
        return mMediator.isInLandscapeMode();
    }

    @Override
    public boolean showSheet(List<BankAccount> bankAccounts) {
        return mMediator.showSheet(bankAccounts);
    }

    @Override
    public void showProgressScreen() {
        mMediator.showProgressScreen();
    }

    @Override
    public void showErrorScreen() {
        mMediator.showErrorScreen();
    }

    @Override
    public void dismiss() {
        mMediator.dismiss();
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     *
     * @param model A {@link PropertyModel} built with {@link
     *     FacilitatedPaymentsPaymentMethodsProperties}.
     * @param view A {@link FacilitatedPaymentsPaymentMethodsView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(
            PropertyModel model, FacilitatedPaymentsPaymentMethodsView view) {
        PropertyModelChangeProcessor.create(
                model,
                view,
                FacilitatedPaymentsPaymentMethodsViewBinder
                        ::bindFacilitatedPaymentsPaymentMethodsView);
    }

    PropertyModel createModel(FacilitatedPaymentsPaymentMethodsMediator mediator) {
        return new PropertyModel.Builder(FacilitatedPaymentsPaymentMethodsProperties.ALL_KEYS)
                .with(VISIBLE_STATE, HIDDEN)
                .with(SCREEN, UNINITIALIZED)
                .with(DISMISS_HANDLER, mediator::onDismissed)
                .build();
    }

    PropertyModel getModelForTesting() {
        return mFacilitatedPaymentsPaymentMethodsModel;
    }

    FacilitatedPaymentsPaymentMethodsMediator getMediatorForTesting() {
        return mMediator;
    }
}
