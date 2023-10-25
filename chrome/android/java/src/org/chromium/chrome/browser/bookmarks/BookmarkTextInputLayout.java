// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.TypedArray;
import android.text.Editable;
import android.text.TextUtils;
import android.util.AttributeSet;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.chrome.R;
import org.chromium.ui.text.EmptyTextWatcher;

/**
 * Wraps around {@link TextInputLayout} to implement a basic empty field error behavior for the
 * Bookmark related TextInputLayouts.
 */
public class BookmarkTextInputLayout extends TextInputLayout {
    private String mEmptyErrorMessage;

    public BookmarkTextInputLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        final TypedArray a =
                context.obtainStyledAttributes(attrs, R.styleable.BookmarkTextInputLayout);
        final int emptyErrorMessageId =
                a.getResourceId(R.styleable.BookmarkTextInputLayout_emptyErrorMessage, 0);
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
     * Check the text and show or hide error message if needed. If there is a need for extra
     * validation, this method should be overridden and extra validation statements should be added
     * after calling super.validate()
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
