// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE;

import android.content.Context;
import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.text.style.StrikethroughSpan;
import android.text.style.StyleSpan;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.ui.base.LocalizationUtils;

/** A {@link TextResolver} for the {@link TabCardLabelView} when showing a price drop. */
public class PriceDropTextResolver implements TextResolver {
    private String mPrice;
    private String mPreviousPrice;

    /**
     * @param price The current price.
     * @param previousPrice The previousPrice.
     */
    public PriceDropTextResolver(String price, String previousPrice) {
        mPrice = price;
        mPreviousPrice = previousPrice;
    }

    @Override
    public @Nullable CharSequence resolve(Context context) {
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        String firstItem = isRtl ? mPreviousPrice : mPrice;
        String secondItem = isRtl ? mPrice : mPreviousPrice;
        SpannableString string = new SpannableString(String.format("%s %s", firstItem, secondItem));
        ForegroundColorSpan greyFcs =
                new ForegroundColorSpan(
                        context.getColor(R.color.default_text_color_secondary_dark));
        ForegroundColorSpan greenFcs =
                new ForegroundColorSpan(context.getColor(R.color.google_green_600));
        StyleSpan boldSpan = new StyleSpan(Typeface.BOLD);
        StrikethroughSpan strikeSpan = new StrikethroughSpan();
        int secondItemStart = firstItem.length() + 1;
        int secondItemEnd = secondItemStart + secondItem.length();
        if (isRtl) {
            string.setSpan(strikeSpan, 0, firstItem.length(), SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(greyFcs, 0, firstItem.length(), SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(boldSpan, secondItemStart, secondItemEnd, SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(greenFcs, secondItemStart, secondItemEnd, SPAN_EXCLUSIVE_EXCLUSIVE);
        } else {
            string.setSpan(boldSpan, 0, firstItem.length(), SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(greenFcs, 0, firstItem.length(), SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(strikeSpan, secondItemStart, secondItemEnd, SPAN_EXCLUSIVE_EXCLUSIVE);
            string.setSpan(greyFcs, secondItemStart, secondItemEnd, SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        return string;
    }
}
