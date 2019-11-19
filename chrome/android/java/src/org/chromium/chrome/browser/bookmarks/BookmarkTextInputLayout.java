// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.TypedArray;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ChromeTextInputLayout;

/**
 * Wraps around {@link ChromeTextInputLayout} to implement a basic empty field error behavior
 * for the Bookmark related TextInputLayouts.
 */
public class BookmarkTextInputLayout extends ChromeTextInputLayout {
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
        getEditText().addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                validate();
            }
        });
    }

    /**
     * @return Trimmed text for validation.
     */
    String getTrimmedText() {
        return getEditText().getText().toString().trim();
    }

    /**
     * @return Whether the content is empty.
     */
    boolean isEmpty() {
        return TextUtils.isEmpty(getTrimmedText());
    }

    /**
     * Check the text and show or hide error message if needed.
     * If there is a need for extra validation, this method should be overridden
     * and extra validation statements should be added after calling super.validate()
     */
    void validate() {
        if (mEmptyErrorMessage != null) {
            setError(isEmpty() ? mEmptyErrorMessage : null);
        }
    }
}
