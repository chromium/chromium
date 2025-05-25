// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.ACCEPT_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.ALL_KEYS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.DECLINE_BUTTON_CALLBACK;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

/** This class is used to show the PIX account linking prompt. */
@NullMarked
public class PixAccountLinkingPrompt implements FacilitatedPaymentsSequenceView {
    private LinearLayout mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        mView =
                (LinearLayout)
                        LayoutInflater.from(viewContainer.getContext())
                                .inflate(R.layout.pix_account_linking_prompt, viewContainer, false);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public PropertyModel getModel() {
        PropertyModel model = new PropertyModel.Builder(ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, mView, PixAccountLinkingPrompt::bind);
        return model;
    }

    // The Pix account linking prompt isn't scrollable.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ACCEPT_BUTTON_CALLBACK) {
            ButtonCompat acceptButton = view.findViewById(R.id.accept_button);
            acceptButton.setOnClickListener(model.get(ACCEPT_BUTTON_CALLBACK));
        } else if (propertyKey == DECLINE_BUTTON_CALLBACK) {
            ButtonCompat declineButton = view.findViewById(R.id.decline_button);
            declineButton.setOnClickListener(model.get(DECLINE_BUTTON_CALLBACK));
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }
}
