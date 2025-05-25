// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** This class is used to show the PIX account linking prompt. */
@NullMarked
public class PixAccountLinkingPrompt implements FacilitatedPaymentsSequenceView {
    private LinearLayout mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        // TODO(crbug.com/417330610): Replace with actual layout for PIX account linking.
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
        return mView;
    }

    @Override
    public PropertyModel getModel() {
        // TODO(crbug.com/417330610): Define callbacks as properties.
        return new PropertyModel();
    }

    // The Pix account linking prompt isn't scrollable.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }
}
