// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ProgressScreenProperties.MESSAGE_TEXT;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ProgressScreenProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** A view that shows a progress spinner. */
@NullMarked
public class FacilitatedPaymentsProgressScreen implements FacilitatedPaymentsSequenceView {
    private View mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        mView =
                LayoutInflater.from(viewContainer.getContext())
                        .inflate(
                                R.layout.facilitated_payments_progress_screen,
                                viewContainer,
                                false);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public PropertyModel getModel() {
        PropertyModel model = new PropertyModel.Builder(ProgressScreenProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                model, mView, FacilitatedPaymentsProgressScreen::bindProgressScreen);
        return model;
    }

    /**
     * Updates the progress screen view based on property changes within the underlying model, such
     * as setting or hiding the progress message text.
     */
    static void bindProgressScreen(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == MESSAGE_TEXT) {
            TextView messageView = view.findViewById(R.id.progress_message);
            String message = model.get(MESSAGE_TEXT);
            if (message == null || message.isEmpty()) {
                messageView.setVisibility(View.INVISIBLE);
            } else {
                messageView.setText(message);
                messageView.setVisibility(View.VISIBLE);
            }
        }
    }

    // The progress screen isn't scrollable.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }
}
