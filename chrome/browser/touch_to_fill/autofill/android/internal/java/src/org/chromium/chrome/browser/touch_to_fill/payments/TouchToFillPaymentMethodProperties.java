// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

/** Properties defined here reflect the visible state of the TouchToFillPaymentMethod component. */
final class TouchToFillPaymentMethodProperties {
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableIntPropertyKey CURRENT_SCREEN =
            new WritableIntPropertyKey("current_screen");
    static final WritableIntPropertyKey FOCUSED_VIEW_ID_FOR_ACCESSIBILITY =
            new WritableIntPropertyKey("focused_view_id_for_accessibility");
    public static final WritableObjectPropertyKey<ModelList> SHEET_ITEMS =
            new WritableObjectPropertyKey<>("sheet_items");
    static final ReadableObjectPropertyKey<Runnable> BACK_PRESS_HANDLER =
            new ReadableObjectPropertyKey<>("back_press_handler");
    static final ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new ReadableObjectPropertyKey<>("dismiss_handler");
    static final ReadableObjectPropertyKey<Callback<Integer>> TAB_SELECTION_HANDLER =
            new ReadableObjectPropertyKey<>("tab_selection_handler");
    static final WritableIntPropertyKey SHEET_CONTENT_DESCRIPTION_ID =
            new WritableIntPropertyKey("sheet_content_description_id");
    static final WritableIntPropertyKey SHEET_HALF_HEIGHT_DESCRIPTION_ID =
            new WritableIntPropertyKey("sheet_half_height_description_id");
    static final WritableIntPropertyKey SHEET_FULL_HEIGHT_DESCRIPTION_ID =
            new WritableIntPropertyKey("sheet_full_height_description_id");
    static final WritableIntPropertyKey SHEET_CLOSED_DESCRIPTION_ID =
            new WritableIntPropertyKey("sheet_closed_description_id");
    static final WritableIntPropertyKey SELECTED_TAB_INDEX =
            new WritableIntPropertyKey("selected_tab_index");
    static final WritableIntPropertyKey TABBED_HEADER_LOGO_DRAWABLE_ID =
            new WritableIntPropertyKey("tabbed_header_logo_drawable_id");
    static final WritableIntPropertyKey TABBED_HEADER_TITLE_ID =
            new WritableIntPropertyKey("tabbed_header_title_id");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE,
        CURRENT_SCREEN,
        FOCUSED_VIEW_ID_FOR_ACCESSIBILITY,
        SHEET_ITEMS,
        BACK_PRESS_HANDLER,
        DISMISS_HANDLER,
        TAB_SELECTION_HANDLER,
        SHEET_CONTENT_DESCRIPTION_ID,
        SHEET_HALF_HEIGHT_DESCRIPTION_ID,
        SHEET_FULL_HEIGHT_DESCRIPTION_ID,
        SHEET_CLOSED_DESCRIPTION_ID,
        SELECTED_TAB_INDEX,
        TABBED_HEADER_LOGO_DRAWABLE_ID,
        TABBED_HEADER_TITLE_ID
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

