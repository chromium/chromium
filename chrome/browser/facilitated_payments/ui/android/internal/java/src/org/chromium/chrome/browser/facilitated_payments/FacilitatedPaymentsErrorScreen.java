// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ErrorScreenProperties.PRIMARY_BUTTON_CALLBACK;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ErrorScreenProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

// This class is used to show a error screen.
public class FacilitatedPaymentsErrorScreen implements FacilitatedPaymentsSequenceView {
    private LinearLayout mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        mView =
                (LinearLayout)
                        LayoutInflater.from(viewContainer.getContext())
                                .inflate(
                                        R.layout.facilitated_payments_error_screen,
                                        viewContainer,
                                        false);
    }

    @Override
    public View getView() {
        // TODO: b/351738890 - Enable features to set string resources so it can be reused.
        return mView;
    }

    /**
     * The {@link PropertyModel} for the error screen has a single property:
     *
     * <p>PRIMARY_BUTTON_CALLBACK: Callback for the primary button.
     */
    @Override
    public PropertyModel getModel() {
        PropertyModel model = new PropertyModel.Builder(ErrorScreenProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                model, mView, FacilitatedPaymentsErrorScreen::bindErrorScreen);
        return model;
    }

    // The error screen isn't scrollable.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    static void bindErrorScreen(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == PRIMARY_BUTTON_CALLBACK) {
            ButtonCompat primaryButton = view.findViewById(R.id.primary_button);
            primaryButton.setOnClickListener(model.get(PRIMARY_BUTTON_CALLBACK));
        }
    }
}
