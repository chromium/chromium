// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Px;

import org.chromium.ui.widget.ChipView;

/**
 * View responsible for showing CSN result items.
 */
public class ContinuousSearchChipView extends ChipView {
    private boolean mIsTwoLineChipView;

    public ContinuousSearchChipView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTwoLineChipView = false;
    }

    /**
     * Puts primary and secondary {@link TextView}s in a vertical {@link LinearLayout}.
     * TODO(crbug.com/1231230): Move this logic to ChipView.
     */
    void initTwoLineChipView() {
        assert !mIsTwoLineChipView;
        TextView primaryText = getPrimaryTextView();
        // Initialize secondary TextView
        TextView secondaryText = getSecondaryTextView();

        LinearLayout layout = new LinearLayout(getContext());
        layout.setLayoutParams(new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        layout.setOrientation(VERTICAL);

        // Modify chip  to accommodate vertically stacked TextViews
        @Px
        int sidePadding = getResources().getDimensionPixelSize(R.dimen.csn_chip_side_padding);
        @Px
        int verticalPadding =
                getResources().getDimensionPixelSize(R.dimen.csn_chip_vertical_padding);
        setPaddingRelative(sidePadding, getPaddingTop(), sidePadding, getPaddingBottom());
        layout.setPaddingRelative(0, verticalPadding, 0, verticalPadding);
        setupTextView(primaryText);
        setupTextView(secondaryText);

        // TODO(crbug.com/1231230): Rearranging the views is temporary. Double-line text support in
        // ChipView eliminates the need for this.
        removeView(primaryText);
        removeView(secondaryText);
        layout.addView(primaryText);
        layout.addView(secondaryText);
        addView(layout);
        mIsTwoLineChipView = true;
    }

    boolean isTwoLineChipView() {
        return mIsTwoLineChipView;
    }

    private void setupTextView(TextView textView) {
        textView.setPaddingRelative(0, 0, 0, 0);
        textView.setGravity(Gravity.START);
        textView.setTextAlignment(TEXT_ALIGNMENT_TEXT_START);
    }
}
