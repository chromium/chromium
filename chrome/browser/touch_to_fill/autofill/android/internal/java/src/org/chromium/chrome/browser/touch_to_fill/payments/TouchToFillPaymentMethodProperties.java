// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
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
    public static final PropertyModel.ReadableObjectPropertyKey<ModelList> SHEET_ITEMS =
            new PropertyModel.ReadableObjectPropertyKey("sheet_items");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");

    static final PropertyKey[] ALL_KEYS = {VISIBLE, SHEET_ITEMS, DISMISS_HANDLER};

    @interface ItemType {
        // The header at the top of the touch to fill sheet.
        int HEADER = 0;

        // A section containing the credit card data.
        int CREDIT_CARD = 1;

        // A section containing the IBAN data.
        int IBAN = 2;

        // A "Continue" button, which is shown when there is only one payment
        // method available.
        int FILL_BUTTON = 3;

        // A footer section containing additional actions.
        int FOOTER = 4;

        // A section with a terms label is present when card benefits are available.
        int TERMS_LABEL = 5;
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
        static final PropertyModel.ReadableObjectPropertyKey<String> NETWORK_NAME =
                new PropertyModel.ReadableObjectPropertyKey<>("network_name");
        static final PropertyModel.ReadableObjectPropertyKey<String> MAIN_TEXT =
                new PropertyModel.ReadableObjectPropertyKey<>("main_text");
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
            NETWORK_NAME,
            MAIN_TEXT,
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

        static final PropertyKey[] ALL_KEYS = {IMAGE_DRAWABLE_ID};

        private HeaderProperties() {}
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
        static final PropertyModel.ReadableObjectPropertyKey<Runnable>
                SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK =
                        new ReadableObjectPropertyKey<>("show_payment_method_settings_callback");

        static final PropertyKey[] ALL_KEYS = {
            SHOULD_SHOW_SCAN_CREDIT_CARD,
            SCAN_CREDIT_CARD_CALLBACK,
            SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK
        };

        private FooterProperties() {}
    }

    private TouchToFillPaymentMethodProperties() {}
}
