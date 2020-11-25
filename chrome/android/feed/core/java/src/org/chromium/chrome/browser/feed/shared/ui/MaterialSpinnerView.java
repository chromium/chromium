// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared.ui;

import android.content.Context;
import android.content.res.Resources.Theme;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatImageView;
import androidx.swiperefreshlayout.widget.CircularProgressDrawable;

import org.chromium.chrome.R;

/** View that shows a Material themed spinner. */
public class MaterialSpinnerView extends AppCompatImageView {
    private final CircularProgressDrawable mSpinner;

    public MaterialSpinnerView(Context context) {
        this(context, null);
    }

    public MaterialSpinnerView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    // In android everything is initialized properly after the constructor call, so it is fine to do
    // this work after.
    @SuppressWarnings({"nullness:argument.type.incompatible", "nullness:method.invocation.invalid"})
    public MaterialSpinnerView(Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mSpinner = new CircularProgressDrawable(context);
        mSpinner.setStyle(CircularProgressDrawable.DEFAULT);
        setImageDrawable(mSpinner);
        TypedValue typedValue = new TypedValue();
        Theme theme = context.getTheme();
        theme.resolveAttribute(R.attr.feedSpinnerColor, typedValue, true);
        mSpinner.setColorSchemeColors(typedValue.data);

        if (getVisibility() == View.VISIBLE) {
            mSpinner.start();
        }
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);

        if (mSpinner.isRunning() && getVisibility() != View.VISIBLE) {
            mSpinner.stop();
        } else if (!mSpinner.isRunning() && getVisibility() == View.VISIBLE) {
            mSpinner.start();
        }
    }
}
