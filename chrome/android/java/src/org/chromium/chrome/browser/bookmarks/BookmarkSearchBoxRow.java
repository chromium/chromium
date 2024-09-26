// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Row in a {@link androidx.recyclerview.widget.RecyclerView} for querying and filtering shown
 * bookmarks.
 */
public class BookmarkSearchBoxRow extends LinearLayout {
    private EditText mSearchText;
    private ChromeImageButton mClearSearchTextButton;
    private @Nullable Callback<String> mSearchTextCallback;

    public BookmarkSearchBoxRow(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSearchText = findViewById(R.id.row_search_text);
        mSearchText.setOnEditorActionListener(this::onEditorAction);
        mSearchText.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                        if (mSearchTextCallback != null) {
                            mSearchTextCallback.onResult(charSequence.toString());
                        }
                    }
                });
        mClearSearchTextButton = findViewById(R.id.clear_text_button);
    }

    void setSearchTextCallback(Callback<String> searchTextCallback) {
        mSearchTextCallback = searchTextCallback;
    }

    void setSearchText(String modelText) {
        if (!TextUtils.equals(modelText, mSearchText.getText())) {
            mSearchText.setText(modelText);
        }
    }

    void setFocusChangeCallback(Callback<Boolean> focusChangeCallback) {
        mSearchText.setOnFocusChangeListener(
                (view, hasFocus) -> {
                    assert view == mSearchText;
                    focusChangeCallback.onResult(hasFocus);
                });
    }

    void setHasFocus(boolean modelHasFocus) {
        boolean viewHasFocus = mSearchText.hasFocus();
        if (modelHasFocus == viewHasFocus) return;
        if (modelHasFocus) {
            mSearchText.requestFocus();
        } else {
            mSearchText.clearFocus();
        }
    }

    void setClearSearchTextButtonRunnable(Runnable onClearSearchTextButtonRunnable) {
        mClearSearchTextButton.setOnClickListener(
                (view) -> {
                    assert view == mClearSearchTextButton;
                    onClearSearchTextButtonRunnable.run();
                });
    }

    void setClearSearchTextButtonVisibility(boolean isVisible) {
        mClearSearchTextButton.setVisibility(isVisible ? View.VISIBLE : View.INVISIBLE);
    }

    private boolean onEditorAction(TextView textView, int actionId, KeyEvent keyEvent) {
        assert textView == mSearchText;
        if (actionId == EditorInfo.IME_ACTION_SEARCH
                || (keyEvent != null && keyEvent.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(textView);
            mSearchText.clearFocus();
            return true;
        } else {
            return false;
        }
    }
}
