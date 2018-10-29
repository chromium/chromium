// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ListMenuButton;

/** The toolbar view, containing an icon, title and close button. */
public class ToolbarView extends FrameLayout {
    private View mCloseButton;
    private ListMenuButton mMenuButton;
    private TextView mTitle;
    private View mShadow;

    public ToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCloseButton = findViewById(R.id.close_button);
        mMenuButton = findViewById(R.id.more);
        mTitle = (TextView) findViewById(R.id.title);
        mShadow = findViewById(R.id.shadow);
    }

    void setCloseButtonOnClickListener(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    void setMenuButtonDelegate(ListMenuButton.Delegate delegate) {
        mMenuButton.setDelegate(delegate);
    }

    void setTitle(String title) {
        mTitle.setText(title);
    }

    void setShadowVisibility(boolean visible) {
        mShadow.setVisibility(visible ? View.VISIBLE : View.GONE);
    }
}
