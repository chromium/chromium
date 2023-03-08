// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.Paint;
import android.graphics.drawable.BitmapDrawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkMetrics.PriceTrackingState;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;

import java.util.Locale;

/** A row view that shows shopping info in the bookmarks UI. */
public class PowerBookmarkShoppingItemRow extends BookmarkItemRow {
    private static final long MICRO_CURRENCY_QUOTIENT = 1000000;

    private ImageFetcher mImageFetcher;
    private BookmarkModel mBookmarkModel;

    private boolean mIsPriceTrackingEnabled;
    private CurrencyFormatter mCurrencyFormatter;
    private CommerceSubscription mSubscription;
    private boolean mSubscriptionChangeInProgress;
    private SnackbarManager mSnackbarManager;
    private Profile mProfile;

    /**
     * Constructor for inflating from XML.
     */
    public PowerBookmarkShoppingItemRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initialize properties for the item row.
     * @param imageFetcher {@link ImageFetcher} used to fetch shopping images.
     * @param bookmarkModel The {@link BookmarkModel} used to query power bookmark metadata.
     */
    void init(ImageFetcher imageFetcher, BookmarkModel bookmarkModel,
            SnackbarManager snackbarManager, Profile profile) {
        mImageFetcher = imageFetcher;
        mBookmarkModel = bookmarkModel;
        mSnackbarManager = snackbarManager;
        mProfile = profile;
    }

    // BookmarkItemRow overrides:
    @Override
    BookmarkItem setBookmarkId(
            BookmarkId bookmarkId, @Location int location, boolean fromFilterView) {
        BookmarkItem bookmarkItem = super.setBookmarkId(bookmarkId, location, fromFilterView);
        PowerBookmarkMeta meta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
        assert meta != null;

        ShoppingSpecifics specifics = meta.getShoppingSpecifics();
        ProductPrice currentPrice = specifics.getCurrentPrice();
        ProductPrice previousPrice = specifics.getPreviousPrice();
        mSubscription = PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);
        mCurrencyFormatter =
                new CurrencyFormatter(currentPrice.getCurrencyCode(), Locale.getDefault());

        mIsPriceTrackingEnabled = false;
        initPriceTrackingUI(meta.getLeadImage().getUrl(), mIsPriceTrackingEnabled,
                previousPrice.getAmountMicros(), currentPrice.getAmountMicros());

        PriceTrackingUtils.isBookmarkPriceTracked(mProfile, bookmarkId.getId(), (isTracked) -> {
            mIsPriceTrackingEnabled = isTracked;
            updatePriceTrackingImageForCurrentState();
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
                        mStartIconViewSize, mStartIconViewSize),
                (image) -> {
                    if (image == null) return;
                    // We've successfully fetched an image. Cancel any pending requests for the
                    // favicon.
                    cancelFavicon();
                    setStartIconDrawable(new BitmapDrawable(getResources(), image));
                });

        setPriceInfoChip(originalPrice, currentPrice);
        setPriceTrackingButton(priceTrackingEnabled);
        mTitleView.setLabelFor(mEndStartButtonView.getId());
        PowerBookmarkMetrics.reportBookmarkShoppingItemRowPriceTrackingState(
                PriceTrackingState.PRICE_TRACKING_SHOWN);
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
            ViewGroup row = (ViewGroup) LayoutInflater.from(getContext())
                                    .inflate(R.layout.compact_price_drop_view, null, false);
            TextView primaryText = row.findViewById(R.id.primary_text);
            TextView secondaryText = row.findViewById(R.id.secondary_text);
            setCustomContent(row);

            assignPriceDropProperties(primaryText, secondaryText,
                    getFormattedCurrencyStringForPrice(originalPrice), formattedCurrentPrice);
        }
    }

    private void assignPriceDropProperties(TextView primaryText, TextView secondaryText,
            String formattedOriginalPrice, String formattedCurrentPrice) {
        // Primary text displays the current price.
        primaryText.setText(formattedCurrentPrice);
        primaryText.setTextColor(getContext().getColor(R.color.price_drop_annotation_text_green));

        // Secondary text displays the original price with a strikethrough.
        secondaryText.setText(formattedOriginalPrice);
        secondaryText.setPaintFlags(secondaryText.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
    }

    /** Sets up the button that allows you to un/subscribe to price-tracking updates. */
    private void setPriceTrackingButton(boolean priceTrackingEnabled) {
        mIsPriceTrackingEnabled = priceTrackingEnabled;
        mEndStartButtonView.setContentDescription(getContext().getResources().getString(
                priceTrackingEnabled ? R.string.disable_price_tracking_menu_item
                                     : R.string.enable_price_tracking_menu_item));
        mEndStartButtonView.setVisibility(View.VISIBLE);
        updatePriceTrackingImageForCurrentState();
        Callback<Boolean> subscriptionCallback = (success) -> {
            mSubscriptionChangeInProgress = false;
            // TODO(crbug.com/1243383): Handle the failure edge case.
            if (!success) return;
            mIsPriceTrackingEnabled = !mIsPriceTrackingEnabled;
            updatePriceTrackingImageForCurrentState();
        };
        mEndStartButtonView.setOnClickListener((v) -> {
            if (mSubscriptionChangeInProgress) return;
            mSubscriptionChangeInProgress = true;

            PowerBookmarkMetrics.reportBookmarkShoppingItemRowPriceTrackingState(
                    !mIsPriceTrackingEnabled ? PriceTrackingState.PRICE_TRACKING_ENABLED
                                             : PriceTrackingState.PRICE_TRACKING_DISABLED);
            PowerBookmarkUtils.setPriceTrackingEnabledWithSnackbars(mBookmarkModel, mBookmarkId,
                    !mIsPriceTrackingEnabled, mSnackbarManager, getContext().getResources(),
                    subscriptionCallback);
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
