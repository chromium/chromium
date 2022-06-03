// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.sort_ui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.R;
import org.chromium.ui.widget.ChipView;

/**
 * View class representing sorting options of a feed. This view contains multiple
 * ChipViews, only one of which is ever selected.
 */
public class SortView extends LinearLayout {
    private ChipView mCurrentlySelected;

    /** Creates a SortView with Context and attribute set. */
    public SortView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    void onChipSelected(ChipView chipView) {
        if (mCurrentlySelected != null) {
            mCurrentlySelected.setSelected(false);
        }
        chipView.setSelected(true);
        mCurrentlySelected = chipView;
    }

    void addButton(String text, Runnable onSelectedCallback, boolean isSelected) {
        ChipView chip = new ChipView(getContext(), null, 0, R.style.SuggestionChip);
        chip.getPrimaryTextView().setText(text);
        chip.setOnClickListener((View v) -> {
            onChipSelected((ChipView) v);
            onSelectedCallback.run();
        });
        addView(chip);
        MarginLayoutParams marginParams = (MarginLayoutParams) chip.getLayoutParams();
        marginParams.setMarginEnd(getContext().getResources().getDimensionPixelSize(
                R.dimen.feed_options_chip_margin));
        if (isSelected) {
            onChipSelected(chip);
        }
    }
}
