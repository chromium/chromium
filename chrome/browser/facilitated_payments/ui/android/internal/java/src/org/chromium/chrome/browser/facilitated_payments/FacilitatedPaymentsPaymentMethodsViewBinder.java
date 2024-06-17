// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties.DESCRIPTION_1_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_SUMMARY;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.ON_BANK_ACCOUNT_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link FacilitatedPaymentsPaymentMethodsProperties} changes in a
 * {@link PropertyModel} to the suitable method in {@link FacilitatedPaymentsPaymentMethodsView}.
 */
class FacilitatedPaymentsPaymentMethodsViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link FacilitatedPaymentsPaymentMethodsView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindFacilitatedPaymentsPaymentMethodsView(
            PropertyModel model,
            FacilitatedPaymentsPaymentMethodsView view,
            PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == SHEET_ITEMS) {
            FacilitatedPaymentsPaymentMethodsCoordinator.setUpPaymentMethodsItems(model, view);
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private FacilitatedPaymentsPaymentMethodsViewBinder() {}

    /**
     * Factory used to create a new header inside the ListView inside the
     * FacilitatedPaymentsPaymentMethodsView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createHeaderItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.facilitated_payments_payment_methods_sheet_header_item,
                        parent,
                        false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TITLE_ID) {
            TextView sheetTitleText = view.findViewById(R.id.sheet_title);
            sheetTitleText.setText(view.getContext().getResources().getString(model.get(TITLE_ID)));
        } else if (propertyKey == DESCRIPTION_ID) {
            TextView sheetDescriptionText = view.findViewById(R.id.description_text);
            sheetDescriptionText.setText(
                    view.getContext().getResources().getString(model.get(DESCRIPTION_ID)));
        } else if (propertyKey == IMAGE_DRAWABLE_ID) {
            ImageView sheetHeaderImage = view.findViewById(R.id.branding_icon);
            sheetHeaderImage.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(IMAGE_DRAWABLE_ID)));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create additional info below the payment methods inside the
     * FacilitatedPaymentsPaymentMethodsView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createAdditionalInfoView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.facilitated_payments_payment_methods_additional_info,
                        parent,
                        false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the additional info to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindAdditionalInfoView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == DESCRIPTION_1_ID) {
            TextView descriptionLine1 = view.findViewById(R.id.description_line_1);
            descriptionLine1.setText(
                    view.getContext().getResources().getString(model.get(DESCRIPTION_1_ID)));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static View createContinueButtonView(ViewGroup parent) {
        View buttonView =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_fill_button, parent, false);
        return buttonView;
    }

    static void bindContinueButtonView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ON_BANK_ACCOUNT_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_BANK_ACCOUNT_CLICK_ACTION).run());
            TextView buttonTitleText = view.findViewById(R.id.touch_to_fill_button_title);
            buttonTitleText.setText(R.string.autofill_payment_method_continue_button);
        } else if (propertyKey == BANK_NAME
                || propertyKey == BANK_ACCOUNT_SUMMARY
                || propertyKey == BANK_ACCOUNT_DRAWABLE_ID) {
            // Skip, because none of these changes affect the button
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }
}
