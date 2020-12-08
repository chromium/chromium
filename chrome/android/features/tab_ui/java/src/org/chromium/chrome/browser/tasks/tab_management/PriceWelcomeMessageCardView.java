// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

import java.lang.ref.WeakReference;

class PriceWelcomeMessageCardView extends FrameLayout {
    private static WeakReference<Bitmap> sCloseButtonBitmapWeakRef;

    private PriceCardView mPriceInfoBox;
    private TextView mTitle;
    private TextView mContent;
    private ButtonCompat mActionButton;
    private ChromeImageView mCloseButton;

    public PriceWelcomeMessageCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPriceInfoBox = findViewById(R.id.price_info_box);
        mTitle = findViewById(R.id.title);
        mContent = findViewById(R.id.content);
        mActionButton = findViewById(R.id.action_button);
        mCloseButton = findViewById(R.id.close_button);

        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize =
                    (int) getResources().getDimension(R.dimen.tab_grid_close_button_size);
            Bitmap bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.btn_close);
            sCloseButtonBitmapWeakRef = new WeakReference<>(
                    Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true));
        }
        mCloseButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
    }

    /**
     * Set title text.
     * @param titleText Text to be displayed.
     */
    void setTitleText(String titleText) {
        mTitle.setText(titleText);
    }

    /**
     * Set content text.
     * @param contentText Text to be displayed.
     */
    void setContentText(String contentText) {
        mContent.setText(contentText);
    }

    /**
     * Set action text for the action button.
     * @param actionText Text to be displayed.
     */
    void setActionText(String actionText) {
        mActionButton.setText(actionText);
    }

    /**
     * Set click listener for the action button.
     * @param listener {@link android.view.View.OnClickListener} for the action button.
     */
    void setActionButtonOnClickListener(OnClickListener listener) {
        mActionButton.setOnClickListener(listener);
    }

    /**
     * Set content description for dismiss button.
     * @param description The content description.
     */
    void setDismissButtonContentDescription(String description) {
        mCloseButton.setContentDescription(description);
    }

    /**
     * Set {@link android.view.View.OnClickListener} for dismiss button.
     * @param listener {@link android.view.View.OnClickListener} to set.
     */
    void setDismissButtonOnClickListener(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    /**
     * Set price strings for the price info box.
     */
    void setPriceInfoBoxStrings(ShoppingPersistedTabData.PriceDrop priceDrop) {
        mPriceInfoBox.setPriceStrings(priceDrop.price, priceDrop.previousPrice);
    }
}
