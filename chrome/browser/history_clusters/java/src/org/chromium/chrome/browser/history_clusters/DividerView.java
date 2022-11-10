// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout.LayoutParams;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.base.ViewUtils;

/**
 * A horizontal divider view with a fixed height that spans the width of its parent along the
 * bottom. Initially hidden.
 */
class DividerView extends View {
    public DividerView(
            Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        setBackground(AppCompatResources.getDrawable(getContext(), R.drawable.rectangle_surface_1));
        setVisibility(View.GONE);
    }

    public void addToParent(ViewGroup parent, LayoutParams layoutParams) {
        layoutParams.gravity = Gravity.BOTTOM;
        layoutParams.width = LayoutParams.MATCH_PARENT;
        layoutParams.height = getResources().getDimensionPixelSize(R.dimen.divider_height);
        parent.addView(this, layoutParams);
    }

    public void setIsThickDivider(boolean isThick) {
        LayoutParams layoutParams = (LayoutParams) getLayoutParams();
        if (isThick) {
            layoutParams.height =
                    getResources().getDimensionPixelSize(R.dimen.thick_divider_height);
        } else {
            layoutParams.height = getResources().getDimensionPixelSize(R.dimen.divider_height);
        }
        ViewUtils.requestLayout(this, "DividerView.setIsThickDivider");
    }
}
