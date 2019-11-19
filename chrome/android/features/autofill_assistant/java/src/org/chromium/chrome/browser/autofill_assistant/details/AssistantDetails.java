// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Java side equivalent of autofill_assistant::DetailsProto.
 */
@JNINamespace("autofill_assistant")
public class AssistantDetails {
    private final String mTitle;
    private final int mTitleMaxLines;
    private final String mImageUrl;
    private final ImageClickthroughData mImageClickthroughData;
    private final boolean mShowImagePlaceholder;
    private final String mDescriptionLine1;
    private final String mDescriptionLine2;
    private final String mDescriptionLine3;
    private final String mPriceAttribution;
    /** Whether user approval is required (i.e., due to changes). */
    private boolean mUserApprovalRequired;
    /** Whether the title should be highlighted. */
    private boolean mHighlightTitle;
    /** Whether the first description line should be highlighted. */
    private boolean mHighlightLine1;
    /** Whether the second description line should be highlighted. */
    private boolean mHighlightLine2;
    /** Whether the third description line should be highlighted. */
    private boolean mHighlightLine3;
    /** Whether empty fields should have the animated placeholder background. */
    private final boolean mAnimatePlaceholders;
    /**
     * The correctly formatted price for the client locale, including the currency.
     * Example: '$20.50' or '20.50 €'.
     */
    private final String mTotalPrice;
    /** An optional price label, such as 'Estimated Total incl. VAT'. */
    private final String mTotalPriceLabel;

    public AssistantDetails(String title, int titleMaxLines, String imageUrl,
            ImageClickthroughData imageClickthroughData, boolean showImagePlaceholder,
            String totalPriceLabel, String totalPrice, String descriptionLine1,
            String descriptionLine2, String descriptionLine3, String priceAttribution,
            boolean userApprovalRequired, boolean highlightTitle, boolean highlightLine1,
            boolean highlightLine2, boolean highlightLine3, boolean animatePlaceholders) {
        this.mTotalPriceLabel = totalPriceLabel;
        this.mTitle = title;
        this.mTitleMaxLines = titleMaxLines;
        this.mImageUrl = imageUrl;
        this.mImageClickthroughData = imageClickthroughData;
        this.mShowImagePlaceholder = showImagePlaceholder;
        this.mTotalPrice = totalPrice;
        this.mDescriptionLine1 = descriptionLine1;
        this.mDescriptionLine2 = descriptionLine2;
        this.mDescriptionLine3 = descriptionLine3;
        this.mPriceAttribution = priceAttribution;

        this.mUserApprovalRequired = userApprovalRequired;
        this.mHighlightTitle = highlightTitle;
        this.mHighlightLine1 = highlightLine1;
        this.mHighlightLine2 = highlightLine2;
        this.mHighlightLine3 = highlightLine3;
        this.mAnimatePlaceholders = animatePlaceholders;
    }

    String getTitle() {
        return mTitle;
    }

    int getTitleMaxLines() {
        return mTitleMaxLines;
    }

    String getImageUrl() {
        return mImageUrl;
    }

    boolean hasImageClickthroughData() {
        return mImageClickthroughData != null;
    }

    ImageClickthroughData getImageClickthroughData() {
        return mImageClickthroughData;
    }

    boolean getShowImagePlaceholder() {
        return mShowImagePlaceholder;
    }

    String getDescriptionLine1() {
        return mDescriptionLine1;
    }

    String getDescriptionLine2() {
        return mDescriptionLine2;
    }

    String getDescriptionLine3() {
        return mDescriptionLine3;
    }

    String getPriceAttribution() {
        return mPriceAttribution;
    }

    String getTotalPrice() {
        return mTotalPrice;
    }

    String getTotalPriceLabel() {
        return mTotalPriceLabel;
    }

    boolean getUserApprovalRequired() {
        return mUserApprovalRequired;
    }

    boolean getHighlightTitle() {
        return mHighlightTitle;
    }

    boolean getHighlightLine1() {
        return mHighlightLine1;
    }

    boolean getHighlightLine2() {
        return mHighlightLine2;
    }

    boolean getHighlightLine3() {
        return mHighlightLine3;
    }

    boolean getAnimatePlaceholders() {
        return mAnimatePlaceholders;
    }

    /**
     * Create details with the given values.
     */
    @CalledByNative
    private static AssistantDetails create(String title, int titleMaxLines, String imageUrl,
            boolean allowImageClickthrough, String imageClickthroughDesc,
            String imageClickthroughPostiveText, String imageClickthroughNegativeText,
            String imageClickthroughUrl, boolean showImagePlaceholder, String totalPriceLabel,
            String totalPrice, String descriptionLine1, String descriptionLine2,
            String descriptionLine3, String priceAttribution, boolean userApprovalRequired,
            boolean highlightTitle, boolean highlightLine1, boolean highlightLine2,
            boolean highlightLine3, boolean animatePlaceholders) {
        return new AssistantDetails(title, titleMaxLines, imageUrl,
                new ImageClickthroughData(allowImageClickthrough, imageClickthroughDesc,
                        imageClickthroughPostiveText, imageClickthroughNegativeText,
                        imageClickthroughUrl),
                showImagePlaceholder, totalPriceLabel, totalPrice, descriptionLine1,
                descriptionLine2, descriptionLine3, priceAttribution, userApprovalRequired,
                highlightTitle, highlightLine1, highlightLine2, highlightLine3,
                animatePlaceholders);
    }
}