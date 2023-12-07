// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ShoppingAccessoryViewProperties.PriceInfo;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** View for the shopping price chip. */
public class ShoppingAccessoryView extends FrameLayout {
    private ViewGroup mContainer;
    private ImageView mPriceTrackedIcon;
    // Used for when there's no price drop.
    private TextView mNormalPriceText;
    // Used for the new price when there's a price drop.
    private TextView mPriceDropText;
    // Used for the original price when there's a price drop.
    private TextView mOriginalPriceText;

    private PriceInfo mInfo;

    /** Constructor for inflating from XML. */
    public ShoppingAccessoryView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mContainer = findViewById(R.id.container);

        mPriceTrackedIcon = findViewById(R.id.price_tracked_icon);
        mNormalPriceText = findViewById(R.id.normal_price_text);
        mPriceDropText = findViewById(R.id.price_drop_text);
        mOriginalPriceText = findViewById(R.id.original_price_text);
    }

    void setPriceTracked(boolean tracked) {
        mPriceTrackedIcon.setVisibility(tracked ? View.VISIBLE : View.GONE);
        mContainer.setBackgroundResource(
                tracked ? R.drawable.shopping_accessory_view_background : Resources.ID_NULL);
        if (tracked) {
            int padding =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.shopping_chip_padding);
            mContainer.setPadding(padding, 0, padding, 0);
        }
        // Changing the tracked status should update the price info, since the background might
        // not have been updated.
        if (mInfo != null) setPriceInfo(mInfo);
    }

    void setPriceInfo(PriceInfo info) {
        mInfo = info;

        boolean priceDrop = info.isPriceDrop();
        mNormalPriceText.setVisibility(priceDrop ? View.GONE : View.VISIBLE);
        mPriceDropText.setVisibility(priceDrop ? View.VISIBLE : View.GONE);
        mOriginalPriceText.setVisibility(priceDrop ? View.VISIBLE : View.GONE);

        if (priceDrop) {
            // Our background will be null if the price isn't tracked.
            if (mContainer.getBackground() != null) {
                mContainer
                        .getBackground()
                        .setColorFilter(
                                getContext()
                                        .getColor(R.color.price_drop_annotation_bg_color),
                                PorterDuff.Mode.SRC_ATOP);
            }
            ImageViewCompat.setImageTintList(
                    mPriceTrackedIcon,
                    ColorStateList.valueOf(
                            getContext()
                                    .getColor(R.color.price_drop_annotation_text_green)));

            // Primary text displays the current price.
            mPriceDropText.setText(info.getCurrentPriceText());

            // Secondary text displays the original price with a strikethrough.
            mOriginalPriceText.setText(info.getOriginalPriceText());
            mOriginalPriceText.setPaintFlags(
                    mOriginalPriceText.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
        } else {
            // Our background will be null if the price isn't tracked.
            if (mContainer.getBackground() != null) {
                mContainer
                        .getBackground()
                        .setColorFilter(
                                ChromeColors.getSurfaceColor(
                                        getContext(), R.dimen.default_elevation_2),
                                PorterDuff.Mode.SRC_ATOP);
            }

            ImageViewCompat.setImageTintList(
                    mPriceTrackedIcon,
                    ColorStateList.valueOf(
                            SemanticColorUtils.getDefaultIconColorSecondary(getContext())));

            mNormalPriceText.setText(info.getCurrentPriceText());
        }
    }
}
