// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.R;
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

    /**
     * Factory constructor for building the view programmatically.
     * @param context The calling context, usually the parent view.
     * @param visual Whether the visual row should be used.
     */
    protected static ShoppingAccessoryView buildView(Context context) {
        ShoppingAccessoryView view = new ShoppingAccessoryView(context, null);
        view.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        LayoutInflater.from(context).inflate(
                org.chromium.chrome.R.layout.shopping_accessory_view_layout, view);
        view.onFinishInflate();
        return view;
    }

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

    void setPriceTracked(boolean tracked, boolean showIcon) {
        mPriceTrackedIcon.setVisibility(showIcon ? View.VISIBLE : View.GONE);
        mPriceTrackedIcon.setImageResource(tracked ? R.drawable.price_tracking_enabled_outline
                                                   : R.drawable.price_tracking_disabled);
    }

    void setPriceInformation(long originalPrice, String originalPriceText, long currentPrice,
            String currentPriceText) {
        boolean priceDrop = originalPrice > currentPrice;
        mNormalPriceText.setVisibility(priceDrop ? View.GONE : View.VISIBLE);
        mPriceDropText.setVisibility(priceDrop ? View.VISIBLE : View.GONE);
        mOriginalPriceText.setVisibility(priceDrop ? View.VISIBLE : View.GONE);

        if (priceDrop) {
            mContainer.getBackground().setColorFilter(
                    getContext().getResources().getColor(R.color.price_drop_annotation_bg_color),
                    PorterDuff.Mode.SRC_ATOP);
            ImageViewCompat.setImageTintList(mPriceTrackedIcon,
                    ColorStateList.valueOf(getContext().getResources().getColor(
                            R.color.price_drop_annotation_text_green)));

            // Primary text displays the current price.
            mPriceDropText.setText(currentPriceText);

            // Secondary text displays the original price with a strikethrough.
            mOriginalPriceText.setText(originalPriceText);
            mOriginalPriceText.setPaintFlags(
                    mOriginalPriceText.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
        } else {
            mContainer.getBackground().setColorFilter(
                    ChromeColors.getSurfaceColor(getContext(), R.dimen.default_elevation_2),
                    PorterDuff.Mode.SRC_ATOP);

            ImageViewCompat.setImageTintList(mPriceTrackedIcon,
                    ColorStateList.valueOf(
                            SemanticColorUtils.getDefaultIconColorSecondary(getContext())));

            mNormalPriceText.setText(currentPriceText);
        }
    }
}
