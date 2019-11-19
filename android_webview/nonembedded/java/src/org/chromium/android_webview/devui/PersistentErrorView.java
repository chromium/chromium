// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Shows a text message at the top of a LinearLayout to show error and warning messages.
 */
public class PersistentErrorView {
    /**
     * Error message type.
     */
    public enum Type {
        ERROR,
        WARNING,
    }

    private TextView mTextView;

    /**
     * @param context The Activity where this View is shon.
     * @param type View type.
     */
    public PersistentErrorView(Activity context, Type type) {
        mTextView = (TextView) context.getLayoutInflater().inflate(
                R.layout.persistent_error_message, null);

        switch (type) {
            case ERROR:
                mTextView.setBackgroundResource(R.color.error_red);
                mTextView.setTextColor(Color.WHITE);
                break;
            case WARNING:
                mTextView.setBackgroundResource(R.color.warning_yellow);
                mTextView.setTextColor(Color.BLACK);
                break;
        }
    }

    /**
     * Add the error message view at the top of the given {@link LinearLayout}
     * @param layout the linear layout to add this view at the top.
     * @return object reference for chaining.
     */
    public PersistentErrorView prependToLinearLayout(LinearLayout layout) {
        layout.addView(mTextView, 0);
        return this;
    }

    /**
     * Set click event listener of this view.
     * @param listener listener object to handle the click event.
     * @return object reference for chaining.
     */
    public PersistentErrorView setOnClickListener(OnClickListener listener) {
        mTextView.setOnClickListener(listener);
        return this;
    }

    /**
     * Set a dialog to show when the view is clicked.
     * @param dialog {@link Dialog} object to to show when the view is clicked.
     * @return object reference for chaining.
     */
    public PersistentErrorView setDialog(Dialog dialog) {
        setOnClickListener(v -> dialog.show());
        return this;
    }

    /**
     * Set view text.
     * @param text text {@link String} to show in the error View.
     * @return object reference for chaining.
     */
    public PersistentErrorView setText(String text) {
        mTextView.setText(text);
        return this;
    }

    /**
     * Show the view by setting its visibility.
     */
    public void show() {
        mTextView.setVisibility(View.VISIBLE);
    }

    /**
     * Hide the view by setting its visibility.
     */
    public void hide() {
        mTextView.setVisibility(View.GONE);
    }
}
