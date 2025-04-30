// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * This view represents a section of Google Wallet loyalty card details in the payment methods tab
 * of the keyboard accessory (manual fallback) sheet.
 */
class LoyaltyCardInfoView extends LinearLayout {
    private TextView mMerchantName;
    private ImageView mIcon;
    private ChipView mLoyaltyCardNumber;

    /** Constructor for inflating from XML. */
    public LoyaltyCardInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mMerchantName = findViewById(R.id.merchant_name);
        mIcon = findViewById(R.id.loyalty_card_icon);
        mLoyaltyCardNumber = findViewById(R.id.loyalty_card_number);
    }

    TextView getMerchantName() {
        return mMerchantName;
    }

    ChipView getLoyaltyCardNumber() {
        return mLoyaltyCardNumber;
    }

    public void setIcon(@Nullable Drawable drawable) {
        if (drawable == null) {
            mIcon.setVisibility(View.GONE);
            return;
        }
        mIcon.setVisibility(View.VISIBLE);
        mIcon.setImageDrawable(drawable);
    }
}
