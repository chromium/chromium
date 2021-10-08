// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.BitmapDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.ui.widget.ChipView;

/** A row view that shows shopping info in the bookmarks UI. */
public class PowerBookmarkShoppingItemRow extends BookmarkItemRow {
    private static final long MICRO_CURRENCY_QUOTIENT = 1000000;

    private ImageFetcher mImageFetcher;

    private final int mDesiredImageSize;
    private boolean mIsPriceTrackingEnabled;
    private CurrencyFormatter mCurrencyFormatter;

    /**
     * Constructor for inflating from XML.
     */
    public PowerBookmarkShoppingItemRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDesiredImageSize =
                getResources().getDimensionPixelSize(R.dimen.list_item_v2_start_icon_width);
    }

    /**
     * Initialize properties for the item row.
     * @param imageFetcher {@link ImageFetcher} used to fetch shopping images.
     */
    void init(ImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
    }

    // BookmarkItemRow overrides:
    @Override
    BookmarkItem setBookmarkId(BookmarkId bookmarkId, @Location int location) {
        BookmarkItem bookmarkItem = super.setBookmarkId(bookmarkId, location);
        // TODO(crbug.com/1243383): Retrieve power bookmark data and use it to populate fields.
        // TODO(crbug.com/1243383): Create the currency formatter according to the currency code.
        return bookmarkItem;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mCurrencyFormatter != null) mCurrencyFormatter.destroy();
    }

    @VisibleForTesting
    void initPriceTrackingUI(String leadImageUrl, boolean priceTrackingEnabled, long originalPrice,
            long currentPrice) {
        assert mCurrencyFormatter != null;

        mImageFetcher.fetchImage(
                ImageFetcher.Params.create(leadImageUrl, ImageFetcher.POWER_BOOKMARKS_CLIENT_NAME,
                        mDesiredImageSize, mDesiredImageSize),
                (image) -> { setStartIconDrawable(new BitmapDrawable(getResources(), image)); });

        setPriceInfoChip(originalPrice, currentPrice);
        setPriceTrackingButton(priceTrackingEnabled);
    }

    /** Sets up the chip that displays product price information. */
    private void setPriceInfoChip(long originalPrice, long currentPrice) {
        String formattedCurrentPrice = getFormattedCurrencyStringForPrice(currentPrice);
        if (originalPrice == currentPrice) {
            TextView textView = new TextView(getContext(), null);
            ApiCompatibilityUtils.setTextAppearance(
                    textView, R.styleable.ChipView_primaryTextAppearance);
            textView.setText(formattedCurrentPrice);
            setCustomContent(textView);
        } else {
            ChipView cv = new ChipView(getContext(), null);
            cv.setBorder(0, Color.TRANSPARENT);
            cv.setBackgroundColor(ApiCompatibilityUtils.getColor(getResources(),
                    originalPrice > currentPrice ? R.color.google_green_300
                                                 : R.color.google_red_300));

            // Primary text displays the current price.
            TextView primaryText = cv.getPrimaryTextView();
            primaryText.setText(formattedCurrentPrice);
            primaryText.setTextColor(ApiCompatibilityUtils.getColor(getResources(),
                    originalPrice > currentPrice ? R.color.google_green_600
                                                 : R.color.google_red_600));

            // Secondary text displays the original price with a strikethrough.
            TextView secondaryText = cv.getSecondaryTextView();
            secondaryText.setText(getFormattedCurrencyStringForPrice(originalPrice));
            secondaryText.setPaintFlags(
                    secondaryText.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
            ApiCompatibilityUtils.setTextAppearance(
                    secondaryText, R.style.TextAppearance_TextSmall_Secondary);
            setCustomContent(cv);
        }
    }

    /** Sets up the button that allows you to un/subscribe to price-tracking updates. */
    private void setPriceTrackingButton(boolean priceTrackingEnabled) {
        mIsPriceTrackingEnabled = priceTrackingEnabled;
        mEndStartButtonView.setVisibility(View.VISIBLE);
        mEndStartButtonView.setImageResource(mIsPriceTrackingEnabled
                        ? R.drawable.price_tracking_enabled
                        : R.drawable.price_tracking_disabled);
        mEndStartButtonView.setOnClickListener((v) -> {
            mIsPriceTrackingEnabled = !mIsPriceTrackingEnabled;
            mEndStartButtonView.setImageResource(mIsPriceTrackingEnabled
                            ? R.drawable.price_tracking_enabled
                            : R.drawable.price_tracking_disabled);
            // TODO(crbug.com/1243383): Flip the price-tracking bit once available.
        });
    }

    private String getFormattedCurrencyStringForPrice(long price) {
        // Note: We'll lose some precision here, but it's fine.
        return mCurrencyFormatter.format("" + (price / MICRO_CURRENCY_QUOTIENT));
    }

    void setCurrencyFormatterForTesting(CurrencyFormatter currencyFormatter) {
        mCurrencyFormatter = currencyFormatter;
    }
}
