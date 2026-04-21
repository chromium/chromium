// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FooterProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map changes of {@link
 * FacilitatedPaymentsPaymentMethodsProperties.FooterProperties} in a {@link PropertyModel} to the
 * footer view in the payment methods bottom sheet.
 */
@NullMarked
class FooterViewBinder {
    static View createFooterItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.facilitated_payments_payment_methods_sheet_footer_item,
                        parent,
                        false);
    }

    static void bindFooterView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK) {
            setShowPaymentMethodsSettingsCallback(
                    view, model.get(FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private static void setShowPaymentMethodsSettingsCallback(View view, Runnable callback) {
        View managePaymentMethodsButton = view.findViewById(R.id.manage_payment_methods);
        managePaymentMethodsButton.setOnClickListener(unused -> callback.run());
    }

    private FooterViewBinder() {}
}
