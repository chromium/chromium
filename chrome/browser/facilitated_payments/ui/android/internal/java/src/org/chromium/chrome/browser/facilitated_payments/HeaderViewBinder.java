// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.PAYMENT_LINK_TITLE_TOP_MARGIN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.PRODUCT_ICON_CONTENT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.PRODUCT_ICON_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.PRODUCT_ICON_HEIGHT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.SECURITY_CHECK_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.TITLE;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map changes of {@link
 * FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties} in a {@link PropertyModel} to the
 * header view in the payment methods bottom sheet.
 */
@NullMarked
class HeaderViewBinder {
    static View createHeaderItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.facilitated_payments_payment_methods_sheet_header_item,
                        parent,
                        false);
    }

    static void bindHeaderView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TITLE) {
            TextView sheetTitleText = view.findViewById(R.id.sheet_title);
            sheetTitleText.setText(model.get(TITLE));
        } else if (propertyKey == DESCRIPTION_ID) {
            TextView sheetDescriptionText = view.findViewById(R.id.description_text);
            sheetDescriptionText.setText(view.getContext().getString(model.get(DESCRIPTION_ID)));
            sheetDescriptionText.setVisibility(View.VISIBLE);
        } else if (propertyKey == PRODUCT_ICON_DRAWABLE_ID) {
            ImageView sheetProductIconImage = view.findViewById(R.id.branding_icon);
            int productIconDrawableId = model.get(PRODUCT_ICON_DRAWABLE_ID);
            if (productIconDrawableId != 0) {
                sheetProductIconImage.setImageDrawable(
                        AppCompatResources.getDrawable(view.getContext(), productIconDrawableId));
            } else {
                sheetProductIconImage.setVisibility(View.GONE);
            }
        } else if (propertyKey == PRODUCT_ICON_HEIGHT) {
            ImageView sheetProductIconImage = view.findViewById(R.id.branding_icon);
            sheetProductIconImage.getLayoutParams().height = model.get(PRODUCT_ICON_HEIGHT);
        } else if (propertyKey == PRODUCT_ICON_CONTENT_DESCRIPTION_ID) {
            ImageView sheetProductIconImage = view.findViewById(R.id.branding_icon);
            sheetProductIconImage.setContentDescription(
                    view.getContext().getString(model.get(PRODUCT_ICON_CONTENT_DESCRIPTION_ID)));
        } else if (propertyKey == SECURITY_CHECK_DRAWABLE_ID) {
            ImageView sheetSecurityCheckImage = view.findViewById(R.id.security_check_illustration);
            sheetSecurityCheckImage.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(SECURITY_CHECK_DRAWABLE_ID)));
            sheetSecurityCheckImage.setVisibility(View.VISIBLE);
        } else if (propertyKey == PAYMENT_LINK_TITLE_TOP_MARGIN) {
            TextView sheetTitleText = view.findViewById(R.id.sheet_title);
            ViewGroup.MarginLayoutParams params =
                    (ViewGroup.MarginLayoutParams) sheetTitleText.getLayoutParams();
            params.topMargin = model.get(PAYMENT_LINK_TITLE_TOP_MARGIN);
            sheetTitleText.setLayoutParams(params);
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private HeaderViewBinder() {}
}
