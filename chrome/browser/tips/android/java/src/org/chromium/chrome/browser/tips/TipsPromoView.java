// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.ViewFlipper;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.widget.ButtonCompat;

/** The custom view for the tips promo bottom sheet. */
@NullMarked
public class TipsPromoView extends ViewFlipper {
    private ButtonCompat mMainPagePositiveButton;
    private ButtonCompat mDetailPagePositiveButton;
    private TextView mMainPageTitle;
    private TextView mMainPageDescription;
    private TextView mDetailPageTitle;
    private ButtonCompat mDetailsButton;
    private ImageButton mBackButton;

    /**
     * Constructor for inflating from XML.
     *
     * @param context The Context the view is running in.
     * @param attrs The attributes of the XML tag that is inflating the view.
     */
    public TipsPromoView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Initializer
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMainPagePositiveButton = findViewById(R.id.tips_promo_settings_button);
        mDetailPagePositiveButton = findViewById(R.id.tips_promo_details_settings_button);
        mMainPageTitle = findViewById(R.id.main_page_title_text);
        mMainPageDescription = findViewById(R.id.main_page_description_text);
        mDetailPageTitle = findViewById(R.id.details_page_title_text);
        mDetailsButton = findViewById(R.id.tips_promo_details_button);
        mBackButton = findViewById(R.id.details_page_back_button);
        if (LocalizationUtils.isLayoutRtl()) {
            mBackButton.setScaleX(-1);
        }
    }

    void setPromoData(FeatureTipPromoData promoData) {
        mMainPagePositiveButton.setText(promoData.positiveButtonText);
        mDetailPagePositiveButton.setText(promoData.positiveButtonText);
        mMainPageTitle.setText(promoData.mainPageTitle);
        mMainPageDescription.setText(promoData.mainPageDescription);
        mDetailPageTitle.setText(promoData.detailPageTitle);
    }

    void setDetailsButtonClickListener(@Nullable OnClickListener listener) {
        mDetailsButton.setOnClickListener(listener);
    }

    void setSettingsButtonClickListener(@Nullable OnClickListener listener) {
        mMainPagePositiveButton.setOnClickListener(listener);
        mDetailPagePositiveButton.setOnClickListener(listener);
    }

    void setBackButtonClickListener(@Nullable OnClickListener listener) {
        mBackButton.setOnClickListener(listener);
    }

    void setDetailsButtonVisibility(boolean visible) {
        mDetailsButton.setVisibility(visible ? VISIBLE : GONE);
    }

    void setDescriptionVisibility(boolean visible) {
        mMainPageDescription.setVisibility(visible ? VISIBLE : GONE);
    }

    ButtonCompat getMainPagePositiveButtonForTesting() {
        return mMainPagePositiveButton;
    }

    ButtonCompat getDetailPagePositiveButtonForTesting() {
        return mDetailPagePositiveButton;
    }

    TextView getMainPageTitleForTesting() {
        return mMainPageTitle;
    }

    TextView getMainPageDescriptionForTesting() {
        return mMainPageDescription;
    }

    TextView getDetailPageTitleForTesting() {
        return mDetailPageTitle;
    }

    ButtonCompat getDetailsButtonForTesting() {
        return mDetailsButton;
    }

    ImageButton getBackButtonForTesting() {
        return mBackButton;
    }
}
