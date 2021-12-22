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
import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.chrome.browser.power_bookmarks.ProductPrice;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.ui.widget.ChipView;

import java.util.Arrays;
import java.util.Locale;

/** A row view that shows shopping info in the bookmarks UI. */
public class PowerBookmarkShoppingItemRow extends BookmarkItemRow {
    private static final long MICRO_CURRENCY_QUOTIENT = 1000000;

    private ImageFetcher mImageFetcher;
    private BookmarkModel mBookmarkModel;
    private SubscriptionsManager mSubscriptionsManager;

    private final int mDesiredImageSize;
    private boolean mIsPriceTrackingEnabled;
    private CurrencyFormatter mCurrencyFormatter;
    private CommerceSubscription mSubscription;
    private boolean mSubscriptionChangeInProgress;
    private SnackbarManager mSnackbarManager;

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
     * @param bookmarkModel The {@link BookmarkModel} used to query power bookmark metadata.
     * @param subscriptionsManager Used to manage the price-tracking subscriptions.
     */
    void init(ImageFetcher imageFetcher, BookmarkModel bookmarkModel,
            SubscriptionsManager subscriptionsManager, SnackbarManager snackbarManager) {
        mImageFetcher = imageFetcher;
        mBookmarkModel = bookmarkModel;
        mSubscriptionsManager = subscriptionsManager;
        mSnackbarManager = snackbarManager;
    }

    // BookmarkItemRow overrides:
    @Override
    BookmarkItem setBookmarkId(BookmarkId bookmarkId, @Location int location) {
        BookmarkItem bookmarkItem = super.setBookmarkId(bookmarkId, location);
        PowerBookmarkMeta meta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
        assert meta != null;

        // TODO(crbug.com/1243383): Pull price updates once they're available.
        ProductPrice originalPrice = meta.getShoppingSpecifics().getCurrentPrice();
        mSubscription = PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);
        mCurrencyFormatter =
                new CurrencyFormatter(originalPrice.getCurrencyCode(), Locale.getDefault());

        boolean mIsPriceTrackingEnabled =
                meta != null && meta.getShoppingSpecifics().getIsPriceTracked();
        initPriceTrackingUI(meta.getLeadImage().getUrl(), mIsPriceTrackingEnabled,
                originalPrice.getAmountMicros(), originalPrice.getAmountMicros());
        // Request an updated price then push updates to the UI.
        mBookmarkModel.getUpdatedProductPrices(
                Arrays.asList(bookmarkId), (id, url, updatedPrice) -> {
                    if (!mBookmarkId.equals(id)
                            || !originalPrice.getCurrencyCode().equals(
                                    updatedPrice.getCurrencyCode())) {
                        return;
                    }

                    if (updatedPrice.getAmountMicros() > originalPrice.getAmountMicros()) {
                        PowerBookmarkUtils.updatePriceForBookmarkId(
                                mBookmarkModel, bookmarkId, updatedPrice);
                    }

                    setPriceInfoChip(
                            originalPrice.getAmountMicros(), updatedPrice.getAmountMicros());
                });
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

        mStartIconView.setOutlineProvider(
                new RoundedCornerOutlineProvider(getResources().getDimensionPixelSize(
                        R.dimen.list_item_v2_start_icon_corner_radius)));
        mStartIconView.setClipToOutline(true);
        mImageFetcher.fetchImage(
                ImageFetcher.Params.create(leadImageUrl, ImageFetcher.POWER_BOOKMARKS_CLIENT_NAME,
                        mDesiredImageSize, mDesiredImageSize),
                (image) -> {
                    // We've successfully fetched an image. Cancel any pending requests for the
                    // favicon.
                    cancelFavicon();
                    setStartIconDrawable(new BitmapDrawable(getResources(), image));
                });

        setPriceInfoChip(originalPrice, currentPrice);
        setPriceTrackingButton(priceTrackingEnabled);
    }

    /** Sets up the chip that displays product price information. */
    private void setPriceInfoChip(long originalPrice, long currentPrice) {
        String formattedCurrentPrice = getFormattedCurrencyStringForPrice(currentPrice);
        // Note: chips should only be shown for price drops
        if (originalPrice <= currentPrice) {
            TextView textView = new TextView(getContext(), null);
            ApiCompatibilityUtils.setTextAppearance(
                    textView, R.styleable.ChipView_primaryTextAppearance);
            textView.setText(formattedCurrentPrice);
            setCustomContent(textView);
        } else {
            ChipView cv = new ChipView(getContext(), null);
            cv.setBorder(0, Color.TRANSPARENT);
            cv.setBackgroundColor(ApiCompatibilityUtils.getColor(
                    getResources(), R.color.price_drop_annotation_bg_color));

            // Primary text displays the current price.
            TextView primaryText = cv.getPrimaryTextView();
            ApiCompatibilityUtils.setTextAppearance(
                    primaryText, R.styleable.ChipView_primaryTextAppearance);
            primaryText.setText(formattedCurrentPrice);
            primaryText.setTextColor(ApiCompatibilityUtils.getColor(
                    getResources(), R.color.price_drop_annotation_text_green));

            // Secondary text displays the original price with a strikethrough.
            TextView secondaryText = cv.getSecondaryTextView();
            secondaryText.setText(getFormattedCurrencyStringForPrice(originalPrice));
            secondaryText.setPaintFlags(
                    secondaryText.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
            ApiCompatibilityUtils.setTextAppearance(
                    secondaryText, R.styleable.ChipView_primaryTextAppearance);
            setCustomContent(cv);
        }
    }

    /** Sets up the button that allows you to un/subscribe to price-tracking updates. */
    private void setPriceTrackingButton(boolean priceTrackingEnabled) {
        mIsPriceTrackingEnabled = priceTrackingEnabled;
        mEndStartButtonView.setVisibility(View.VISIBLE);
        updatePriceTrackingImageForCurrentState();
        Callback<Integer> subscriptionCallback = (status) -> {
            mSubscriptionChangeInProgress = false;
            // TODO(crbug.com/1243383): Handle the failure edge case.
            if (status != SubscriptionsManager.StatusCode.OK) return;
            mIsPriceTrackingEnabled = !mIsPriceTrackingEnabled;
            updatePriceTrackingImageForCurrentState();
        };
        mEndStartButtonView.setOnClickListener((v) -> {
            if (mSubscriptionChangeInProgress) return;
            mSubscriptionChangeInProgress = true;

            PowerBookmarkUtils.setPriceTrackingEnabledWithSnackbars(mSubscriptionsManager,
                    mBookmarkModel, mBookmarkId, !mIsPriceTrackingEnabled, mSnackbarManager,
                    getContext().getResources(), subscriptionCallback);
        });
    }

    private void updatePriceTrackingImageForCurrentState() {
        mEndStartButtonView.setImageResource(mIsPriceTrackingEnabled
                        ? R.drawable.price_tracking_enabled_filled
                        : R.drawable.price_tracking_disabled);
    }

    private String getFormattedCurrencyStringForPrice(long price) {
        // Note: We'll lose some precision here, but it's fine.
        return mCurrencyFormatter.format("" + (price / MICRO_CURRENCY_QUOTIENT));
    }

    void setCurrencyFormatterForTesting(CurrencyFormatter currencyFormatter) {
        mCurrencyFormatter = currencyFormatter;
    }

    View getPriceTrackingButtonForTesting() {
        return mEndStartButtonView;
    }
}
