// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_ICON_BITMAP;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.ON_EWALLET_CLICK_ACTION;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map changes of {@link EwalletProperties} in a {@link PropertyModel} to
 * the suitable method in {@link FacilitatedPaymentsPaymentMethodsView}.
 */
class EwalletViewBinder {
    static View createEwalletItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.ewallet_item, parent, false);
    }

    static void bindEwalletItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == EWALLET_NAME) {
            TextView eWalletName = view.findViewById(R.id.ewallet_name);
            eWalletName.setText(model.get(EWALLET_NAME));
        } else if (propertyKey == ACCOUNT_DISPLAY_NAME) {
            TextView accountDisplayName = view.findViewById(R.id.account_display_name);
            accountDisplayName.setText(model.get(ACCOUNT_DISPLAY_NAME));
        } else if (propertyKey == EWALLET_DRAWABLE_ID) {
            ImageView eWalletIcon = view.findViewById(R.id.ewallet_icon);
            eWalletIcon.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(EWALLET_DRAWABLE_ID)));
        } else if (propertyKey == EWALLET_ICON_BITMAP) {
            ImageView eWalletIcon = view.findViewById(R.id.ewallet_icon);
            eWalletIcon.setImageBitmap(model.get(EWALLET_ICON_BITMAP));
        } else if (propertyKey == ON_EWALLET_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_EWALLET_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private EwalletViewBinder() {}
}
