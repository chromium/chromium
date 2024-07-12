// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Paint;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.tab_ui.R;

/**
 * Contains pricing information relating to a shopping offer website.  Currently only
 * supports displaying the old and new price of the offer when a price drop is detected.
 */
public class PriceCardView extends FrameLayout {
    private TextView mPriceInfoBox;
    private TextView mPreviousPriceInfoBox;

    public PriceCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Sets the current price string and previous price string when a price drop is detected */
    public void setPriceStrings(String priceString, String previousPriceString) {
        assert !TextUtils.isEmpty(priceString) && !TextUtils.isEmpty(previousPriceString);
        mPriceInfoBox.setText(priceString);
        mPreviousPriceInfoBox.setText(previousPriceString);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        LayoutInflater.from(getContext()).inflate(R.layout.price_card, this);
        mPriceInfoBox = findViewById(R.id.current_price);
        mPreviousPriceInfoBox = findViewById(R.id.previous_price);
        mPreviousPriceInfoBox.setPaintFlags(
                mPreviousPriceInfoBox.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
        mPriceInfoBox.setTextColor(getContext().getColor(R.color.price_drop_annotation_text_green));
        mPreviousPriceInfoBox.setTextColor(
                getContext().getColor(R.color.chip_text_color_secondary_list));
    }
}
