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
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkMetrics.PriceTrackingState;
import org.chromium.chrome.browser.bookmarks.ShoppingAccessoryViewProperties.PriceInfo;
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
    // UI elements.
    // Allows subclasses to add special views below the description (e.g. the price for shopping).
    protected FrameLayout mCustomTextContainer;
    private ImageView mPriceTrackingButton;
    // Used for when there's no price drop.
    private TextView mNormalPriceText;
    // Used for the new price when there's a price drop.
    private TextView mPriceDropText;
    // Used for the original price when there's a price drop.
    private TextView mOriginalPriceText;

    // Dependencies.
    private ImageFetcher mImageFetcher;
    private BookmarkModel mBookmarkModel;

    private boolean mIsPriceTrackingEnabled;
    private CurrencyFormatter mCurrencyFormatter;
    private CommerceSubscription mSubscription;
    private boolean mSubscriptionChangeInProgress;
    private SnackbarManager mSnackbarManager;
    private Profile mProfile;

    private final int mPreferredImageSize;

    /**
     * Factory constructor for building the view programmatically.
     * @param context The calling context, usually the parent view.
     * @param isVisualRefreshEnabled Whether to show the visual or compact bookmark row.
     */
    public static PowerBookmarkShoppingItemRow buildView(
            Context context, boolean isVisualRefreshEnabled) {
        PowerBookmarkShoppingItemRow row = new PowerBookmarkShoppingItemRow(context, null);
        BookmarkItemRow.buildView(row, context, isVisualRefreshEnabled);
        return row;
    }

    /** Constructor for inflating from XML. */
    public PowerBookmarkShoppingItemRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        mPreferredImageSize = getResources().getDimensionPixelSize(
                R.dimen.bookmark_refresh_preferred_start_icon_size);
    }

    // FrameLayout implementation.

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPriceTrackingButton = findViewById(R.id.end_icon);
        mPriceTrackingButton.setVisibility(View.GONE);

        mCustomTextContainer = findViewById(R.id.custom_content_container);

        LayoutInflater.from(getContext())
                .inflate(R.layout.compact_price_drop_view, mCustomTextContainer);
        mNormalPriceText = findViewById(R.id.normal_price_text);
        mPriceDropText = findViewById(R.id.price_drop_text);
        mOriginalPriceText = findViewById(R.id.original_price_text);
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

    // BookmarkItemRow implementation.

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

        getIconView().setOutlineProvider(
                new RoundedCornerOutlineProvider(getResources().getDimensionPixelSize(
                        R.dimen.list_item_v2_start_icon_corner_radius)));
        getIconView().setClipToOutline(true);
        mImageFetcher.fetchImage(
                ImageFetcher.Params.create(leadImageUrl, ImageFetcher.POWER_BOOKMARKS_CLIENT_NAME,
                        mPreferredImageSize, mPreferredImageSize),
                (image) -> {
                    if (image == null) return;
                    // We've successfully fetched an image. Cancel any pending requests for the
                    // favicon.
                    cancelFavicon();
                    setIconDrawable(new BitmapDrawable(getResources(), image));
                });

        setPriceInfoChip(new PriceInfo(originalPrice, currentPrice, mCurrencyFormatter));
        setPriceTrackingButton(priceTrackingEnabled);
        mTitleView.setLabelFor(mPriceTrackingButton.getId());
        PowerBookmarkMetrics.reportBookmarkShoppingItemRowPriceTrackingState(
                PriceTrackingState.PRICE_TRACKING_SHOWN);
    }

    /** Sets up the chip that displays product price information. */
    private void setPriceInfoChip(PriceInfo info) {
        // Note: chips should only be shown for price drops
        if (info.isPriceDrop()) {
            // Primary text displays the current price.
            mPriceDropText.setText(info.getCurrentPriceText());

            // Secondary text displays the original price with a strikethrough.
            mOriginalPriceText.setText(info.getOriginalPriceText());
            mOriginalPriceText.setPaintFlags(
                    mOriginalPriceText.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);

            mNormalPriceText.setVisibility(View.GONE);
            mPriceDropText.setVisibility(View.VISIBLE);
            mOriginalPriceText.setVisibility(View.VISIBLE);
        } else {
            mNormalPriceText.setText(info.getCurrentPriceText());
            mNormalPriceText.setVisibility(View.VISIBLE);
            mPriceDropText.setVisibility(View.GONE);
            mOriginalPriceText.setVisibility(View.GONE);
        }
    }

    /** Sets up the button that allows you to un/subscribe to price-tracking updates. */
    private void setPriceTrackingButton(boolean priceTrackingEnabled) {
        mIsPriceTrackingEnabled = priceTrackingEnabled;
        mPriceTrackingButton.setContentDescription(getContext().getResources().getString(
                priceTrackingEnabled ? R.string.disable_price_tracking_menu_item
                                     : R.string.enable_price_tracking_menu_item));
        mPriceTrackingButton.setVisibility(View.VISIBLE);
        updatePriceTrackingImageForCurrentState();
        Callback<Boolean> subscriptionCallback = (success) -> {
            mSubscriptionChangeInProgress = false;
            // TODO(crbug.com/1243383): Handle the failure edge case.
            if (!success) return;
            mIsPriceTrackingEnabled = !mIsPriceTrackingEnabled;
            updatePriceTrackingImageForCurrentState();
        };
        mPriceTrackingButton.setOnClickListener((v) -> {
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
        mPriceTrackingButton.setImageResource(mIsPriceTrackingEnabled
                        ? R.drawable.price_tracking_enabled_filled
                        : R.drawable.price_tracking_disabled);
    }

    void setCurrencyFormatterForTesting(CurrencyFormatter currencyFormatter) {
        mCurrencyFormatter = currencyFormatter;
    }

    View getPriceTrackingButtonForTesting() {
        return mPriceTrackingButton;
    }
}
