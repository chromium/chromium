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

import org.chromium.base.Callback;
import org.chromium.chrome.browser.merchant_viewer.RatingStarSpan.RatingStarType;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.text.NumberFormat;

/**
 * This is a util class for creating the property model of the MerchantTrustMessage.
 */
class MerchantTrustMessageViewModel {
    private static final int BASELINE_RATING = 5;

    public static PropertyModel create(Context context, MerchantTrustSignals trustSignals,
            Callback<Integer> onDismissed, Callback<MerchantTrustSignals> onPrimaryAction) {
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(MessageBannerProperties.ICON,
                        ResourcesCompat.getDrawable(context.getResources(),
                                R.drawable.ic_logo_googleg_24dp, context.getTheme()))
                .with(MessageBannerProperties.ICON_TINT_COLOR, MessageBannerProperties.TINT_NONE)
                .with(MessageBannerProperties.TITLE,
                        context.getResources().getString(R.string.merchant_viewer_message_title))
                .with(MessageBannerProperties.DESCRIPTION,
                        getMessageDescription(context, trustSignals))
                .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                        context.getResources().getString(R.string.merchant_viewer_message_action))
                .with(MessageBannerProperties.ON_DISMISSED, onDismissed)
                .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                        () -> { onPrimaryAction.onResult(trustSignals); })
                .build();
    }

    private static Spannable getMessageDescription(
            Context context, MerchantTrustSignals trustSignals) {
        SpannableStringBuilder builder = new SpannableStringBuilder();
        NumberFormat numberFormatter = NumberFormat.getIntegerInstance();
        numberFormatter.setMaximumFractionDigits(1);
        if (MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_RATING_BAR.getValue()) {
            builder.append(numberFormatter.format(trustSignals.getMerchantStarRating()));
            builder.append(" ");
            builder.append(getRatingBarSpan(context, trustSignals.getMerchantStarRating()));
        } else {
            builder.append(context.getResources().getString(
                    R.string.merchant_viewer_message_description_rating,
                    numberFormatter.format(trustSignals.getMerchantStarRating()),
                    numberFormatter.format(BASELINE_RATING)));
            builder.setSpan(new StyleSpan(Typeface.BOLD), 0, builder.length(),
                    Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        builder.append(" ");
        builder.append(context.getResources().getQuantityString(
                R.plurals.merchant_viewer_message_description_reviews,
                trustSignals.getMerchantCountRating(), trustSignals.getMerchantCountRating()));
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