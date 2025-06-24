// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.url.GURL;

import java.util.Objects;

/** Properties defined here reflect the visible state of the TouchToFillPaymentMethod component. */
class TouchToFillPaymentMethodProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel.WritableIntPropertyKey CURRENT_SCREEN =
            new PropertyModel.WritableIntPropertyKey("current_screen");
    public static final PropertyModel.WritableObjectPropertyKey<ModelList> SHEET_ITEMS =
            new PropertyModel.WritableObjectPropertyKey("sheet_items");
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> BACK_PRESS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("back_press_handler");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE, CURRENT_SCREEN, SHEET_ITEMS, BACK_PRESS_HANDLER, DISMISS_HANDLER
    };

    // Identifies different screens that can be dynamically displayed by the payments TTF bottom
    // sheet.
    @interface ScreenId {
        // The initial bottom sheet screen which offers the user to fill data into the form.
        int HOME_SCREEN = 0;

        // The screen displaying all loyalty cards of a user.
        int ALL_LOYALTY_CARDS_SCREEN = 1;
    }

    @interface ItemType {
        // The header at the top of the touch to fill sheet.
        int HEADER = 0;

        // A section containing the credit card data.
        int CREDIT_CARD = 1;

        // A section containing the IBAN data.
        int IBAN = 2;

        // A section containing the loyalty card data.
        int LOYALTY_CARD = 3;

        // An item which displays all user's loyalty cards upon click.
        int ALL_LOYALTY_CARDS = 4;

        // A "Continue" button, which is shown when there is only one payment
        // method available.
        int FILL_BUTTON = 5;

        // A button that redirects the user to the Wallet settings in Chrome.
        int WALLET_SETTINGS_BUTTON = 6;

        // A footer section containing additional actions.
        int FOOTER = 7;

        // A section with a terms label is present when card benefits are available.
        int TERMS_LABEL = 8;
    }

    /** Metadata associated with a card's image. */
    static class CardImageMetaData {
        public final int iconId;
        public final GURL artUrl;

        public CardImageMetaData(int iconId, GURL artUrl) {
            this.iconId = iconId;
            this.artUrl = artUrl;
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof CardImageMetaData)) return false;
            CardImageMetaData otherMetaData = ((CardImageMetaData) obj);
            return iconId == otherMetaData.iconId && Objects.equals(artUrl, otherMetaData.artUrl);
        }
    }

    /** Properties for a credit card suggestion entry in the TouchToFill sheet for payments. */
    static class CreditCardSuggestionProperties {
        static final PropertyModel.ReadableTransformingObjectPropertyKey<
                        CardImageMetaData, Drawable>
                CARD_IMAGE =
                        new PropertyModel.ReadableTransformingObjectPropertyKey<>("card_image");
        static final PropertyModel.ReadableObjectPropertyKey<String> MAIN_TEXT =
                new PropertyModel.ReadableObjectPropertyKey<>("main_text");
        static final PropertyModel.ReadableObjectPropertyKey<String> MAIN_TEXT_CONTENT_DESCRIPTION =
                new PropertyModel.ReadableObjectPropertyKey<>("main_text_content_description");
        static final PropertyModel.ReadableObjectPropertyKey<String> MINOR_TEXT =
                new PropertyModel.ReadableObjectPropertyKey<>("minor_text");
        static final PropertyModel.ReadableObjectPropertyKey<String> FIRST_LINE_LABEL =
                new PropertyModel.ReadableObjectPropertyKey<>("first_line_label");
        static final PropertyModel.ReadableObjectPropertyKey<String> SECOND_LINE_LABEL =
                new PropertyModel.ReadableObjectPropertyKey<>("second_line_label");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_CREDIT_CARD_CLICK_ACTION =
                new PropertyModel.ReadableObjectPropertyKey<>("on_credit_card_click_action");
        static final PropertyModel.ReadableBooleanPropertyKey APPLY_DEACTIVATED_STYLE =
                new PropertyModel.ReadableBooleanPropertyKey("apply_deactivated_style");
        static final PropertyModel.ReadableObjectPropertyKey<FillableItemCollectionInfo>
                ITEM_COLLECTION_INFO =
                        new PropertyModel.ReadableObjectPropertyKey<>("item_collection_info");

        static final PropertyKey[] NON_TRANSFORMING_CREDIT_CARD_SUGGESTION_KEYS = {
            MAIN_TEXT,
            MAIN_TEXT_CONTENT_DESCRIPTION,
            MINOR_TEXT,
            FIRST_LINE_LABEL,
            SECOND_LINE_LABEL,
            ON_CREDIT_CARD_CLICK_ACTION,
            APPLY_DEACTIVATED_STYLE,
            ITEM_COLLECTION_INFO
        };

        private CreditCardSuggestionProperties() {}
    }

    /** Properties for an IBAN entry in the TouchToFill sheet for payments. */
    static class IbanProperties {
        static final PropertyModel.ReadableObjectPropertyKey<String> IBAN_VALUE =
                new PropertyModel.ReadableObjectPropertyKey<>("iban_value");
        static final PropertyModel.ReadableObjectPropertyKey<String> IBAN_NICKNAME =
                new PropertyModel.ReadableObjectPropertyKey<>("iban_nickname");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_IBAN_CLICK_ACTION =
                new PropertyModel.ReadableObjectPropertyKey<>("on_iban_click_action");

        static final PropertyKey[] NON_TRANSFORMING_IBAN_KEYS = {
            IBAN_VALUE, IBAN_NICKNAME, ON_IBAN_CLICK_ACTION
        };

        private IbanProperties() {}
    }

    /** Properties for a loyalty card entry in the TouchToFill sheet for payments. */
    static class LoyaltyCardProperties {
        static final PropertyModel.ReadableObjectPropertyKey<String> LOYALTY_CARD_NUMBER =
                new PropertyModel.ReadableObjectPropertyKey<>("loyalty_card_number");
        static final PropertyModel.ReadableObjectPropertyKey<String> MERCHANT_NAME =
                new PropertyModel.ReadableObjectPropertyKey<>("merchant_name");
        static final PropertyModel.ReadableTransformingObjectPropertyKey<LoyaltyCard, Drawable>
                LOYALTY_CARD_ICON =
                        new PropertyModel.ReadableTransformingObjectPropertyKey<>(
                                "loyalty_card_icon");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable>
                ON_LOYALTY_CARD_CLICK_ACTION =
                        new PropertyModel.ReadableObjectPropertyKey<>(
                                "on_loyalty_card_click_action");

        static final PropertyKey[] NON_TRANSFORMING_LOYALTY_CARD_KEYS = {
            LOYALTY_CARD_NUMBER, MERCHANT_NAME, ON_LOYALTY_CARD_CLICK_ACTION
        };

        private LoyaltyCardProperties() {}
    }

    /** Properties for the "All your loyalty cards" item in the TouchToFill sheet for payments. */
    static class AllLoyaltyCardsItemProperties {
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_CLICK_ACTION =
                new PropertyModel.ReadableObjectPropertyKey<>("all_loyalty_cards_on_click_action");

        static final PropertyKey[] ALL_KEYS = {ON_CLICK_ACTION};

        private AllLoyaltyCardsItemProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the terms message in the TouchToFill
     * sheet for payments.
     */
    static class TermsLabelProperties {
        static final PropertyModel.WritableBooleanPropertyKey CARD_BENEFITS_TERMS_AVAILABLE =
                new PropertyModel.WritableBooleanPropertyKey("card_benefits_terms_available");

        static final PropertyKey[] ALL_TERMS_LABEL_KEYS = {CARD_BENEFITS_TERMS_AVAILABLE};

        private TermsLabelProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the header in the TouchToFill sheet for
     * payments.
     */
    static class HeaderProperties {
        static final PropertyModel.ReadableIntPropertyKey IMAGE_DRAWABLE_ID =
                new PropertyModel.ReadableIntPropertyKey("image_drawable_id");
        static final PropertyModel.ReadableIntPropertyKey TITLE_ID =
                new PropertyModel.ReadableIntPropertyKey("title_id");
        static final PropertyModel.ReadableIntPropertyKey SUBTITLE_ID =
                new PropertyModel.ReadableIntPropertyKey("subtitle_id");

        static final PropertyKey[] ALL_KEYS = {IMAGE_DRAWABLE_ID, TITLE_ID, SUBTITLE_ID};

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of a button in the TouchToFill sheet for
     * payments.
     */
    static class ButtonProperties {
        static final PropertyModel.ReadableIntPropertyKey TEXT_ID =
                new PropertyModel.ReadableIntPropertyKey("text_id");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("open_click_action");

        static final PropertyKey[] ALL_KEYS = {TEXT_ID, ON_CLICK_ACTION};

        private ButtonProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the footer in the TouchToFill sheet for
     * payments.
     */
    static class FooterProperties {
        static final PropertyModel.WritableBooleanPropertyKey SHOULD_SHOW_SCAN_CREDIT_CARD =
                new PropertyModel.WritableBooleanPropertyKey("should_show_scan_credit_card");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> SCAN_CREDIT_CARD_CALLBACK =
                new ReadableObjectPropertyKey<>("scan_credit_card_callback");
        static final PropertyModel.ReadableIntPropertyKey OPEN_MANAGEMENT_UI_TITLE_ID =
                new PropertyModel.ReadableIntPropertyKey("open_management_ui_title_id");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> OPEN_MANAGEMENT_UI_CALLBACK =
                new ReadableObjectPropertyKey<>("open_management_ui_callback");

        static final PropertyKey[] ALL_KEYS = {
            SHOULD_SHOW_SCAN_CREDIT_CARD,
            SCAN_CREDIT_CARD_CALLBACK,
            OPEN_MANAGEMENT_UI_TITLE_ID,
            OPEN_MANAGEMENT_UI_CALLBACK
        };

        private FooterProperties() {}
    }

    private TouchToFillPaymentMethodProperties() {}
}
