// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.merchant_viewer.RatingStarSpan.RatingStarType;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignalsV2;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is a util class for creating the property model of the MerchantTrustMessage.
 */
class MerchantTrustMessageViewModel {
    private static final int BASELINE_RATING = 5;

    @IntDef({MessageTitleUI.VIEW_STORE_INFO, MessageTitleUI.SEE_STORE_REVIEWS})
    @Retention(RetentionPolicy.SOURCE)
    @interface MessageTitleUI {
        int VIEW_STORE_INFO = 0;
        int SEE_STORE_REVIEWS = 1;
    }

    @IntDef({MessageDescriptionUI.NONE, MessageDescriptionUI.RATING_AND_REVIEWS,
            MessageDescriptionUI.REVIEWS_FROM_GOOGLE})
    @Retention(RetentionPolicy.SOURCE)
    @interface MessageDescriptionUI {
        int NONE = 0;
        int RATING_AND_REVIEWS = 1;
        int REVIEWS_FROM_GOOGLE = 2;
    }

    /** Handles message actions. */
    interface MessageActionsHandler {
        /**
         * Called when message is dismissed.
         * @param dismissReason The reason why the message is dismissed.
         * @param messageAssociatedUrl The url associated with this message context.
         */
        void onMessageDismissed(@DismissReason int dismissReason, String messageAssociatedUrl);

        /**
         * Called when message primary action is tapped.
         * @param trustSignals The signal associated with this message.
         * @param messageAssociatedUrl The url associated with this message context.
         */
        void onMessagePrimaryAction(
                MerchantTrustSignalsV2 trustSignals, String messageAssociatedUrl);
    }

    public static PropertyModel create(Context context, MerchantTrustSignalsV2 trustSignals,
            String messageAssociatedUrl, MessageActionsHandler actionsHandler) {
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .build();
    }

    @Nullable
    public static Spannable getMessageDescription(
            Context context, MerchantTrustSignalsV2 trustSignals, int descriptionUI) {
        return null;
    }

    private static SpannableStringBuilder getRatingBarSpan(Context context, double ratingValue) {
        assert ratingValue >= 0 && ratingValue <= BASELINE_RATING;
        SpannableStringBuilder ratingBarSpan = new SpannableStringBuilder();
        int floorRatingValue = (int) Math.floor(ratingValue);
        int ceilRatingValue = (int) Math.ceil(ratingValue);
        for (int i = 0; i < floorRatingValue; i++) {
            ratingBarSpan.append(" ");
            ratingBarSpan.setSpan(new RatingStarSpan(context, RatingStarType.FULL), i, i + 1,
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        if (ratingValue - floorRatingValue > 0) {
            ratingBarSpan.append(" ");
            ratingBarSpan.setSpan(new RatingStarSpan(context, RatingStarType.HALF),
                    floorRatingValue, floorRatingValue + 1, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        for (int i = ceilRatingValue; i < BASELINE_RATING; i++) {
            ratingBarSpan.append(" ");
            ratingBarSpan.setSpan(new RatingStarSpan(context, RatingStarType.OUTLINE), i, i + 1,
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        return ratingBarSpan;
    }

}
