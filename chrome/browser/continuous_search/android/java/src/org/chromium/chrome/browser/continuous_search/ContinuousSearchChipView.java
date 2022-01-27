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
import androidx.core.view.ViewCompat;

import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * View responsible for showing CSN result items.
 */
public class ContinuousSearchChipView extends ChipView {
    private boolean mIsTwoLineChipView;

    public ContinuousSearchChipView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTwoLineChipView = false;
        // One-row chip padding modification.
        int padding =
                getResources().getDimensionPixelOffset(R.dimen.csn_single_row_chip_side_padding);
        ViewCompat.setPaddingRelative(this, padding, 0, padding, 0);
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

        // Modify TextViews to have them vertically stacked.
        setupTextView(primaryText);
        setupTextView(secondaryText);

        // TODO(crbug.com/1231230): Rearranging the views is temporary. Double-line text support in
        // ChipView eliminates the need for this.
        removeView(primaryText);
        removeView(secondaryText);
        layout.addView(primaryText);
        layout.addView(secondaryText);
        addView(layout);

        // Adjust chip paddings
        @Px
        int sidePadding =
                getResources().getDimensionPixelSize(R.dimen.csn_double_row_chip_side_padding);
        @Px
        int verticalPadding =
                getResources().getDimensionPixelSize(R.dimen.csn_double_row_chip_vertical_padding);
        ViewCompat.setPaddingRelative(
                this, sidePadding, verticalPadding, sidePadding, verticalPadding);
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
