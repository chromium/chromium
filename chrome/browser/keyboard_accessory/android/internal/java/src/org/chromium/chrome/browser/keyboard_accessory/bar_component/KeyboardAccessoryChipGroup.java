// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.content.Context;
import android.content.res.Resources;
import android.widget.LinearLayout;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * Groups 2 or 3 {@link ChipView}.
 *
 * <p>Keyboard Accessory chips are displayed in a {@link RecyclerView}. By default, the Keyboard
 * Accessory chips aren't limited by width. This class is used to limit the width of the first chip
 * or the first 2 chips so that the next one is partially displayed on the screen. This is done to
 * hint the user that the Keyboard Accessory UI is scrollable.
 *
 * <p>TODO: crbug.com/385172647 - Add margins between children and take them into account during
 * measurement.
 */
@NullMarked
class KeyboardAccessoryChipGroup extends LinearLayout {

    public KeyboardAccessoryChipGroup(Context context) {
        super(context);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        resetMaxWidth();
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        if (getChildCount() < 2) {
            return;
        }

        ChipView firstChip = (ChipView) getChildAt(0);
        ChipView secondChip = (ChipView) getChildAt(1);

        Resources resources = getContext().getResources();
        @Px
        int lastChipPeekWidth =
                resources.getDimensionPixelSize(R.dimen.keyboard_accessory_last_chip_peek_width);
        @Px int chipBuffer = resources.getDisplayMetrics().widthPixels - lastChipPeekWidth;
        if (firstChip.getMeasuredWidth() > chipBuffer) {
            firstChip.setMaxWidth(chipBuffer);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        if (getChildCount() < 3) {
            return;
        }
        if (firstChip.getMeasuredWidth() + secondChip.getMeasuredWidth() > chipBuffer) {
            // Proportionally decreases the width of both chips:
            //
            // firstChip.getMeasuredWidth() = a,
            // secondChip.getMeasuredWidth() = b,
            //
            // firstWidth = chipBuffer * (a / (a + b)),
            // secondWidth = chipBuffer * (b / (a + b)).
            int firstWidth =
                    (chipBuffer * firstChip.getMeasuredWidth())
                            / (firstChip.getMeasuredWidth() + secondChip.getMeasuredWidth());
            int secondWidth =
                    (chipBuffer * secondChip.getMeasuredWidth())
                            / (firstChip.getMeasuredWidth() + secondChip.getMeasuredWidth());
            firstChip.setMaxWidth(firstWidth);
            secondChip.setMaxWidth(secondWidth);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    private void resetMaxWidth() {
        for (int i = 0; i < getChildCount(); i++) {
            ChipView chip = (ChipView) getChildAt(i);
            chip.setMaxWidth(Integer.MAX_VALUE);
        }
    }
}
