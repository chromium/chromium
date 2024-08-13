// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.ui.widget.ButtonCompat;

/** Container view for the Safety Hub Magic Stack module. */
class SafetyHubMagicStackView extends LinearLayout {
    private TextView mHeaderView;
    private TextView mTitleView;
    private TextView mSummaryView;
    private ImageView mIconView;
    private ButtonCompat mButtonView;

    public SafetyHubMagicStackView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHeaderView = findViewById(R.id.header);
        mTitleView = findViewById(R.id.title);
        mSummaryView = findViewById(R.id.summary);
        mIconView = findViewById(R.id.icon);
        mButtonView = findViewById(R.id.button);
    }

    void setHeader(String header) {
        mHeaderView.setText(header);
    }

    void setTitle(String title) {
        mTitleView.setText(title);
    }

    void setSummary(String summary) {
        mSummaryView.setText(summary);
        mSummaryView.setVisibility(TextUtils.isEmpty(summary) ? View.GONE : View.VISIBLE);
    }

    void setIconDrawable(Drawable icon) {
        mIconView.setImageDrawable(icon);
    }

    void setButtonText(String text) {
        mButtonView.setText(text);
    }

    void setButtonOnClickListener(OnClickListener onClickListener) {
        mButtonView.setOnClickListener(onClickListener);
    }
}
