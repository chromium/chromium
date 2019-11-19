// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.empty;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.download.home.empty.EmptyProperties.State;
import org.chromium.chrome.browser.ui.widget.LoadingView;
import org.chromium.chrome.download.R;

/** A view that represents the visuals required for the empty state of the download home list. */
class EmptyView {
    private final ViewGroup mView;
    private final View mEmptyContainer;
    private final TextView mEmptyView;
    private final LoadingView mLoadingView;

    /** Creates a new {@link EmptyView} instance from {@code context}. */
    public EmptyView(Context context) {
        mView = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.downloads_empty_view, null);
        mEmptyContainer = mView.findViewById(R.id.empty_container);
        mEmptyView = (TextView) mView.findViewById(R.id.empty);
        mLoadingView = (LoadingView) mView.findViewById(R.id.loading);
    }

    /** The Android {@link View} representing the empty view. */
    public View getView() {
        return mView;
    }

    /** Sets the internal UI based on {@code state}. */
    public void setState(@State int state) {
        mEmptyContainer.setVisibility(state == State.EMPTY ? View.VISIBLE : View.INVISIBLE);

        if (state == State.LOADING) {
            mLoadingView.showLoadingUI();
        } else {
            mLoadingView.hideLoadingUI();
        }
    }

    /** Sets the text resource to use for the empty view. */
    public void setEmptyText(@StringRes int textId) {
        mEmptyView.setText(textId);
    }
}