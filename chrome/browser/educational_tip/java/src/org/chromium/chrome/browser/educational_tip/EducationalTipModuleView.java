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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.ui.widget.ButtonCompat;

/** View for the educational tip module. */
public class EducationalTipModuleView extends LinearLayout {
    private TextView mContentTitleView;
    private TextView mContentDescriptionView;
    private ImageView mContentImageView;
    private ButtonCompat mModuleButtonView;

    public EducationalTipModuleView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContentTitleView = findViewById(R.id.educational_tip_module_content_title);
        mContentDescriptionView = findViewById(R.id.educational_tip_module_content_description);
        mContentImageView = findViewById(R.id.educational_tip_module_content_image);
        mModuleButtonView = findViewById(R.id.educational_tip_module_button);
    }

    void setContentTitle(@NonNull String title) {
        mContentTitleView.setText(title);
    }

    void setContentDescription(@NonNull String description) {
        mContentDescriptionView.setText(description);
    }

    void setContentImageResource(int imageResource) {
        mContentImageView.setImageResource(imageResource);
    }

    void setModuleButtonOnClickListener(@NonNull View.OnClickListener onClickListener) {
        mModuleButtonView.setOnClickListener(onClickListener);
    }
}
