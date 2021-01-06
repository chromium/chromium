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

import org.chromium.base.FeatureList;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** View that shows a Material themed spinner. */
public class MaterialSpinnerView extends AppCompatImageView {
    private final CircularProgressDrawable mSpinner;
    private final boolean mAlwaysAnimate;

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
        TraceEvent.begin("MaterialSpinnerView");
        mSpinner = new CircularProgressDrawable(context);
        mSpinner.setStyle(CircularProgressDrawable.DEFAULT);
        setImageDrawable(mSpinner);
        TypedValue typedValue = new TypedValue();
        Theme theme = context.getTheme();
        theme.resolveAttribute(R.attr.feedSpinnerColor, typedValue, true);
        mSpinner.setColorSchemeColors(typedValue.data);
        mAlwaysAnimate = FeatureList.isInitialized() ? ChromeFeatureList.isEnabled(
                                 ChromeFeatureList.INTEREST_FEED_SPINNER_ALWAYS_ANIMATE)
                                                     : false;
        updateAnimationState(isAttachedToWindow());
        TraceEvent.end("MaterialSpinnerView");
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        updateAnimationState(isAttachedToWindow());
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        updateAnimationState(/*isAttached=*/true);
    }

    @Override
    protected void onDetachedFromWindow() {
        // isAttachedToWindow() doesn't turn false during onDetachedFromWindow(), so we pass the new
        // attachment state into updateAnimationState() here explicitly.
        updateAnimationState(/*isAttached=*/false);
        super.onDetachedFromWindow();
    }

    private void updateAnimationState(boolean isAttached) {
        // Some Android versions call onVisibilityChanged() during the View's constructor before the
        // spinner is created.
        if (mSpinner == null) return;

        // TODO(crbug.com/1151391): This feature is used for A:B testing to determine the impact of
        // a bug fix. Remove after experiment is complete.
        if (mAlwaysAnimate) {
            if (!mSpinner.isRunning()) mSpinner.start();
            return;
        }

        boolean visible = isShown() && isAttached;
        if (mSpinner.isRunning() && !visible) {
            mSpinner.stop();
        } else if (!mSpinner.isRunning() && visible) {
            mSpinner.start();
        }
    }
}
