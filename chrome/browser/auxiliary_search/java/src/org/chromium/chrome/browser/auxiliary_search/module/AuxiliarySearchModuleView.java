// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.auxiliary_search.R;

/** The view for the auxiliary search module. */
public class AuxiliarySearchModuleView extends LinearLayout {
    private TextView mFirstButtonView;
    private Button mSecondButtonView;

    public AuxiliarySearchModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mFirstButtonView = findViewById(R.id.auxiliary_search_first_button);
        mSecondButtonView = findViewById(R.id.auxiliary_search_second_button);
    }

    void setFirstButtonOnClickListener(@NonNull View.OnClickListener onClickListener) {
        mFirstButtonView.setOnClickListener(onClickListener);
    }

    void setSecondButtonOnClickListener(@NonNull View.OnClickListener onClickListener) {
        mSecondButtonView.setOnClickListener(onClickListener);
    }
}
