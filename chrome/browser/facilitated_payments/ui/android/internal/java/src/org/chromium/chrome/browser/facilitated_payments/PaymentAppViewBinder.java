// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.ON_PAYMENT_APP_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.PAYMENT_APP_ICON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.PAYMENT_APP_NAME;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map changes of {@link PaymentAppProperties} in a {@link PropertyModel} to
 * the suitable method in {@link FacilitatedPaymentsPaymentMethodsView}.
 */
@NullMarked
class PaymentAppViewBinder {
    /**
     * Inflates and returns a new payment app item view.
     *
     * @param parent The parent view group.
     * @return The inflated payment app item view.
     */
    static View createPaymentAppItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.payment_app_item, parent, false);
    }

    /**
     * Binds a {@link PropertyModel} to a payment app item view.
     *
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property key to update.
     */
    static void bindPaymentAppItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == PAYMENT_APP_NAME) {
            TextView paymentAppName = view.findViewById(R.id.payment_app_name);
            paymentAppName.setText(model.get(PAYMENT_APP_NAME));
        } else if (propertyKey == PAYMENT_APP_ICON) {
            ImageView paymentAppIcon = view.findViewById(R.id.payment_app_icon);
            paymentAppIcon.setImageDrawable(model.get(PAYMENT_APP_ICON));
        } else if (propertyKey == ON_PAYMENT_APP_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_PAYMENT_APP_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private PaymentAppViewBinder() {}
}
