// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.MaterialProgressBar;
import org.chromium.ui.widget.ChromeImageButton;

class PageInfoBottomSheetView extends ConstraintLayout {

    TextView mTitleText;
    TextView mContentText;
    TextView mLearnMoreText;
    Button mAcceptButton;
    Button mCancelButton;
    ChromeImageButton mBackButton;
    android.view.View mFeedbackDivider;
    ChromeImageButton mPositiveFeedbackButton;
    ChromeImageButton mNegativeFeedbackButton;
    MaterialProgressBar mLoadingIndicator;

    public PageInfoBottomSheetView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setLayoutParams(
                new LayoutParams(
                        /* width= */ LayoutParams.MATCH_PARENT,
                        /* height= */ LayoutParams.WRAP_CONTENT));
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleText = findViewById(R.id.sheet_title);
        mContentText = findViewById(R.id.summary_text);
        mLearnMoreText = findViewById(R.id.learn_more_text);
        mAcceptButton = findViewById(R.id.accept_button);
        mCancelButton = findViewById(R.id.cancel_button);
        mBackButton = findViewById(R.id.back_button);
        mFeedbackDivider = findViewById(R.id.feedback_divider);
        mPositiveFeedbackButton = findViewById(R.id.positive_feedback_button);
        mNegativeFeedbackButton = findViewById(R.id.negative_feedback_button);
        mLoadingIndicator = (MaterialProgressBar) findViewById(R.id.loading_indicator);

        mLoadingIndicator.setIndeterminate(true);
    }
}
