// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyModel;

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

        // TODO: b/351738890 - Enable features to set string resources so it can be reused.
        TextView titleView = mView.findViewById(R.id.title);
        titleView.setText(
                mView.getContext()
                        .getResources()
                        .getString(R.string.pix_payment_error_screen_title));
        TextView descriptionView = mView.findViewById(R.id.description);
        descriptionView.setText(
                mView.getContext()
                        .getResources()
                        .getString(R.string.pix_payment_error_screen_description));
    }

    @Override
    public View getView() {
        return mView;
    }

    // The error screen doesn't have any properties set by the feature. So its view model is empty.
    @Override
    public PropertyModel getModel() {
        return new PropertyModel();
    }

    // The error screen isn't scrollable.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }
}
