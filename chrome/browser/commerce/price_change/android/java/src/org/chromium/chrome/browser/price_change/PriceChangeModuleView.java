// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

/** View for the price change module. */
public class PriceChangeModuleView extends FrameLayout {
    private TextView mModuleTitleView;
    private TextView mProductTitleView;
    private TextView mPriceChangeDomainView;
    private ImageView mProductImageView;
    private ImageView mFaviconImageView;
    private TextView mPreviousPriceView;
    private TextView mCurrentPriceView;

    public PriceChangeModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mModuleTitleView = findViewById(R.id.header_text);
        mProductTitleView = findViewById(R.id.product_title);
        mProductImageView = findViewById(R.id.product_image);
        mPriceChangeDomainView = findViewById(R.id.price_drop_domain);
        mFaviconImageView = findViewById(R.id.favicon_image);
        mPreviousPriceView = findViewById(R.id.previous_price);
        mCurrentPriceView = findViewById(R.id.current_price);
    }

    void setModuleTitle(String title) {
        mModuleTitleView.setText(title);
    }

    void setProductTitle(String title) {
        mProductTitleView.setText(title);
    }

    void setPriceChangeDomain(String domain) {
        mPriceChangeDomainView.setText(domain);
    }

    void setProductImage(Bitmap bitmap) {
        if (bitmap != null) {
            mProductImageView.setImageBitmap(bitmap);
            return;
        }
        initializeDefaultImage();
    }

    void setFaviconImage(Bitmap bitmap) {
        mFaviconImageView.setImageBitmap(bitmap);
    }

    void setCurrentPrice(String price) {
        mCurrentPriceView.setText(price);
    }

    void setPreviousPrice(String price) {
        mPreviousPriceView.setPaintFlags(
                mPreviousPriceView.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
        mPreviousPriceView.setText(price);
    }

    private void initializeDefaultImage() {
        Context context = getContext();
        // Initialize default background.
        GradientDrawable background = new GradientDrawable();
        background.setColor(PriceChangeModuleViewUtils.getBackgroundColor(context));
        background.setCornerRadius(
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.price_change_module_product_image_corner_radius));
        mProductImageView.setBackgroundDrawable(background);

        // Initialize default icon.
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_tab_placeholder);
        int padding =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.price_change_module_image_placeholder_padding);
        mProductImageView.setImageDrawable(drawable);
        mProductImageView.setPadding(padding, padding, padding, padding);
        mProductImageView.setColorFilter(
                PriceChangeModuleViewUtils.getIconColor(context), PorterDuff.Mode.SRC_IN);
    }
}
