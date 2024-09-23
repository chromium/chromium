// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * This view represents a section of IBAN details in the payment methods tab of the keyboard
 * accessory (manual fallback) sheet.
 */
class IbanAccessoryInfoView extends LinearLayout {
    private ImageView mIcon;
    private ChipView mValue;

    /** Constructor for inflating from XML. */
    public IbanAccessoryInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIcon = findViewById(R.id.icon);
        mValue = findViewById(R.id.value);
    }

    void setIcon(@Nullable Drawable drawable) {
        if (drawable == null) {
            mIcon.setVisibility(View.GONE);
            return;
        }
        mIcon.setVisibility(View.VISIBLE);
        mIcon.setImageDrawable(drawable);
    }

    ChipView getValue() {
        return mValue;
    }
}
