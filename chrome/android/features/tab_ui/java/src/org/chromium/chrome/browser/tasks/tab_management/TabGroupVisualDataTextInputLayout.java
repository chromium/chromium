// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.TypedArray;
import android.text.Editable;
import android.text.TextUtils;
import android.util.AttributeSet;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.text.EmptyTextWatcher;

/**
 * Wraps around {@link TextInputLayout} to implement a basic empty field error behavior for the tab
 * group title during tab group creation.
 */
public class TabGroupVisualDataTextInputLayout extends TextInputLayout {
    private String mEmptyErrorMessage;

    public TabGroupVisualDataTextInputLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        final TypedArray a =
                context.obtainStyledAttributes(attrs, R.styleable.TabGroupVisualDataTextInputLayout);
        final int emptyErrorMessageId =
                a.getResourceId(R.styleable.TabGroupVisualDataTextInputLayout_emptyErrorMessage, 0);
        if (emptyErrorMessageId != 0) {
            mEmptyErrorMessage = context.getResources().getString(emptyErrorMessageId);
        }

        a.recycle();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        getEditText()
                .addTextChangedListener(
                        new EmptyTextWatcher() {
                            @Override
                            public void afterTextChanged(Editable s) {
                                validate();
                            }
                        });
    }

    /**
     * @return Trimmed text for validation.
     */
    public String getTrimmedText() {
        return getEditText().getText().toString().trim();
    }

    /**
     * @return Whether the content is empty.
     */
    public boolean isEmpty() {
        return TextUtils.isEmpty(getTrimmedText());
    }

    /**
     * Check the text and show or hide error message if needed.
     *
     * @return whether the input is valid.
     */
    public boolean validate() {
        boolean isValid = !isEmpty();
        if (mEmptyErrorMessage != null) {
            setError(isValid ? null : mEmptyErrorMessage);
            setErrorEnabled(!isValid);
        }

        return isValid;
    }
}
