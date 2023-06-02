// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.text.EmptyTextWatcher;

/**
 * Row in a {@link androidx.recyclerview.widget.RecyclerView} for querying and filtering shown
 * bookmarks.
 */
public class BookmarkSearchBoxRow extends LinearLayout {
    private Callback<String> mQueryCallback;

    public BookmarkSearchBoxRow(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        EditText searchText = findViewById(R.id.search_text);
        searchText.addTextChangedListener(new EmptyTextWatcher() {
            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                mQueryCallback.onResult(charSequence.toString());
            }
        });
        searchText.setOnEditorActionListener(this::onEditorAction);
    }

    void setQueryCallback(Callback<String> queryCallback) {
        mQueryCallback = queryCallback;
    }

    private boolean onEditorAction(TextView textView, int actionId, KeyEvent keyEvent) {
        if (actionId == EditorInfo.IME_ACTION_SEARCH
                || keyEvent != null && keyEvent.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(textView);
            clearFocus();
            return true;
        } else {
            return false;
        }
    }
}
