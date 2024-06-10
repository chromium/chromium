// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.ADDITIONAL_INFO;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

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
            Context context, BottomSheetController bottomSheetController, Delegate delegate) {
        mFacilitatedPaymentsPaymentMethodsModel = createModel();
        mMediator.initialize(context, mFacilitatedPaymentsPaymentMethodsModel, delegate);
        setUpModelChangeProcessors(
                mFacilitatedPaymentsPaymentMethodsModel,
                new FacilitatedPaymentsPaymentMethodsView(context, bottomSheetController));
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

    /**
     * Register payment methods items to RecyclerViewAdapter.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link FacilitatedPaymentsPaymentMethodsView} to update.
     */
    public static void setUpPaymentMethodsItems(
            PropertyModel model, FacilitatedPaymentsPaymentMethodsView view) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(model.get(SHEET_ITEMS));
        adapter.registerType(
                HEADER,
                FacilitatedPaymentsPaymentMethodsViewBinder::createHeaderItemView,
                FacilitatedPaymentsPaymentMethodsViewBinder::bindHeaderView);
        adapter.registerType(
                BANK_ACCOUNT,
                BankAccountViewBinder::createBankAccountItemView,
                BankAccountViewBinder::bindBankAccountItemView);
        adapter.registerType(
                ADDITIONAL_INFO,
                FacilitatedPaymentsPaymentMethodsViewBinder::createAdditionalInfoView,
                FacilitatedPaymentsPaymentMethodsViewBinder::bindAdditionalInfoView);
        view.getSheetItemListView().setAdapter(adapter);
    }

    PropertyModel createModel() {
        return new PropertyModel.Builder(FacilitatedPaymentsPaymentMethodsProperties.ALL_KEYS)
                .with(VISIBLE, false)
                .with(SHEET_ITEMS, new ModelList())
                .build();
    }
}
