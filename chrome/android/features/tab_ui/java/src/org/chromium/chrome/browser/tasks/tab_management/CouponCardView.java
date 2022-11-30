// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.tab_ui.R;

/**
 * Contains coupon information related to a shopping website. Currently only
 * supports displaying the name of coupon when available coupon is detected.
 */
public class CouponCardView extends FrameLayout {
    private TextView mCouponInfoBox;

    public CouponCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the coupon annotation text when coupon is available.
     */
    public void setCouponString(String couponText) {
        mCouponInfoBox.setText(couponText);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        LayoutInflater.from(getContext()).inflate(R.layout.coupon_card, this);
        mCouponInfoBox = (TextView) findViewById(R.id.coupon_name);
        mCouponInfoBox.setTextColor(
                getContext().getColor(R.color.price_drop_annotation_text_green));
    }
}
