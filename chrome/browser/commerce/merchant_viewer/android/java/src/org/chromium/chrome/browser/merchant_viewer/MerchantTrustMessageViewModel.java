// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.StyleSpan;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.chrome.browser.merchant_viewer.RatingStarSpan.RatingStarType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.NumberFormat;

/** This is a util class for creating the property model of the MerchantTrustMessage. */
class MerchantTrustMessageViewModel {
    private static final int BASELINE_RATING = 5;

    @IntDef({MessageTitleUI.VIEW_STORE_INFO, MessageTitleUI.SEE_STORE_REVIEWS})
    @Retention(RetentionPolicy.SOURCE)
    @interface MessageTitleUI {
        int VIEW_STORE_INFO = 0;
        int SEE_STORE_REVIEWS = 1;
    }

    @IntDef({
        MessageDescriptionUI.NONE,
        MessageDescriptionUI.RATING_AND_REVIEWS,
        MessageDescriptionUI.REVIEWS_FROM_GOOGLE
    })
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
        void onMessagePrimaryAction(MerchantInfo merchantInfo, String messageAssociatedUrl);
    }

    public static PropertyModel create(
            Context context,
            MerchantInfo merchantInfo,
            String messageAssociatedUrl,
            MessageActionsHandler actionsHandler) {
        var resources = context.getResources();
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(MessageBannerProperties.MESSAGE_IDENTIFIER, MessageIdentifier.MERCHANT_TRUST)
                .with(
                        MessageBannerProperties.ICON,
                        ResourcesCompat.getDrawable(resources, getIconRes(), context.getTheme()))
                .with(MessageBannerProperties.ICON_TINT_COLOR, MessageBannerProperties.TINT_NONE)
                .with(MessageBannerProperties.TITLE, resources.getString(getTitleStringRes()))
                .with(
                        MessageBannerProperties.DESCRIPTION,
                        getMessageDescription(
                                context,
                                merchantInfo,
                                MerchantViewerConfig.getTrustSignalsMessageDescriptionUI()))
                .with(
                        MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                        resources.getString(R.string.merchant_viewer_message_action))
                .with(
                        MessageBannerProperties.ON_DISMISSED,
                        (reason) -> actionsHandler.onMessageDismissed(reason, messageAssociatedUrl))
                .with(
                        MessageBannerProperties.ON_PRIMARY_ACTION,
                        () -> {
                            actionsHandler.onMessagePrimaryAction(
                                    merchantInfo, messageAssociatedUrl);
                            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                        })
                .build();
    }

    public static @Nullable Spannable getMessageDescription(
            Context context, MerchantInfo merchantInfo, int descriptionUI) {
        if (descriptionUI == MessageDescriptionUI.NONE) {
            return null;
        }
        var resources = context.getResources();

        SpannableStringBuilder builder = new SpannableStringBuilder();
        NumberFormat numberFormatter = NumberFormat.getIntegerInstance();
        numberFormatter.setMaximumFractionDigits(1);
        if (descriptionUI == MessageDescriptionUI.REVIEWS_FROM_GOOGLE
                && merchantInfo.countRating > 0) {
            String message =
                    resources.getQuantityString(
                            R.plurals.merchant_viewer_message_description_reviews_from_google,
                            merchantInfo.countRating,
                            numberFormatter.format(merchantInfo.countRating));
            builder.append(message);
            return builder;
        }

        // The zero rating value means we have no rating data for the merchant, under which
        // condition we shouldn't call this method to generate the description.
        assert merchantInfo.starRating > 0;
        // Only keep one decimal to avoid inaccurate double value.
        double ratingValue = Math.round(merchantInfo.starRating * 10) / 10.0;
        NumberFormat ratingValueFormatter = NumberFormat.getIntegerInstance();
        ratingValueFormatter.setMaximumFractionDigits(1);
        ratingValueFormatter.setMinimumFractionDigits(1);
        if (MerchantViewerConfig.doesTrustSignalsMessageUseRatingBar()) {
            builder.append(ratingValueFormatter.format(ratingValue));
            builder.append(" ");
            builder.append(getRatingBarSpan(context, ratingValue));
        } else {
            builder.append(
                    resources.getString(
                            R.string.merchant_viewer_message_description_rating,
                            numberFormatter.format(ratingValue),
                            numberFormatter.format(BASELINE_RATING)));
            builder.setSpan(
                    new StyleSpan(Typeface.BOLD),
                    0,
                    builder.length(),
                    Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        builder.append(" ");
        if (merchantInfo.countRating > 0) {
            builder.append(
                    resources.getQuantityString(
                            R.plurals.merchant_viewer_message_description_reviews,
                            merchantInfo.countRating,
                            numberFormatter.format(merchantInfo.countRating)));
        } else {
            builder.append(
                    resources.getString(R.string.page_info_store_info_description_with_no_review));
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
            ratingBarSpan.setSpan(
                    new RatingStarSpan(context, RatingStarType.FULL),
                    i,
                    i + 1,
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        if (ratingValue - floorRatingValue > 0) {
            ratingBarSpan.append(" ");
            ratingBarSpan.setSpan(
                    new RatingStarSpan(context, RatingStarType.HALF),
                    floorRatingValue,
                    floorRatingValue + 1,
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        for (int i = ceilRatingValue; i < BASELINE_RATING; i++) {
            ratingBarSpan.append(" ");
            ratingBarSpan.setSpan(
                    new RatingStarSpan(context, RatingStarType.OUTLINE),
                    i,
                    i + 1,
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        return ratingBarSpan;
    }

    private static @DrawableRes int getIconRes() {
        return MerchantViewerConfig.doesTrustSignalsMessageUseGoogleIcon()
                ? R.drawable.ic_logo_googleg_24dp
                : R.drawable.ic_storefront_blue;
    }

    private static @StringRes int getTitleStringRes() {
        int titleUI = MerchantViewerConfig.getTrustSignalsMessageTitleUI();
        if (titleUI == MessageTitleUI.SEE_STORE_REVIEWS) {
            return R.string.merchant_viewer_message_title_see_reviews;
        }
        assert titleUI == MessageTitleUI.VIEW_STORE_INFO : "Invalid title UI";
        return R.string.merchant_viewer_message_title;
    }
}
