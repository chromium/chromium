// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * Provides functions that map changes of {@link
 * FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties} in a {@link PropertyModel}
 * to the additional info view in the payment methods bottom sheet.
 */
@NullMarked
class AdditionalInfoViewBinder {
    static View createAdditionalInfoView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.facilitated_payments_payment_methods_additional_info,
                        parent,
                        false);
    }

    static void bindAdditionalInfoView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == AdditionalInfoProperties.DESCRIPTION_ID
                || propertyKey == SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK) {
            TextViewWithClickableSpans descriptionLine1 =
                    (TextViewWithClickableSpans) view.findViewById(R.id.description_line);
            descriptionLine1.setText(
                    getSpannableStringWithClickableSpansToOpenLinks(
                            view.getContext(),
                            model.get(AdditionalInfoProperties.DESCRIPTION_ID),
                            model.get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)));
            descriptionLine1.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private static SpannableString getSpannableStringWithClickableSpansToOpenLinks(
            Context context, int stringResourceId, Runnable callback) {
        return SpanApplier.applySpans(
                context.getString(stringResourceId),
                new SpanApplier.SpanInfo(
                        "<link1>",
                        "</link1>",
                        new ChromeClickableSpan(context, unused -> callback.run())));
    }

    private AdditionalInfoViewBinder() {}
}
