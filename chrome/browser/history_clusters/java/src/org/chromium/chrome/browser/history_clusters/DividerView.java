// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout.LayoutParams;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;

/**
 * A horizontal divider view with a fixed height that spans the width of its parent along the
 * bottom. Initially hidden.
 */
class DividerView extends View {
    public DividerView(
            Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        setVisibility(View.GONE);
    }

    public void addToParent(ViewGroup parent, LayoutParams layoutParams) {
        layoutParams.gravity = Gravity.BOTTOM;
        layoutParams.width = LayoutParams.MATCH_PARENT;
        layoutParams.height = getResources().getDimensionPixelSize(R.dimen.divider_height);
        parent.addView(this, layoutParams);
    }

    public void setHeightRes(@DimenRes int dimenResId) {
        getLayoutParams().height = getResources().getDimensionPixelSize(dimenResId);
        requestLayout();
    }
}
