// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FopSelectorProperties.SCREEN_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.ADDITIONAL_INFO;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.CONTINUE_BUTTON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.HEADER;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FopSelectorProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * This class can be used to show a list of items in a bottom sheet, for e.g. a user's payment
 * instruments.
 */
public class FacilitatedPaymentsFopSelectorScreen implements FacilitatedPaymentsSequenceView {
    private RecyclerView mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        mView =
                (RecyclerView)
                        LayoutInflater.from(viewContainer.getContext())
                                .inflate(
                                        R.layout.facilitated_payments_fop_selector,
                                        viewContainer,
                                        false);
        mView.setLayoutManager(
                new LinearLayoutManager(mView.getContext(), LinearLayoutManager.VERTICAL, false) {
                    @Override
                    public boolean isAutoMeasureEnabled() {
                        return true;
                    }

                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            RecyclerView.Recycler recycler,
                            RecyclerView.State state,
                            AccessibilityNodeInfoCompat info) {}
                });
    }

    @Override
    public View getView() {
        return mView;
    }

    /**
     * The {@link PropertyModel} for the FOP selector has a single property:
     *
     * <p>SCREEN_ITEMS: A {@Llink ModelList} to which items of different view types can be added to
     * show them in a list. To show a new view type, register it with the adapter.
     */
    @Override
    public PropertyModel getModel() {
        ModelList viewData = new ModelList();
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(viewData);
        // TODO: b/348595414 - Create a new view binder class.
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
        adapter.registerType(
                CONTINUE_BUTTON,
                FacilitatedPaymentsPaymentMethodsViewBinder::createContinueButtonView,
                FacilitatedPaymentsPaymentMethodsViewBinder::bindContinueButtonView);
        adapter.registerType(
                FOOTER,
                FacilitatedPaymentsPaymentMethodsViewBinder::createFooterItemView,
                FacilitatedPaymentsPaymentMethodsViewBinder::bindFooterView);
        mView.setAdapter(adapter);
        return new PropertyModel.Builder(FopSelectorProperties.ALL_KEYS)
                .with(SCREEN_ITEMS, viewData)
                .build();
    }

    @Override
    public int getVerticalScrollOffset() {
        return mView.computeVerticalScrollOffset();
    }
}
