// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletAccountLinkingPromptProperties.ACCEPT_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletAccountLinkingPromptProperties.ALL_KEYS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletAccountLinkingPromptProperties.DECLINE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletAccountLinkingPromptProperties.DECLINE_BUTTON_TEXT_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletAccountLinkingPromptProperties.EWALLET_NAME;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

/** This class is used to show the eWallet account linking prompt. */
@NullMarked
public class EwalletAccountLinkingPrompt implements FacilitatedPaymentsSequenceView {
    private ScrollView mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        mView =
                (ScrollView)
                        LayoutInflater.from(viewContainer.getContext())
                                .inflate(
                                        R.layout.ewallet_account_linking_prompt,
                                        viewContainer,
                                        false);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public PropertyModel getModel() {
        PropertyModel model = new PropertyModel.Builder(ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, mView, EwalletAccountLinkingPrompt::bind);
        return model;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mView.getScrollY();
    }

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ACCEPT_BUTTON_CALLBACK) {
            ButtonCompat acceptButton = view.findViewById(R.id.accept_button);
            acceptButton.setOnClickListener(model.get(ACCEPT_BUTTON_CALLBACK));
        } else if (propertyKey == DECLINE_BUTTON_CALLBACK) {
            ButtonCompat declineButton = view.findViewById(R.id.decline_button);
            declineButton.setOnClickListener(model.get(DECLINE_BUTTON_CALLBACK));
        } else if (propertyKey == EWALLET_NAME) {
            TextView title = view.findViewById(R.id.title);
            String providerName = model.get(EWALLET_NAME);
            title.setText(
                    view.getContext()
                            .getString(
                                    R.string.ewallet_account_linking_prompt_title,
                                    providerName != null ? providerName : ""));
        } else if (propertyKey == DECLINE_BUTTON_TEXT_ID) {
            ButtonCompat declineButton = view.findViewById(R.id.decline_button);
            declineButton.setText(model.get(DECLINE_BUTTON_TEXT_ID));
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }
}
