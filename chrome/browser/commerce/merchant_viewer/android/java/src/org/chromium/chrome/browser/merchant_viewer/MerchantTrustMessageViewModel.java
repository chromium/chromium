// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.StyleSpan;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.chrome.browser.merchant_viewer.RatingStarSpan.RatingStarType;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignalsV2;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.ui.modelutil.PropertyModel;

import java.text.NumberFormat;

/**
 * This is a util class for creating the property model of the MerchantTrustMessage.
 */
class MerchantTrustMessageViewModel {
    private static final int BASELINE_RATING = 5;

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
                .with(MessageBannerProperties.MESSAGE_IDENTIFIER, MessageIdentifier.MERCHANT_TRUST)
                .with(MessageBannerProperties.ICON,
                        ResourcesCompat.getDrawable(context.getResources(),
                                R.drawable.ic_storefront_blue, context.getTheme()))
                .with(MessageBannerProperties.ICON_TINT_COLOR, MessageBannerProperties.TINT_NONE)
                .with(MessageBannerProperties.TITLE,
                        context.getResources().getString(R.string.merchant_viewer_message_title))
                .with(MessageBannerProperties.DESCRIPTION,
                        getMessageDescription(context, trustSignals))
                .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                        context.getResources().getString(R.string.merchant_viewer_message_action))
                .with(MessageBannerProperties.ON_DISMISSED,
                        (reason) -> actionsHandler.onMessageDismissed(reason, messageAssociatedUrl))
                .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                        ()
                                -> actionsHandler.onMessagePrimaryAction(
                                        trustSignals, messageAssociatedUrl))
                .build();
    }

    public static Spannable getMessageDescription(
            Context context, MerchantTrustSignalsV2 trustSignals) {
        // The zero rating value means we have no rating data for the merchant, under which
        // condition we shouldn't call this method to generate the description.
        assert trustSignals.getMerchantStarRating() > 0;
        // Only keep one decimal to avoid inaccurate double value.
        double ratingValue = Math.round(trustSignals.getMerchantStarRating() * 10) / 10.0;
        SpannableStringBuilder builder = new SpannableStringBuilder();
        NumberFormat numberFormatter = NumberFormat.getIntegerInstance();
        numberFormatter.setMaximumFractionDigits(1);
        NumberFormat ratingValueFormatter = NumberFormat.getIntegerInstance();
        ratingValueFormatter.setMaximumFractionDigits(1);
        ratingValueFormatter.setMinimumFractionDigits(1);
        if (MerchantViewerConfig.doesTrustSignalsMessageUseRatingBar()) {
            builder.append(ratingValueFormatter.format(ratingValue));
            builder.append(" ");
            builder.append(getRatingBarSpan(context, ratingValue));
        } else {
            builder.append(context.getResources().getString(
                    R.string.merchant_viewer_message_description_rating,
                    numberFormatter.format(ratingValue), numberFormatter.format(BASELINE_RATING)));
            builder.setSpan(new StyleSpan(Typeface.BOLD), 0, builder.length(),
                    Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        builder.append(" ");
        if (trustSignals.getMerchantCountRating() > 0) {
            builder.append(context.getResources().getQuantityString(
                    R.plurals.merchant_viewer_message_description_reviews,
                    trustSignals.getMerchantCountRating(),
                    numberFormatter.format(trustSignals.getMerchantCountRating())));
        } else {
            builder.append(context.getResources().getString(
                    R.string.page_info_store_info_description_with_no_review));
        }
        return builder;
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