        // The tabbed version of the home screen, showing Pay now and Pay later options.
        int TABBED_HOME_SCREEN = 6;
    }

    @IntDef({PaymentMethodTabId.PAY_NOW, PaymentMethodTabId.PAY_LATER})
    @Retention(RetentionPolicy.SOURCE)
    @interface PaymentMethodTabId {
        int PAY_NOW = 0;
        int PAY_LATER = 1;
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
    static final class CardImageMetaData {
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
    static final class CreditCardSuggestionProperties {
        static final WritableObjectPropertyKey<Drawable> CARD_IMAGE =
                new WritableObjectPropertyKey<>("card_image");
        static final ReadableObjectPropertyKey<GURL> CARD_ART_URL =
                new ReadableObjectPropertyKey<>("card_art_url");
        static final ReadableIntPropertyKey CARD_ICON_ID =
                new ReadableIntPropertyKey("card_icon_id");
        static final ReadableObjectPropertyKey<String> MAIN_TEXT =
                new ReadableObjectPropertyKey<>("main_text");
        static final ReadableObjectPropertyKey<String> MAIN_TEXT_CONTENT_DESCRIPTION =
                new ReadableObjectPropertyKey<>("main_text_content_description");
        static final ReadableObjectPropertyKey<String> MINOR_TEXT =
                new ReadableObjectPropertyKey<>("minor_text");
        static final ReadableObjectPropertyKey<String> FIRST_LINE_LABEL =
                new ReadableObjectPropertyKey<>("first_line_label");
        static final ReadableObjectPropertyKey<String> SECOND_LINE_LABEL =
                new ReadableObjectPropertyKey<>("second_line_label");
        static final ReadableObjectPropertyKey<Runnable> ON_CREDIT_CARD_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("on_credit_card_click_action");
        static final ReadableBooleanPropertyKey APPLY_DEACTIVATED_STYLE =
                new ReadableBooleanPropertyKey("apply_deactivated_style");
        static final ReadableObjectPropertyKey<FillableItemCollectionInfo> ITEM_COLLECTION_INFO =
                new ReadableObjectPropertyKey<>("item_collection_info");

        static final PropertyKey[] NON_TRANSFORMING_CREDIT_CARD_SUGGESTION_KEYS = {
            CARD_IMAGE,
            CARD_ART_URL,
            CARD_ICON_ID,
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
    static final class IbanProperties {
        static final ReadableObjectPropertyKey<String> IBAN_VALUE =
                new ReadableObjectPropertyKey<>("iban_value");
        static final ReadableObjectPropertyKey<String> IBAN_NICKNAME =
                new ReadableObjectPropertyKey<>("iban_nickname");
        static final ReadableObjectPropertyKey<Runnable> ON_IBAN_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("on_iban_click_action");

        static final PropertyKey[] NON_TRANSFORMING_IBAN_KEYS = {
            IBAN_VALUE, IBAN_NICKNAME, ON_IBAN_CLICK_ACTION
        };

        private IbanProperties() {}
    }

    /** Properties for a loyalty card entry in the TouchToFill sheet for payments. */
    static final class LoyaltyCardProperties {
        static final ReadableObjectPropertyKey<LoyaltyCard> LOYALTY_CARD =
                new ReadableObjectPropertyKey<>("loyalty_card");
        static final WritableObjectPropertyKey<Drawable> LOYALTY_CARD_ICON =
                new WritableObjectPropertyKey<>("loyalty_card_icon");
        static final ReadableObjectPropertyKey<Runnable> ON_LOYALTY_CARD_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("on_loyalty_card_click_action");

        static final PropertyKey[] NON_TRANSFORMING_LOYALTY_CARD_KEYS = {
            LOYALTY_CARD, LOYALTY_CARD_ICON, ON_LOYALTY_CARD_CLICK_ACTION
        };

        private LoyaltyCardProperties() {}
    }

    /** Properties for the "All your loyalty cards" item in the TouchToFill sheet for payments. */
    static final class AllLoyaltyCardsItemProperties {
        static final ReadableObjectPropertyKey<Runnable> ON_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("all_loyalty_cards_on_click_action");

        static final PropertyKey[] ALL_KEYS = {ON_CLICK_ACTION};

        private AllLoyaltyCardsItemProperties() {}
    }

    /** Properties for the BNPL ToS screen item in the TouchToFill sheet for payments. */
    static final class BnplIssuerTosTextItemProperties {
        static final ReadableIntPropertyKey BNPL_TOS_ICON_ID =
                new ReadableIntPropertyKey("bnpl_tos_icon_id");
        static final ReadableObjectPropertyKey<CharSequence> DESCRIPTION_TEXT =
                new ReadableObjectPropertyKey<>("description_text");

        static final PropertyKey[] ALL_KEYS = {BNPL_TOS_ICON_ID, DESCRIPTION_TEXT};

        private BnplIssuerTosTextItemProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the terms message in the TouchToFill
     * sheet for payments.
     */
    static final class TermsLabelProperties {
        static final ReadableIntPropertyKey TERMS_LABEL_TEXT_ID =
                new ReadableIntPropertyKey("terms_label_text_id");
        static final PropertyKey[] ALL_TERMS_LABEL_KEYS = {TERMS_LABEL_TEXT_ID};

        private TermsLabelProperties() {}
    }

    /** Properties for a BNPL entry in the TouchToFill sheet for payments. */
    static final class BnplSuggestionProperties {
        static final ReadableIntPropertyKey BNPL_ICON_ID =
                new ReadableIntPropertyKey("bnpl_icon_id");
        static final ReadableObjectPropertyKey<String> PRIMARY_TEXT =
                new ReadableObjectPropertyKey<>("primary_text");
        static final WritableObjectPropertyKey<String> SECONDARY_TEXT =
                new WritableObjectPropertyKey<>("secondary_text");
        static final ReadableObjectPropertyKey<Runnable> ON_BNPL_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("on_bnpl_click_action");
        static final WritableBooleanPropertyKey IS_ENABLED =
                new WritableBooleanPropertyKey("is_enabled");
        static final ReadableObjectPropertyKey<FillableItemCollectionInfo>
                BNPL_ITEM_COLLECTION_INFO =
                        new ReadableObjectPropertyKey<>("bnpl_item_collection_info");

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
    static final class ProgressIconProperties {
        static final ReadableIntPropertyKey PROGRESS_CONTENT_DESCRIPTION_ID =
                new ReadableIntPropertyKey("progress_content_description_id");

        static final PropertyKey[] ALL_KEYS = {PROGRESS_CONTENT_DESCRIPTION_ID};

        private ProgressIconProperties() {}
    }

    /** Properties for a BNPL issuer entry in the TouchToFill sheet for payments. */
    static final class BnplIssuerContextProperties {
        static final ReadableObjectPropertyKey<String> ISSUER_NAME =
                new ReadableObjectPropertyKey<>("issuer_name");
        static final ReadableObjectPropertyKey<String> ISSUER_SELECTION_TEXT =
                new ReadableObjectPropertyKey<>("issuer_selection_text");
        static final ReadableIntPropertyKey ISSUER_ICON_ID =
                new ReadableIntPropertyKey("issuer_icon_id");
        static final ReadableBooleanPropertyKey ISSUER_LINKED =
                new ReadableBooleanPropertyKey("issuer_linked");
        static final ReadableObjectPropertyKey<Runnable> ON_ISSUER_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("on_issuer_click_action");
        static final ReadableBooleanPropertyKey APPLY_ISSUER_DEACTIVATED_STYLE =
                new ReadableBooleanPropertyKey("apply_issuer_deactivated_style");

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
    static final class HeaderProperties {
        static final ReadableIntPropertyKey IMAGE_DRAWABLE_ID =
                new ReadableIntPropertyKey("image_drawable_id");
        static final ReadableIntPropertyKey TITLE_ID = new ReadableIntPropertyKey("title_id");
        static final ReadableIntPropertyKey SUBTITLE_ID = new ReadableIntPropertyKey("subtitle_id");
        static final ReadableObjectPropertyKey<String> TITLE_STRING =
                new ReadableObjectPropertyKey<>("title_string");

        static final PropertyKey[] ALL_KEYS = {
            IMAGE_DRAWABLE_ID, TITLE_ID, SUBTITLE_ID, TITLE_STRING
        };

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the BNPL ToS header in the TouchToFill
     * sheet for payments.
     */
    static final class BnplTosHeaderProperties {
        static final ReadableIntPropertyKey ISSUER_IMAGE_DRAWABLE_ID =
                new ReadableIntPropertyKey("issuer_image_drawable_id");
        static final ReadableObjectPropertyKey<String> ISSUER_TITLE_STRING =
                new ReadableObjectPropertyKey<>("issuer_title_string");

        static final ReadableIntPropertyKey ICON_CONTENT_DESCRIPTION_ID =
                new ReadableIntPropertyKey("icon_content_description_id");

        static final PropertyKey[] ALL_KEYS = {
            ISSUER_IMAGE_DRAWABLE_ID, ISSUER_TITLE_STRING, ICON_CONTENT_DESCRIPTION_ID
        };

        private BnplTosHeaderProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the BNPL header for selection and
     * progress screen in the TouchToFill sheet for payments.
     */
    static final class BnplSelectionProgressHeaderProperties {
        static final ReadableBooleanPropertyKey BNPL_BACK_BUTTON_ENABLED =
                new ReadableBooleanPropertyKey("bnpl_back_button_enabled");
        static final ReadableObjectPropertyKey<Runnable> BNPL_ON_BACK_BUTTON_CLICKED =
                new ReadableObjectPropertyKey<>("bnpl_on_back_button_clicked");

        static final PropertyKey[] ALL_KEYS = {
            BNPL_BACK_BUTTON_ENABLED, BNPL_ON_BACK_BUTTON_CLICKED
        };

        private BnplSelectionProgressHeaderProperties() {}
    }

    /** Properties for an error description entry in the TouchToFill sheet for payments. */
    static final class ErrorDescriptionProperties {
        static final ReadableObjectPropertyKey<String> ERROR_DESCRIPTION_STRING =
                new ReadableObjectPropertyKey<>("error_description_string");

        static final PropertyKey[] ALL_KEYS = {ERROR_DESCRIPTION_STRING};

        private ErrorDescriptionProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of a button in the TouchToFill sheet for
     * payments.
     */
    static final class ButtonProperties {
        static final ReadableIntPropertyKey TEXT_ID = new ReadableIntPropertyKey("text_id");
        static final ReadableObjectPropertyKey<Runnable> ON_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("on_click_action");

        static final PropertyKey[] ALL_KEYS = {TEXT_ID, ON_CLICK_ACTION};

        private ButtonProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the footer in the TouchToFill sheet for
     * payments.
     */
    static final class FooterProperties {
        static final WritableBooleanPropertyKey SHOULD_SHOW_SCAN_CREDIT_CARD =
                new WritableBooleanPropertyKey("should_show_scan_credit_card");
        static final ReadableObjectPropertyKey<Runnable> SCAN_CREDIT_CARD_CALLBACK =
                new ReadableObjectPropertyKey<>("scan_credit_card_callback");
        static final ReadableIntPropertyKey OPEN_MANAGEMENT_UI_TITLE_ID =
                new ReadableIntPropertyKey("open_management_ui_title_id");
        static final ReadableObjectPropertyKey<Runnable> OPEN_MANAGEMENT_UI_CALLBACK =
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
    static final class BnplSelectionProgressTermsProperties {
        static final ReadableObjectPropertyKey<CharSequence> TERMS_TEXT =
                new ReadableObjectPropertyKey<>("terms_text");
        static final ReadableBooleanPropertyKey TERMS_LINK_ENABLED =
                new ReadableBooleanPropertyKey("terms_link_enabled");
        static final PropertyKey[] ALL_KEYS = {TERMS_TEXT, TERMS_LINK_ENABLED};

        private BnplSelectionProgressTermsProperties() {}
    }

    /** Properties defined here reflect the visible state of the footer showing legal messages. */
    // TODO(crbug.com/486199794): Change TosFooterProperties to only contain a CharSequence
    //
    static final class TosFooterProperties {
        static final ReadableObjectPropertyKey<List<LegalMessageLine>> LEGAL_MESSAGE_LINES =
                new ReadableObjectPropertyKey<>("legal_message_lines");
        static final ReadableObjectPropertyKey<Consumer<String>> LINK_OPENER =
                new ReadableObjectPropertyKey<>("link_opener");

        static final PropertyKey[] ALL_KEYS = {LEGAL_MESSAGE_LINES, LINK_OPENER};

        private TosFooterProperties() {}
    }

    private TouchToFillPaymentMethodProperties() {}
}
