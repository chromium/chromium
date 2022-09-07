// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.app.Dialog;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.IdRes;

/**
 * Shows a text message at the top of a Layout to show error messages.
 */
public class PersistentErrorView {
    private ViewGroup mViewGroup;

    /**
     * @param context The Activity where this View is shon.
     * @param type View type.
     */
    public PersistentErrorView(Activity context, @IdRes int errorViewId) {
        mViewGroup = (ViewGroup) context.findViewById(errorViewId);
    }

    /**
     * Set click event listener of this view.
     * @param listener listener object to handle the click event.
     * @return object reference for chaining.
     */
    public PersistentErrorView setOnClickListener(OnClickListener listener) {
        mViewGroup.setOnClickListener(listener);
        return this;
    }

    /**
     * Set a dialog to show when the view is clicked.
     * @param dialog {@link Dialog} object to to show when the view is clicked.
     * @return object reference for chaining.
     */
    public PersistentErrorView setDialog(Dialog dialog) {
        if (dialog == null) {
            setOnClickListener(null);
        } else {
            setOnClickListener(v -> dialog.show());
        }
        return this;
    }

    /**
     * Set and show the main action button. If {@code text} is {@null} the button will be hidden.
     * @param text Button text.
     * @param listener the listener to execute when the button is clicked.
     * @return object reference for chaining.
     */
    public PersistentErrorView setActionButton(String text, OnClickListener listener) {
        Button button = (Button) mViewGroup.findViewById(R.id.action_button);
        if (text == null) {
            button.setVisibility(View.GONE);
            button.setOnClickListener(null);
        } else {
            button.setVisibility(View.VISIBLE);
            button.setText(text);
            button.setOnClickListener(listener);
        }
        return this;
    }

    /**
     * Set view text.
     * @param text text {@link String} to show in the error View.
     * @return object reference for chaining.
     */
    public PersistentErrorView setText(String text) {
        TextView textView = (TextView) mViewGroup.findViewById(R.id.error_text);
        textView.setText(text);
        return this;
    }

    /**
     * Show the view by setting its visibility.
     */
    public void show() {
        mViewGroup.setVisibility(View.VISIBLE);
    }

    /**
     * Hide the view by setting its visibility.
     */
    public void hide() {
        mViewGroup.setVisibility(View.GONE);
    }
}
