// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.ui.widget.ChromeImageButton;

class PageInfoBottomSheetView extends LinearLayout {

    TextView mTitleText;
    TextView mContentText;
    Button mAcceptButton;
    Button mCancelButton;
    ChromeImageButton mRefreshButton;
    View mLoadingIndicator;

    public PageInfoBottomSheetView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setLayoutParams(
                new LayoutParams(
                        /* width= */ LayoutParams.MATCH_PARENT,
                        /* height= */ LayoutParams.WRAP_CONTENT));
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleText = findViewById(R.id.sheet_title);
        mContentText = findViewById(R.id.sheet_content);
        mAcceptButton = findViewById(R.id.accept_button);
        mCancelButton = findViewById(R.id.cancel_button);
        mRefreshButton = findViewById(R.id.refresh_button);
        mLoadingIndicator = findViewById(R.id.loading_indicator);
    }
}
