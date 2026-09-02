// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View.OnClickListener;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.widget.ButtonCompat;

/** View for the Safety Promo Carousel during the First Run Experience (FRE). */
@NullMarked
public class SafetyPromoCarouselView extends RelativeLayout {
    private RecyclerView mRecyclerView;
    private TextView mTitleView;
    private TextView mSubtitleView;
    private ButtonCompat mContinueButton;

    public SafetyPromoCarouselView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mRecyclerView = findViewById(R.id.safety_promo_carousel_recycler_view);
        mTitleView = findViewById(R.id.safety_promo_carousel_title);
        mSubtitleView = findViewById(R.id.safety_promo_carousel_subtitle);
        mContinueButton = findViewById(R.id.fre_continue_button);
    }

    public void setContinueButtonOnClickListener(OnClickListener listener) {
        mContinueButton.setOnClickListener(listener);
    }

    public void setTitleText(@StringRes int stringResId) {
        mTitleView.setText(stringResId);
    }

    public void setSubtitleText(@StringRes int stringResId) {
        mSubtitleView.setText(stringResId);
    }

    RecyclerView getRecyclerView() {
        return mRecyclerView;
    }
}
