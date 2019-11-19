// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * The dialog content view for illustration dialogs used by the password manager (e.g. onboarding,
 * leak warning).
 */
public class PasswordManagerDialogView extends ScrollView {
    private ImageView mIllustrationView;
    private TextView mTitleView;
    private TextView mDetailsView;

    public PasswordManagerDialogView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIllustrationView = findViewById(R.id.password_manager_dialog_illustration);
        mTitleView = findViewById(R.id.password_manager_dialog_title);
        mDetailsView = findViewById(R.id.password_manager_dialog_details);
    }

    void setIllustration(int illustration) {
        mIllustrationView.setImageResource(illustration);
    }

    public void updateIllustrationVisibility(boolean illustrationVisible) {
        if (illustrationVisible) {
            mIllustrationView.setVisibility(VISIBLE);
        } else {
            mIllustrationView.setVisibility(GONE);
        }
    }

    void setTitle(String title) {
        mTitleView.setText(title);
    }

    void setDetails(CharSequence details) {
        mDetailsView.setText(details);
    }
}
