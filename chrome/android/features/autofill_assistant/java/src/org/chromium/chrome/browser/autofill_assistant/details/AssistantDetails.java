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
    private final String mImageUrl;
    private final String mImageAccessibilityHint;
    private final ImageClickthroughData mImageClickthroughData;
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
    /**
     * The correctly formatted price for the client locale, including the currency.
     * Example: '$20.50' or '20.50 €'.
     */
    private final String mTotalPrice;
    /** An optional price label, such as 'Estimated Total incl. VAT'. */
    private final String mTotalPriceLabel;
    /** The configuration for the placeholders. */
    private final AssistantPlaceholdersConfiguration mPlaceholdersConfiguration;

    public AssistantDetails(String title, String imageUrl, String imageAccessibilityHint,
            ImageClickthroughData imageClickthroughData, String totalPriceLabel, String totalPrice,
            String descriptionLine1, String descriptionLine2, String descriptionLine3,
            String priceAttribution, boolean userApprovalRequired, boolean highlightTitle,
            boolean highlightLine1, boolean highlightLine2, boolean highlightLine3,
            AssistantPlaceholdersConfiguration placeholdersConfiguration) {
        this.mTotalPriceLabel = totalPriceLabel;
        this.mTitle = title;
        this.mImageUrl = imageUrl;
        this.mImageAccessibilityHint = imageAccessibilityHint;
        this.mImageClickthroughData = imageClickthroughData;
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
        this.mPlaceholdersConfiguration = placeholdersConfiguration;
    }

    String getTitle() {
        return mTitle;
    }

    String getImageUrl() {
        return mImageUrl;
    }

    String getImageAccessibilityHint() {
        return mImageAccessibilityHint;
    }

    boolean hasImageClickthroughData() {
        return mImageClickthroughData != null;
    }

    ImageClickthroughData getImageClickthroughData() {
        return mImageClickthroughData;
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

    public AssistantPlaceholdersConfiguration getPlaceholdersConfiguration() {
        return mPlaceholdersConfiguration;
    }

    /**
     * Create details with the given values.
     */
    @CalledByNative
    private static AssistantDetails create(String title, String imageUrl,
            String imageAccessibilityHint, boolean allowImageClickthrough,
            String imageClickthroughDesc, String imageClickthroughPostiveText,
            String imageClickthroughNegativeText, String imageClickthroughUrl,
            String totalPriceLabel, String totalPrice, String descriptionLine1,
            String descriptionLine2, String descriptionLine3, String priceAttribution,
            boolean userApprovalRequired, boolean highlightTitle, boolean highlightLine1,
            boolean highlightLine2, boolean highlightLine3,
            AssistantPlaceholdersConfiguration placeholdersConfiguration) {
        return new AssistantDetails(title, imageUrl, imageAccessibilityHint,
                new ImageClickthroughData(allowImageClickthrough, imageClickthroughDesc,
                        imageClickthroughPostiveText, imageClickthroughNegativeText,
                        imageClickthroughUrl),
                totalPriceLabel, totalPrice, descriptionLine1, descriptionLine2, descriptionLine3,
                priceAttribution, userApprovalRequired, highlightTitle, highlightLine1,
                highlightLine2, highlightLine3, placeholdersConfiguration);
    }
}
