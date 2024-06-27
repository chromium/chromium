// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE;

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
        FacilitatedPaymentsPaymentMethodsView view =
                new FacilitatedPaymentsPaymentMethodsView(context, bottomSheetController);
        // TODO(b/348142774): Undo temporary change when FacilitatedPaymentsPaymentMethodsViewBinder
        // is able to get the model from the screen to be shown.
        mFacilitatedPaymentsPaymentMethodsModel = createModel(mMediator, view);
        mMediator.initialize(context, mFacilitatedPaymentsPaymentMethodsModel, delegate, profile);
        setUpModelChangeProcessors(mFacilitatedPaymentsPaymentMethodsModel, view);
    }

    @Override
    public boolean showSheet(List<BankAccount> bankAccounts) {
        return mMediator.showSheet(bankAccounts);
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

    PropertyModel createModel(
            FacilitatedPaymentsPaymentMethodsMediator mediator,
            FacilitatedPaymentsPaymentMethodsView view) {
        return new PropertyModel.Builder(FacilitatedPaymentsPaymentMethodsProperties.ALL_KEYS)
                .with(VISIBLE, false)
                .with(SCREEN_VIEW_MODEL, view.getCurrentScreen().getModel())
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
