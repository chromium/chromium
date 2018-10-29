// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.BoundedLinearLayout;

/**
 * Displays the status of a payment request to the user.
 */
public class PaymentRequestUiErrorView extends BoundedLinearLayout {

    public PaymentRequestUiErrorView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the view with the correct strings.
     *
     * @param title         Title of the webpage.
     * @param origin        Origin of the webpage.
     * @param securityLevel The security level of the page that invoked PaymentRequest.
     */
    public void initialize(String title, String origin, int securityLevel) {
        ((PaymentRequestHeader) findViewById(R.id.header))
                .setTitleAndOrigin(title, origin, securityLevel);

        // Remove the close button, then expand the page information to take up the space formerly
        // occupied by the X.
        View toRemove = findViewById(R.id.close_button);
        ((ViewGroup) toRemove.getParent()).removeView(toRemove);

        int titleEndMargin = getContext().getResources().getDimensionPixelSize(
                R.dimen.editor_dialog_section_large_spacing);
        View pageInfoGroup = findViewById(R.id.page_info);
        MarginLayoutParamsCompat.setMarginEnd(
                (MarginLayoutParams) pageInfoGroup.getLayoutParams(), titleEndMargin);
    }

    /**
     * Sets the callback to run upon hitting the OK button.
     *
     * @param callback Callback to run upon hitting the OK button.
     */
    public void setDismissRunnable(final Runnable callback) {
        // Make the user explicitly click on the OK button to dismiss the dialog.
        View confirmButton = findViewById(R.id.ok_button);
        confirmButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                callback.run();
            }
        });
    }

    /**
     * Sets what icon is displayed in the header.
     *
     * @param bitmap Icon to display.
     */
    public void setBitmap(Bitmap bitmap) {
        ((PaymentRequestHeader) findViewById(R.id.header)).setTitleBitmap(bitmap);
    }
}
