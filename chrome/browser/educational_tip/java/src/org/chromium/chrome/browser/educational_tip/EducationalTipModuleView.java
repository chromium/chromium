// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.widget.ButtonCompat;

/** View for the educational tip module. */
@NullMarked
public class EducationalTipModuleView extends LinearLayout {
    private static final String TAG = "EducationalTipModuleView";
    private TextView mContentTitleView;
    private TextView mContentDescriptionView;
    private ImageView mContentImageView;
    private ButtonCompat mModuleButtonView;
    private boolean mIsTitleSingleLine;
    private @Nullable OnLayoutChangeListener mOnLayoutChangeListener;

    public EducationalTipModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContentTitleView = findViewById(R.id.educational_tip_module_content_title);
        mContentDescriptionView = findViewById(R.id.educational_tip_module_content_description);
        mContentImageView = findViewById(R.id.educational_tip_module_content_image);
        mModuleButtonView = findViewById(R.id.educational_tip_module_button);
        mIsTitleSingleLine = true;

        setContentTitleViewOnLayoutChangeListener();
    }

    @VisibleForTesting
    void setContentTitleViewOnLayoutChangeListener() {
        mOnLayoutChangeListener =
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
                        mContentTitleView.post(this::updateContentTitleAndDescriptionMaxLines);
        mContentTitleView.addOnLayoutChangeListener(mOnLayoutChangeListener);
    }

    /**
     * If the title exceeds the available horizontal space, wrap it to two lines and limit the
     * description to a single line. Conversely, if the title fits within a single line, the
     * description should span two lines.
     */
    @VisibleForTesting
    void updateContentTitleAndDescriptionMaxLines() {
        try (TraceEvent e = TraceEvent.scoped(TAG + ".OnContentTitleLayoutChange()")) {
            if (mContentTitleView.getLayout() == null) {
                return;
            }

            if (mIsTitleSingleLine
                    && mContentTitleView.getLayout().getEllipsisCount(/* line= */ 0) > 0) {
                mContentTitleView.setMaxLines(2);
                mContentDescriptionView.setMaxLines(1);
                mIsTitleSingleLine = false;
                return;
            }

            if (!mIsTitleSingleLine && mContentTitleView.getLineCount() == 1) {
                mContentTitleView.setMaxLines(1);
                mContentDescriptionView.setMaxLines(2);
                mIsTitleSingleLine = true;
            }
        }
    }

    void destroyForTesting() {
        mContentTitleView.removeOnLayoutChangeListener(mOnLayoutChangeListener);
    }

    void setContentTitle(String title) {
        mContentTitleView.setText(title);
    }

    void setContentDescription(String description) {
        mContentDescriptionView.setText(description);
    }

    void setButtonText(String buttonText) {
        mModuleButtonView.setText(buttonText);
    }

    void setContentImageResource(int imageResource) {
        mContentImageView.setImageResource(imageResource);
    }

    void setModuleButtonOnClickListener(View.OnClickListener onClickListener) {
        mModuleButtonView.setOnClickListener(onClickListener);
    }

    void setContentTitleViewForTesting(TextView contentTitleView) {
        mContentTitleView = contentTitleView;
    }

    void setContentDescriptionViewForTesting(TextView contentDescriptionView) {
        mContentDescriptionView = contentDescriptionView;
    }

    boolean getIsTitleSingleLineForTesting() {
        return mIsTitleSingleLine;
    }
}
