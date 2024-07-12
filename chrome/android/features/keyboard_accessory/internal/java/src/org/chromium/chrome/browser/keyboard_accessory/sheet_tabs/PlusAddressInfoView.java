// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;

class PlusAddressInfoView extends LinearLayout {
    private ChipView mPlusAddress;
    private ImageView mIcon;

    /** Constructor for inflating from XML. */
    public PlusAddressInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPlusAddress = findViewById(R.id.plus_address);
        mIcon = findViewById(R.id.favicon);
    }

    void setIconForBitmap(@Nullable Drawable icon) {
        final int kIconSize =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.keyboard_accessory_suggestion_icon_size);
        if (icon != null) icon.setBounds(0, 0, kIconSize, kIconSize);
        mIcon.setImageDrawable(icon);
    }

    ChipView getPlusAddress() {
        return mPlusAddress;
    }
}
