// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.ui.widget.TextViewWithLeading;

/** The view for the auxiliary search module. */
@NullMarked
public class AuxiliarySearchModuleView extends LinearLayout {
    private TextViewWithLeading mContentView;
    private Button mFirstButtonView;
    private Button mSecondButtonView;

    public AuxiliarySearchModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mContentView = findViewById(R.id.auxiliary_search_module_content);
        mFirstButtonView = findViewById(R.id.auxiliary_search_first_button);
        mSecondButtonView = findViewById(R.id.auxiliary_search_second_button);
    }

    void setFirstButtonOnClickListener(View.OnClickListener onClickListener) {
        mFirstButtonView.setOnClickListener(onClickListener);
    }

    void setSecondButtonOnClickListener(View.OnClickListener onClickListener) {
        mSecondButtonView.setOnClickListener(onClickListener);
    }

    void setContentText(String text) {
        mContentView.setText(text);
    }

    void setFirstButtonText(String text) {
        mFirstButtonView.setText(text);
    }

    void setSecondButtonText(String text) {
        mSecondButtonView.setText(text);
    }

    void setContentTextResId(int resId) {
        mContentView.setText(getContext().getString(resId));
    }

    void setFirstButtonTextResId(int resId) {
        mFirstButtonView.setText(getContext().getString(resId));
    }

    void setSecondButtonTextResId(int resId) {
        mSecondButtonView.setText(getContext().getString(resId));
    }
}
