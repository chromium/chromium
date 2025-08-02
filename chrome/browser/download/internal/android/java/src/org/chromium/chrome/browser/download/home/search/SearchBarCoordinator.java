// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.search;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.internal.R;

/** A coordinator for the search toolbar in the download home. */
@NullMarked
public class SearchBarCoordinator {
    private final View mView;
    private final EditText mEditText;
    private final Callback<String> mQueryCallback;

    public SearchBarCoordinator(
            Context context, Callback<String> queryCallback, boolean autoFocusSearchBox) {
        mView = LayoutInflater.from(context).inflate(R.layout.download_search_bar, null);
        mEditText = mView.findViewById(R.id.search_text);
        mQueryCallback = queryCallback;

        mEditText.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {}

                    @Override
                    public void afterTextChanged(Editable s) {
                        mQueryCallback.onResult(s.toString());
                    }
                });

        if (autoFocusSearchBox) {
            mEditText.addOnAttachStateChangeListener(
                    new View.OnAttachStateChangeListener() {
                        @Override
                        public void onViewAttachedToWindow(View v) {
                            v.requestFocus();
                        }

                        @Override
                        public void onViewDetachedFromWindow(View v) {}
                    });
        }
    }

    /**
     * @return The view of the search bar.
     */
    public View getView() {
        return mView;
    }
}
