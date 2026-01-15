// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

/** Properties defined here reflect the visible state of the TouchToFillPaymentMethod component. */
class TouchToFillPaymentMethodProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel.WritableIntPropertyKey CURRENT_SCREEN =
            new PropertyModel.WritableIntPropertyKey("current_screen");
    static final PropertyModel.WritableIntPropertyKey FOCUSED_VIEW_ID_FOR_ACCESSIBILITY =
            new PropertyModel.WritableIntPropertyKey("focused_view_id_for_accessibility");
    public static final PropertyModel.WritableObjectPropertyKey<ModelList> SHEET_ITEMS =
            new PropertyModel.WritableObjectPropertyKey("sheet_items");
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> BACK_PRESS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("back_press_handler");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");
    static final PropertyModel.WritableIntPropertyKey SHEET_CONTENT_DESCRIPTION_ID =
            new PropertyModel.WritableIntPropertyKey("sheet_content_description_id");
    static final PropertyModel.WritableIntPropertyKey SHEET_HALF_HEIGHT_DESCRIPTION_ID =
            new PropertyModel.WritableIntPropertyKey("sheet_half_height_description_id");
    static final PropertyModel.WritableIntPropertyKey SHEET_FULL_HEIGHT_DESCRIPTION_ID =
            new PropertyModel.WritableIntPropertyKey("sheet_full_height_description_id");
    static final PropertyModel.WritableIntPropertyKey SHEET_CLOSED_DESCRIPTION_ID =
            new PropertyModel.WritableIntPropertyKey("sheet_closed_description_id");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE,
        CURRENT_SCREEN,
        FOCUSED_VIEW_ID_FOR_ACCESSIBILITY,
        SHEET_ITEMS,
        BACK_PRESS_HANDLER,
        DISMISS_HANDLER,
        SHEET_CONTENT_DESCRIPTION_ID,
        SHEET_HALF_HEIGHT_DESCRIPTION_ID,
        SHEET_FULL_HEIGHT_DESCRIPTION_ID,
        SHEET_CLOSED_DESCRIPTION_ID
    };

    // Identifies different screens that can be dynamically displayed by the payments TTF bottom
    // sheet.
    @interface ScreenId {
        // The initial bottom sheet screen which offers the user to fill data into the form.
        int HOME_SCREEN = 0;

        // The screen displaying all loyalty cards of a user.
        int ALL_LOYALTY_CARDS_SCREEN = 1;

        // The screen displaying the progress spinner.
        int PROGRESS_SCREEN = 2;

        // The screen displaying all available BNPL issuers.
        int BNPL_ISSUER_SELECTION_SCREEN = 3;

        // The screen displaying the error message and "OK" button.
        int ERROR_SCREEN = 4;

        // The screen displaying the legal messages for linking a new BNPL issuer.
        int BNPL_ISSUER_TOS_SCREEN = 5;
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

        // A section containing a clickable button with filled background color.
        int FILL_BUTTON = 5;

        // A button that redirects the user to the Wallet settings in Chrome.
        int WALLET_SETTINGS_BUTTON = 6;

        // A footer section containing additional actions.
        int FOOTER = 7;

        // A section with a terms label is present when card benefits are available.
        int TERMS_LABEL = 8;

        // A section containing the BNPL data.
        int BNPL = 9;

        // A section containing the progress spinner icon.
        int PROGRESS_ICON = 10;

        // The header at the top of the BNPL selection and progress screens.
        int BNPL_SELECTION_PROGRESS_HEADER = 11;

        // A section containing the BNPL issuer data.
        int BNPL_ISSUER = 12;

        // A section containing the error description.
        int ERROR_DESCRIPTION = 13;

        // A section contains texts shown on BNPL ToS screen.
        int BNPL_TOS_TEXT = 14;

        // The terms at the bottom of the BNPL selection and progress screens.
        int BNPL_SELECTION_PROGRESS_TERMS = 15;

        // A section contains legal messages shown in the screen footer.
        int TOS_FOOTER = 16;

        // A section containing a clickable button with no background.
        int TEXT_BUTTON = 17;

        // The header at the top of the BNPL ToS screen.
        int TOS_HEADER = 18;
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

    /** Properties for the BNPL ToS screen item in the TouchToFill sheet for payments. */
    static class BnplIssuerTosTextItemProperties {
        static final PropertyModel.ReadableIntPropertyKey BNPL_TOS_ICON_ID =
                new PropertyModel.ReadableIntPropertyKey("bnpl_tos_icon_id");
        static final PropertyModel.ReadableObjectPropertyKey<CharSequence> DESCRIPTION_TEXT =
                new PropertyModel.ReadableObjectPropertyKey<>("description_text");

        static final PropertyKey[] ALL_KEYS = {BNPL_TOS_ICON_ID, DESCRIPTION_TEXT};

        private BnplIssuerTosTextItemProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the terms message in the TouchToFill
     * sheet for payments.
     */
    static class TermsLabelProperties {
        static final PropertyModel.ReadableIntPropertyKey TERMS_LABEL_TEXT_ID =
                new PropertyModel.ReadableIntPropertyKey("terms_label_text_id");
        static final PropertyKey[] ALL_TERMS_LABEL_KEYS = {TERMS_LABEL_TEXT_ID};

        private TermsLabelProperties() {}
    }

    /** Properties for a BNPL entry in the TouchToFill sheet for payments. */
    static class BnplSuggestionProperties {
        static final PropertyModel.ReadableIntPropertyKey BNPL_ICON_ID =
                new PropertyModel.ReadableIntPropertyKey("bnpl_icon_id");
        static final PropertyModel.ReadableObjectPropertyKey<String> PRIMARY_TEXT =
                new PropertyModel.ReadableObjectPropertyKey<>("primary_text");
        static final PropertyModel.WritableObjectPropertyKey<String> SECONDARY_TEXT =
                new PropertyModel.WritableObjectPropertyKey<>("secondary_text");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_BNPL_CLICK_ACTION =
                new PropertyModel.ReadableObjectPropertyKey<>("on_bnpl_click_action");
        static final PropertyModel.WritableBooleanPropertyKey IS_ENABLED =
                new PropertyModel.WritableBooleanPropertyKey("is_enabled");
        static final PropertyModel.ReadableObjectPropertyKey<FillableItemCollectionInfo>
                BNPL_ITEM_COLLECTION_INFO =
                        new PropertyModel.ReadableObjectPropertyKey<>("bnpl_item_collection_info");

        static final PropertyKey[] NON_TRANSFORMING_BNPL_SUGGESTION_KEYS = {
            BNPL_ICON_ID,
            PRIMARY_TEXT,
            SECONDARY_TEXT,
            ON_BNPL_CLICK_ACTION,
            IS_ENABLED,
            BNPL_ITEM_COLLECTION_INFO
        };

        private BnplSuggestionProperties() {}
    }

    /** Properties for a progress icon entry in the TouchToFill sheet for payments. */
    static class ProgressIconProperties {
        static final PropertyModel.ReadableIntPropertyKey PROGRESS_CONTENT_DESCRIPTION_ID =
                new PropertyModel.ReadableIntPropertyKey("progress_content_description_id");

        static final PropertyKey[] ALL_KEYS = {PROGRESS_CONTENT_DESCRIPTION_ID};

        private ProgressIconProperties() {}
    }

    /** Properties for a BNPL issuer entry in the TouchToFill sheet for payments. */
    static class BnplIssuerContextProperties {
        static final PropertyModel.ReadableObjectPropertyKey<String> ISSUER_NAME =
                new PropertyModel.ReadableObjectPropertyKey<>("issuer_name");
        static final PropertyModel.ReadableObjectPropertyKey<String> ISSUER_SELECTION_TEXT =
                new PropertyModel.ReadableObjectPropertyKey<>("issuer_selection_text");
        static final PropertyModel.ReadableIntPropertyKey ISSUER_ICON_ID =
                new PropertyModel.ReadableIntPropertyKey("issuer_icon_id");
        static final PropertyModel.ReadableBooleanPropertyKey ISSUER_LINKED =
                new PropertyModel.ReadableBooleanPropertyKey("issuer_linked");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_ISSUER_CLICK_ACTION =
                new PropertyModel.ReadableObjectPropertyKey<>("on_issuer_click_action");
        static final PropertyModel.ReadableBooleanPropertyKey APPLY_ISSUER_DEACTIVATED_STYLE =
                new PropertyModel.ReadableBooleanPropertyKey("apply_issuer_deactivated_style");

        static final PropertyKey[] NON_TRANSFORMING_BNPL_ISSUER_CONTEXT_KEYS = {
            ISSUER_NAME,
            ISSUER_SELECTION_TEXT,
            ISSUER_ICON_ID,
            ISSUER_LINKED,
            ON_ISSUER_CLICK_ACTION,
            APPLY_ISSUER_DEACTIVATED_STYLE
        };

        private BnplIssuerContextProperties() {}
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
        static final PropertyModel.ReadableObjectPropertyKey<String> TITLE_STRING =
                new PropertyModel.ReadableObjectPropertyKey<>("title_string");

        static final PropertyKey[] ALL_KEYS = {
            IMAGE_DRAWABLE_ID, TITLE_ID, SUBTITLE_ID, TITLE_STRING
        };

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the BNPL ToS header in the TouchToFill
     * sheet for payments.
     */
    static class BnplTosHeaderProperties {
        static final PropertyModel.ReadableIntPropertyKey ISSUER_IMAGE_DRAWABLE_ID =
                new PropertyModel.ReadableIntPropertyKey("issuer_image_drawable_id");
        static final PropertyModel.ReadableObjectPropertyKey<String> ISSUER_TITLE_STRING =
                new PropertyModel.ReadableObjectPropertyKey<>("issuer_title_string");

        static final PropertyKey[] ALL_KEYS = {ISSUER_IMAGE_DRAWABLE_ID, ISSUER_TITLE_STRING};

        private BnplTosHeaderProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the BNPL header for selection and
     * progress screen in the TouchToFill sheet for payments.
     */
    static class BnplSelectionProgressHeaderProperties {
        static final PropertyModel.ReadableBooleanPropertyKey BNPL_BACK_BUTTON_ENABLED =
                new PropertyModel.ReadableBooleanPropertyKey("bnpl_back_button_enabled");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> BNPL_ON_BACK_BUTTON_CLICKED =
                new ReadableObjectPropertyKey<>("bnpl_on_back_button_clicked");

        static final PropertyKey[] ALL_KEYS = {
            BNPL_BACK_BUTTON_ENABLED, BNPL_ON_BACK_BUTTON_CLICKED
        };

        private BnplSelectionProgressHeaderProperties() {}
    }

    /** Properties for an error description entry in the TouchToFill sheet for payments. */
    static class ErrorDescriptionProperties {
        static final PropertyModel.ReadableObjectPropertyKey<String> ERROR_DESCRIPTION_STRING =
                new PropertyModel.ReadableObjectPropertyKey<>("error_description_string");

        static final PropertyKey[] ALL_KEYS = {ERROR_DESCRIPTION_STRING};

        private ErrorDescriptionProperties() {}
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

    /**
     * Properties defined here reflect the visible state of the BNPL terms for selection and
     * progress screen in the TouchToFill sheet for payments.
     */
    static class BnplSelectionProgressTermsProperties {
        static final PropertyModel.ReadableIntPropertyKey TERMS_TEXT_ID =
                new PropertyModel.ReadableIntPropertyKey("terms_text_id");
        static final PropertyModel.ReadableObjectPropertyKey<String> HIDE_OPTIONS_LINK_TEXT =
                new PropertyModel.ReadableObjectPropertyKey<>("hide_options_link_text");
        static final PropertyModel.ReadableObjectPropertyKey<Callback<View>>
                ON_LINK_CLICK_CALLBACK = new ReadableObjectPropertyKey<>("on_link_click_callback");
        static final PropertyModel.ReadableBooleanPropertyKey APPLY_LINK_DEACTIVATED_STYLE =
                new PropertyModel.ReadableBooleanPropertyKey("apply_link_deactivated_style");
        static final PropertyKey[] ALL_KEYS = {
            TERMS_TEXT_ID,
            HIDE_OPTIONS_LINK_TEXT,
            ON_LINK_CLICK_CALLBACK,
            APPLY_LINK_DEACTIVATED_STYLE
        };

        private BnplSelectionProgressTermsProperties() {}
    }

    /** Properties defined here reflect the visible state of the footer showing legal messages. */
    static class TosFooterProperties {
        static final PropertyModel.ReadableObjectPropertyKey<List<LegalMessageLine>>
                LEGAL_MESSAGE_LINES =
                        new PropertyModel.ReadableObjectPropertyKey<>("legal_message_lines");
        static final PropertyModel.ReadableObjectPropertyKey<Consumer<String>> LINK_OPENER =
                new PropertyModel.ReadableObjectPropertyKey<>("link_opener");

        static final PropertyKey[] ALL_KEYS = {LEGAL_MESSAGE_LINES, LINK_OPENER};

        private TosFooterProperties() {}
    }

    private TouchToFillPaymentMethodProperties() {}
}
