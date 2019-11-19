// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * The dialog content view for password generation dialog.
 */
public class PasswordGenerationDialogCustomView extends LinearLayout {
    // TODO(crbug.com/835234): Make the generated password editable.
    private TextView mGeneratedPasswordTextView;
    private TextView mSaveExplantaionTextView;

    /**
     * Constructor for inflating from XML.
     */
    public PasswordGenerationDialogCustomView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mGeneratedPasswordTextView = findViewById(R.id.generated_password);
        mSaveExplantaionTextView = findViewById(R.id.generation_save_explanation);
    }

    public void setGeneratedPassword(String generatedPassword) {
        mGeneratedPasswordTextView.setText(generatedPassword);
    }

    public void setSaveExplanationText(String saveExplanationText) {
        mSaveExplantaionTextView.setText(saveExplanationText);
    }
}
