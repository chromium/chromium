// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE;

import android.content.Context;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.text.style.StrikethroughSpan;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.base.LocalizationUtils;

/** A {@link TextResolver} for the {@link TabCardLabelView} when showing a price drop. */
@NullMarked
public class PriceDropTextResolver implements TextResolver {
    private final String mPrice;
    private final String mPreviousPrice;

    /**
     * @param price The current price.
     * @param previousPrice The previousPrice.
     */
    public PriceDropTextResolver(String price, String previousPrice) {
        mPrice = price;
        mPreviousPrice = previousPrice;
    }

    @Override
    public CharSequence resolve(Context context) {
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        String firstItem = isRtl ? mPreviousPrice : mPrice;
        String secondItem = isRtl ? mPrice : mPreviousPrice;
        SpannableString string = new SpannableString(String.format("%s %s", firstItem, secondItem));
        ForegroundColorSpan greyFcs =
                new ForegroundColorSpan(
                        context.getColor(R.color.default_text_color_secondary_list));
        ForegroundColorSpan greenFcs =
                new ForegroundColorSpan(context.getColor(R.color.price_drop_annotation_text_green));
        StrikethroughSpan strikeSpan = new StrikethroughSpan();
        int secondItemStart = firstItem.length() + 1;
        int secondItemEnd = secondItemStart + secondItem.length();
        if (isRtl) {
            string.setSpan(strikeSpan, 0, firstItem.length(), SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(greyFcs, 0, firstItem.length(), SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(greenFcs, secondItemStart, secondItemEnd, SPAN_EXCLUSIVE_EXCLUSIVE);
        } else {
            string.setSpan(greenFcs, 0, firstItem.length(), SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(strikeSpan, secondItemStart, secondItemEnd, SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(greyFcs, secondItemStart, secondItemEnd, SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        return string;
    }
}
