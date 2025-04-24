// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * This view represents a section of Google Wallet loyalty card details in the payment methods tab
 * of the keyboard accessory (manual fallback) sheet.
 */
class LoyaltyCardInfoView extends LinearLayout {
    private TextView mMerchantName;
    private ChipView mLoyaltyCardNumber;

    /** Constructor for inflating from XML. */
    public LoyaltyCardInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mMerchantName = findViewById(R.id.merchant_name);
        mLoyaltyCardNumber = findViewById(R.id.loyalty_card_number);
    }

    TextView getMerchantName() {
        return mMerchantName;
    }

    ChipView getLoyaltyCardNumber() {
        return mLoyaltyCardNumber;
    }
}
